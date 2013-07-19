/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

/*
testrecovery.cpp

This file provides a base class for use when creating recovery tests.
Each individual test will derive from TestRecoveryBase and then add its own specific scenario (doTest()).

The resulting test is run with
	"tests TESTNAME"

All tests of this type will create two stores (from scratch), 
one inside "reference" directory, second inside "crash" directory.
The reference directory contains the scenario data with the store properly shut down.
The crash has similar data but without a proper shutdown.
The test will fail if the pins are not the same in both stores, 
and each test can add further checks (testRecoveredStore())
*/

#include "app.h"
#include "mvauto.h"
#include "md5stream.h"
#include "teststream.h"

#define TEST_RECOVERY_FROM_SCRATCH 1 // 6273, 6274 - Store is corrupted if it hasn't had a clean shutdown

/*
To REALLY test recovery you have to restart the machine completely without clean shutdown.

To do this:

1-Enable this flag
2-Run test with "t" flag
3-Restart machine when it says "PULL THE PLUG"
4-Run test with "2" flag to do the second stage
*/
#define TEST_REBOOT_ENTIRE_MACHINE 0 

#ifdef WIN32
#define REFERENCE_DIR ".\\ms0"
#define CRASH_DIR ".\\ms1"
#else
#define REFERENCE_DIR "./ms0"
#define CRASH_DIR "./ms1"
#endif

#define REFERENCE_INDEX 0
#define CRASH_INDEX 1

using namespace std;

class TestRecoveryBase : public ITest
{
	public:
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Recovery scenario test"; }
		virtual int execute();
		virtual void destroy() { delete this; }

		virtual bool includeInSmokeTest(char const *& pReason) const { /* Note: (1) Specialized test that erases and crashs (2) takes arguments */ return true; }
		virtual bool isPerformingFullScanQueries() const { return true; }
	protected:
		void doReferenceScenario();
		void doCrashScenario();
		void testRecovery();
		bool compareStorePins() ;
		void addBasicContent() ;
		void getReferenceState() ;

		// Derived classes implements this method to perform actions
		// in either the reference or crashing store.  Normally the actions 
		// for both stores should be exactly the same,
		// except for any uncommitted data 
		virtual void doTest( bool inReference ) = 0 ;

		// Override this method to do additional checks on the expected content of the store
		virtual void testRecoveredStore() { ; } 

	protected:
		struct PinInfo 
		{			
			PID pid ;
			unsigned char md5[16] ;
		} ;
		vector<PinInfo> mReferencePins; // List of all PINs in the reference store

		ISession * mSession ;
		string mStoreDir ;
};

int TestRecoveryBase::execute()
{
	string inMode; 
	if(!mpArgs->get_param("mode", inMode))
	{
		mLogger.out() << "--mode parameter no specified. Running full test" << endl;
	}
	    
	if ( isVerbose() ) 
		mLogger.out() << "TestRecoveryBase mode " << inMode << endl;

	if ( inMode.empty() ||
		 0 == strncasecmp( inMode.c_str(), "t", 1 ) )
	{
		// Running full test
		MVTUtil::ensureDir( REFERENCE_DIR ) ;
		MVTUtil::ensureDir( CRASH_DIR ) ;

		MVTApp::sReporter.enable(isVerbose()); // Reduced messages
		doReferenceScenario() ;
		MVTApp::sReporter.enable(true);

		//Call back into this test as another separate process to perform the crashing portion
		string cmd; stringstream lCrashArgs;
		lCrashArgs << getName();
		lCrashArgs << " --mode=c -noui";
		MVTApp::buildCommandLine( MVTApp::Suite(), lCrashArgs );
		MVTUtil::executeProcess( MVTApp::mAppName.c_str(), lCrashArgs.str().c_str(), NULL,NULL,false,isVerbose()) ;		

		if (isVerbose()) mLogger.out() << "executeProcess done" << endl;
		testRecovery() ;

		// Cleanup (disable if you want to investigate)
		mStoreDir = REFERENCE_DIR ; 
		string ioinit = MVTUtil::completeMultiStoreIOInitString(MVTApp::Suite().mIOInit.c_str(), REFERENCE_INDEX);
		MVTUtil::deleteStore(ioinit.c_str(),mStoreDir.c_str());
		mStoreDir = CRASH_DIR ; 
		ioinit = MVTUtil::completeMultiStoreIOInitString(MVTApp::Suite().mIOInit.c_str(), CRASH_INDEX);
		MVTUtil::deleteStore(ioinit.c_str(),mStoreDir.c_str());

	}
	else if ( 0 == strncasecmp( inMode.c_str(), "c", 1 ) )
	{
		// Test is being re-launched with the crash scenario	
		doCrashScenario() ;
	}
	/*
	// If useful these stages can also be performed individually
	else if ( 0 == strncasecmp( inMode.c_str(), "r", 1 ) )
	{
		doReferenceScenario() ;
	}
	*/
	else if ( 0 == strncasecmp( inMode.c_str(), "2", 1 ) )
	{
		mStoreDir = REFERENCE_DIR ; 
		string ioinit = MVTUtil::completeMultiStoreIOInitString(MVTApp::Suite().mIOInit.c_str(), REFERENCE_INDEX);
		if (!MVTApp::startStore(NULL,NULL,mStoreDir.c_str(),NULL,NULL,0,0,0,ioinit.c_str()))
		{
			mLogger.out() << "Reference not found - run \"t\" mode first" << endl ;
		}
		mSession = MVTApp::startSession() ;
		getReferenceState() ;
		mSession->terminate() ; mSession = NULL ;
		MVTApp::stopStore() ;

		testRecovery() ;
	}
	return RC_OK  ;
}

