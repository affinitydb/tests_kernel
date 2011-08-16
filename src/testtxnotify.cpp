/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

// Specific test for IStoreNotification::txNotify 
// (Original test coverage removed from TestTxNotify.cpp)

// TIP: use -v arg to show trace of notifications

#include "app.h"
#include "mvauto.h"
#include "teststream.h"
using namespace std;

class TxNotifyTester : public IStoreNotification
{
	// Implementation of the notification callback
	// for this test.  TestTxNotify will inform
	// it of expected notifications that should come in and
	// this class will confirm these with the actual notifications

public:
	TxNotifyTester() 
		: mTest(NULL)
	{
	}


	void setParent( ITest * inTest ) { mTest=inTest;}
	void setSession( ISession* inSession ) { mSession= inSession; }	

	//IStoreNotification Methods 

	void notify(NotificationEvent *events,unsigned nEvents,uint64_t txid)
	{
	}

	void replicationNotify(NotificationEvent *events,unsigned nEvents,uint64_t txid)
	{
	}

	void txNotify(TxEventType inType,uint64_t txid)
	{
		mLock.lock();

		ThreadTxCtxt & ctxt = getCtxt();
		
		TV_R( txid != 0, mTest ) ;

		if ( mTest->isVerbose() )
		{
			THREADID threadId = getThreadId(); 

			std::stringstream msg; // Temp string helps avoid jumbled output

			msg << "[" << threadId << "] txNotify " << txid  ;

			switch( inType )
			{
			case( NTX_START ) : msg << " Start" << endl ; break ;
			case( NTX_SAVEPOINT ) : msg << " SavePoint" << endl ; break ;
			case( NTX_COMMIT ) : msg << " Commit" << endl ; break ;
			case( NTX_ABORT ) : msg << " Abort" << endl ; break ;
			case( NTX_COMMIT_SP ) : msg << " CommitSavePoint" << endl ; break ;
			case( NTX_ABORT_SP ) : msg << " AbortSavePoint" << endl ; break ;
			default: assert(0) ; break ;
			}

			mTest->getLogger().out() << msg.str();
		}

		if ( ctxt.mCurrentTrans.empty() )
		{
			// Starting a new transaction
			TV_R( inType == NTX_START, mTest ) ;
			ctxt.mCurrentTxid = txid ;
		}
		else
		{
			// Continuation of a transaction (handles all events until end() called)
			TV_R( ctxt.mCurrentTxid == txid, mTest ) ; 

			TV_R( !ctxt.mAborted &&	!ctxt.mCommitted, mTest ) ; // Transaction already complete, only a single ABORT or COMMIT expected!

			if ( inType == NTX_ABORT )
			{
				ctxt.mAborted = true;
			}
			else if ( inType == NTX_COMMIT )
			{
				ctxt.mCommitted = true;
			}
		}

		ctxt.mCurrentTrans.push_back(inType) ;

		// Verify that we haven't recieved more aborts or commits than would match 
		// the start and savepoints
		TV_R(testStack(),mTest);

		mLock.unlock();
	}

public:
	void start( const char * inTestCaseName )
	{
		// (Not locking, assumed only a single thread controls scenarios)

		flush() ;  // End any previous test case

		// Prepare for some store API calls (e.g. an individual case inside the test).
		// After this call any expected callbacks should be
		// preped with methods like addExpectedNotify
		mTestName = inTestCaseName ;

		mTest->getLogger().out() << inTestCaseName << endl ;
	}

	void end(bool bExpectNoNotifs=false) 
	{
		// Test case is done.  This will validate that any expected
		// calls really were made.
		for( std::map<THREADID,ThreadTxCtxt>::iterator it = mTxs.begin();
			it != mTxs.end() ;
			it++ )
		{
			ThreadTxCtxt & ctxt = (*it).second;
			// Some tests expect no notifications at all
			if ( ctxt.mCurrentTrans.empty() ) TV_R( bExpectNoNotifs, mTest ) ;
			else
			{
				TV_R( !bExpectNoNotifs, mTest );
				TV_R( ctxt.mAborted | ctxt.mCommitted, mTest ) ; // expect it to be complete
			}
		}

		// Optional to call until very last test because start()ing next
		// scenario will do the same thing
		mTestName = NULL ;
		flush() ;
	}

