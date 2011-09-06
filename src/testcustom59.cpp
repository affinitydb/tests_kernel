/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace MVStoreKernel; // InterLock

// Publish this test.
class TestCustom59 : public ITest
{
	public:
		TEST_DECLARE(TestCustom59);
		virtual char const * getName() const { return "testcustom59"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "step-by-step scenarios related to transaction locking"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	public:
		enum eSteps { kSWaiting = 0, kSSleepAndGo, kSSleep, kSLastSynchroStep = kSSleep, kSStartTx, kSCommitTx, kSRead1, kSRead2, kSRead3, kSWrite1, kSWrite2, kSWrite3, kSQuickCheckFTIndexingIsolation };
		typedef std::vector<eSteps> TThreadProgram;
		typedef std::vector<TThreadProgram> TDeadlockProgram;
		MVStoreKernel::StoreCtx * mStoreCtx;
		MVTestsPortability::Mutex mSteppingL, mLoggingL;
		PID mPID1, mPID2, mPID3;
		PropertyID mPropIds[2];
		long volatile mStep, mStepCnt, mStepCntReset;
	public:
		static THREAD_SIGNATURE threadTestCustom59(void * pC);
		void nextStep(long pWaitForStep = -1)
		{
			long lWaitForStep = pWaitForStep;
			{
				MVTestsPortability::MutexP lLock(&mSteppingL);
				if (0 == --mStepCnt)
				{
					InterlockedIncrement(&mStep);
					mStepCnt = mStepCntReset;
				}
				else
					lWaitForStep = mStep;
			}
			while (mStep <= lWaitForStep)
				MVTestsPortability::threadSleep(100);
		}
		void nextStep(bool pFreeRun, long & pFreeStep)
		{
			if (pFreeRun)
				pFreeStep++;
			else
				nextStep();
		}
		long detachStepping(bool & pDetached)
		{
			MVTestsPortability::MutexP lLock(&mSteppingL);
			long const lResult = mStep;
			if (pDetached)
				return lResult;
			pDetached = true;
			mStepCntReset--;
			if (0 == --mStepCnt)
			{
				InterlockedIncrement(&mStep);
				mStepCnt = mStepCntReset;
			}
			return lResult;
		}
		static char const * stepName(eSteps pS)
		{
			switch (pS)
			{
				case kSWaiting: return "waiting";
				case kSSleepAndGo: return "sleep&go";
				case kSSleep: return "sleep";
				case kSStartTx: return "starttx";
				case kSCommitTx: return "committx";
				case kSRead1: return "read1";
				case kSRead2: return "read2";
				case kSRead3: return "read3";
				case kSWrite1: return "write1";
				case kSWrite2: return "write2";
				case kSWrite3: return "write3";
				case kSQuickCheckFTIndexingIsolation: return "quick_check_ft_indexing_isolation";
			}
			return "unknown";
		}
		void logLineHeader(long pThreadIndex)
		{
			getLogger().out() << "[" << std::dec << pThreadIndex << ":" << std::hex << getThreadId() << "] ";
		}
	public:
		static void addToProgram(TDeadlockProgram & pProgram, eSteps const * pSteps, size_t pNumSteps)
		{
			TThreadProgram lTP;
			size_t i;
			for (i = 0; i < pNumSteps; i++)
				lTP.push_back(pSteps[i]);
			pProgram.push_back(lTP);
		}
		bool runProgram(ISession & pSession, TestCustom59::TDeadlockProgram const & pProgram, char const * pTitle, bool pDeadlockExpected);
		bool program1(ISession & pSession);
		bool program2(ISession & pSession);
		bool program3(ISession & pSession);
		bool program4(ISession & pSession);
		bool program5(ISession & pSession);
		bool program5b(ISession & pSession);
		bool program6(ISession & pSession);
		bool program7(ISession & pSession);
		bool program8(ISession & pSession);
		bool program9(ISession & pSession);
		bool program10(ISession & pSession);
		bool program11(ISession & pSession);
};
TEST_IMPLEMENT(TestCustom59, TestLogger::kDStdOut);

class testCustom59_s
{
	public:
		TestCustom59 & mTest;
		TestCustom59::eSteps const * const mSteps;
		long const mNumSteps;
		long const mThreadIndex;
		long mThreadId;
		bool mDeadlocked, mDetachedStepping;
		testCustom59_s(TestCustom59 & pTest, TestCustom59::eSteps const * const pSteps, long pNumSteps, long pThreadIndex)
			: mTest(pTest), mSteps(pSteps), mNumSteps(pNumSteps), mThreadIndex(pThreadIndex), mThreadId(0), mDeadlocked(false), mDetachedStepping(false) {}
};

THREAD_SIGNATURE TestCustom59::threadTestCustom59(void * pC)
{
	testCustom59_s * const lC = (testCustom59_s *)pC;
	ISession * const lSession = MVTApp::startSession(lC->mTest.mStoreCtx);
	lC->mThreadId = (long int)getThreadId();
	bool lFreeRun = false;
	long lFreeStep;
	long volatile * lStepPtr = &lC->mTest.mStep;
	while (*lStepPtr < lC->mNumSteps)
	{
		RC lRC = RC_OK;
		Value lV[2];
		SETVALUE(lV[0], lC->mTest.mPropIds[0], "pomme poire banane", OP_SET);
		SETVALUE_C(lV[1], lC->mTest.mPropIds[1], (int)lC->mThreadId, OP_ADD, STORE_LAST_ELEMENT);
		long const lStepIndex = *lStepPtr;
		TestCustom59::eSteps const lStep = lC->mSteps[lStepIndex];
		if (TestCustom59::kSLastSynchroStep < lStep)
		{
			lC->mTest.mOutputLock.lock();
			lC->mTest.logLineHeader(lC->mThreadIndex);
			lC->mTest.getLogger().out() << "step #" << std::dec << lStepIndex << ":";
			lC->mTest.getLogger().out() << lC->mTest.stepName(lStep);
			lC->mTest.getLogger().out() << std::endl;
			lC->mTest.mOutputLock.unlock();
		}
		switch (lStep)
		{
			case TestCustom59::kSWaiting: if (!lFreeRun) lC->mTest.nextStep(lStepIndex); break;
			case TestCustom59::kSSleepAndGo:
			{
				MVTestsPortability::threadSleep(1000);
				if (!lFreeRun)
				{
					lFreeStep = lC->mTest.detachStepping(lC->mDetachedStepping);
					lStepPtr = &lFreeStep;
					lFreeRun = true;
				}
				lFreeStep++;
				break;
			}
			case TestCustom59::kSSleep: MVTestsPortability::threadSleep(1000); lC->mTest.nextStep(lFreeRun, lFreeStep); break;
			case TestCustom59::kSStartTx: lSession->startTransaction(); lC->mTest.nextStep(lFreeRun, lFreeStep); break;
			case TestCustom59::kSCommitTx: lRC = lSession->commit(); lC->mTest.nextStep(lFreeRun, lFreeStep); break;
			case TestCustom59::kSRead1: if (NULL == lSession->getPIN(lC->mTest.mPID1)) lC->mDeadlocked = true; lC->mTest.nextStep(lFreeRun, lFreeStep); break;
			case TestCustom59::kSRead2: if (NULL == lSession->getPIN(lC->mTest.mPID2)) lC->mDeadlocked = true; lC->mTest.nextStep(lFreeRun, lFreeStep); break;
			case TestCustom59::kSRead3: if (NULL == lSession->getPIN(lC->mTest.mPID3)) lC->mDeadlocked = true; lC->mTest.nextStep(lFreeRun, lFreeStep); break;
			case TestCustom59::kSWrite1: lRC = lSession->modifyPIN(lC->mTest.mPID1, lV, 2); lC->mTest.nextStep(lFreeRun, lFreeStep); break;
			case TestCustom59::kSWrite2: lRC = lSession->modifyPIN(lC->mTest.mPID2, lV, 2); lC->mTest.nextStep(lFreeRun, lFreeStep); break;
			case TestCustom59::kSWrite3: lRC = lSession->modifyPIN(lC->mTest.mPID3, lV, 2); lC->mTest.nextStep(lFreeRun, lFreeStep); break;
			case TestCustom59::kSQuickCheckFTIndexingIsolation:
			{
				IStmt * lQWord = lSession->createStmt();
				unsigned char lVar = lQWord->addVariable();
				lQWord->setConditionFT(lVar, "poire");
				ICursor * lRWord = NULL;
				lQWord->execute(&lRWord);
				if (lRWord)
				{
					IPIN * lNext;
					while (NULL != (lNext = lRWord->next()))
					{
						if (lC->mTest.mPID1 == lNext->getPID() || lC->mTest.mPID2 == lNext->getPID() || lC->mTest.mPID3 == lNext->getPID())
						{
							lC->mTest.mOutputLock.lock();
							lC->mTest.logLineHeader(lC->mThreadIndex);
							lC->mTest.getLogger().out() << "WARNING: FT found pin " << std::hex << lNext->getPID().pid << std::endl;
							lC->mTest.mOutputLock.unlock();
						}	
						lNext->destroy();
					}
					lRWord->destroy();
				}
				else
				{
					lC->mTest.logLineHeader(lC->mThreadIndex);
					lC->mTest.getLogger().out() << "WARNING: Couldn't obtain a query result!?" << std::endl;
				}
				lQWord->destroy();
				lC->mTest.nextStep(lFreeRun, lFreeStep);
				break;
			}
			default: assert(false); break;
		}

		if (RC_DEADLOCK == lRC)
			lC->mDeadlocked = true;
		else if (RC_OK != lRC)
		{
			lC->mTest.mOutputLock.lock();
			lC->mTest.logLineHeader(lC->mThreadIndex);
			lC->mTest.getLogger().out() << "Failure " << std::dec << lRC << std::endl;
			lC->mTest.mOutputLock.unlock();
		}

		if (lC->mDeadlocked)
		{
			lSession->rollback(); // Review: This will not be needed later on.

			lC->mTest.mOutputLock.lock();
			lC->mTest.logLineHeader(lC->mThreadIndex);
			lC->mTest.getLogger().out() << "DEADLOCKED" << std::endl;
			lC->mTest.mOutputLock.unlock();

			lC->mTest.detachStepping(lC->mDetachedStepping);
			lFreeStep = lC->mNumSteps;
			lStepPtr = &lFreeStep;
			lFreeRun = true;
		}
	}
	lC->mTest.detachStepping(lC->mDetachedStepping);
	lC->mTest.mOutputLock.lock();
	lC->mTest.logLineHeader(lC->mThreadIndex);
	lC->mTest.getLogger().out() << "done" << std::endl;
	lC->mTest.mOutputLock.unlock();
	lSession->terminate();
	return 0;
}

int TestCustom59::execute()
{
	mStoreCtx = NULL;

	// Open the store.
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		mStoreCtx = MVTApp::getStoreCtx();
		ISession * lSession = MVTApp::startSession();
		MVTApp::mapURIs(lSession, "testcustom59.prop.", 2, mPropIds);

		if (!program1(*lSession)) lSuccess = false;
		if (!program2(*lSession)) lSuccess = false;
		if (!program3(*lSession)) lSuccess = false;
		if (!program4(*lSession)) lSuccess = false;
		if (!program5(*lSession)) lSuccess = false;
		if (!program5b(*lSession)) lSuccess = false;
		if (!program6(*lSession)) lSuccess = false;
		if (!program7(*lSession)) lSuccess = false;
		if (!program8(*lSession)) lSuccess = false;
		if (!program9(*lSession)) lSuccess = false;
		if (!program10(*lSession)) lSuccess = false;
		if (!program11(*lSession)) lSuccess = false;

		// Cleanup.
		lSession->terminate();
		MVTApp::stopStore();
	}