void TestRecoveryBase::getReferenceState()
{
	//
	// remember all the pid data
	//
	IStmt * lQ = mSession->createStmt() ;
	lQ->addVariable() ;

	uint64_t cntPins ;
	lQ->count(cntPins) ;

	mLogger.out() << "Found " << cntPins << " in reference store" << endl ;

	mReferencePins.resize((size_t)cntPins);

	IPIN * p = NULL ;
	size_t i = 0 ;

	ICursor * lR = NULL;
	TVERIFYRC(lQ->execute(&lR));
	
	while ( NULL != ( p = lR->next() ) )
	{
		mReferencePins[i].pid = p->getPID() ;

		Md5Stream lS1 ;
		MvStoreSerialization::ContextOutComparisons lSerCtx1(lS1, *mSession);
		MvStoreSerialization::OutComparisons::properties(lSerCtx1, *p); 
		lS1.flush_md5(mReferencePins[i].md5);

		if(isVerbose())
			mLogger.out() << std::hex << mReferencePins[i].pid << ":" << std::dec << endl ;

		p->destroy();
		i++;
		assert( i <= cntPins ) ;
	}	
	lR->destroy() ; lR = NULL ;
	lQ->destroy() ; lQ = NULL ;
}

void TestRecoveryBase::doReferenceScenario()
{
	mLogger.out() << "Generating test reference store" << endl ;

	mStoreDir = REFERENCE_DIR ; 
	string ioinit = MVTUtil::completeMultiStoreIOInitString(MVTApp::Suite().mIOInit.c_str(), REFERENCE_INDEX);
	MVTUtil::deleteStore(ioinit.c_str(),mStoreDir.c_str());

	MVTApp::startStore(NULL,NULL,mStoreDir.c_str(),NULL,NULL,0,0,0,ioinit.c_str()) ;

	mSession = MVTApp::startSession() ;

#if !TEST_RECOVERY_FROM_SCRATCH
	addBasicContent() ;
	
	// We need to commit even in the reference scenario to 
	// force the same pin/page layout as happens in the crash scenario
	mLogger.out() << "\tStopping store to commit initial content" << endl ;
	mSession->terminate() ; mSession = NULL ;
	MVTApp::stopStore() ;
	MVTApp::startStore(NULL,NULL,mStoreDir.c_str(),NULL,NULL,0,0,0,ioinit.c_str()) ;
	mSession = MVTApp::startSession() ;
#endif

	doTest( true /*reference*/ ) ;

	getReferenceState() ;

	// Only single store can be open in this thread so we have to close the 
	// reference store
	mSession->terminate() ; mSession = NULL ;
	MVTApp::stopStore() ;
}

