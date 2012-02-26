/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

#define sNumStores 40
#define sNumPINs 1000
#define sNumProps 10
#define NUMTHREADSPERSTORE 1
#define THREADSTACKSIZE 524288
#define NUMDUMMYTHREADS 100

#ifdef WIN32
	#define SLASH  "\\"
#else
	#define SLASH  "/"
#endif

class TestStoreInfo 
{
	enum eStoreStates { kSNone = 0, kSCreated, kSOpened, kSClosing, kSClosed, kSError };
	MVTestsPortability::Mutex mStoreLock;
	long volatile mSessionCount;
	long volatile mThreadCount;
public:
	ITest *mTest;
	MVTestsPortability::Mutex *mLock;
	MVTestsPortability::Event *mStart; 
	bool &fStarted;
	long &mCounter;
	MVTestsPortability::Event *mFinish;
	AfyKernel::StoreCtx *mStoreCtx;
	Tstring mIdentity;
	Tstring mPassword;
	Tstring mDirectory;	
	unsigned short mStoreID;	
	PropertyID mPropIDs[sNumProps];		
	long mState;
	HTHREAD mTHREAD[NUMTHREADSPERSTORE];

public:
	TestStoreInfo(ITest *pTest, MVTestsPortability::Mutex *pLock, MVTestsPortability::Event *pStart, MVTestsPortability::Event *pFinished, bool &pStarted, long &pCounter,/* MVTestsPortability::Event &pFinish,*/ const char *pIdentity, 
		const char *pPwd, const char *pDir, unsigned short pStoreID) 
		: mTest(pTest), mLock(pLock), mStart(pStart), fStarted(pStarted), mCounter(pCounter), mFinish(pFinished), mIdentity(pIdentity)
		, mPassword(pPwd), mDirectory(pDir), mStoreID(pStoreID)		
	{
		mState = kSNone;
		mSessionCount = mThreadCount = 0;
		MVTUtil::ensureDir(mDirectory.c_str());
		mLock->lock();
		MVTUtil::deleteStoreFiles(mDirectory.c_str());
		mLock->unlock();
		if(startStore(true))
		{
			ISession *lSession = startSession();
			MVTUtil::mapURIs(lSession, "TestDynamicMount.prop.", sNumProps, mPropIDs);
			terminateSession(lSession);
			stopStore();
		}
		for(int i = 0; i < NUMTHREADSPERSTORE; i++)
		{
			if (createThread(storeThread, this, mTHREAD[i])!=RC_OK) 
				assert(false && "failed to create thread");
		}
	}
	static THREAD_SIGNATURE storeThread(void * pInfo) 
	{ 
		#ifndef WIN32 
			pthread_detach(pthread_self());
		#endif
		
		((TestStoreInfo *)pInfo)->start(); 
		
		#ifdef WIN32
			SwitchToThread();
		#else
	    #ifdef Darwin
			::sched_yield();
        #else
			pthread_yield();
        #endif

		#endif
		return 0; 
	}
	void toString()
	{
		mLock->lock();
		mTest->getLogger().out() << std::endl 
								<< " Identity : " << mIdentity.c_str() 
								<< " Password : " << mPassword.c_str() 
								<< " Directory : " << mDirectory.c_str() 
								<< std::endl;
		mLock->unlock();
	}
	bool canShutdown() { return mState == kSOpened && mSessionCount == 0; }	
	void dynamicShutdown()
	{
		bool lShutdown = false;
		{
			MVTestsPortability::MutexP lLock(&mStoreLock);
			if(canShutdown())
			{
				mState = kSClosing;
				lShutdown = true;
			}
		}
		if(lShutdown)
			stopStore();		
	}
protected:
	void start(int pIters = 10)
	{
		INTERLOCKEDI(&mThreadCount);
		mLock->lock();
		while (!fStarted) mStart->wait(*mLock,0);
		mLock->unlock();

		int i = 0;
		for(i = 0; i < pIters; i++)
		{
			mLock->lock();
			mTest->getLogger().out() << std::endl << "iteration #" << i << " : " << mIdentity.c_str();
			mLock->unlock();
			if(startStore())
			{
				ISession *lSession = startSession();
				if(lSession)
				{
					createPINs(lSession);
					terminateSession(lSession);
				}else pIters++;
			}
			else TV_R(false && "Failed to start store", mTest);			
			MVTestsPortability::threadSleep((MVTRand::getRange(3, 10) * 1000));
		}		
		mLock->lock();
		INTERLOCKEDD(&mCounter);
		mTest->getLogger().out() << std::endl << "Existing thread" << std::endl;
		if (mCounter == 0) mFinish->signalAll();
		mLock->unlock();
		INTERLOCKEDD(&mThreadCount);
	}
	bool startStore(bool pCreate = false)
	{		
		while(mState == kSClosing) MVTestsPortability::threadSleep(1000);
		MVTestsPortability::MutexP lLock(&mStoreLock); 
		if(mState == kSOpened) return true;

		StartupParameters const lSP(0, mDirectory.c_str(), DEFAULT_MAX_FILES, MVTApp::Suite().mNBuffer,  
		DEFAULT_ASYNC_TIMEOUT, NULL, NULL, mPassword.c_str(), NULL, NULL);
		StoreCreationParameters lSCP(0, MVTApp::Suite().mPageSize, 0x200, mIdentity.c_str(), mStoreID, mPassword.c_str(), false); 
		lSCP.fEncrypted = !mPassword.empty();

		AfyKernel::StoreCtx *& lStoreCtx = mStoreCtx;

		RC lRC = RC_OK;		
		{
			//if (RC_OK != ( lRC = (RC)MVTApp::sDynamicLinkMvstore->openStore(lSP, lStoreCtx, NULL, lSCP.storeId)))
			if (RC_OK !=openStore(lSP, lStoreCtx))
			{
				mStoreCtx = NULL ; 
				//if (pCreate && RC_OK != (lRC = (RC)MVTApp::sDynamicLinkMvstore->createStore(lSCP, lSP, lStoreCtx, 0, NULL)))
				if (pCreate && RC_OK != (lRC =createStore(lSCP, lSP, lStoreCtx, 0)))
				{
					mStoreCtx = NULL ; 
					mTest->getLogger().out() << "Could not create store for identity " << mIdentity.c_str() << "(RC = " << lRC << ")" << std::endl;
					mState = kSError;
					return false;
				}
				if(lRC == RC_OK)
				{
					mLock->lock();
					mTest->getLogger().out() << std::endl << "Created new store for identity: " << mIdentity.c_str();
					mLock->unlock();
				}
			}
			else
			{
				mLock->lock();
				mTest->getLogger().out() << std::endl << "Opened store for identity: " << mIdentity.c_str();
				mLock->unlock();
			}
		}
		mState = kSOpened;
		return true;
	}

