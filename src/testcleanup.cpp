/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <map>
using namespace std;

//testcleanup: deletes and purges all pins in store. 
//This is currently usedby replication smoke test suite to remove all/any pins from previous tests before running any test.

// Publish this test.
class TestCleanup : public ITest
{
	public:
		TEST_DECLARE(TestCleanup);
		virtual char const * getName() const { return "testcleanup"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Deletes (optionally purges) all pins in the store. ** Only for custom test runs **"; }
		
		virtual int execute();
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "(1)deletes all pins, (2)takes arguments"; return false; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual bool excludeInIPCSmokeTest(char const *& pReason) const { pReason = "deletes all pins"; return true; }
		virtual bool isLongRunningTest() const { return false; }
		virtual bool includeInLongRunningSmoke(char const *& pReason) const { pReason = "deletes all pins"; return false; }
		virtual bool includeInBashTest(char const *& pReason) const {pReason = "deletes all pins"; return false; }
		virtual bool includeInPerfTest() const { return false; }

		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestCleanup, TestLogger::kDStdOut);

// Implement this test.
int TestCleanup::execute()
{
	int retVal = 0; string pMode; bool pparsing = true;
	
	if(!mpArgs->get_param("pmode", pMode)){
		mLogger.out() << "Problem with --pmode parameter initialization!" << endl;
		pparsing = false;
	}
	if(!pparsing){
		mLogger.out() << "Parameter initialization problems! " << endl; 
		mLogger.out() << "Test name: testcleanup" << endl; 	
		mLogger.out() << getDescription() << endl;	
		mLogger.out() << "Example: ./tests testcleanup --pmode={...} " << endl; 
			
		return RC_INVPARAM;
	}
	
	bool bStarted = MVTApp::startStore() ;
	
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return 1; }

	ISession * mSession = MVTApp::startSession();
	if ( mSession == NULL ) { TVERIFY2(0,"Could not start a session") ; return 1; }

	IStmt * const lQ = mSession->createStmt();
	lQ->addVariable();
	ICursor * lR = NULL;
	TVERIFYRC(lQ->execute(&lR));
	if (lR)
	{	
	int lDeletedPinCount = 0, lTotalPinCount = 0;
	IPIN * lNext;
	
		while (NULL != (lNext = lR->next() ) )
		{	++lTotalPinCount;
			AfyDB::PID lPid = lNext->getPID();
			RC lRC = RC_OK; 
			
			if( NULL != strstr(pMode.c_str(), "purge") )
				lRC = mSession->deletePINs(&lPid, 1, MODE_PURGE);
			else 
				lRC = mSession->deletePINs(&lPid, 1);

			if(RC_OK != lRC)
			{
				mLogger.out() << "ERROR while trying to delete pid:"<<std::hex<<((AfyDB::PID) lNext->getPID()).pid<<", RC="<<lRC<<std::dec;
			}
			else 
				++lDeletedPinCount;
			lNext->destroy();
		}
		mLogger.out() << std::endl<<"Total number of pins="<<lTotalPinCount<<" and successfully deleted="<<lDeletedPinCount<<std::endl;
		lR->destroy();
	}
	else
	{ 
		mLogger.out() <<std::endl<<"ERROR while executing the query"<<std::endl ;
		retVal = 1;
	}

	mSession->terminate(); 
	MVTApp::stopStore();  

	return retVal;
}