	bool isComplete()
	{
		mLock.lock();
		ThreadTxCtxt & ctxt = getCtxt();
		bool bRet=  ctxt.mAborted | 
					ctxt.mCommitted;
		mLock.unlock();
		return bRet;
	}

	bool isAborted()
	{
		mLock.lock();
		ThreadTxCtxt & ctxt = getCtxt();
		bool bret = ctxt.mAborted ;
		mLock.unlock();
		return bret;
	}

	bool isCommitted()
	{
		mLock.lock();
		ThreadTxCtxt & ctxt = getCtxt();
		bool bret = ctxt.mCommitted ;
		mLock.unlock();
		return bret;
	}

private:
	//
	// Helper functions
	//
	void flush()
	{
		// Number of start+savepoints should match commit+abort
		mTxs.clear();		
	}

	bool testStack()
	{
		// Validate a sequence of transaction steps.
		// Returns true even if the transaction is incomplete, as long as things are 
		// progressing correctly
		ThreadTxCtxt & ctxt = getCtxt();

		if ( ctxt.mCurrentTrans.size() == 0 ) return true ; // hasn't started, nothing can be faulted
		if ( ctxt.mCurrentTrans[0] != NTX_START ) return false;

		int cntSP = 0 ;

		for ( size_t i = 0 ; i < ctxt.mCurrentTrans.size() ; i++ )
		{
			switch( ctxt.mCurrentTrans[i] )
			{
			case( NTX_START ) : 
					// Only expected as first item
					if ( i != 0 ) return false; 
					break ;
			case( NTX_SAVEPOINT ) : 
					// Nested transaction
					cntSP++ ; 
					break ;

			// Transaction automatically over, can match any number of start + savepoint, see 9174
			case( NTX_ABORT ) : 
			case( NTX_COMMIT ) : 
						//SavePoints don't need to be at 0 - e.g. you can commit/rollback ALL with single call
						cntSP=0 ; 
						// Should come at the very end
						if ( i != ctxt.mCurrentTrans.size() -1 ) return false;
						break ;

			case( NTX_COMMIT_SP ) : 
			case( NTX_ABORT_SP ) : 
					cntSP-- ; 

					// Must match some previous NTX_SAVEPOINT
					if ( cntSP < 0 ) return false;
					break ;
			default: assert(0) ; break ;
			}
		}
		return true ;
	}

	struct ThreadTxCtxt {

		ThreadTxCtxt() : mCurrentTxid((uint64_t)-1), mAborted(false), mCommitted(false) {}

		vector<TxEventType> mCurrentTrans ; // Track the callbacks
		uint64_t mCurrentTxid ;		
		bool mAborted ; // if transaction has completed with overall rollback
		bool mCommitted ; // if transaction has completed and was committed
	} ;

	std::map<THREADID,ThreadTxCtxt> mTxs;

	MVStoreKernel::Mutex mLock;

	ThreadTxCtxt & getCtxt()
	{
		THREADID threadId = getThreadId(); 
		if ( mTxs.find(threadId) == mTxs.end() )
		{
			ThreadTxCtxt info;
			info.mCurrentTxid=(uint64_t)-1;	
			mTxs[threadId] = info;
		}
		return mTxs[threadId];
	}

	ITest * mTest ; // Parent test
	ISession* mSession ; // Assuming single thread/single session test
	const char * mTestName ; // Current test case (for debugging)
} ;

// Publish this test.
class TestTxNotify : public ITest
{
	public:
		TEST_DECLARE(TestTxNotify);
		virtual char const * getName() const { return "testtxnotify"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "txNotify coverage"; }
		virtual bool isStandAloneTest() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void doTest() ;
		void testPinCreation();
		void testDeadlockNotifications();
	private:
		ISession * mSession ;
		PropertyID mProps[10] ;
	public:
		TxNotifyTester mNotifImpl ;
};
TEST_IMPLEMENT(TestTxNotify, TestLogger::kDStdOut);

int TestTxNotify::execute()
{
	doTest() ;
	return RC_OK  ;
}

void TestTxNotify::doTest()
{
	bool bStarted = MVTApp::startStore(NULL, &mNotifImpl ) ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	// Override default session flag of ITF_DEFAULT_REPLICATION now
	// set in MVTApp helper.
	mSession->setInterfaceMode( 0 ) ;

	mNotifImpl.setParent(this) ;
	mNotifImpl.setSession(mSession) ;
	
	MVTApp::mapURIs( mSession, "TestTxNotify", 10, mProps );

	testPinCreation() ;

	testDeadlockNotifications();

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
}