	bool stopStore()
	{		
		MVTestsPortability::MutexP lLock(&mStoreLock);
		//if(RC_OK == MVTApp::sDynamicLinkMvstore->shutdown(mStoreCtx, true))
		if(RC_OK == shutdownStore(mStoreCtx))
		{
			mLock->lock();
			mTest->getLogger().out() << std::endl << "Store shutdown for identity: " << mIdentity.c_str();
			mLock->unlock();
			mState = kSClosed;
			mStoreCtx = NULL;
		}
		else
		{
			mTest->getLogger().out() << "Store shutdown failed for identity: " << mIdentity.c_str() <<  std::endl ;
			mState = kSError;
		}
		return mState == kSClosed;
	}
	ISession *startSession()
	{		
		MVTestsPortability::MutexP lLock(&mStoreLock);
		if(mState == kSClosing || mState == kSClosed) return NULL;
		assert(mState == kSOpened || mStoreCtx != NULL);
		//ISession *lSession = MVTApp::sDynamicLinkMvstore->startSession(mStoreCtx, mIdentity.c_str(), mPassword.c_str());
		ISession *lSession = AfyDB::ISession::startSession(mStoreCtx, mIdentity.c_str(), mPassword.c_str());
		TV_R(lSession != NULL, mTest);
		INTERLOCKEDI(&mSessionCount);
		return lSession;
	}
	void terminateSession(ISession *pSession)
	{
		assert(pSession != NULL);
		pSession->terminate();
		INTERLOCKEDD(&mSessionCount);
	}
	void createPINs(ISession *pSession, int pNumPINs = sNumPINs)
	{
		int i = 0;
		for(i = 0; i < pNumPINs; i++)
		{
			if(i%100==0) mTest->getLogger().out() << ".";
			Value lV[sNumProps];
			Tstring lStr; MVTRand::getString(lStr, 10, 20);
			SETVALUE(lV[0], mPropIDs[0], i, OP_SET);
			SETVALUE(lV[1], mPropIDs[1], lStr.c_str(), OP_SET);
			PID lPID; CREATEPIN(pSession, lPID, lV, 2);
		}		
	}
};
class TestDynamicMount : public ITest
{		
		int mNumStores;
		long mCounter;
		std::vector<TestStoreInfo*>	mStoreInfos;
		MVTestsPortability::Mutex mLock;
		MVTestsPortability::Event mStart; 
		MVTestsPortability::Event mFinish;
		MVTestsPortability::Event mComplete;
		bool fStarted;

