/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

class testmetapropmodify : public ITest
{
public:
	TEST_DECLARE(testmetapropmodify);
	virtual char const * getName() const { return "testmetapropmodify"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "test to verify meta_prop_noindex update in pin modify"; }
	virtual int execute();
	virtual void destroy() { delete this; }

public:
	ISession * mSession ;
	PropertyID	mProp[1] ;
	IPIN *mIPIN;
};
TEST_IMPLEMENT(testmetapropmodify, TestLogger::kDStdOut);

int testmetapropmodify::execute()
{
	if (!MVTApp::startStore()) {mLogger.print("Failed to start store\n"); return RC_NOACCESS;}
	mSession = MVTApp::startSession();

	Value lV[1];
	char URIName[32]="prop0.testmetapropmodify";
	mProp[0]=MVTUtil::getPropRand(mSession,URIName);
	string strRand ; MVTRand::getString( strRand, 20, 0 ) ;
	strRand.append("testmetapropmodify ");

	lV[0].set(strRand.c_str());		//Creating pin with strRand
	lV[0].property=mProp[0];
	mIPIN = mSession->createPIN(lV,1);	
	mSession->commitPINs(&mIPIN,1); 
	PID lpid=mIPIN->getPID();
 
	lV[0].set("modified");  //Modifying pin to replace strRand
	lV[0].property=mProp[0];
	TVERIFYRC(mSession->modifyPIN(lpid,lV,1)); 

	IStmt *query = mSession->createStmt();
	unsigned char var = query->addVariable();
	uint64_t cnt =0;
	query->addConditionFT(var,strRand.c_str(),MODE_ALL_WORDS); //FT query for strRand
	query->count(cnt,0,0,~0,MODE_ALL_WORDS);
	TVERIFY(cnt==0); //FT index should be updated to remove strRand
	query->destroy();

	mSession->terminate();
	MVTApp::stopStore();
	return RC_OK;
}
