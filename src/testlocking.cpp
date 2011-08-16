/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

// Publish this test.
class TestLocking : public ITest
{
		MVStoreKernel::StoreCtx *mStoreCtx;
	public:
		TEST_DECLARE(TestLocking);
		virtual char const * getName() const { return "testlocking"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "tests 2PL deadlock resolution"; }
		virtual int execute();
		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestLocking, TestLogger::kDStdOut);

PropertyID gProp101, gProp102;

// Implement this test.
class testLocking_s
{
	public:
		PID mPID1, mPID2;
		RC mRC1, mRC2;
		volatile long mStep;
		MVStoreKernel::Event mNextStep;
		MVStoreKernel::StoreCtx *mStoreCtx;
		testLocking_s() : mRC1(RC_OK), mRC2(RC_OK), mStep(0), mStoreCtx(NULL){}
		void setStoreCtx(MVStoreKernel::StoreCtx *pCtx) { mStoreCtx = pCtx; }
		void nextStep() { INTERLOCKEDI(&mStep); mNextStep.signalAll(); }
		void waitStep(long pStep) { MVStoreKernel::Mutex lBogus; lBogus.lock(); while (pStep != mStep) mNextStep.wait(lBogus, 100000); mNextStep.reset(); lBogus.unlock(); }
};

THREAD_SIGNATURE threadTestLocking1(void * pTL)
{
	testLocking_s * const lTL = (testLocking_s *)pTL;
	ISession * const lSession = MVTApp::startSession(lTL->mStoreCtx);
	Value lV;
	lTL->waitStep(0);
	if (RC_OK == (lTL->mRC1=lSession->startTransaction()))
	{
		IPIN * const lPIN1 = lSession->getPIN(lTL->mPID1);
		if (lPIN1==NULL || NULL==lPIN1->getValue((PropertyID)gProp101))
			lTL->mRC1 = RC_NOTFOUND;
		else 
		{
			lTL->nextStep();

			lTL->waitStep(2);
			IPIN * const lPIN2 = lSession->getPIN(lTL->mPID2);
			if (NULL == lPIN2)
				lTL->mRC1 = RC_NOTFOUND;
			else
			{
				SETVALUE(lV, gProp102, "Bye completed!", OP_SET);
				if (RC_OK == (lTL->mRC1=lPIN2->modify(&lV, 1)))
					lTL->mRC1 = lSession->commit();
				lPIN2->destroy();
			}
			lPIN1->destroy();	
		}
	}
	lSession->terminate();
	return 0;
}

THREAD_SIGNATURE threadTestLocking2(void * pTL)
{
	testLocking_s * const lTL = (testLocking_s *)pTL;
	ISession * const lSession = MVTApp::startSession(lTL->mStoreCtx);
	Value lV;
	lTL->waitStep(1);
	if (RC_OK == (lTL->mRC2=lSession->startTransaction()))
	{
		IPIN * const lPIN2 = lSession->getPIN(lTL->mPID2);
		if (NULL == lPIN2)
			lTL->mRC2 = RC_NOTFOUND;
		else
		{
			SETVALUE(lV, gProp102, "Bye modified...", OP_SET);
			if (RC_OK == (lTL->mRC2=lPIN2->modify(&lV, 1)))
			{
				lTL->nextStep();

				IPIN * const lPIN1 = lSession->getPIN(lTL->mPID1);
				if (NULL == lPIN1)
					lTL->mRC2 = RC_NOTFOUND;
				else
				{
					SETVALUE(lV, gProp101, "Hello completed!", OP_SET);
					if (RC_OK == (lTL->mRC2=lPIN1->modify(&lV, 1)))
						lTL->mRC2 = lSession->commit();
					lPIN1->destroy();
				}
			}
			lPIN2->destroy();
		}
	}
	lSession->terminate();
	return 0;
}

int TestLocking::execute()
{
	// Open the store.
	bool lSuccess = false;
	if (MVTApp::startStore())
	{
		lSuccess = true;
		ISession * const lSession =	MVTApp::startSession();
		mStoreCtx = MVTApp::getStoreCtx();

		// Create 2 PINs for testing purposes.
		testLocking_s lTL;
		lTL.setStoreCtx(mStoreCtx);

		gProp101=MVTUtil::getProp(lSession,"TestLocking.101");
		gProp102=MVTUtil::getProp(lSession,"TestLocking.102");

		Value lV;
		SETVALUE(lV, gProp101, "Hello", OP_SET);
		CREATEPIN(lSession, lTL.mPID1, &lV, 1);
		SETVALUE(lV, gProp102, "Bye", OP_SET);
		CREATEPIN(lSession, lTL.mPID2, &lV, 1);

		// Create 2 threads executing the typical deadlocking scenario (for 2PL protocol):
		// t1: rx      wy
		// t2:   wy wx
		HTHREAD t1, t2;
		createThread(&threadTestLocking1, &lTL, t1);
		createThread(&threadTestLocking2, &lTL, t2);
		HTHREAD ts[] = {t1, t2}; MVStoreKernel::threadsWaitFor(sizeof(ts)/sizeof(ts[0]), ts);

		if ((lTL.mRC1 != RC_OK || lTL.mRC2 != RC_NOTFOUND) && (lTL.mRC1 != RC_NOTFOUND || lTL.mRC2 != RC_OK)) {
			if (lTL.mRC1 == RC_OK && lTL.mRC2 == RC_OK)
				mLogger.out() << "No deadlock detected! Test failed" << std::endl;
			lSuccess = false;
		}

		lSession->terminate();
		MVTApp::stopStore();
	}

	return lSuccess ? 0 : 1;
}
