/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h" // MV auto pointers
using namespace std;

class TestConvertValue : public ITest
{
	public:
		TEST_DECLARE(TestConvertValue);

		virtual char const * getName() const { return "testConvertValue"; }
		virtual char const * getDescription() const { return "Session convertValue"; }
		virtual char const * getHelp() const { return ""; } // Optional
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void doTest() ;
	private:	
		ISession * mSession ;
};
TEST_IMPLEMENT(TestConvertValue, TestLogger::kDStdOut);

int TestConvertValue::execute()
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

void TestConvertValue::doTest()
{
	MVStore::Value v1, v2;

	v1.set(1); v2.set(VT_ERROR);
	TVERIFYRC(mSession->convertValue(v1,v2,VT_DOUBLE));
	TVERIFY(v1.type == VT_INT); TVERIFY(v1.i==1); // Unchanged by convertion
	TVERIFY(v2.type == VT_DOUBLE); TVERIFY(v2.d==1.0);

	// rounding data loss due to rounding
	v1.set(1.5); 
	TVERIFYRC(mSession->convertValue(v1,v2,VT_INT));
	TVERIFY(v2.type == VT_INT); TVERIFY(v2.i==1);

	// Overflow - high order bits are chopped off
	uint64_t u = ( 2LL << 33 ) + 1;
	v1.setU64(u);
	TVERIFYRC(mSession->convertValue(v1,v2,VT_INT));
	TVERIFY(v2.type == VT_INT); TVERIFY(v2.i==1); 
}
