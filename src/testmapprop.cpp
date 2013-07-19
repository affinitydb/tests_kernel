/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

class Testmapprop : public ITest
{
public:
	TEST_DECLARE(Testmapprop);
	virtual char const * getName() const { return "Testmapprop"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "Test to repro the exception in mapproperties bug# 23233"; }
	virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Assertion failure and exceptions"; return false; }
	virtual int execute();
	virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(Testmapprop, TestLogger::kDStdOut);

int Testmapprop::execute()
{
	static const int sNumProps = 2;
	PropertyID	myProp[sNumProps] ;
	PropertyID	myProp1[sNumProps] ;
	PropertyID	myProp2[sNumProps] ;
	PropertyID	myProp3[sNumProps] ;

	ISession * mSession ;

	if (!MVTApp::startStore()) {mLogger.print("Failed to start store\n"); return RC_NOACCESS;}
	
	ISession * mSession1 ;
	mSession1 = MVTApp::startSession();
	MVTApp::mapURIs(mSession1, "Testnamespace", sNumProps, myProp2);
	MVTApp::mapURIs(mSession1, "Testnamespace", sNumProps, myProp3, "http://vmware.com/core/");
	mSession1->terminate();

	mSession = MVTApp::startSession();
	MVTApp::mapURIs(mSession, "Testnamespace", sNumProps, myProp, "http://vmware.com/core/");
	MVTApp::mapURIs(mSession, "Testnamespace", sNumProps, myProp1); //Exception thrown
	mSession->terminate();
	
	MVTApp::stopStore();
	return RC_OK;
}