	return lSuccess ? 0 : 1;
}

static bool containsValue(MVStore::Value const & pV, long pValue)
{
	switch (pV.type)
	{
		case MVStore::VT_INT: return (int32_t)pValue == pV.i;
		case MVStore::VT_ARRAY: { size_t i; for (i = 0; i < pV.length; i++) if (containsValue(pV.varray[i], pValue)) return true; } return false;
		case MVStore::VT_COLLECTION: if (pV.nav) { Value const * lNext; for (lNext = pV.nav->navigate(GO_FIRST); NULL != lNext; lNext = pV.nav->navigate(GO_NEXT)) { if (containsValue(*lNext, pValue)) return true; } } return false;
		default: return false;
	}
}

bool TestCustom59::runProgram(ISession & pSession, TestCustom59::TDeadlockProgram const & pProgram, char const * pTitle, bool pDeadlockExpected)
{
	bool lSuccess = true;
	getLogger().out() << std::endl << "Program: " << pTitle << std::endl;

	// Create 2 pins for this program.
	CREATEPIN(&pSession, mPID1, NULL, 0); getLogger().out() << "pin1: " << std::hex << mPID1.pid << std::endl;
	CREATEPIN(&pSession, mPID2, NULL, 0); getLogger().out() << "pin2: " << std::hex << mPID2.pid << std::endl;
	CREATEPIN(&pSession, mPID3, NULL, 0); getLogger().out() << "pin3: " << std::hex << mPID3.pid << std::endl;
	mStep = 0;
	mStepCnt = mStepCntReset = (long)pProgram.size();

	// Start all threads of the program.
	HTHREAD * lT = (HTHREAD *)alloca(sizeof(HTHREAD) * pProgram.size());
	testCustom59_s ** lCtx = (testCustom59_s **)alloca(sizeof(testCustom59_s *) * pProgram.size());
	size_t iT;
	for (iT = 0; iT < pProgram.size(); iT++)
	{
		lCtx[iT] = new testCustom59_s(*this, &pProgram[iT][0], (long)pProgram[iT].size(), (long)iT);
		createThread(&TestCustom59::threadTestCustom59, lCtx[iT], lT[iT]);
	}

	// Wait for completion.
	MVTestsPortability::threadsWaitFor((int)pProgram.size(), lT);

	// Verifications and cleanup.
	bool lDeadlockFound = false;
	for (iT = 0; iT < pProgram.size(); iT++)
	{
		if (lCtx[iT]->mDeadlocked)
		{
			lDeadlockFound = true;

			// Verify that the pins of this test don't contain committed modifs from the threads that were rolled back by deadlock.
			IPIN * const lTstPIN = pSession.getPIN(mPID1);
			Value const * lV = lTstPIN->getValue(mPropIds[1]);
			if (lV && containsValue(*lV, lCtx[iT]->mThreadId))
			{
				getLogger().out() << "[" << std::dec << lCtx[iT]->mThreadIndex << ":" << std::hex << lCtx[iT]->mThreadId << "] *** UNEXPECTED SURVIVING MODIF!" << std::endl;
				MVTApp::output(*lTstPIN, getLogger().out(), &pSession);
				lSuccess = false;
			}
			lTstPIN->destroy();
		}
		else
		{
			// Verify that the pins of this test do contain committed modifs from the threads that were not rolled back by deadlock.
			bool lWrites = false;
			size_t iStep;
			for (iStep = 0; iStep < pProgram[iT].size() && !lWrites; iStep++)
				lWrites = (kSWrite1 == pProgram[iT][iStep] || kSWrite1 == pProgram[iT][iStep]);
			if (lWrites)
			{
				IPIN * const lTstPIN = pSession.getPIN(mPID1);
				Value const * lV = lTstPIN->getValue(mPropIds[1]);
				if (!lV || !containsValue(*lV, lCtx[iT]->mThreadId))
				{
					getLogger().out() << "[" << std::dec << lCtx[iT]->mThreadIndex << ":" << std::hex << lCtx[iT]->mThreadId << "] *** EXPECTED A MODIF!" << std::endl;
					MVTApp::output(*lTstPIN, getLogger().out(), &pSession);
					lSuccess = false; 
				}
				lTstPIN->destroy();
			}
		}
		delete lCtx[iT];
	}
	if (lDeadlockFound && !pDeadlockExpected)
		{ getLogger().out() << "*** UNEXPECTED DEADLOCK!" << std::endl; lSuccess = false; }
	else if (!lDeadlockFound && pDeadlockExpected)
		{ getLogger().out() << "*** EXPECTED A DEADLOCK!" << std::endl; lSuccess = false; }
	return lSuccess;
}