bool TestRecoveryBase::compareStorePins()
{
	// With the recovered store open, compare with the PIN data
	// extracted from the reference store

	IStmt * lQ ;
	IPIN * p = NULL ;
	size_t i = 0 ;
	ICursor * lR ;
	std::stringstream err ;

	lQ = mSession->createStmt() ;
	lQ->addVariable() ;

	uint64_t recoveredCntPins ;
	lQ->count(recoveredCntPins) ;

	if ( mReferencePins.size() != recoveredCntPins )
	{
		err << "Invalid number of pins in store.  Expected: "
			<< (unsigned int)mReferencePins.size() << " got " << (unsigned int)recoveredCntPins  ;
		TVERIFY2(0,err.str().c_str());

		return false ;
	}

	i = 0 ;
	TVERIFYRC(lQ->execute(&lR)) ;

	PID recoveredpid ;
	unsigned char recoveredmd5[16] ;

	while ( NULL != ( p = lR->next() ) )
	{
		recoveredpid = p->getPID() ;

		if ( isVerbose() )
			mLogger.out() << std::hex << "Comparing: " << mReferencePins[i].pid << " got: " << recoveredpid << std::dec << endl ;

		if ( recoveredpid != mReferencePins[i].pid ) 
		{
			err << "PID mismatch" 
				<< std::hex << "Expected: " << mReferencePins[i].pid 
				<< " got: " << recoveredpid << std::dec ;
			TVERIFY2(0,err.str().c_str());

			return false ;
		}

		Md5Stream lS1 ;
		MvStoreSerialization::ContextOutComparisons lSerCtx1(lS1, *mSession);
		MvStoreSerialization::OutComparisons::properties(lSerCtx1, *p); 
		lS1.flush_md5(recoveredmd5);

		if ( 0 != memcmp(recoveredmd5,mReferencePins[i].md5,16) )
		{
			TVERIFY(!"PIN mismatch") ; 

			// REVIEW: we would have to open the original reference store also if we wanted to print that
			// pin
			MVTApp::output( *p, mLogger.out(), mSession ) ; 
			return false ;
		}

		p->destroy();
		i++;

		if ( i > mReferencePins.size() ) 
		{ 
			err << "Too many pins in recovered store, after" << std::hex << recoveredpid.pid << std::dec ;
			TVERIFY2(0,err.str().c_str());
			return false ;
		}
	}	
	lR->destroy() ; lR = NULL ;
	lQ->destroy() ; lQ = NULL ;

	mLogger.out() << "All pins have been recovered" << endl ;
	return true ;
}

void TestRecoveryBase::doCrashScenario()
{
	mLogger.out() << "Performing crash scenario..." << endl ;
	assert( mReferencePins.empty() ) ; // Expect to be called in separate process 

	mStoreDir = CRASH_DIR ;

	string ioinit = MVTUtil::completeMultiStoreIOInitString(MVTApp::Suite().mIOInit.c_str(), CRASH_INDEX);
	MVTUtil::deleteStore(ioinit.c_str(),mStoreDir.c_str());

	MVTApp::startStore(NULL,NULL,mStoreDir.c_str(),NULL,NULL,0,0,0,ioinit.c_str()) ;

#if !TEST_RECOVERY_FROM_SCRATCH
	// To force the store to be established on disk.
	// This doesn't help as a workaround to pins being lost
	mSession = MVTApp::startSession() ;
	addBasicContent() ;
	mLogger.out() << "\tStopping store to commit initial content" << endl ;
	mSession->terminate() ; mSession = NULL ;
	MVTApp::stopStore() ;
	MVTApp::startStore(NULL,NULL,mStoreDir.c_str(),NULL,NULL,0,0,0,ioinit.c_str()) ;
#endif

	mSession = MVTApp::startSession() ;
	doTest( false /*reference*/ ) ;

#if TEST_REBOOT_ENTIRE_MACHINE
	mLogger.out() << "\n\nPULL THE PLUG NOW!\n\n" ;
	getchar() ;
#elif 1
//	_exit(1) ; // _exit is messier exit than exit
	exit(1);

//	abort() ; // testframework -noui option now works to allow this without message box
#elif 0
	// No crash but don't properly shut down the store	
#else
	// For a clean shutdown you would need to reach this...
	// (enable this to test that the test is correct!)
	mSession->terminate() ;
	MVTApp::stopStore() ;
#endif
}

