/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

// This is just a convenience for deleting the store file.
// Normally you can use -newstore argument before another test, e.g.
// tests testbasic -newstore.
// But in some multi-stage tests, e.g. testscenario you cannot use
// that argument.  This pseudo test allows you to erase the store with
// no strings attached.
// 
// (Most useful with s3io stores, where the s3 component must be
// deleted as well.  It is easier than deleting the local files + 
// calling s3tool delete)
#include "app.h"
using namespace std;

class TestDelete : public ITest
{
	public:
		TEST_DECLARE(TestDelete);
		virtual char const * getName() const { return "delete"; }
		virtual char const * getDescription() const { return "Delete the store file and do nothing else"; }
		virtual char const * getHelp() const { return ""; } // Optional
		virtual int execute();
		virtual void destroy() { delete this; }		
		
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Not a real teset"; return false; }
};
TEST_IMPLEMENT(TestDelete, TestLogger::kDStdOut);
int TestDelete::execute()
{
	MVTApp::deleteStore();
	return RC_OK;
}
