/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include <bitset>
#include "app.h"
#define MAX_PROPERTIES 256
typedef std::bitset<MAX_PROPERTIES> Tbitset;

// Publish this test.
class TestIsolation : public ITest
{
        Afy::IAffinity *mStoreCtx;
    public:
        TEST_DECLARE(TestIsolation);
        virtual char const * getName() const { return "testisolation"; }
        virtual char const * getHelp() const { return ""; }
        virtual char const * getDescription() const { return "runs concurrent transactions and checks isolation"; }
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Until the test passes..."; return false; }
        virtual int execute();
        virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestIsolation, TestLogger::kDStdOut);

// Implement this test.
// PITTestIsolation: a context for all threads; holds a parallel in-memory representation of the pins.
class PITMemoryPIN;
class PITTestIsolation
{
    public:
        static PropertyID sPropIDs[15];
        typedef std::vector<PID> TPIDs;
        TPIDs mPIDs;
        Tofstream mLog;
        MVTestsPortability::Mutex mLogLock;
        MVTestsPortability::Event mSynchro;
        Afy::IAffinity *mStoreCtx;
        volatile long mSynchroVal, mSynchroTarget, mFailure;
        volatile long mClock;
    public:
        PITTestIsolation() : mLog("testisolationlog.txt", std::ios::ate), mStoreCtx(NULL), mSynchroVal(0), mSynchroTarget(0), mFailure(0), mClock(0) {}
        void setStoreCtx(Afy::IAffinity *pCtx) { mStoreCtx = pCtx; }
        long nextClock() { return INTERLOCKEDI(&mClock); }
        bool beginLog() { mLogLock.lock(); return true; }
        Tofstream & log() { return mLog; }
        void endLog() { mLogLock.unlock(); }
        void synchro()
        {
            if (mSynchroTarget <= 0)
                return;
            if (mSynchroTarget == INTERLOCKEDI(&mSynchroVal))
            {
                #ifdef WIN32
                    InterlockedExchange(&mSynchroVal, 0);
                #else
                    mSynchroVal = 0; // Review
                #endif
                mSynchro.signal();
            }
            else
            {
                MVTestsPortability::Mutex lBogus;
                lBogus.lock();
                mSynchro.wait(lBogus, 100000);
                lBogus.unlock();
            }
        }
    public:
        void logChange(int pTIndex, int pPINIndex, long pPropIndex, Tstring const & pString, bool pFinal, RC pRC)
        {
            if (beginLog())
            {
                log() << (pFinal ? "*" : " ");
                if (RC_OK != pRC)
                    log() << "[ERROR" << pRC << "] ";
                log() << "thread" << pTIndex << ".pin" << pPINIndex << ".prop" << pPropIndex;
                log() << "(" << nextClock() << "):" << pString.c_str() << std::endl;
                log().flush();
                endLog();
            }
        }
};
PropertyID PITTestIsolation::sPropIDs[15];

class testIsolation_s
{
    public:
        TestLogger & mLogger;
        PITTestIsolation * mContext;
        int mIndex;
        unsigned int mRandomSeed;
        testIsolation_s(TestLogger & pLogger, PITTestIsolation * pContext, int pIndex, unsigned int pRandomSeed)
            : mLogger(pLogger), mContext(pContext), mIndex(pIndex), mRandomSeed(pRandomSeed) {}
};