void TestRecoveryBase::testRecovery()
{
	mLogger.out() << "Sleeping..." << endl ;	
	MVTestsPortability::threadSleep(3000); // 11227

	mLogger.out() << "Recovering from crash..." << endl ;	

	assert( mSession == NULL ) ;

	//Open the crash store to force the recovery
	//
	string ioinit = MVTUtil::completeMultiStoreIOInitString(MVTApp::Suite().mIOInit.c_str(), CRASH_INDEX);
	if ( !MVTApp::startStore(NULL,NULL,CRASH_DIR,NULL,NULL,0,0,0,ioinit.c_str()) )
	{
		TVERIFY(!"Could not open crashed store") ;
		return ;
	}	

	mSession = MVTApp::startSession() ;
	TVERIFY( mSession != NULL );

	if ( compareStorePins() )
	{
		// Perform any test specific checks
		testRecoveredStore() ;
	}

	mSession->terminate() ;
	MVTApp::stopStore() ;	
}

void TestRecoveryBase::addBasicContent()
{
	// Workaround to ensure that propertyid and pin heap dir get
	// established on disk 6273,6274
	PropertyID id = MVTApp::getProp(mSession,"DummyProp"); 

	Value v ; v.set( "dummy property" ) ; v.property = id ;
	PID pid ;
	TVERIFYRC(mSession->createPINAndCommit(pid,&v,1,0));	

#if 1
	// Forcing a class into the store will prevent recovery 
	// problems - workaround to 6274
	IStmt * lQ = mSession->createStmt() ;
	lQ->setPropCondition(lQ->addVariable(),&id,1);
	ClassID cid = STORE_INVALID_CLASSID;
	TVERIFYRC(defineClass(mSession,"PinsWithDummyProp",lQ,&cid));
	TVERIFY( cid != STORE_INVALID_CLASSID) ;
	lQ->destroy() ;
#endif
}

//
// TestRecoveryBasic - simple recovery of pins
//

class TestRecoveryBasic : public TestRecoveryBase
{
	public:
		TEST_DECLARE(TestRecoveryBasic);
		virtual char const * getName() const { return "recoverybasic"; }
		virtual void destroy() { delete this; }
	protected:
		virtual void doTest( bool inReference )
		{
			if ( inReference ) 
				mLogger.out() << "Creating PINs in reference store" << endl ;
			else 
				mLogger.out() << "Creating PINs in crash store" << endl ;

			PropertyID id = MVTApp::getProp(mSession,"a prop"); 

			for ( size_t i = 0 ; i < 100 ; i++ )
			{
				Value v ; v.set( 100 ) ; v.property = id ;
				PID pid ;
				TVERIFYRC(mSession->createPINAndCommit(pid,&v,1,0));
			}
		}
};
TEST_IMPLEMENT(TestRecoveryBasic, TestLogger::kDStdOut);

//
// TestRecoveryMeta - recovery of classes, identity, property id and other
// "meta" data inside the store
//

class TestRecoveryMeta : public TestRecoveryBase
{
	public:
		TEST_DECLARE(TestRecoveryMeta);
		virtual char const * getName() const { return "recoverymeta"; }
		virtual void destroy() { delete this; }
	protected:
		virtual void doTest( bool inReference )
		{
			if ( inReference ) 
				mLogger.out() << "Creating meta data in reference store" << endl ;
			else 
				mLogger.out() << "Creating meta data in crash store" << endl ;
	
			MVTApp::getProp(mSession,"PropA");
			MVTApp::getProp(mSession,"PropB");
			mExpectedPropIDForC = MVTApp::getProp(mSession,"PropC");

			IStmt * lQ = mSession->createStmt() ;
			lQ->setPropCondition(lQ->addVariable(),&mExpectedPropIDForC,1);
			ClassID cid = STORE_INVALID_CLASSID;
			TVERIFYRC(defineClass(mSession,"PinsWithPropC",lQ,&cid));
			TVERIFY( cid != STORE_INVALID_CLASSID) ;
			lQ->destroy() ;

			mSession->storeIdentity("IdentA","password1",true) ;
			mSession->storeIdentity("IdentB","password1",true) ;
			mExpectedIdentityForC = mSession->storeIdentity("IdentC","password1",true) ;
		}

		virtual void testRecoveredStore()
		{
#ifdef TEST_RECOVERY_FROM_SCRATCH
			// member variables not valid if test run twice
			mLogger.out() << "WARNING - not implemented yet" << endl ;
			return ;
#endif
			// If properties are recovered this will retrieve valid property
			// Otherwise it will be "re-registered" with a different id
			PropertyID lPropC = MVTApp::getProp(mSession,"PropC");
			TVERIFY(lPropC==mExpectedPropIDForC); mLogger.out() << lPropC << endl ;

			ClassID cid ;
			TVERIFYRC(mSession->getClassID("PinsWithPropC",cid));
			TVERIFY( cid != STORE_INVALID_CLASSID) ;

			IdentityID lookupOfIdentityC = mSession->getIdentityID("IdentC");
			TVERIFY( lookupOfIdentityC == mExpectedIdentityForC ) ; //mLogger.out() << lookupOfIdentityC << endl ;
		}

