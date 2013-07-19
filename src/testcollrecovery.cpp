/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

class TestCollRecovery:  public ITest
{
		static const int sNumProps = 10;
		PropertyID mPropIds[sNumProps];
		Afy::IAffinity *mStoreCtx;
	public:
		TEST_DECLARE(TestCollRecovery);
		virtual char const * getName() const { return "testcollrecovery"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "repro for bug 15276"; }
		virtual bool excludeInIPCSmokeTest(char const *& pReason) const { pReason = "Big collection.... outparameter overflow"; return true; }

		virtual void destroy() { delete this; }
		virtual int execute();
		static THREAD_SIGNATURE Thread1Function(void * pThis){((TestCollRecovery *)pThis)->thread1Implementation(); return 0;}
		static THREAD_SIGNATURE Thread2Function(void * pThis){((TestCollRecovery *)pThis)->thread2Implementation(); return 0;}
		void thread1Implementation();
		void thread2Implementation();	
		void addCollection(ISession *pSession, PID pPID, int pNumElements, PropertyID pPropID);
	private:
		ISession * mSession ;
		long volatile mStartRollback;
		long volatile mReadyToExit;		
};

TEST_IMPLEMENT(TestCollRecovery, TestLogger::kDStdOut);

void TestCollRecovery::thread1Implementation()
{
	ISession *lSession = MVTApp::startSession(mStoreCtx);
	
	// createPIN1
	PID lPID1;
	CREATEPIN(lSession, lPID1, NULL, 0);

	// createPIN2
	PID lPID2;
	CREATEPIN(lSession, lPID2, NULL, 0);

	// beginTransaction
	lSession->startTransaction();

	// addCollection with 355 items for prop2 for PIN1
	addCollection(lSession, lPID1, 355, mPropIds[2]);

	// addCollection with 456 items for prop0 for PIN2
	addCollection(lSession, lPID2, 456, mPropIds[0]);

	// addCollection with 450 items for prop3 for PIN1
	addCollection(lSession, lPID1, 450, mPropIds[3]);

	INTERLOCKEDI(&mReadyToExit);

	while(!mStartRollback) MVTestsPortability::threadSleep(500);

	// rollback
	lSession->rollback();	

	lSession->terminate();
}

void TestCollRecovery::thread2Implementation()
{
	ISession *lSession = MVTApp::startSession(mStoreCtx);
	
	// createPIN1
	PID lPID1;
	CREATEPIN(lSession, lPID1, NULL, 0);

	// createPIN2
	PID lPID2;
	CREATEPIN(lSession, lPID2, NULL, 0);

	// beginTransaction
	lSession->startTransaction();

	// deletePIN PIN1
	TVRC_R(lSession->deletePINs(&lPID1, 1), this);

	addCollection(lSession, lPID2, 22, mPropIds[0]);	

	// addCollection with 299 items for prop1 for PIN2
	addCollection(lSession, lPID2, 299, mPropIds[1]);	

	// addCollection with 391 items for prop2 for PIN2
	addCollection(lSession, lPID2, 391, mPropIds[2]);	

	// addCollection with 120 items for prop3 for PIN2
	addCollection(lSession, lPID2, 120, mPropIds[3]);	

	// addCollection with 456 items for prop4 for PIN2
	addCollection(lSession, lPID2, 456, mPropIds[4]);	

	INTERLOCKEDI(&mReadyToExit);

	while(!mStartRollback) MVTestsPortability::threadSleep(500);

	// rollback
	lSession->rollback();
	
	lSession->terminate();
}

void TestCollRecovery::addCollection(ISession *pSession, PID pPID, int pNumElements, PropertyID pPropID)
{
	std::vector<Tstring> lStrings;
	int lMaxElementSize = (int)(65024/pNumElements);
	int i = 0;
	for( i = 0; i < pNumElements; i++)
	{
		Tstring lStr; MVTRand::getString(lStr, (int)(lMaxElementSize/6), lMaxElementSize, true, true);
		lStrings.push_back(lStr);
	}

	IPIN *lPIN = pSession->getPIN(pPID); TVERIFY(pPID.pid != STORE_INVALID_PID);
	if(lPIN)
	{
		Value lV;
		SETVALUE(lV, pPropID, lStrings[0].c_str(), OP_ADD);
		TVERIFYRC(lPIN->modify(&lV, 1));

		Value *lVal = (Value *)pSession->malloc(sizeof(Value) * pNumElements-1);
		int k = 0;
		for(k = 0; k < pNumElements - 1; k++)
		{
			SETVALUE_C(lVal[k], pPropID, lStrings[k+1].c_str(), OP_ADD, STORE_LAST_ELEMENT); 
		}
		
		lV.set(lVal, k); lV.setPropID(pPropID); lV.op = OP_ADD; lV.eid = STORE_LAST_ELEMENT;
		TVERIFYRC(lPIN->modify(&lV, 1));
		pSession->free(lVal);
		lPIN->destroy();
	}
}

int TestCollRecovery::execute() 
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();
		mStoreCtx = MVTApp::getStoreCtx();
		MVTApp::mapURIs(mSession, "TestCollRecovery.prop", sNumProps, mPropIds);
		mStartRollback = 0; mReadyToExit = 0;
		static const size_t sNumThreads = 2;
		HTHREAD mThreads[sNumThreads];
		createThread(&Thread1Function, this, mThreads[0]);
		createThread(&Thread2Function, this, mThreads[1]);
		
		// createPIN1
		PID lPID1;
		CREATEPIN(mSession, lPID1, NULL, 0);

		// addCollection with 137 items for prop0 for PIN1
		addCollection(mSession, lPID1, 137, mPropIds[0]);

		// addCollection with 163 items for prop1 for PIN1
		addCollection(mSession, lPID1, 163, mPropIds[1]);

		// createPIN2
		PID lPID2;
		CREATEPIN(mSession, lPID2, NULL, 0);

		// beginTransaction
		mSession->startTransaction();

		// addCollection with 443 items for prop0 for PIN2
		addCollection(mSession, lPID2, 443, mPropIds[0]);

		// addCollection with 51 items for prop5 for PIN1
		addCollection(mSession, lPID1, 51, mPropIds[5]);

		// rollback
		mSession->rollback();

		while(mReadyToExit < 2) MVTestsPortability::threadSleep(500);
		INTERLOCKEDI(&mStartRollback);
		MVTestsPortability::threadSleep(750);
		MVTestsPortability::threadsWaitFor(sNumThreads, mThreads);
		mSession->terminate();
		MVTApp::stopStore();
	}
	return lSuccess?0:1;
}
