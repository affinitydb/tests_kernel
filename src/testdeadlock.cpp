/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

//
// This test covers "deadlock detection"
// It creates threads that intentionally grab at shared resources.
// When they do this in the context of transactions a deadlock can appear
// which the kernel will resolve by introducing a failing to one of the
// threads.
//
// This test also attempts to demonstrate best practices for handling 
// deadlock errors when they occur.  But just to be clear, the best technique
// is to AVOID deadlocks in the first place, so the way that the threads grab 
// resources and get into trouble is also a warning.
//
// REVIEW: Should we also cover pins over multiple pages? or pin data
// on separate pages?

#include "app.h"
#include "mvauto.h" // MV auto pointers
#include "classhelper.h" // MV auto pointers
using namespace std;

#define CNT_THREAD_PASS1 2  // First pass with small number for sanity check
#define CNT_THREAD_PASS2 10  // Must be <64
#define CNT_PINS 10 // the smaller the number, the more contention
#define CNT_ITER 10 // Iterations per thread

class TestDeadLock : public ITest
{
	public:
		TEST_DECLARE(TestDeadLock);

		// By convention all test names start with test....
		virtual char const * getName() const { return "testdeadlock"; }
		virtual char const * getDescription() const { return "Deadlock detection scenario"; }
		virtual char const * getHelp() const { return ""; } // Optional
		
		virtual int execute();
		virtual void destroy() { delete this; }
		
		//Example suite membership methods
		virtual bool isLongRunningTest() const { return true; }
		virtual bool includeInMultiStoreTests() const { return false; }
		//virtual bool includeInBashTest(char const *& pReason) const {pReason = ""; return true;}
	protected:
		enum ThreadAction {
			actionFirst=0,

			actionReadAndModify=0,
			actionReadQuery=1,
			actionModifyAllQuery,
			actionReadAndModifyQuery,
			actionModifyInOrder,
			actionReadAndModifyInOrder,

			actionAll
		} ;

		// Keep these in sync 
		static const char * sTestDesc[actionAll];
		static const bool sDeadlockExpectedWithTransaction[actionAll];

		struct DeadlockThreadInfo
		{
			TestDeadLock *mTest;
			ThreadAction mAction;
			int mSeed;
			int mUseTransaction;
			int success ;
			int fail ;

			void reset() {success=fail=0;}
		} ;

		static THREAD_SIGNATURE deadlockThread(void * pDeadlockThreadInfo);

		void readQuery(DeadlockThreadInfo *info);
		void readAndModifyQuery(DeadlockThreadInfo *info);
		void readAndModify(DeadlockThreadInfo *info);
		void modifyInOrder(DeadlockThreadInfo *info,bool);
		void modifyAllQuery(DeadlockThreadInfo *info);

		void doTests(bool bUseTransaction, int cntThreads);

		bool finishAndReport(DeadlockThreadInfo *threadCtxt,HTHREAD *threads,int cntThreads);
		void flushAllTestPins( ISession *pSession );
		bool printReport(DeadlockThreadInfo * aThreads, int cnt);
		void logError( RC err );
	private:	
		PropertyID mProp;
		PID mPids[CNT_PINS]; // Pins created by test (no pins added/deleted by threads)
		ClassID mClass; // Tracks same pins as in mPids
};
TEST_IMPLEMENT(TestDeadLock, TestLogger::kDStdOut);

//both lists must be kept in sync with enum
const char * TestDeadLock::sTestDesc[] = { 
"actionReadAndModify",
"actionReadQuery",
"actionModifyAllQuery",
"actionReadAndModifyQuery",
"actionModifyInOrder",
"actionReadAndModifyInOrder"
} ;

// Some examples are intended to deadlock,
// others should be safe
// (This only applies if we run ONLY that single thread together)
const bool TestDeadLock::sDeadlockExpectedWithTransaction[] = {
true, //actionReadAndModify
false, //actionReadQuery
true, //actionModifyAllQuery
true, // actionReadAndModifyQuery
false, // actionModifyInOrder
true // actionReadAndModifyInOrder
} ;

