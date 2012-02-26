/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h" // MV auto pointers
using namespace std;

class TestCompareValues : public ITest
{
	public:
		TEST_DECLARE(TestCompareValues);

		virtual char const * getName() const { return "testCompareValues"; }
		virtual char const * getDescription() const { return "Session compareValues"; }
		virtual char const * getHelp() const { return ""; } // Optional
		virtual int execute();
		virtual void destroy() { delete this; }
		
	protected:
		void doTest() ;
	private:	
		ISession * mSession ;
};
TEST_IMPLEMENT(TestCompareValues, TestLogger::kDStdOut);

int TestCompareValues::execute()
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

void TestCompareValues::doTest()
{
	AfyDB::Value v1, v2;

	v1.set(1); v2.set(1);
	TVERIFY(0==mSession->compareValues(v1,v2,true));

	v1.set(1.0); v2.set(1);
	TVERIFY(0==mSession->compareValues(v1,v2,true));
	TVERIFY(0==mSession->compareValues(v2,v1,true));

	v1.set(1); v2.set(1.0f);
	TVERIFY(0==mSession->compareValues(v1,v2,true));
	TVERIFY(0==mSession->compareValues(v2,v1,true));

	// kernel isn't fooled by rounding!
	v1.set(5); v2.set(5.1);
	TVERIFY(0!=mSession->compareValues(v1,v2,true));
	TVERIFY(0!=mSession->compareValues(v2,v1,true));

	AfyDB::PID pid;
	TVERIFYRC(mSession->createPIN(pid,NULL,0));
	v1.set(pid); v2.set(pid);
	TVERIFY(0==mSession->compareValues(v1,v2,true));

	v1.set(pid); pid.pid++; v2.set(pid);
	TVERIFY(0!=mSession->compareValues(v1,v2,true));

	// Perhaps arguable as boolean == non-zero a common convention
	v1.set(true); v2.set(1);
	TVERIFY(0!=mSession->compareValues(v1,v2,true));
	TVERIFY(0!=mSession->compareValues(v2,v1,true));

	v1.set(false); v2.set(0);
	TVERIFY(0!=mSession->compareValues(v1,v2,true));
	TVERIFY(0!=mSession->compareValues(v2,v1,true));

	// Although numerically equivalent you cannot compare a date and number
	uint64_t dt=987897;
	v1.setU64(dt); v2.setDateTime(dt);
	TVERIFY(0!=mSession->compareValues(v1,v2,true));
	TVERIFY(0!=mSession->compareValues(v2,v1,true));

	// Case sensitivity testing
#define CASE_SENSITIVE true
#define NO_CASE false

	v1.set("hello"); v2.set("hello");
	TVERIFY(0==mSession->compareValues(v1,v2,CASE_SENSITIVE));
	TVERIFY(0==mSession->compareValues(v1,v2,NO_CASE));

	v1.set("hello"); v2.set("HELLO");
	TVERIFY(0==mSession->compareValues(v1,v2,CASE_SENSITIVE));
	TVERIFY(0!=mSession->compareValues(v1,v2,NO_CASE));

	v1.set(0); v2.set("HELLO");
	TVERIFY(0!=mSession->compareValues(v1,v2,CASE_SENSITIVE));

	v1.set(0); v2.set("");
	TVERIFY(0!=mSession->compareValues(v1,v2,CASE_SENSITIVE));

	// Store supports this!
	v1.set("12"); v2.set(12);
	TVERIFY(0==mSession->compareValues(v1,v2,CASE_SENSITIVE));
	TVERIFY(0==mSession->compareValues(v2,v1,CASE_SENSITIVE));
}