THREAD_SIGNATURE threadTestIsolation(void * pTI)
{
    PITTestIsolation * const lTI = ((testIsolation_s *)pTI)->mContext;
    ISession * const lSession = MVTApp::startSession(lTI->mStoreCtx);	
    srand(((testIsolation_s *)pTI)->mRandomSeed);
    int const lTIndex = ((testIsolation_s *)pTI)->mIndex;
    int i, j, k;
    // Run for a while, or until we hit a failure.
    for (i = 0; i < 1000 && !lTI->mFailure; i++)
    {
        // Iterate over all test PINs.
        // Note: Every iteration is synchronized across all threads; they all talk to the same PIN at the same time.
        ((testIsolation_s *)pTI)->mLogger.out() << ".";
        for (j = 0; j < (int)lTI->mPIDs.size() && !lTI->mFailure; j++)
        {
            // Start the transaction (isolated, in principle).
            if (RC_OK != lSession->startTransaction())
                continue;

            // Retrieve the iterated PIN, and clone it.
            // The clone is our guarantied thread-local (tx-local) view of the PIN.
            PID const lPID = lTI->mPIDs[j];
            IPIN * const lPIN = lSession->getPIN(lPID);
            IPIN * const lMemPIN = lPIN->clone();

            // Do lots of operations in this transaction (reads and writes on the iterated PIN).
            for (k = 0; k < 100 && !lTI->mFailure; k++)
            {
                lPIN->refresh(); // No amount of refreshing should break isolation.
                bool const lRead = (0 != (k%5)); // Review (maxw): Maybe it would be better to just elect lTIndex==1 as the writer, to avoid deadlocks.
                unsigned const lPropIndex = (unsigned)((double)lPIN->getNumberOfProperties() * rand() / RAND_MAX);

                // Read.
                // Here we get the elected property from the actual PIN, and compare it with our in-memory copy.
                if (lRead && lPropIndex < lPIN->getNumberOfProperties())
                {
                    Value const * const lVreal = lPIN->getValueByIndex(lPropIndex);
                    Value const * const lVmem = lMemPIN->getValueByIndex(lPropIndex);
                    if (0 != strcmp(lVreal->str, lVmem->str))
                    {
                        if (lTI->beginLog())
                        {
                            INTERLOCKEDI(&lTI->mFailure);
                            lTI->log() << std::endl;
                            lTI->log() << "Problem detected: thread" << lTIndex << ".pin" << j << ".prop" << lPropIndex;
                            lTI->log() << "(" << lTI->nextClock() << ")" << std::endl;
                            lTI->log() << "  store:  " << lVreal->str << std::endl;
                            lTI->log() << "  cache:  " << lVmem->str << std::endl << std::endl;
                            lTI->endLog();
                            ((testIsolation_s *)pTI)->mLogger.out() << std::endl << "Problem detected (see testisolationlog.txt)!" << std::endl;
                        }
                    }
                }

                // Write.
                // Here we write to the actual PIN and to our in-memory copy; in principle, the actual PIN is modified only
                // inside the current {thread, transaction}; but if isolation is buggy, then those changes would become visible
                // to other threads.
                else
                {
                    Tstring lValue;
                    MVTRand::getString(lValue, 10, 100);
                    Value lV;
                    SETVALUE(lV, PITTestIsolation::sPropIDs[lPropIndex], lValue.c_str(), OP_SET);
                    RC const lRCMod = lPIN->modify(&lV, 1);
                    lTI->logChange(lTIndex, j, lPropIndex, lValue, false, lRCMod);
                    if (RC_OK == lRCMod)
                        lMemPIN->modify(&lV, 1);
                }
            }
            lPIN->destroy();

            // Commit or rollback, for variety; in principle, neither should affect the other threads.
            bool const lDoCommit = (5.0 * rand() / RAND_MAX) > 2.5;
            RC const lRCtx = lDoCommit ? lSession->commit() : lSession->rollback();
            if (RC_OK != lRCtx)
                lTI->log() << "Failed to " << (lDoCommit ? "commit" : "rollback") << ": " << lRCtx << std::endl;
            lMemPIN->destroy();

            // Synchronize the progression through the test across all threads.
            lTI->synchro();
        }
    }
    lSession->terminate();
    return 0;
}

int TestIsolation::execute()
{
    // Open the store.
    bool lSuccess = false;
    if (MVTApp::startStore())
    {
        lSuccess = true;
        ISession * const lSession =	MVTApp::startSession();
        mStoreCtx = MVTApp::getStoreCtx();

        PITTestIsolation lTI;
        lTI.setStoreCtx(mStoreCtx);

        MVTApp::mapURIs(lSession, "testisolation.prop", sizeof(PITTestIsolation::sPropIDs) / sizeof(PITTestIsolation::sPropIDs[0]), PITTestIsolation::sPropIDs);

        // Create some PINs.
        TestLogger lOutV(TestLogger::kDStdOutVerbose);
        lOutV.out() << "{" << getName() << "} Creating PINs..." << std::endl;
        int i, j;
        for (i = 0; i < 10; i++)
        {
            PID lPID;
            CREATEPIN(lSession, lPID, NULL, 0);
            IPIN * lPIN = lSession->getPIN(lPID);
            int const lNumProps = rand() * 10 / RAND_MAX; // Note: < MAX_PROPERTIES...
            for (j = 0; j < lNumProps; j++)
            {
                Tstring lValue;
                MVTRand::getString(lValue, 10, 100);
                Value lV;
                SETVALUE(lV, PITTestIsolation::sPropIDs[j], lValue.c_str(), OP_ADD);
                lPIN->modify(&lV, 1);
            }
            lPIN->destroy();
            lTI.mPIDs.push_back(lPID);
        }

        // Create threads to play with these PINs, and make sure that their transactions are isolated,
        // i.e. that one transaction never sees the data modified by another concurrent (uncommitted) transaction.
        #define TESTISOLATION_NUMTHREADS 2
        lTI.mSynchroTarget = TESTISOLATION_NUMTHREADS;
        HTHREAD th[TESTISOLATION_NUMTHREADS];
        testIsolation_s * ti[TESTISOLATION_NUMTHREADS];
        for (i = 0; i < TESTISOLATION_NUMTHREADS; i++)
        {
            ti[i] = new testIsolation_s(mLogger, &lTI, 1 + i, mRandomSeed);
            createThread(&threadTestIsolation, ti[i], th[i]);
        }
        MVTestsPortability::threadsWaitFor(sizeof(th)/sizeof(th[0]), th);
        if (lTI.mFailure)
            lSuccess = false;

        // Cleanup.
        for (i = 0; i < TESTISOLATION_NUMTHREADS; i++)
            delete ti[i];
        lSession->terminate();
        MVTApp::stopStore();
    }

    return lSuccess ? 0 : 1;
}