void TestTxNotify::testPinCreation()
{	
	PID pid ;

	mSession->setInterfaceMode( 0 ) ;

	// Initial properties
	Value vals[2] ; 
	vals[0].set( 50 ) ; vals[0].property=mProps[0] ;
	vals[1].set( "test" ) ; vals[1].property=mProps[1] ;

	// Case 0 Simple PIN creation 
	mNotifImpl.start( "pincreate:0" ) ;
	PID pid0 ;
	TVERIFYRC(mSession->createPIN( pid0, vals, 2 )) ;
	mNotifImpl.end() ;
/*
example traces:
txNotify 20496 Start
txNotify 20496 SavePoint
txNotify 0 Commit
txNotify 20496 Commit
*/

	//Case 1 Multiple pins
	mNotifImpl.start( "pincreate:1" ) ;
	static const int cntBatch = 3 ;
	IPIN *pins[cntBatch] ;
	int i ;
	for ( i = 0 ; i < cntBatch ; i++ )
	{
		pins[i] = mSession->createUncommittedPIN(NULL,0,0,0);
	}
	TVERIFYRC(mSession->commitPINs(pins,cntBatch)) ;
	for ( i = 0 ; i < cntBatch ; i++ )
	{
		pins[i]->destroy();
	}
/*
txNotify 20497 Start
txNotify 20497 Commit
*/

	//Explicit transactions
	mNotifImpl.start( "pincreate:2" ) ;
	TVERIFYRC(mSession->startTransaction());
	TVERIFYRC(mSession->createPIN( pid, NULL, 0 )) ;
	TVERIFYRC(mSession->commit());	
/*
txNotify 20498 Start
txNotify 20498 SavePoint
txNotify 0 Commit
txNotify 20498 Commit
*/

	mNotifImpl.end() ;
	mNotifImpl.start( "pincreate:3" ) ;
	TVERIFYRC(mSession->startTransaction());
	TVERIFYRC(mSession->startTransaction());
	TVERIFYRC(mSession->createPIN( pid, NULL, 0 )) ;
	TVERIFYRC(mSession->commit());	
	TVERIFYRC(mSession->commit());	
	mNotifImpl.end() ;
/*
txNotify 20499 Start
txNotify 20499 SavePoint
txNotify 20499 SavePoint
txNotify 0 Commit
txNotify 0 Commit
txNotify 20499 Commit
*/


	mNotifImpl.start( "pincreate:4" ) ;
	TVERIFYRC(mSession->startTransaction());
	TVERIFYRC(mSession->startTransaction());
	TVERIFYRC(mSession->createPIN( pid, NULL, 0 )) ;
	TVERIFYRC(mSession->commit(true /*commit both*/));	
	mNotifImpl.end() ;
/*
//REVIEW IMPORTANT: With Commit TRUE number of Start+SavePoints won't match number of commits
//By getting a Commit rather than a CommitSavePoint we are being signalled that 
//the entire transaction is committed, not just the first SavePoint
[7704] txNotify 916 Start
[7704] txNotify 916 SavePoint
[7704] txNotify 916 SavePoint
[7704] txNotify 916 CommitSavePoint
[7704] txNotify 916 Commit
*/

	mNotifImpl.start( "pincreate:5" ) ;
	TVERIFYRC(mSession->startTransaction());
	TVERIFYRC(mSession->createPIN( pid, NULL, 0 )) ;
	TVERIFYRC(mSession->rollback(false /*all*/));	
	mNotifImpl.end() ;
/*
txNotify 20501 Start
txNotify 20501 SavePoint
txNotify 0 Commit
txNotify 20501 Abort
*/

	mNotifImpl.start( "pincreate:6" ) ;
	TVERIFYRC(mSession->startTransaction());
	TVERIFYRC(mSession->createPIN( pid, NULL, 0 )) ;
	TVERIFYRC(mSession->startTransaction());
	TVERIFYRC(mSession->createPIN( pid, NULL, 0 )) ;
	TVERIFYRC(mSession->rollback(false /*all*/));	
	TVERIFYRC(mSession->commit(false));	
	mNotifImpl.end() ;
/*
txNotify 20502 Start
txNotify 20502 SavePoint
txNotify 0 Commit
txNotify 20502 SavePoint
txNotify 20502 SavePoint
txNotify 0 Commit
txNotify 0 Abort			// INTERESTING
txNotify 20502 Commit
*/

	mNotifImpl.start( "pincreate:7" ) ;
	TVERIFYRC(mSession->startTransaction());
	TVERIFYRC(mSession->createPIN( pid, NULL, 0 )) ;
	TVERIFYRC(mSession->startTransaction());
	TVERIFYRC(mSession->createPIN( pid, NULL, 0 )) ;
	TVERIFYRC(mSession->rollback(true /*all*/));	
	mNotifImpl.end() ;
/*
// Like Case 4: The rollback all triggers an Abort, which doesn't match the Number of active Starts + SavePoints.
// Abort always means the full transaction is complete
[2552] txNotify 947 Start
[2552] txNotify 947 SavePoint
[2552] txNotify 947 CommitSavePoint
[2552] txNotify 947 SavePoint
[2552] txNotify 947 SavePoint
[2552] txNotify 947 CommitSavePoint
[2552] txNotify 947 Abort
*/

// READ-ONLY Transactions send no notifications
	mNotifImpl.start( "pincreate:8" ) ;
	TVERIFYRC(mSession->startTransaction(TXT_READONLY));
	TVERIFYRC(mSession->commit(false /*all*/));	
	mNotifImpl.end(true) ;

	mNotifImpl.start( "pincreate:9" ) ;
	TVERIFYRC(mSession->startTransaction(TXT_READONLY));
	TVERIFYRC(mSession->rollback(false /*all*/));	
	mNotifImpl.end(true) ;

	mNotifImpl.start( "pincreate:10" ) ;
	TVERIFYRC(mSession->startTransaction(TXT_READONLY));
	TVERIFY(RC_OK!=mSession->createPIN( pid, NULL, 0 )) ;
	TVERIFYRC(mSession->commit(false /*all*/));	
	mNotifImpl.end(true) ;
}

