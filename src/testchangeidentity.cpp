/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

// Publish this test.
class TestChangeIdentity : public ITest
{
		static const int sCharSize = 64;
		char mOldStoreIdentity[sCharSize], mNewStoreIdentity[sCharSize];
		Afy::IAffinity * mStoreCtx;
	public:
		TEST_DECLARE(TestChangeIdentity);
		virtual char const * getName() const { return "testChangeIdentity"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Test changing store owner identity[#25748]"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Test changes identity thus failing subsequent tests"; return false; }
		virtual bool isStandAloneTest() const { return true;}
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		int doTest();
		int doValidateOnRestart();
	private:
		ISession * mSession ;
};

TEST_IMPLEMENT(TestChangeIdentity, TestLogger::kDStdOut);

int TestChangeIdentity::doTest()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return 0; }
	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;
	
	TVERIFY(0 != mSession->getStoreIdentityName(mOldStoreIdentity, sCharSize));
	mLogger.out() << "Old store identity name : " << mOldStoreIdentity << std::endl;
	TVERIFYRC(mSession->changeStoreIdentity("newStoreIdentity"));
	TVERIFY(0 != mSession->getStoreIdentityName(mNewStoreIdentity, sCharSize));
	mLogger.out() << "Old store identity name : " << mNewStoreIdentity << std::endl;
	TVERIFY(0 != strcmp(mOldStoreIdentity, mNewStoreIdentity));

	char lIDName[sCharSize];
	TVERIFY(0 != mSession->getIdentityName(STORE_OWNER, lIDName, sCharSize));
	TVERIFY(0 == strcmp(lIDName, mNewStoreIdentity));
	
	IdentityID lOldID = mSession->getIdentityID(mOldStoreIdentity); TVERIFY(STORE_INVALID_IDENTITY == lOldID);
	IdentityID lNewID = mSession->getIdentityID(mNewStoreIdentity); TVERIFY(STORE_OWNER == lNewID);

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();
	return 0;
}

int TestChangeIdentity::doValidateOnRestart()
{
	bool bStarted = MVTApp::startStore(0, 0, 0, mNewStoreIdentity) ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return 0; }
	mStoreCtx = MVTApp::getStoreCtx();

	mSession = MVTApp::startSession(mStoreCtx);
	TVERIFY( mSession != NULL ) ;

	char lStoreIdentity[sCharSize];
	TVERIFY(0 != mSession->getStoreIdentityName(lStoreIdentity, sCharSize));
	mLogger.out() << "New store identity name : " << lStoreIdentity << std::endl;

	TVERIFY(0 == strcmp(lStoreIdentity, mNewStoreIdentity));	
	TVERIFY(0 != strcmp(mOldStoreIdentity, lStoreIdentity));

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();
	return 0;
}

int TestChangeIdentity::execute()
{
	TVERIFY(!doTest() && !doValidateOnRestart());
	return 0;
}