	private:
		IdentityID mExpectedIdentityForC ;
		PropertyID mExpectedPropIDForC ; 
};
TEST_IMPLEMENT(TestRecoveryMeta, TestLogger::kDStdOut);

//
// Test unclean exit during a transaction.  Any uncommitted pins should 
// completely disappear when store is reopened
//

class TestRecoveryTransaction : public TestRecoveryBase
{
	public:
		TEST_DECLARE(TestRecoveryTransaction);
		virtual char const * getName() const { return "recoverytransaction"; }
		virtual void destroy() { delete this; }
	protected:
		virtual void doTest( bool inReference )
		{
			if ( inReference ) 
				mLogger.out() << "Creating meta data in reference store" << endl ;
			else 
				mLogger.out() << "Creating meta data in crash store" << endl ;
	
			PropertyID idA = MVTApp::getProp(mSession,"PropA"); 
			PropertyID idB = MVTApp::getProp(mSession,"PropB"); 
			PropertyID idC = MVTApp::getProp(mSession,"PropC"); 

			// Index will be affected by the uncommitted pins
			IStmt * lQ = mSession->createStmt() ;
			lQ->setPropCondition(lQ->addVariable(),&idC,1);
			ClassID cid = STORE_INVALID_CLASSID;
			TVERIFYRC(defineClass(mSession,"PinsWithPropC",lQ,&cid));
			TVERIFY( cid != STORE_INVALID_CLASSID) ;
			lQ->destroy() ;

			Value v ; 
			PID pid ;

			mSession->startTransaction() ;
			v.set( "should be there" ) ; v.property = idA ;
			TVERIFYRC(mSession->createPINAndCommit(pid,&v,1,0));

			// Also add this pin as part of the "PinsWithPropC" index
			v.set(99) ; v.property=idC ;
			TVERIFYRC(mSession->modifyPIN( pid, &v, 1 )) ;

			mSession->commit() ;

			mSession->startTransaction() ;
			v.set( "should not be there" ) ; v.property = idA ;
			TVERIFYRC(mSession->createPINAndCommit(pid,&v,1,0));
			mSession->rollback() ;

			if ( !inReference )
			{
				// For the crash scenario leave uncommitted pins
				// that should not be persisted
				mSession->startTransaction() ;

				Value vals[2] ;

				for ( size_t i = 0 ; i < 100 ; i++ )
				{
					vals[0].set( "uncommitted should not be there" ) ; vals[0].property = idA ;

					// Add some larger data also so that multiple pages are used
					IStream *stream = MVTApp::wrapClientStream(mSession,new TestStringStream(16000,VT_STRING));
					vals[1].set( stream ) ; vals[1].property = idB ;
				
					TVERIFYRC(mSession->createPINAndCommit(pid,vals,2,0));

					for ( int k = 0 ; k < 100 ; k++ )
					{
						vals[0].set(k) ; vals[0].property=idC ; vals[0].op = OP_ADD ;
						vals[0].meta = META_PROP_SSTORAGE ;
						TVERIFYRC(mSession->modifyPIN( pid, vals,1 )) ;
					}
				}
			}
		}

		virtual void testRecoveredStore()
		{
			// See if the index finds anything related to the uncommitted pins
			ClassID cid ;
			TVERIFYRC(mSession->getClassID("PinsWithPropC",cid));
			TVERIFY( cid != STORE_INVALID_CLASSID) ;

			SourceSpec spec ;
			spec.objectID = cid ;
			spec.nParams = 0 ;
			spec.params = NULL ;

			IStmt * lQ = mSession->createStmt() ;
			lQ->addVariable( &spec, 1 ) ;

			uint64_t cnt ;
			lQ->count( cnt ) ;

			TVERIFY( cnt == 1 ) ; //mLogger.out() << cnt << endl ;
		}
};
TEST_IMPLEMENT(TestRecoveryTransaction, TestLogger::kDStdOut);