// stolen and adapted from testtran.cpp
struct DeadlockThreadInfo
{
	DeadlockThreadInfo( TestTxNotify * inCtxt, volatile long * inSyncPoint , MVStoreKernel::StoreCtx *pStoreCtx, int idx)
	{
		ctxt = inCtxt ;
		syncPoint = inSyncPoint ;
		mStoreCtx = pStoreCtx;
		index=idx;
		bLoser=false;
		unrelatedPID.pid=STORE_INVALID_PID;
	}

	Value valpin0 ;  // Value to stick in the first pin
	Value valpin1 ; 
	PID pids[2] ;    // Two pids that will get the threads into deadlock trouble
	TestTxNotify * ctxt ;  // TestTransactions object
	volatile long * syncPoint ;
	MVStoreKernel::StoreCtx *mStoreCtx;
	int index ;  // Gives a readable name to the thread
	bool bLoser; // Whether this thread failed
	PID unrelatedPID; // An unrelated PIN that doesn't contribute to deadlock
} ;

static THREAD_SIGNATURE threadTestDeadlock(void * pDeadlockThreadInfo)
{
	DeadlockThreadInfo * pTI = (DeadlockThreadInfo*)pDeadlockThreadInfo ;
	ISession * session = MVTApp::startSession(pTI->mStoreCtx) ;

	//TIP: Thread can't use TVERIFY because not part of the test.  But TV variants redirected to the test

	TVRC_R(session->startTransaction(), pTI->ctxt) ; 

	//
	// Also create a dummy pin as part of this transaction
	// to see whether deadlock will roll back entire transaction
	// 
	TVRC_R(session->createPIN( pTI->unrelatedPID,NULL,0), pTI->ctxt);

	TVRC_R(session->modifyPIN( pTI->pids[0], &pTI->valpin0, 1 ), pTI->ctxt) ;

	INTERLOCKEDD(pTI->syncPoint); // first thread reduces from 2 to 1.  Second thread does 1 to 0
	while( *(pTI->syncPoint) > 0 )
	{
		MVStoreKernel::threadSleep(50); 
	}

	// At this point we know that the other thread has already modified
	// pids[1].  	
	// It may be blocked from finished its own transaction
	// until we finish this threads transaction and commit the change to pids[0]. 
	// But we potentially can't modify this PIN until it finishes its own transaction.
	// Result: deadlock
	// 

	RC rc=session->modifyPIN( pTI->pids[1], &pTI->valpin1, 1 );
	
	pTI->bLoser=(rc==RC_DEADLOCK);

	if ( pTI->ctxt->isVerbose() )
	{
		std::stringstream msg;
		if ( rc==RC_DEADLOCK )
		{
			msg<< "I lost the deadlock resolution (thread " << pTI->index << ")" << endl;
			pTI->ctxt->getLogger().out() << msg.str(); 
		}
		else if ( rc==RC_OK )
		{
			msg << "I won the deadlock resolution (thread " << pTI->index << ")" << endl;
			pTI->ctxt->getLogger().out() << msg.str(); 
		}
	}

	TV_R(rc==RC_OK || rc==RC_DEADLOCK, pTI->ctxt) ;

	// Rollback happens automatically
	if ( rc == RC_OK )
	{
		TVRC_R( session->commit(), pTI->ctxt ) ;

		// This confirms that notifications have occurred correctly
		TV_R( pTI->ctxt->mNotifImpl.isCommitted(),  pTI->ctxt ) ;
	}
	else
	{
		// This confirms that rollback notifications have been recieved
		TV_R( pTI->ctxt->mNotifImpl.isAborted(),  pTI->ctxt ) ;
	}

	session->terminate() ;

	return 0 ;
}

