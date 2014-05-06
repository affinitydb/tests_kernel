/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h"
#include "mvauto.h"

class TestAbortQuery : public ITest
{
		static const int sNumProps = 30, nTimes = 30;
		bool mStop;
		int cnt;
		PropertyID mPropIDs[sNumProps];
		static const int sNumPINs = 1000;	
		Tstring mClassStr;
		bool mStartAbortQ;
		MVTestsPortability::Mutex mLock;

	public:
		TEST_DECLARE(TestAbortQuery);
		virtual char const * getName() const { return "testabortquery"; }
		virtual char const * getHelp() const { return "To test abort query functionality in multithread scenario."; }		
		virtual char const * getDescription() const { return "tests ISession::abortQuery()"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Multithreading environment,timing based"; return false; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual bool isLongRunningTest() const { return false; }
		virtual void destroy() { delete this; }		
		virtual int execute();

	protected:
		void doTest();
		void createPINs(int pNumPINs = sNumPINs);
		void abortQuery();
		static THREAD_SIGNATURE abortQueryThread(void * pThis){((TestAbortQuery *)pThis)->abortQuery(); return 0;}
		static THREAD_SIGNATURE abortQueryMultiThread(void *pThis)
		{
			ISession *mSession = MVTApp::startSession();
			TestAbortQuery *threadCtx = new TestAbortQuery;
			threadCtx->mStartAbortQ = ((TestAbortQuery *)pThis)->mStartAbortQ;
			threadCtx->mSession = mSession;
			threadCtx->mStop = ((TestAbortQuery *)pThis)->mStop;
			HTHREAD lThreads;
			createThread(&abortQueryThread,threadCtx,lThreads);
			/*HTHREAD lThreads = CreateThread(NULL,0,&abortQueryThread,threadCtx,0,NULL);
			if(lThreads == NULL)
			{
				printf("Failed to create thread...\n\n");
				return -1;
			}
			SetThreadPriority(lThreads,2);*/
			
			threadCtx->quickTest();
			MVTestsPortability::threadsWaitFor(1, &lThreads);
			mSession->terminate();

			return 0;
		}
		
		void setAbortQ();
		void resetAbortQ();
		
		ISession * mSession ;
		Afy::IAffinity *mStoreCtx;	
		void quickTest();
};
TEST_IMPLEMENT(TestAbortQuery, TestLogger::kDStdOut);

void TestAbortQuery::doTest()
{
	mStop = false;
	cnt = 0;
	resetAbortQ(); 

	// Create PINs for querying...
	createPINs(500);

	// quick test with a full scan
	{
		HTHREAD mThread;
		createThread(&abortQueryThread,this,mThread);
		/*HTHREAD mThread = CreateThread(NULL,0,&abortQueryThread,this,0,NULL);
		if(mThread == NULL)
		{
			printf("Failed to create thread...\n\n");
			exit(-1);			
		}
		SetThreadPriority(mThread,2);*/
		
		quickTest();
		MVTestsPortability::threadsWaitFor(1, &mThread);
	}

	// Test Multi threaded (Session) abortQuery()
	{
		static const size_t sNumThreads = 5;
		HTHREAD lThreads[sNumThreads];

		for(size_t i = 0; i < sNumThreads; i++) 
			createThread(&abortQueryMultiThread,this, lThreads[i]);

		MVTestsPortability::threadsWaitFor(sNumThreads, lThreads);
	}
}

void TestAbortQuery::resetAbortQ()
{
	while(!mLock.trylock())
		continue;

	mStartAbortQ = false;
	mLock.unlock();
}

void TestAbortQuery::setAbortQ()
{
	while(!mLock.trylock())
		continue;
	
	mStartAbortQ = true; 
	cnt = 0;
	mLock.unlock();
}

void TestAbortQuery::quickTest()
{
	CmvautoPtr<IStmt> lQ(mSession->createStmt());
	unsigned char lVar = lQ->addVariable();
	IExprTree *lET;
	{
		Value lV[2];
		lV[0].setVarRef(0,mPropIDs[0]);
		lV[1].set(10000);
		IExprTree *lET1 = mSession->expr(OP_LT, 2, lV);

		lV[0].setVarRef(0,mPropIDs[4]);
		lV[1].set(10);
		IExprTree *lET2 = mSession->expr(OP_GT, 2, lV);

		lV[0].set(lET1);
		lV[1].set(lET2);
		IExprTree *lET3 = mSession->expr(OP_LAND, 2, lV);

		lV[0].setVarRef(0,mPropIDs[1]);
		lV[1].set("a");
		IExprTree *lET4 = mSession->expr(OP_CONTAINS, 2, lV, CASE_INSENSITIVE_OP);

		lV[0].set(lET3);
		lV[1].set(lET4);
		lET = mSession->expr(OP_LAND, 2, lV);
	}
	lQ->addCondition(lVar,lET); lET->destroy();
	uint64_t lCount = 0;
	
	ICursor *lRes;
	setAbortQ();
	for(int i = 0;i<nTimes;i++)
	{
		TVERIFYRC(lQ->execute(&lRes));
		if( (RC_TIMEOUT == lQ->count(lCount)) && (NULL == lRes) )
			cnt++;
	}
	mLogger.print("lQ->count(lCount)\n");
	mLogger.print("lQ->execute()\n");
	TVERIFY(cnt > 0);
		
	if(lRes) lRes->destroy();
	resetAbortQ();

	ICursor *lR;
	unsigned long lResultCount = 0;
	
	TVERIFYRC(lQ->count(lCount));
	TVERIFYRC(lQ->execute(&lR));
	
	if(lR)
	{
		for(IPIN *lPIN = lR->next(); lPIN != NULL; lPIN = lR->next(), lResultCount++)
			lPIN->destroy();
		lR->destroy();
	}else
		TVERIFY(false && "NULL ICursor returned");

	TVERIFY(lResultCount < lCount || lResultCount == lCount);
	
	//Values to be used by copyPINs,modifyPINs calls.
	Value lV[8];

	lV[0].setNow(); SETVATTR(lV[0], mPropIDs[7], OP_SET);
	IStream *lStream = new TestStringStream(MVTRand::getRange(10, 30) * 1024);
	SETVALUE(lV[1], mPropIDs[8], MVTApp::wrapClientStream(mSession, lStream), OP_SET);
	IStream *lStream1 = new TestStringStream(MVTRand::getRange(10, 30) * 1024);
	SETVALUE(lV[2], mPropIDs[9], MVTApp::wrapClientStream(mSession, lStream1), OP_SET);
	SETVALUE(lV[3], mPropIDs[10], MVTRand::getRange(10, 100), OP_SET);
	Tstring lStr; MVTRand::getString(lStr, 10, 100, true, false);
	SETVALUE(lV[4], mPropIDs[11], lStr.c_str(), OP_SET);
	SETVALUE(lV[5], mPropIDs[12], lStr.c_str(), OP_SET);
	Tstring lWStr; MVTRand::getString(lWStr, 10, 100, true, false);
	SETVALUE(lV[6], mPropIDs[13], lWStr.c_str(), OP_SET);
	double lDouble = (double)MVTRand::getRange(10, 1000);
	SETVALUE(lV[7], mPropIDs[14], lDouble, OP_SET);

	IStmt *lModQ = lQ->clone(STMT_UPDATE);
	lModQ->setValues(lV,8);
	IStmt *lDelQ = lQ->clone(STMT_DELETE);
	
	setAbortQ();
	for(int i = 0; i<nTimes; i++)
		if((RC_TIMEOUT == lModQ->execute()) && (RC_TIMEOUT == lDelQ->execute()))
			cnt++;
	mLogger.print("lQ->modifyPINs(lV, 8)\n");
	TVERIFY(cnt>0);

	resetAbortQ();
	TVERIFYRC(lModQ->execute());
	TVERIFYRC(lDelQ->execute());
	lModQ->destroy();
	lDelQ->destroy();
		
	setAbortQ();
	IStmt *lUDelQ = lQ->clone(STMT_UNDELETE);
	cnt = 0;
	for(int i = 0; i<nTimes; i++)
		if(RC_TIMEOUT == lUDelQ->execute())
			cnt++;	
	mLogger.print("lQ->undeletePINs()\n");
	TVERIFY(cnt>0);

	resetAbortQ();
	TVERIFYRC(lUDelQ->execute());
	lUDelQ->destroy();
	
	setAbortQ();
	IStmt *lCopyQ = lQ->clone(STMT_INSERT);
	lCopyQ->setValues(lV,8);
	for(int i = 0; i<nTimes; i++)
		if(RC_TIMEOUT == lCopyQ->execute())
			cnt++;	
	mLogger.print("lQ->copyPINs(lV,8)\n");
	TVERIFY(cnt>0);
	lCopyQ->destroy();

	mStop = true;
}

void TestAbortQuery::createPINs(int pNumPINs)
{
	mLogger.out() << " Creating " << pNumPINs << " PINs ...";
	int i = 0;
	for(i = 0; i < pNumPINs; i++)
	{
		if(i % 100 == 0) mLogger.out() << ".";
		PID lPID;
		Value lV[20];
		SETVALUE(lV[0], mPropIDs[0], i, OP_SET);
		Tstring lStr; MVTRand::getString(lStr, 100, 100, true, false);
		SETVALUE(lV[1], mPropIDs[1], lStr.c_str(), OP_SET);
		SETVALUE(lV[2], mPropIDs[2], lStr.c_str(), OP_SET);
		Tstring lWStr; MVTRand::getString(lWStr, 100, 100, true, false);
		SETVALUE(lV[3], mPropIDs[3], lWStr.c_str(), OP_SET);
		double lDouble = (double)MVTRand::getRange(10, 1000);
		SETVALUE(lV[4], mPropIDs[4], lDouble, OP_SET);
		SETVALUE(lV[5], mPropIDs[5], MVTRand::getBool(), OP_SET);
		lV[6].setNow(); SETVATTR(lV[6], mPropIDs[6], OP_SET);
		lV[7].setNow(); SETVATTR(lV[7], mPropIDs[7], OP_SET);
		IStream *lStream = new TestStringStream(MVTRand::getRange(10, 30) * 1024);
		SETVALUE(lV[8], mPropIDs[8], MVTApp::wrapClientStream(mSession, lStream), OP_SET);
		IStream *lStream1 = new TestStringStream(MVTRand::getRange(10, 30) * 1024);
		SETVALUE(lV[9], mPropIDs[9], MVTApp::wrapClientStream(mSession, lStream1), OP_SET);
		SETVALUE(lV[10], PROP_SPEC_CREATED, 1, OP_SET);
		SETVALUE(lV[11], PROP_SPEC_UPDATED, i, OP_SET);
		CREATEPIN(mSession, &lPID, lV, 12);
	}

	mLogger.out() << " DONE " << std::endl;
}

void TestAbortQuery::abortQuery()
{
	while(!mStop)
	{
		while(!mLock.trylock())
		{
			MVTestsPortability::threadSleep(1);
			continue;
		}
		if(mStartAbortQ)
			mSession->abortQuery();
		mLock.unlock();
		MVTestsPortability::threadSleep(50);
	}
}

int TestAbortQuery::execute()
{
	bool lSuccess = true;	
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();
		MVTApp::mapURIs(mSession, "TestAbortQuery.prop.", sNumProps, mPropIDs);
		mStoreCtx = MVTApp::getStoreCtx();
		doTest();
		mSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	return lSuccess?0:1;
}