		int mNumThreads;
		HTHREAD *mTHREADs;
		long volatile mDummyThreads;
		long mDummyAllocLimit;
		std::vector<void *> mDummyAllocs;
	public:
		TEST_DECLARE(TestDynamicMount);
		virtual char const * getName() const { return "testdynamicmount"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Simulates dynamic mounting: stores open/close in parallel"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "creates stores on its own..."; return false; }
		virtual int execute();
		virtual void destroy() { delete this; }	
	protected:
		void initArgs();		
		void fragmentMemory();
	protected:
		static THREAD_SIGNATURE threadFunction(void * pInfo) { ((TestDynamicMount*)pInfo)->threadFunctionImpl(); return 0; }
		void threadFunctionImpl();
};
TEST_IMPLEMENT(TestDynamicMount, TestLogger::kDStdOut);

void TestDynamicMount::initArgs() 
{	
	mCounter = 0;
	mDummyThreads = 0;
	fStarted = false;
	if(!mpArgs->get_param("numstores", mNumStores))
	{
		mNumStores = sNumStores;
		mLogger.out() << "No --numstores parameter, defaulting to " << mNumStores << endl;
	}
	if(!mpArgs->get_param("numdummythreads", mNumThreads))
	{
		mNumThreads = NUMDUMMYTHREADS;
		mLogger.out() << "No --numdummythreads parameter, defaulting to " << mNumThreads << endl;
	}
	if(!mpArgs->get_param("--dontfragment", mNumThreads))
	{
		mLogger.out() << "No --dontfragment parameter, fragmenting memory " << endl;
		mTHREADs = (HTHREAD*) malloc(sizeof(HTHREAD) * mNumThreads);
		fragmentMemory();
	}
	mDummyAllocLimit = 25 * 1024 * 1024;	
}
void TestDynamicMount::fragmentMemory()
{
	// allocate around ~25Mb of memory (for binaries in memory)
	long lTotalAlloc = 0;
	for(;;)
	{
		int lRand = MVTRand::getRange(10000, 20000);
		void *lAlloc = malloc(lRand);
		if(lAlloc)
		{
			mDummyAllocs.push_back(lAlloc);
			lTotalAlloc+=lRand;
		}
		if(lTotalAlloc >= mDummyAllocLimit)
			break;
	}
	// allocate 1000 * 512KB blocks of memory (to simulate the stack memory used)	
	int i = 0;
	for(i = 0; i < mNumThreads; i++)
	{
		#ifdef WIN32
			mTHREADs[i] = ::CreateThread(NULL, THREADSTACKSIZE, threadFunction, this, /*STACK_SIZE_PARAM_IS_A_RESERVATION*/0, NULL);
		#else			
			createThread(threadFunction, this, mTHREADs[i]);
		#endif
	}
	for(;;)
	{
		int lRand = MVTRand::getRange(10000, 20000);
		void *lAlloc = malloc(lRand);
		if(lAlloc)
		{
			mDummyAllocs.push_back(lAlloc);
			lTotalAlloc+=lRand;
		}
		if(lTotalAlloc >= (mDummyAllocLimit + (5*1024*1024)))
			break;
	}
}
void TestDynamicMount::threadFunctionImpl()
{
	INTERLOCKEDI(&mDummyThreads);
	long lTotalAlloc = 0;
	std::vector<void *> lAllocs;
	for(;;)
	{
		int lRand = MVTRand::getRange(10000, 20000);
		void *lAlloc = malloc(lRand);
		if(lAlloc)
		{
			lAllocs.push_back(lAlloc);
			lTotalAlloc+=lRand;
		}
		if(lTotalAlloc >= THREADSTACKSIZE)
			break;
	}
	for(size_t i = 0; i < lAllocs.size(); i++)
		free(lAllocs[i]);
	mLock.lock();
	while (!fStarted || mCounter!=0) mComplete.wait(mLock,0);
	mLock.unlock();
	INTERLOCKEDD(&mDummyThreads);
}
int TestDynamicMount::execute() 
{
	bool lSuccess = true;
	MVTApp::sReporter.enable(false);
	initArgs();
	
	for(mCounter = 0; mCounter < mNumStores; mCounter++)
	{		
		int lStoreID = MVTApp::Suite().mStoreID + mCounter;
		char lIdentity[32]; sprintf(lIdentity, "kernel%ld.mvstore.org", mCounter);
		Tstring lPwdStr; MVTRand::getString(lPwdStr, 30, 30, false, false);
		char lDir[512]; sprintf(lDir, "%s%s%s", MVTApp::Suite().mDir.c_str(), SLASH, lIdentity);		
		TestStoreInfo *lInfo = new TestStoreInfo(this, &mLock, &mStart, &mFinish, fStarted, mCounter, lIdentity, lPwdStr.c_str(), lDir, lStoreID);
		mStoreInfos.push_back(lInfo);
	}

	mLock.lock();
	fStarted = true;
	mStart.signalAll();
	mLock.unlock();
	size_t j = 0;

/*	
	for(j = 0; j < mStoreInfos.size(); j++)
	{
		TestStoreInfo *lInfo = mStoreInfos[j];
		lInfo->toString();		
	}
*/
	while(mCounter != 0) 
	{
		for(j = 0; j < mStoreInfos.size(); j++)
		{
			TestStoreInfo *lInfo = mStoreInfos[j];
			lInfo->dynamicShutdown();
		}
		MVTestsPortability::threadSleep(5000);
	}
	mLock.lock();
	mComplete.signalAll();
	mLock.unlock();
	
	while(mDummyThreads != 0) 
		MVTestsPortability::threadSleep(5000);

	mLogger.out() << std::endl << "Test completed." << std::endl;
	return lSuccess?0:1;
}
