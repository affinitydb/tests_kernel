/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

class TestSingleCharFT : public ITest
{
public:
	TEST_DECLARE(TestSingleCharFT);
	virtual char const * getName() const { return "TestSingleCharFT"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "Test for single character FT search in MODE_ALL_WORDS"; }
	virtual int execute();
	virtual void destroy() { delete this; }

public:
		ISession * mSession ;
		PropertyID	mProp[1];
		IPIN *mIPIN;
};
TEST_IMPLEMENT(TestSingleCharFT, TestLogger::kDStdOut);

int TestSingleCharFT::execute()
{
	char URIName[32]="test.singleFT.";
	Value tValue[3];
	uint64_t lCount = 0;
	const char *inSearch = "wild a meadow";

	if (!MVTApp::startStore()) {mLogger.print("Failed to start store\n"); return RC_NOACCESS;}

	
	mSession = MVTApp::startSession();
	mProp[0]=MVTUtil::getPropRand(mSession,URIName);
	
	tValue[0].set("wild a meadow");		//first pin
	tValue[0].property=mProp[0];
	mIPIN = mSession->createUncommittedPIN(tValue,1,MODE_COPY_VALUES);	
	mSession->commitPINs(&mIPIN,1); 
        
	tValue[1].set("wild meadow");		//second pin
	tValue[1].property=mProp[0];
	mIPIN = mSession->createUncommittedPIN(&tValue[1],1,MODE_COPY_VALUES);	
	mSession->commitPINs(&mIPIN,1); 

	tValue[2].set("wild meadow angel");		//third pin
	tValue[2].property=mProp[0];
	mIPIN = mSession->createUncommittedPIN(&tValue[2],1,MODE_COPY_VALUES);	
	mSession->commitPINs(&mIPIN,1); 
	mIPIN->destroy();

	IStmt * const lQ =mSession->createStmt();
	unsigned const lVar= lQ->addVariable();	
	TVERIFYRC(lQ->addConditionFT(lVar,inSearch,MODE_ALL_WORDS,mProp,1));
	lQ->count(lCount,0,0,~0,MODE_ALL_WORDS);
	
	TVERIFY(lCount==3); //Only third pin is returned as MODE_ALL_WORDS searches for single character 'a' also
	lQ->destroy();
	
	mSession->terminate();
	MVTApp::stopStore();
	return RC_OK;
}
