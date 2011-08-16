/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

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
		void compareStrVals( const MVStore::Value & val, const MVStore::Value & expected );
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

void TestCopyValue::compareStrVals( const MVStore::Value & val, const MVStore::Value & expected )
{
	// Confirm they are different values but with same string content
	TVERIFY(val.type==VT_STRING);
	TVERIFY(val.str != expected.str); // Expect new buffer
	TVERIFY(0==strcmp(val.str,expected.str));
}

void TestCopyValue::doTest()
{
	MVStore::PropertyID id = MVTUtil::getProp(mSession,"TestCopyValue"); 

	//
	// Scenario 1
	// 

	MVStore::Value regularMemory;
	regularMemory.set("Orange"); 
	regularMemory.property=id;

	MVStore::Value copy1;
	TVERIFYRC(mSession->copyValue(regularMemory,copy1));
	compareStrVals(copy1,regularMemory);
	mSession->freeValue(copy1);

	//
	// Same thing with session memory
	//
	char * orange = (char *) mSession->alloc(strlen("Orange")+1);
	strcpy(orange,"Orange");
	MVStore::Value sessionVal;
	sessionVal.set(orange); 
	sessionVal.property=id;

	MVStore::Value copy2;
	TVERIFYRC(mSession->copyValue(sessionVal,copy2));
	compareStrVals(copy2,sessionVal);
	mSession->freeValue(copy2);
	mSession->free(orange);

	//
	// session memory for string and value
	//
	char * orange2 = (char *) mSession->alloc(strlen("Orange")+1);
	strcpy(orange2,"Orange");
	MVStore::Value * sessionVal2 = (MVStore::Value *)mSession->alloc(sizeof(Value));
	sessionVal2->set(orange); 
	sessionVal2->property=id;

	MVStore::Value copy3;
	TVERIFYRC(mSession->copyValue(*sessionVal2,copy3));
	compareStrVals(copy3,*sessionVal2);
	mSession->freeValue(copy3);

	mSession->freeValue(*sessionVal2);
	mSession->free(sessionVal2);
}