#define TC59_RUN2(s1, s2, title, deadlockexpected) \
	TDeadlockProgram lDP; \
	addToProgram(lDP, s1, sizeof(s1) / sizeof(s1[0])); \
	addToProgram(lDP, s2, sizeof(s2) / sizeof(s2[0])); \
	return runProgram(pSession, lDP, title, deadlockexpected);
#define TC59_RUN3(s1, s2, s3, title, deadlockexpected) \
	TDeadlockProgram lDP; \
	addToProgram(lDP, s1, sizeof(s1) / sizeof(s1[0])); \
	addToProgram(lDP, s2, sizeof(s2) / sizeof(s2[0])); \
	addToProgram(lDP, s3, sizeof(s3) / sizeof(s3[0])); \
	return runProgram(pSession, lDP, title, deadlockexpected);
#define TC59_RUN4(s1, s2, s3, s4, title, deadlockexpected) \
	TDeadlockProgram lDP; \
	addToProgram(lDP, s1, sizeof(s1) / sizeof(s1[0])); \
	addToProgram(lDP, s2, sizeof(s2) / sizeof(s2[0])); \
	addToProgram(lDP, s3, sizeof(s3) / sizeof(s3[0])); \
	addToProgram(lDP, s4, sizeof(s4) / sizeof(s4[0])); \
	return runProgram(pSession, lDP, title, deadlockexpected);

