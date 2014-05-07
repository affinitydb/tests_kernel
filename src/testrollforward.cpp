/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h" // MV auto pointers
#include "collectionhelp.h"			// If reading collections
#include "teststream.h"				// If using streams
using namespace std;

#define TEST_COMPLETE_REBUILD 0 // Not supported.  The master page of the store
								// contains some of the crypto data that would
								// be necessary to regenerate the same store

class TestRollforward : public ITest
{
	public:
		TEST_DECLARE(TestRollforward);

		virtual char const * getName() const { return "testrollforward"; }
		virtual char const * getDescription() const { return "10960"; }
		virtual char const * getHelp() const { return ""; } 
		
		virtual int execute();
		virtual void destroy() { delete this; }

		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Creates/Deletes store files"; return false; }
	protected:
		void doTest(bool bFromScratch) ;

		void insertDataPhase1();
		void insertDataPhase2();
		void testExpectedData();
	private:	
		ISession * mSession ;
		PID mPid1,mPid2;

};
TEST_IMPLEMENT(TestRollforward, TestLogger::kDStdOut);

int TestRollforward::execute()
{
	// rollforward from older store
	doTest( false ) ;

#if TEST_COMPLETE_REBUILD
	// Store should support recreating entire .dat file
	// based on the .log files and initial store
	// parameter (identity, store id etc)
	doTest( true ) ;
#endif
	return RC_OK ;
}

void TestRollforward::doTest( bool bFromScratch )
{
	MVTApp::deleteStore() ;

	// Create new store
	MVTApp::Suite().mbArchiveLogs = true ; // STARTUP_ARCHIVE_LOGS
	if (!MVTApp::startStore()) { TVERIFY(!"Unable to start store"); return ; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	insertDataPhase1();

	mSession->terminate() ;
	MVTApp::stopStore() ;

	// Backup the .dat file
	std::string backupLocation;
	if ( !bFromScratch )
	{
		// Take a backup of the .dat file with the 1 pin inside it
		// (log files are also copied but we don't care about them)
		backupLocation = MVTApp::Suite().mDir ;
	#ifdef WIN32
		backupLocation +="\\";
	#else
		backupLocation +="/";
	#endif
		backupLocation +="rollforwardbackup";
		MVTUtil::backupStoreFiles(MVTApp::Suite().mDir.c_str(),backupLocation.c_str());
		mLogger.out() << "** Backed up store files to " << backupLocation << std::endl;
	}

	// Add second pin
	MVTApp::startStore() ;
	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	insertDataPhase2();

	mSession->terminate() ;
	MVTApp::stopStore() ;
	
	// Restore older dat file (but leave newer logs)
	string lCmd ;
#ifdef WIN32
	lCmd = "/C del " ;
	lCmd += MVTApp::Suite().mDir ;
	lCmd += "\\" STOREPREFIX DATAFILESUFFIX " " ;
	MVTUtil::executeProcess("cmd.exe", lCmd.c_str());

	if ( !bFromScratch )
	{
		// Restore the dat file that contained one pin
		lCmd = "/C copy /Y " ;
		lCmd += backupLocation ;
		lCmd += "\\" STOREPREFIX DATAFILESUFFIX " " ;
		lCmd += MVTApp::Suite().mDir ;
		MVTUtil::executeProcess("cmd.exe", lCmd.c_str());
	}
#else
	lCmd = "bash -c \"rm " ;
	lCmd += MVTApp::Suite().mDir ;
	lCmd += "/" STOREPREFIX DATAFILESUFFIX " " ;
	lCmd += "\"" ;
	TVERIFY(-1 != system(lCmd.c_str()));

	if ( !bFromScratch )
	{
		lCmd = "bash -c \"cp " ;
		lCmd += backupLocation ;
		lCmd += "/" STOREPREFIX "*" DATAFILESUFFIX " " ;
		lCmd += MVTApp::Suite().mDir ;
		lCmd += "\"" ;
		TVERIFY(-1 != system(lCmd.c_str()));
	}
#endif
	mLogger.out() << "** Restored older data file (while preserving new logs): " << lCmd << std::endl;

	// Reopen and roll forward logs 
	// We expect to be restored to the complete store file
	mLogger.out() << "** Reopening the old data file with new logs in STARTUP_ROLLFORWARD mode" << std::endl;
	MVTApp::Suite().mbRollforward = true ; // STARTUP_ROLLFORWARD
	MVTApp::startStore() ;
	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	testExpectedData();

	mSession->terminate() ;
	MVTApp::stopStore() ;
}

void TestRollforward::insertDataPhase1()
{	
	// Insert first chunk of data

	// REVIEW: This could become more sophisticated 
	// to cover more aspects of rollforward recovery.
	// But normal recovery testing should touch on some 
	// of the same ground
	mLogger.out() << "** Insert phase 1" << std::endl;
	IPIN *pin;
	TVERIFYRC(mSession->createPIN (NULL,0,&pin,MODE_PERSISTENT));
	mPid1 = pin->getPID();
	if(pin!=NULL) pin->destroy();
}
void TestRollforward::insertDataPhase2()
{
	mLogger.out() << "** Insert phase 2" << std::endl;
	IPIN *pin;
	TVERIFYRC(mSession->createPIN (NULL,0,&pin,MODE_PERSISTENT));
	mPid2 = pin->getPID();
	TVERIFY(mPid1.pid != mPid2.pid);
	if(pin!=NULL) pin->destroy();
}
void TestRollforward::testExpectedData()
{
	// Verify all data from both phase 1 and phase 2 
	// present in the store

	// Phase 1 data
	CmvautoPtr<IPIN> p1(mSession->getPIN(mPid1));
	TVERIFY(p1.IsValid());

	// Phase 2 data
	CmvautoPtr<IPIN> p2(mSession->getPIN(mPid2));
	TVERIFY(p2.IsValid());
}
