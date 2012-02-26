/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

class testdeletepurged : public ITest
{
	public:
		TEST_DECLARE(testdeletepurged);
		virtual char const * getName() const { return "testdeletepurged"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test to repro what happens upon trying to delete purged pin"; }
		virtual int execute();
		virtual void destroy() { delete this; }
	private:
		ISession * session ;
		
};
TEST_IMPLEMENT(testdeletepurged, TestLogger::kDStdOut);

int testdeletepurged::execute()
{
	if (!MVTApp::startStore()) {mLogger.print("Failed to start store\n"); return RC_NOACCESS;}

	session = MVTApp::startSession();
	unsigned int const lOldMode = session->getInterfaceMode();
	session->setInterfaceMode(lOldMode | ITF_REPLICATION);

	AfyDB::Value *lVal = (AfyDB::Value *)session->alloc(2*sizeof(AfyDB::Value));
	lVal[0].set(1);lVal[0].property=0;
	lVal[1].set(2);lVal[1].property=1; 

	AfyDB::Value *lVal2 = (AfyDB::Value *)session->alloc(2*sizeof(AfyDB::Value));
	lVal2[0].set(1);lVal2[0].property=0;
	lVal2[1].set(2);lVal2[1].property=1; 

	unsigned int lMode = 0;
	AfyDB::PID lpid;
	lpid.pid = 0xe5940000000b0017LL; lpid.ident = STORE_OWNER;

	AfyDB::IPIN *lPin1 = session->createUncommittedPIN(lVal,2);

	lMode = MODE_FORCE_EIDS | PIN_REPLICATED;
	AfyDB::IPIN *lPin2 = session->createUncommittedPIN(lVal2,2,lMode,&lpid);
	
	session->commitPINs(&lPin1,1,0);
	session->commitPINs(&lPin2,1,lMode);
	lpid = lPin1->getPID();
	lPin1->destroy();
	RC rc =session->deletePINs(&lpid,1,MODE_PURGE);
	rc = session->deletePINs(&lpid,1,MODE_PURGE);
	TVERIFY(RC_OK!=rc);
	AfyDB::PID lpid1;
	lpid1 = lPin2->getPID();
	lPin2->destroy();
	rc =session->deletePINs(&lpid1,1,MODE_PURGE); 
	rc = session->deletePINs(&lpid1,1,MODE_PURGE); //This should return RC_NOTFOUND and not RC_FALSE
	TVERIFY(RC_OK!=rc);
	session->terminate();
	MVTApp::stopStore(); 
	return RC_OK;
}