bool TestCustom59::program1(ISession & pSession)
{
	eSteps s1[] = { kSStartTx, kSWaiting, kSWaiting, kSWaiting, kSRead1,      kSRead2,  kSWrite2, kSSleepAndGo, kSCommitTx };
	eSteps s2[] = { kSWaiting, kSStartTx, kSRead1,   kSWrite1,  kSSleepAndGo, kSCommitTx };
	TC59_RUN2(s1, s2, "The initial test case (not a deadlock)", false);
}

bool TestCustom59::program2(ISession & pSession)
{
	eSteps s1[] = { kSStartTx, kSWaiting, kSWaiting, kSWrite2,  kSWrite1,     kSSleepAndGo, kSCommitTx };
	eSteps s2[] = { kSWaiting, kSStartTx, kSWrite1,  kSWaiting, kSSleepAndGo, kSWrite2,     kSCommitTx };
	TC59_RUN2(s1, s2, "Sko's attempt at doing a known deadlock", true);
}

bool TestCustom59::program3(ISession & pSession)
{
	eSteps s1[] = { kSStartTx, kSWaiting, kSWaiting, kSRead2,   kSWrite2,     kSSleepAndGo, kSCommitTx };
	eSteps s2[] = { kSWaiting, kSStartTx, kSRead2,   kSWaiting, kSSleepAndGo, kSCommitTx };
	TC59_RUN2(s1, s2, "Bug 7677 (not a deadlock)", false);
}

