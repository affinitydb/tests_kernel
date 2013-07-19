/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

using namespace std;

class TestQuickIndexRebuild : public ITest
{
	static const int sNumProps = 2;
	PropertyID mPropIDs[sNumProps];
	std::vector<Tstring> className;
	public:
		TEST_DECLARE(TestQuickIndexRebuild);
		virtual char const * getName() const { return "testquickindexrebuild"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test for rebuild class indices"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "quick test. Runs at end of smokes"; return false; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testRebuildAllIndices(ISession *session);
};
TEST_IMPLEMENT(TestQuickIndexRebuild, TestLogger::kDStdOut);

// Implement this test.
int TestQuickIndexRebuild::execute()
{
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		MVTApp::mapURIs(session,"TestQuickIndexRebuild.prop",sNumProps,mPropIDs);
		testRebuildAllIndices(session);
		session->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
 	return 0;
}
void TestQuickIndexRebuild::testRebuildAllIndices(ISession *session)
{
	mLogger.out()<<"WARNING: Will rebuiuld all classess"<<std::endl;
	MVTApp::enumClasses(*session, &className);
	//drop all classes here
	vector<Tstring>::iterator it;
	for (it=className.begin();className.end() != it; it++)
	{
		mLogger.out()<<"Rebuilding Class Index: "<<*it<<std::endl;
		if (RC_OK == updateClass(session, (*it).c_str(),NULL)){
			mLogger.out()<<"Class Index: "<<*it<<" rebuilt sucessfully"<<std::endl;
		}
		else
		{
			mLogger.out()<<"Rebuilding index for class "<<*it<<" failed "<<std::endl;
			TVERIFY(false);
		}
	}
}
