/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h" 

// Publish this test.
class TestPageAlreadyExists : public ITest
{
	static const int sNumProps = 20;
	PropertyID lPropIDs[sNumProps];
	std::vector<PID> pids;
	MVStoreKernel::StoreCtx *mStoreCtx;
	public:
		TEST_DECLARE(TestPageAlreadyExists);
		virtual char const * getName() const { return "testpagealreadyexists"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "repro for bug id: 18951"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "(maxw, Dec2010) Behaves erratically; freezes sometimes."; return false; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		static THREAD_SIGNATURE threadReplicate(void * inThis);
		static THREAD_SIGNATURE threadAddBinary(void * inThis);
};

TEST_IMPLEMENT(TestPageAlreadyExists, TestLogger::kDStdOut);

int TestPageAlreadyExists::execute()
{
	bool lSuccess = true; 
	if (MVTApp::startStore())
	{
		ISession * session =	MVTApp::startSession();
		mStoreCtx = MVTApp::getStoreCtx();
		MVTApp::mapURIs(session, "TestPageAlreadyExists.prop.", sNumProps, lPropIDs);
		const size_t numthreads = 2;
		HTHREAD lThreads[numthreads];
		createThread(&threadReplicate, this, lThreads[0]);
		createThread(&threadAddBinary, this, lThreads[1]);
		MVTestsPortability::threadsWaitFor(numthreads, lThreads);
		session->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
	return lSuccess ? 0 : 1;
}
THREAD_SIGNATURE TestPageAlreadyExists::threadAddBinary(void * inTest)
{
	TestPageAlreadyExists *test = (TestPageAlreadyExists*)inTest;
	ISession *session = MVTApp::startSession(test->mStoreCtx);
	Value val[10];
	const int size = 12*1024;
	unsigned char *buf = (unsigned char *)malloc(size);
	for (int i=0; i < 1000; i ++)
	{
		IPIN *pin = session->createUncommittedPIN();
		val[0].set(buf,size);val[0].setPropID(test->lPropIDs[0]);
		TVRC_R(pin->modify(val,1),test);
		TVRC_R(session->commitPINs(&pin,1),test);
		test->pids.push_back(pin->getPID());
		pin->destroy();
	}
	session->terminate();
	vector<PID>::iterator it;
	const int sizenew = 20*1024;
	unsigned char *bufnew = (unsigned char *)malloc(sizenew);
	session = MVTApp::startSession();
	for (it=test->pids.begin();test->pids.end() != it; it++)
	{
		IPIN *pin = session->getPIN(*it);
		val[0].set(bufnew,sizenew);val[0].setPropID(test->lPropIDs[0]);val[0].setMeta(META_PROP_SSTORAGE);
		TVRC_R(pin->modify(val,1),test);
		pin->destroy();
	}
	session->terminate();
	return 0;
}
THREAD_SIGNATURE TestPageAlreadyExists::threadReplicate(void * inTest)
{
	TestPageAlreadyExists *test = (TestPageAlreadyExists*)inTest;
	MVTestsPortability::threadSleep(5000);
	ISession *session = MVTApp::startSession(test->mStoreCtx);
	for (int i=0; i < 20; i ++)
	{
		IStmt *query = session->createStmt();
		unsigned char var = query->addVariable();
		Value args[1];
		args[0].setVarRef(var,test->lPropIDs[0]);
		IExprTree *expr = session->expr(OP_EXISTS,1,args);
		query->addCondition(var,expr);
		ICursor *result = NULL;
		query->execute(&result);
		for (IPIN *rpin=result->next(); rpin!=NULL; rpin=result->next() ){
			if (NULL != rpin)
				rpin->destroy();
		}
		query->destroy();
		expr->destroy();
		result->destroy();
	}
	session->terminate();
	return 0;
}