void TestTxNotify::testDeadlockNotifications()
{
	// basic deadlock

/*
// Typical output
deadlock:threads
[1264] txNotify 225 Start
[5352] txNotify 226 Start
[1264] txNotify 225 SavePoint
[5352] txNotify 226 SavePoint
[1264] txNotify 225 SavePoint
[5352] txNotify 226 SavePoint
[1264] txNotify 0 Commit
[5352] txNotify 0 Commit
[1264] txNotify 0 Commit
[5352] txNotify 0 Commit
[1264] txNotify 225 SavePoint
[5352] txNotify 226 SavePoint
[1264] txNotify 0 Commit
[5352] txNotify 0 Commit
[5352] txNotify 226 SavePoint
[1264] txNotify 225 SavePoint
[1264] txNotify 0 Abort
[1264] txNotify 225 Abort			<< This is important, the automatic rollback callback
									   sent even though there was no explicit rollback
[5352] txNotify 0 Commit
I lost the deadlock resolution (thread 0)
I won the deadlock resolution (thread 1)
[5352] txNotify 226 Commit
*/
	mNotifImpl.start( "deadlock:prep" ) ;

	PropertyID propids[1] ;
	MVTApp::mapURIs(mSession,"TestTxNotify::testDeadlockNotifications",1,propids);

	PropertyID strProp =propids[0] ;

	PID pids[2] ;
	mSession->startTransaction();
	TVERIFYRC(mSession->createPIN(pids[0],NULL,0));
	TVERIFYRC(mSession->createPIN(pids[1],NULL,0));
	mSession->commit();

	mNotifImpl.end() ;
	mNotifImpl.start( "deadlock:threads" ) ;

	PID pidX = pids[0] ; // For test readability
	PID pidY = pids[1] ;

	long volatile lSyncPoint = 2;

	DeadlockThreadInfo ti0(this,&lSyncPoint,MVTApp::Suite().mStoreCtx,0), 
		ti1(this, &lSyncPoint,MVTApp::Suite().mStoreCtx,1) ;

	ti0.pids[0] = pidX ; ti0.pids[1] = pidY ; 
	ti0.valpin0.set( "info about pinX from thread0" ) ; ti0.valpin0.property = strProp ;
	ti0.valpin1.set( "info about pinY from thread0" ) ; ti0.valpin1.property = strProp ;

	ti1.pids[0] = pidY ; ti1.pids[1] =pidX ; 
	ti1.valpin0.set( "info about pinY from thread1" ) ; ti1.valpin0.property = strProp ;
	ti1.valpin1.set( "info about pinX from thread1" ) ; ti1.valpin1.property = strProp ;
	
	const size_t sNumThreads = 2 ; 
	HTHREAD lThreads[sNumThreads];
	createThread(&threadTestDeadlock, &ti0, lThreads[0]);
	createThread(&threadTestDeadlock, &ti1, lThreads[1]);

	MVStoreKernel::threadsWaitFor(sNumThreads, lThreads);
   
	// (not doing validation of actual deadlock resolution correctness - that
	// is done in testtran.cpp)

	mNotifImpl.end() ;
}
