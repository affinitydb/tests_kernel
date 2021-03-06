/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

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

	Afy::Value *lVal = (Afy::Value *)session->malloc(2*sizeof(Afy::Value));
	lVal[0].set(1);lVal[0].property=0;
	lVal[1].set(2);lVal[1].property=1; 

	Afy::Value *lVal2 = (Afy::Value *)session->malloc(2*sizeof(Afy::Value));
	lVal2[0].set(1);lVal2[0].property=0;
	lVal2[1].set(2);lVal2[1].property=1; 

	unsigned int lMode = 0;
	Afy::PID lpid;
	lpid.pid = 0xe5940000000b0017LL; lpid.ident = STORE_OWNER;

	Afy::IPIN *lPin1;
	TVERIFYRC(session->createPIN(lVal,2,&lPin1,MODE_PERSISTENT));

	lMode = PIN_REPLICATED|MODE_PERSISTENT;
	Afy::IPIN *lPin2;
	TVERIFYRC(session->createPIN(lVal2,2,&lPin2,lMode,&lpid));
	
	lpid = lPin1->getPID();
	lPin1->destroy();
	RC rc =session->deletePINs(&lpid,1,MODE_PURGE);
	rc = session->deletePINs(&lpid,1,MODE_PURGE);
	TVERIFY(RC_OK!=rc);
	Afy::PID lpid1;
	lpid1 = lPin2->getPID();
	lPin2->destroy();
	rc =session->deletePINs(&lpid1,1,MODE_PURGE); 
	rc = session->deletePINs(&lpid1,1,MODE_PURGE); //This should return RC_NOTFOUND and not RC_FALSE
	TVERIFY(RC_OK!=rc);
	session->terminate();
	MVTApp::stopStore(); 
	return RC_OK;
}
