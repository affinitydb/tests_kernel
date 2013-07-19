/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

using namespace std;

class TestDropClass : public ITest
{
	static const int sNumProps = 2;
	PropertyID mPropIDs[sNumProps];
	std::vector<Tstring> className;
	public:
		TEST_DECLARE(TestDropClass);
		virtual char const * getName() const { return "testDropClass"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test for drop class"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Not complete yet"; return false; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testDropAllClasses(ISession *session);
};
TEST_IMPLEMENT(TestDropClass, TestLogger::kDStdOut);

// Implement this test.
int TestDropClass::execute()
{
	
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		MVTApp::mapURIs(session,"TestDropClass.prop",sNumProps,mPropIDs);
		testDropAllClasses(session);		
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
 	return 0;
}
void TestDropClass::testDropAllClasses(ISession *session)
{
	mLogger.out()<<"WARNING: Will drop all classess"<<std::endl;
	MVTApp::enumClasses(*session, &className, NULL);
	//drop all classes here
	vector<Tstring>::iterator it;
	for (it=className.begin();className.end() != it; it++)
	{
		mLogger.out()<<"Dropping class: "<<*it<<std::endl;
		if (RC_OK == dropClass(session,(*it).c_str())){
			mLogger.out()<<"Class: "<<*it<<" dropped sucessfully"<<std::endl;
		}
		else
		{
			mLogger.out()<<"Dropping class "<<*it<<" failed "<<std::endl;
			TVERIFY(false);
		}
	}
}