bool TestCustom59::program4(ISession & pSession)
{
	eSteps s1[] = { kSStartTx, kSWaiting, kSWaiting, kSWaiting, kSRead2,   kSWaiting, kSWaiting,  kSWrite2,  kSCommitTx };
	eSteps s2[] = { kSWaiting, kSStartTx, kSRead1,   kSRead2,   kSWaiting, kSWrite1,  kSCommitTx };
	TC59_RUN2(s1, s2, "A variation of bug 7677 (not a deadlock)", false);
}

bool TestCustom59::program5(ISession & pSession)
{
	eSteps s1[] = { kSStartTx, kSWaiting, kSRead1,   kSRead2,   kSWaiting, kSWaiting, kSSleepAndGo, kSWrite2,  kSCommitTx };
	eSteps s2[] = { kSWaiting, kSStartTx, kSWaiting, kSWaiting, kSRead1,   kSRead2,   kSSleepAndGo, kSWrite1,  kSCommitTx };
	TC59_RUN2(s1, s2, "An example for Roger", true);
}

bool TestCustom59::program5b(ISession & pSession)
{
	eSteps s1[] = { kSStartTx, kSWaiting, kSRead1,   kSWaiting, kSSleepAndGo, kSWrite2,  kSCommitTx };
	eSteps s2[] = { kSWaiting, kSStartTx, kSWaiting, kSRead2,   kSSleepAndGo, kSWrite1,  kSCommitTx };
	TC59_RUN2(s1, s2, "Example for Roger simplified", true);
}

bool TestCustom59::program6(ISession & pSession)
{
	eSteps s1[] = { kSStartTx, kSWrite1,  kSWaiting, kSCommitTx };
	eSteps s2[] = { kSWaiting, kSWaiting, kSQuickCheckFTIndexingIsolation };
	TC59_RUN2(s1, s2, "A quick check of FT indexing vs transaction isolation (not expected to work at the moment)", false);
}