int TestDeadLock::execute()
{
	mClass = STORE_INVALID_CLASSID;
	mProp = STORE_INVALID_URIID;

	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }

	// 
	// Session to create initial state in the store
	//
	ISession* pSession = MVTApp::startSession();
	TVERIFY( pSession != NULL ) ;

	mProp=MVTUtil::getProp(pSession,"TestDeadLock");

	// Create a class with all pins of that property

	IStmt *q=pSession->createStmt();
	q->setPropCondition(q->addVariable(),&mProp,1);
	mClass = MVTUtil::createUniqueClass(pSession, "TestDeadlock", q);
	q->destroy();

	// Erase any existing pins with that property (so that
	// we can make assumptions about CNT_PINS)
	flushAllTestPins( pSession );			

	int i;
	for (i=0;i<CNT_PINS;i++)
	{
		Value v[2]; IPIN *pin;
		v[0].set(0); v[0].property=PROP_SPEC_UPDATED;
		v[1].set(i); v[1].property=mProp;
		TVERIFYRC(pSession->createPIN(v,2,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
		mPids[i] = pin->getPID();
		if(pin!=NULL) pin->destroy();
	}

	pSession->terminate() ; pSession=NULL;

	// Start with no transactions, that shouldn't be a big challenge
	doTests(false,CNT_THREAD_PASS1 );

	// No real deadlock
	doTests(true /*with transactions*/,CNT_THREAD_PASS1);

	//More threads, more fun
	doTests(false,CNT_THREAD_PASS2 );
	doTests(true,CNT_THREAD_PASS2 );

	MVTApp::stopStore() ;
	return RC_OK ;
}

void TestDeadLock::doTests(bool bUseTransaction, int cntThreads)
{
	mLogger.out() <<endl<<"DOING TESTS " 
		<< cntThreads << " threads "
		<< (bUseTransaction?" WITH TRANSACTIONS":" WITH NO TRANSACTIONS")<<endl;
	
	HTHREAD *threads=(HTHREAD*)alloca(cntThreads*sizeof(HTHREAD));
	DeadlockThreadInfo *threadCtxt=(DeadlockThreadInfo *)malloc(sizeof(DeadlockThreadInfo)*cntThreads);

	int i;
	for (i=0;i<cntThreads;i++)
	{
		threadCtxt[i].mTest = this;
		threadCtxt[i].mSeed = mRandomSeed +i;
		threadCtxt[i].mUseTransaction=bUseTransaction;
	}

	for ( int testScenario=actionFirst ; testScenario<(int)actionAll ; testScenario++ )
	{
		mLogger.out() << sTestDesc[testScenario] << endl;

		for (i=0;i<cntThreads;i++)
		{
			threadCtxt[i].reset();
			threadCtxt[i].mAction = (ThreadAction)testScenario;
			createThread(&deadlockThread, &threadCtxt[i], threads[i]);
		}

		bool bNoDeadlockFound = finishAndReport(threadCtxt,threads,cntThreads);

		// Without transactions we don't expect any of our examples to deadlock
		if (!bUseTransaction || !sDeadlockExpectedWithTransaction[testScenario])
			TVERIFY(bNoDeadlockFound );	 // Check return value if we expected NO deadlocks
	}

	mLogger.out() << "random mixes of actions" << endl;
	for (i=0;i<cntThreads;i++)
	{
		threadCtxt[i].reset();
		threadCtxt[i].mAction = (ThreadAction)MVTRand::getRange(0,actionAll-1);
		createThread(&deadlockThread, &threadCtxt[i], threads[i]);
	}
	finishAndReport(threadCtxt,threads,cntThreads);

	free(threadCtxt);
}

bool TestDeadLock::finishAndReport(DeadlockThreadInfo *threadCtxt,HTHREAD *threads,int cntThreads)
{
	MVTestsPortability::threadsWaitFor(cntThreads, threads);
	#ifdef WIN32
	for ( int k = 0 ; k < cntThreads ; k++ ) ::CloseHandle( threads[k] ) ;
	#endif
	return printReport(threadCtxt, cntThreads);
}

void TestDeadLock::flushAllTestPins( ISession *pSession )
{
	CmvautoPtr<IStmt> classQ(ClassHelper::getQueryUsingClass(pSession,mClass,STMT_DELETE));
	TVERIFYRC(classQ->execute(NULL,NULL,0,~0,0,MODE_PURGE));
}

THREAD_SIGNATURE TestDeadLock::deadlockThread(void * pDeadlockThreadInfo)
{
	DeadlockThreadInfo *info=(DeadlockThreadInfo*)pDeadlockThreadInfo;
	srand(info->mSeed) ;	
	rand();

	switch( info->mAction )
	{
		case(actionReadAndModify): info->mTest->readAndModify(info); break;
		case(actionReadQuery): info->mTest->readQuery(info); break;
		case(actionReadAndModifyQuery): info->mTest->readAndModifyQuery(info); break;
		case(actionModifyInOrder): info->mTest->modifyInOrder(info,false); break;
		case(actionReadAndModifyInOrder): info->mTest->modifyInOrder(info,true); break;
		case(actionModifyAllQuery): info->mTest->modifyAllQuery(info); break;

		default: assert(false);
	}
	return 0;
}

void TestDeadLock::readQuery(DeadlockThreadInfo *info)
{
	// This thread does a "read-only" pass through the pins
	// It will hold read locks on the pins that it creates
	// Many of these threads can coexist happily, but when a transaction is present
	// the read pin locks are held and they can block other threads
	// trying to write the same pins.  Therefore it is necessary to think
	// carefully about whether a transaction is REALLY needed for a thread like this

	ISession* pSession = MVTApp::startSession();
	TVERIFY(pSession!=NULL);

	for (int i=0;i<CNT_ITER;i++)
	{
		if (info->mUseTransaction) TVERIFYRC(pSession->startTransaction(TXT_READONLY)); 

		uint64_t cntPins = 0;
		uint64_t cntPinsFound=0;

		RC rc;

		// Exception handling is only one of many possible ways to handle RC failures.
		// It can be convenient if the transaction spans calls into nested functions.
		// Other possibilities include transaction helper classes,
		// careful use of 'goto' statements, or plain-old error checking and if/else
		try
		{
			CmvautoPtr<IStmt> classQ(ClassHelper::getQueryUsingClass(pSession,mClass));
	
			rc=classQ->count(cntPins);
			if (rc!=RC_OK) throw RC_DEADLOCK ;
			TVERIFY(cntPins==CNT_PINS);

			ICursor* lC = NULL;

			TVERIFYRC(classQ->execute(&lC));

			CmvautoPtr<ICursor> r(lC);
	
			//
			// TDB: If a deadlock occurs we would get a premature NULL
			//
			IPIN * next;
			while(NULL!=(next=r->next()))
			{
				next->getValue(mProp);
				cntPinsFound++;
				next->destroy();
			}

			if ( cntPinsFound < cntPins )
			{
				if (isVerbose()) mLogger.out() << "Misses some pins - deadlock might have occurred" << endl;
			}

			if (info->mUseTransaction) TVERIFYRC(pSession->commit());

			info->success++;
		}
		catch(RC err)
		{
			info->fail++;
			logError(err);

			// There are no changes, so rollback should do nothing
			// but good to formally conclude the incomplete read

			//( If there is an RC_DEADLOCK it is an automatic rollback)
			if (info->mUseTransaction && err!=RC_DEADLOCK) TVERIFYRC(pSession->rollback());
		}
	}

	#ifdef WIN32
		SwitchToThread();
	#else
	#ifdef Darwin
			::sched_yield();
    #else
			pthread_yield();
    #endif

	#endif

	pSession->terminate() ;
}

void TestDeadLock::readAndModifyQuery(DeadlockThreadInfo *info)
{
	// Like the previous thread, but in a read/write transaction that
	// modifies some of the pins.  When there is a transaction all the 
	// pins in the class have locks held so the risk of deadlock is VERY HIGH.

	ISession* pSession = MVTApp::startSession();
	TVERIFY(pSession!=NULL);

	for (int i=0;i<CNT_ITER;i++)
	{
		if (info->mUseTransaction) TVERIFYRC(pSession->startTransaction(TXT_READWRITE)); 
		uint64_t cntPins = 0;
		uint64_t cntPinsFound=0;

		RC rc;

		try
		{
			CmvautoPtr<IStmt> classQ(ClassHelper::getQueryUsingClass(pSession,mClass));
	
			rc=classQ->count(cntPins);
			if (rc==RC_DEADLOCK) throw RC_DEADLOCK ;
			TVERIFY(cntPins==CNT_PINS);

			ICursor* lC = NULL;

			TVERIFYRC(classQ->execute(&lC));

			CmvautoPtr<ICursor> r(lC);
	
			IPIN * next;
			while(NULL!=(next=r->next()))
			{
				int i = next->getValue(mProp)->i;
				cntPinsFound++;

				if ( MVTRand::getRange(1,100) < 25 )
				{
					// Attemp to modify 25% of the pins visited

					Value newval;
					newval.set(i+1); newval.property=mProp;
					rc=next->modify(&newval,1);
					if (rc==RC_DEADLOCK) { 
						next->destroy();
						throw RC_DEADLOCK;
					}
				}

				next->destroy();
			}

			if ( cntPinsFound < cntPins )
			{
				// Enumeration can stop on deadlock
				if (isVerbose()) { mLogger.out() << "Pins missed-deadlock might have occurred" << endl; }

				// We only finished part of our work, we shouldn't commit the transaction
				throw RC_DEADLOCK;
			}

			if (info->mUseTransaction) TVERIFYRC(pSession->commit());
			info->success++;
		}
		catch(RC err)
		{
			info->fail++;
			logError(err);
			//( If there is an RC_DEADLOCK it is an automatic rollback)
			if (info->mUseTransaction && err!=RC_DEADLOCK) TVERIFYRC(pSession->rollback());
		}
	}

	#ifdef WIN32
		SwitchToThread();
	#else
	#ifdef Darwin
			::sched_yield();
    #else
			pthread_yield();
    #endif

	#endif

	pSession->terminate() ;
}

void TestDeadLock::modifyAllQuery(DeadlockThreadInfo *info)
{
	// IStmt::modifyPIN provides a way to modify all pins in a single call 

	ISession* pSession = MVTApp::startSession();
	TVERIFY(pSession!=NULL);

	for (int i=0;i<CNT_ITER;i++)
	{
		if (info->mUseTransaction) TVERIFYRC(pSession->startTransaction(TXT_READWRITE)); 

		RC rc;

		try
		{
			CmvautoPtr<IStmt> classQ(ClassHelper::getQueryUsingClass(pSession,mClass,STMT_UPDATE));

			// A filter operation so that PINs are loaded
			Value lV[2];
			lV[0].setVarRef(0 /*assumption about variable*/,mProp);
			lV[1].set(100000); //assumed number > CNT_PINS, e.g. that all pins fit this filter cond
			CmvautoPtr<IExprNode> e(pSession->expr(OP_LT,2,lV));

			TVERIFYRC(classQ->addCondition(0,e));

			// REVIEW: How much locking implications are there to calling count?
			uint64_t cntPins = 0;
			rc=classQ->count(cntPins);
			if (rc==RC_DEADLOCK) throw RC_DEADLOCK ;
			TVERIFY(cntPins==CNT_PINS);

			Value newval; // 99 is < 1000
			newval.set(99); newval.property=mProp;
			classQ->setValues(&newval,1);

			rc = classQ->execute();
			if (rc==RC_DEADLOCK) throw RC_DEADLOCK ;

			if (info->mUseTransaction) TVERIFYRC(pSession->commit());
			info->success++;
		}
		catch(RC err)
		{
			info->fail++;
			logError(err);
			//( If there is an RC_DEADLOCK it is an automatic rollback)
			if (info->mUseTransaction && err!=RC_DEADLOCK) TVERIFYRC(pSession->rollback());
		}
	}

	#ifdef WIN32
		SwitchToThread();
	#else
	#ifdef Darwin
			::sched_yield();
    #else
			pthread_yield();
    #endif

	#endif

	pSession->terminate() ;
}

void TestDeadLock::readAndModify(DeadlockThreadInfo *info)
{
	// This thread does not use the class query, but relies on knowledge of the PIDs
	// that were created by the test.  It does random access and grabs some portion of
	// all the pins, then modifies some of them.  When there is a transaction, because 
	// it holds the pins and grabs resources in random order, there is a HIGH risk of deadlock.

	ISession* pSession = MVTApp::startSession();
	TVERIFY(pSession!=NULL);

	static const int cntPinsToGrab = CNT_PINS / 2 ;

	for (int iter=0;iter<CNT_ITER;iter++)
	{
		if (info->mUseTransaction) TVERIFYRC(pSession->startTransaction(TXT_READWRITE)); 
		
		IPIN * grabAndHoldBag[cntPinsToGrab] ; 

		RC rc;
		int k;
		for (k=0;k<cntPinsToGrab;k++) grabAndHoldBag[k] = NULL;

		try
		{
			for (k=0;k<cntPinsToGrab;k++) 
			{				
				// Note we may grab same pin more than once
				grabAndHoldBag[k] = pSession->getPIN( mPids[MVTRand::getRange(0,CNT_PINS-1)] );
				if ( grabAndHoldBag[k] == NULL )
				{
					// Likely cause is deadlock detection because we know that
					// pins are not deleted during thread execution
					throw RC_DEADLOCK;
				}
			}
			
			for (k=0;k<cntPinsToGrab;k++) 
			{
				if ( MVTRand::getRange(1,100) < 25 )
				{
					// Attemp to modify 25% of the pins gabbed
					Value newval;
					newval.set(k); newval.property=mProp;
					rc=grabAndHoldBag[k]->modify(&newval,1);
					if (rc==RC_DEADLOCK) { 
						throw RC_DEADLOCK;
					}
				}				
			}

			if (info->mUseTransaction) TVERIFYRC(pSession->commit());
			info->success++;
		}
		catch(RC err)
		{
			info->fail++;
			logError(err);
			//( If there is an RC_DEADLOCK it is an automatic rollback)
			if (info->mUseTransaction && err!=RC_DEADLOCK) TVERIFYRC(pSession->rollback());
		}

		// Careful cleanup
		for (k=0;k<cntPinsToGrab;k++) if ( grabAndHoldBag[k] != NULL ) grabAndHoldBag[k]->destroy();
	}

	#ifdef WIN32
		SwitchToThread();
	#else
	#ifdef Darwin
			::sched_yield();
    #else
			pthread_yield();
    #endif

	#endif

	pSession->terminate() ;
}

void TestDeadLock::modifyInOrder(DeadlockThreadInfo *info, bool bReadAndWrite)
{
	// This thread always modifies pins in the same order
	// as other threads of the same type.
	// Therefore they should not hit a deadlock.
	// However the bReadAndWrite flag demonstrates how carefully the
	// API must be used, any extra PIN reads can cause deadlock.

	// Even when avoiding the deadlock something the example is still
	// "contrived", because having multiple threads doing this task means 
	// that all the threads will spend much too much time locked
	// up - there is no real parallelism.  It only makes sense in real code
	// if each thread does a lot of work and only occaisionally modifies these pins,
	// so that waiting (rather than deadlock) is a worst case scenario.

	ISession* pSession = MVTApp::startSession();
	TVERIFY(pSession!=NULL);

	for (int iter=0;iter<CNT_ITER;iter++)
	{
		if (info->mUseTransaction) TVERIFYRC(pSession->startTransaction(TXT_READWRITE)); 

		try
		{
			for ( int k = 0 ; k < CNT_PINS ; k++ )
			{
				if ( bReadAndWrite )
				{
					// This code demonstrates the deadlock dangers of getPIN
					// Although a valid way to modify a PIN is IPIN::modify,
					// it implies a lock escalation from READ lock to WRITE lock

					// Autopointer is convenient to avoid leaks even if exception thrown
					CmvautoPtr<IPIN> p(pSession->getPIN(mPids[k]));
					if (!p.IsValid())
					{
						// Likely cause is deadlock detection because we know that
						// pins are not deleted during thread execution
						throw RC_DEADLOCK;
					}
					Value newval;
					newval.set(k); newval.property=mProp;
					RC rc=p->modify(&newval,1);
					if (rc!=RC_OK) throw rc;
				}
				else
				{
					// ISession::modifyPIN is the deadlock safe way
					// because pin lock is exclusive immediately
					Value newval;
					newval.set(k); newval.property=mProp;
					RC rc=pSession->modifyPIN(mPids[k],&newval,1);
					if (rc!=RC_OK) throw rc;
				}

				if (info->mUseTransaction) TVERIFYRC(pSession->commit());
			}
			info->success++;
		}
		catch(RC err)
		{
			info->fail++;
			logError(err);
			//( If there is an RC_DEADLOCK it is an automatic rollback)
			if (info->mUseTransaction && err!=RC_DEADLOCK) TVERIFYRC(pSession->rollback());
		}

		// Sleep not necessary but can jumble the ordering
		if ( MVTRand::getRange(1,10) < 4 ) MVTestsPortability::threadSleep(MVTApp::randInRange(25,200));
	}

	#ifdef WIN32
		SwitchToThread();
	#else
	#ifdef Darwin
			::sched_yield();
    #else
			pthread_yield();
    #endif
	#endif

	pSession->terminate() ;
}

void TestDeadLock::logError( RC err )
{
	if ( isVerbose() )
	{
		if ( err == RC_DEADLOCK ) mLogger.out() << "Deadlock detected, rollback" << endl;
		else mLogger.out() << "Exception, error " << err << endl;
	}
}

bool TestDeadLock::printReport(DeadlockThreadInfo * aThreads, int cnt)
{
	int ttlfail=0,ttlsuccess=0;

	for (int i=0;i<cnt;i++)
	{
		ttlsuccess+=aThreads[i].success;
		ttlfail+=aThreads[i].fail;
	}

	if ( ttlfail == 0 )
		mLogger.out() << "No deadlock detected (" << ttlsuccess << " successful iterations)" <<endl;
	else
		mLogger.out() << ttlfail << " deadlocks (" << ttlsuccess << " successful iterations)" <<endl;

	return ttlfail==0 ;
}
