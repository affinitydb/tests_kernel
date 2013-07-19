/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h" // MV auto pointers
using namespace std;

class TestCopyValue : public ITest
{
	public:
		TEST_DECLARE(TestCopyValue);
		virtual char const * getName() const { return "testCopyValue"; }
		virtual char const * getDescription() const { return "Session Copy Value"; }
		virtual char const * getHelp() const { return ""; } // Optional
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void doTest() ;
		void compareStrVals( const Afy::Value & val, const Afy::Value & expected );
	private:	
		ISession * mSession ;
};
TEST_IMPLEMENT(TestCopyValue, TestLogger::kDStdOut);

int TestCopyValue::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }
	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;
	doTest() ;
	mSession->terminate() ;
	MVTApp::stopStore() ;
	return RC_OK ;
}

void TestCopyValue::compareStrVals( const Afy::Value & val, const Afy::Value & expected )
{
	// Confirm they are different values but with same string content
	TVERIFY(val.type==VT_STRING);
	TVERIFY(val.str != expected.str); // Expect new buffer
	TVERIFY(0==strcmp(val.str,expected.str));
}

void TestCopyValue::doTest()
{
	Afy::PropertyID id = MVTUtil::getProp(mSession,"TestCopyValue"); 

	//
	// Scenario 1
	// 

	Afy::Value regularMemory;
	regularMemory.set("Orange"); 
	regularMemory.property=id;

	Afy::Value copy1;
	TVERIFYRC(mSession->copyValue(regularMemory,copy1));
	compareStrVals(copy1,regularMemory);
	mSession->freeValue(copy1);

	//
	// Same thing with session memory
	//
	char * orange = (char *) mSession->malloc(strlen("Orange")+1);
	strcpy(orange,"Orange");
	Afy::Value sessionVal;
	sessionVal.set(orange); 
	sessionVal.property=id;

	Afy::Value copy2;
	TVERIFYRC(mSession->copyValue(sessionVal,copy2));
	compareStrVals(copy2,sessionVal);
	mSession->freeValue(copy2);
	mSession->free(orange);

	//
	// session memory for string and value
	//
	char * orange2 = (char *) mSession->malloc(strlen("Orange")+1);
	strcpy(orange2,"Orange");
	Afy::Value * sessionVal2 = (Afy::Value *)mSession->malloc(sizeof(Value));
	sessionVal2->set(orange); 
	sessionVal2->property=id;

	Afy::Value copy3;
	TVERIFYRC(mSession->copyValue(*sessionVal2,copy3));
	compareStrVals(copy3,*sessionVal2);
	mSession->freeValue(copy3);

	mSession->freeValue(*sessionVal2);
	mSession->free(sessionVal2);
}