bool TestCustom59::program7(ISession & pSession)
{
	eSteps s1[] = { kSStartTx, kSWrite1,  kSWaiting,    kSWaiting, kSWaiting, kSWrite2,     kSSleepAndGo, kSCommitTx };
	eSteps s2[] = { kSWaiting, kSWaiting, kSSleepAndGo, kSWrite1 };
	eSteps s3[] = { kSWaiting, kSWaiting, kSWaiting,    kSStartTx, kSWrite2,  kSSleepAndGo, kSRead1,      kSCommitTx };
	TC59_RUN3(s1, s2, s3, "First test case #13061", true);
}

bool TestCustom59::program8(ISession & pSession)
{
	eSteps s1[] = { kSStartTx, kSWrite1,  kSWaiting,    kSWaiting, kSWaiting, kSWaiting, kSWaiting, kSSleepAndGo, kSWrite2, kSCommitTx };
	eSteps s2[] = { kSWaiting, kSWaiting, kSSleepAndGo, kSWrite1 };
	eSteps s3[] = { kSWaiting, kSWaiting, kSWaiting,    kSStartTx, kSWrite3,  kSWaiting, kSWaiting, kSSleepAndGo, kSSleep, kSSleep, kSRead1,   kSCommitTx };
	eSteps s4[] = { kSWaiting, kSWaiting, kSWaiting,    kSWaiting, kSWaiting, kSStartTx, kSWrite2,  kSSleepAndGo, kSSleep, kSRead3, kSCommitTx };
	TC59_RUN4(s1, s2, s3, s4, "Second test case for #13061", true);
}

bool TestCustom59::program9(ISession & pSession)
{
	eSteps s1[] = { kSStartTx, kSWrite1,  kSWaiting, kSWaiting, kSSleepAndGo, kSWrite2, kSCommitTx };
	eSteps s2[] = { kSWaiting, kSWaiting, kSStartTx, kSWrite2,  kSWaiting,    kSSleepAndGo, kSWrite3, kSCommitTx };
	eSteps s3[] = { kSWaiting, kSWaiting, kSWaiting, kSStartTx, kSWrite3,     kSSleepAndGo, kSWrite1, kSCommitTx };
	TC59_RUN3(s1, s2, s3, "Third test case for #13061", true);
}

bool TestCustom59::program10(ISession & pSession)
{
	eSteps s1[] = { kSWaiting, kSWaiting, kSStartTx, kSSleepAndGo, kSWrite1,  kSWrite2,  kSCommitTx };
	eSteps s2[] = { kSWaiting, kSWaiting, kSWaiting, kSWaiting,    kSStartTx, kSWrite2,  kSWaiting,    kSSleepAndGo, kSWrite3, kSCommitTx };
	eSteps s3[] = { kSWaiting, kSWaiting, kSWaiting, kSWaiting,    kSWaiting, kSStartTx, kSWrite3,     kSSleepAndGo, kSWrite1, kSCommitTx };
	eSteps s4[] = { kSStartTx, kSRead1,   kSWaiting, kSWaiting,    kSWaiting, kSWaiting, kSSleepAndGo, kSRead2,      kSCommitTx };
	TC59_RUN4(s1, s2, s3, s4, "Fourth test case for #13061", true);
}

bool TestCustom59::program11(ISession & pSession)
{
	eSteps s1[] = { kSWaiting, kSWaiting, kSStartTx, kSSleepAndGo, kSWrite1,  kSWrite2,  kSCommitTx };
	eSteps s2[] = { kSWaiting, kSWaiting, kSWaiting, kSWaiting,    kSStartTx, kSWrite2,  kSWaiting,    kSSleepAndGo, kSWrite3, kSCommitTx };
	eSteps s3[] = { kSWaiting, kSWaiting, kSWaiting, kSWaiting,    kSWaiting, kSStartTx, kSWrite3,     kSSleepAndGo, kSWrite1, kSSleep, kSWrite2, kSCommitTx };
	eSteps s4[] = { kSStartTx, kSRead1,   kSWaiting, kSWaiting,    kSWaiting, kSWaiting, kSSleepAndGo, kSRead2,      kSCommitTx };
	TC59_RUN4(s1, s2, s3, s4, "Fifth test case for #13061 (chain of deadlocks) - N.b. repros #12808 (and segfaults)", true);
}
