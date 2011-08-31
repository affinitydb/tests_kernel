/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h"
#include "mvauto.h"
using namespace std;
using namespace MVStoreKernel; // Interlock

#ifdef WIN32
#include <Psapi.h>
#pragma comment (lib, "Psapi.lib" )
#endif

//#define TEST_MULTI_STORE_SINGLE_ID // IPC expects each store to have different ID
//#define TEST_VARIABLE_PAGE_SIZE // Currently all stores opened in single process must have the same page size

static int NBUFFERS = 5000 ;  /*default, may be changed below*/

#ifdef TEST_VARIABLE_PAGE_SIZE
static int g_PageSizeOptions[5] = { 0x10000,0x8000,0x4000,0x2000,0x1000 } ;
#endif
static int g_MaxPageSize = 0x10000 ; /* Maximum used by this test, not necessarily max supported by store */

#define	MAXFILES 300
#define	NCTLFILES 0
#define	NLOGFILES 1
#define	PAGESPEREXTENT 0x80 // Reduced from normal (0x200)
							
#define NUM_PINS 20 /* Pins to create in each store */
#define NUM_COLLECTION_ITEMS_PER_PIN 3 // 300 to bash

// Publish this test.
class TestMultiStore : public ITest
{
	public:
		TEST_DECLARE(TestMultiStore);
		virtual char const * getName() const { return "testmultistore"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of creating/opening multiple stores in the same process\n\tArguments: --nstores=NUM [--nthreadsperstore=NUM] [--dumpload]"; }
		virtual bool includeInBashTest(char const *& pReason) const { pReason = "Creates stores..."; return false; }
		// virtual bool isLongRunningTest()const {return true;} // CAN'T anymore because it takes arguments
		// virtual bool includeInPerfTest() const { return true; } // CAN'T anymore because it takes arguments
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "can't since it takes arguments"; return false; }
  
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		struct ThreadInfo 
		{
			unsigned short mStoreID ;
			string mIdentity ;
			string mDir ;
			string mbIOStr ; // io init string
			TestMultiStore * mTest ;
			bool mbCreateStore ;
		} ;

		MVStoreKernel::StoreCtx * createStoreInDir(ThreadInfo * inCtx, const char * inDir, const char * inIdentity, unsigned short inStoreID) ;
		MVStoreKernel::StoreCtx * openStoreInDir(const char * inDir) ;
		void doStuffInStore( ISession* inS  ) ;
		void doGuestStuffInStore( ISession* inS  ) ;
		void simulateDumpload(ISession* session, unsigned short inStoreID) ;

		static THREAD_SIGNATURE testThread(void * inThreadInfo) ;
		void threadImpl( ThreadInfo * inInfo ) ;

		int mThreadsPerStore;
		int mNumStores;
		bool mUseDumpload ;
};
TEST_IMPLEMENT(TestMultiStore, TestLogger::kDStdOut);

static long gFinishedThreads = 0;

// Implement this test.
int TestMultiStore::execute()
{
    bool pparsing = true;
	mThreadsPerStore=1;
	
	if(!mpArgs->get_param("nstores",mNumStores))
	{
		mLogger.out() << "Problem with --nstores parameter initialization!" << endl;
		pparsing = false;
    }
	
	if(!mpArgs->get_param("nthreadsperstore",mThreadsPerStore))
	{
		mLogger.out() << "No --nthreadsperstore argument, defaulting to 1" << endl;
    }

	string dummy;
	mUseDumpload=mpArgs->get_param("--dumpload",dummy);

	if(!pparsing){
	   mLogger.out() << "Parameter initialization problems! " << endl; 
	   mLogger.out() << "Test name: testmultistore" << endl; 	
	   mLogger.out() << getDescription() << endl;	
	   mLogger.out() << "Example: ./tests testmultistore --nstores={...} --nthreadsperstore={...} --dumpload" << endl; 
			
	   return RC_INVPARAM;
    }	
	
	if ( isVerbose() )
		MVTApp::sReporter.setReportingLevel(MvStoreMessageReporter::LMSG_WARNING) ;
	else
		// Reduce extra mvstore messages 
		// Unfortunately there are still a lot of CRITICAL store missing errors
		// that don't signify a real problem
		MVTApp::sReporter.setReportingLevel(MvStoreMessageReporter::LMSG_ERROR) ;

#ifdef WIN32
	MEMORYSTATUSEX lMemStatus;
	lMemStatus.dwLength = sizeof( lMemStatus ) ;
	GlobalMemoryStatusEx( &lMemStatus ) ;
	DWORDLONG lTtlAvailable = lMemStatus.ullAvailPhys;
	DWORDLONG lMaxPages = lTtlAvailable / g_MaxPageSize ;
	
	// Try to use 50% of available space
	NBUFFERS = (int) lMaxPages / 2 ;

	mLogger.out() << "Max potential pages: " << lMaxPages << " Pages to use: " << NBUFFERS << "(" << ( NBUFFERS * g_MaxPageSize ) / (1024.*1024.) << " MB)" << endl ;

	if ( NBUFFERS > 10000 ) {
		NBUFFERS = 10000;
		mLogger.out() << "Reduced NBUFFERS to 10000" << endl;
	}
#else
	// TODO similar for linux
#endif

	
	if (mNumStores == 0)
	{
		TVERIFY( !"Invalid store arg" ) ;
		return RC_FALSE;
	}

    // NOTE: we allow 0 threads per store as a convenient way to flush the store files 
    //(as it takes so much disk space)
	int numThreads = mNumStores * mThreadsPerStore ;

	mUseDumpload = false ;

	// Multiple threads are used for two purposes:
	// 1) test concurrent opening/closing stores
	// 2) workaround fact that currently only a single session 
	//    can be open in each thread, even when sessions are from different stores

	HTHREAD *lThreads = (HTHREAD*)malloc( sizeof(HTHREAD)*numThreads ) ; 
	ThreadInfo *lThreadData = new ThreadInfo[numThreads] ;

	int i;
	for ( i = 0 ; i < mNumStores ; i++ )
	{
		// Build directory unique directory for each store
		// relative to active directory
		char dirbuf[32] ;
	#ifdef WIN32
		sprintf( dirbuf,".\\ms%d", i ) ;
	#else
		sprintf( dirbuf,"./ms%d", i ) ;
	#endif

		string ioinit = MVTUtil::completeMultiStoreIOInitString(MVTApp::Suite().mIOInit.c_str(), i);
		TVERIFY(MVTUtil::deleteStore(ioinit.c_str(),dirbuf)) ;

		char idnm[32] ;
		sprintf( idnm,"piuser%d", i ) ;

		for ( int k = 0 ; k < mThreadsPerStore ; k++ )
		{
			int threadpos = i * mThreadsPerStore + k ;

			lThreadData[threadpos].mTest = this ;

			lThreadData[threadpos].mDir = dirbuf ;

	#ifdef TEST_MULTI_STORE_SINGLE_ID
			lThreadData[threadpos].mIdentity = "piuser" ;
	#else
			lThreadData[threadpos].mIdentity = idnm ;
	#endif
			lThreadData[threadpos].mStoreID = i ;
			lThreadData[threadpos].mbCreateStore = ( k == 0 ) ; // First thread on the store is special

			lThreadData[threadpos].mbIOStr=ioinit;
		}
	}

	for ( i = 0 ; i < numThreads ; i++ )
	{
		//15271 - don't start threads until all deleteStore calls have completed
		createThread(&testThread, &(lThreadData[i]), lThreads[i]) ;
	}

	// MVTestsPortability::threadsWaitFor only supports a limited number of
	// threads (63 on windows) so use a counter instead
	while ( gFinishedThreads < numThreads )
	{
		MVTestsPortability::threadSleep(200); 
	}


#ifdef WIN32
	// REVIEW: This information is not valid if running in IPC mode.
	// We would need to look at the mvstore.exe memory usage rather than
	// test usage.
	PROCESS_MEMORY_COUNTERS meminfo ;
	meminfo.cb = sizeof( PROCESS_MEMORY_COUNTERS ) ;
	::GetProcessMemoryInfo(GetCurrentProcess(),&meminfo, sizeof( PROCESS_MEMORY_COUNTERS )) ;

	mLogger.out() << "Peak memory usage (MB):" << ( meminfo.WorkingSetSize / 1048576.0 ) << endl ; 
#endif

	// Cleanup stores - disable if you want to debug
	for ( i = 0 ; i < mNumStores ; i++ )
	{
		ThreadInfo & info = lThreadData[i*mThreadsPerStore];
		TVERIFY(MVTUtil::deleteStore(info.mbIOStr.c_str(),info.mDir.c_str())) ;
	}

	free( lThreads) ;
	delete [] lThreadData ;

	return RC_OK  ;
}

THREAD_SIGNATURE TestMultiStore::testThread(void * inThreadInfo)
{
	ThreadInfo * lInfo = (ThreadInfo*) inThreadInfo ;
	lInfo->mTest->threadImpl( lInfo ) ;
	InterlockedIncrement(&gFinishedThreads);
	return 0 ;
}

void TestMultiStore::threadImpl( ThreadInfo * inInfo )
{
	srand(inInfo->mStoreID + mRandomSeed ) ;

	MVStoreKernel::StoreCtx *storeCtx = NULL ;

	if ( inInfo->mbCreateStore )
	{
		//Stagger store creation a bit
		MVTestsPortability::threadSleep(MVTRand::getRange(0,250*mNumStores)); 
		storeCtx = createStoreInDir( inInfo, inInfo->mDir.c_str(), inInfo->mIdentity.c_str(), inInfo->mStoreID ) ;
		if ( isVerbose() )
            mLogger.out() <<"*****CREATOR OPEN "<< inInfo->mDir.c_str() <<endl;
	}
	else
	{
		MVTestsPortability::threadSleep(MVTRand::getRange(1000,2000)); // Give first thread a second headstart
		storeCtx = openStoreInDir( inInfo->mDir.c_str() ) ;
		if ( isVerbose() )
            mLogger.out() <<"*****GUEST OPEN " << inInfo->mDir.c_str() << " - " << getThreadId() <<endl;
	}

	// REVIEW: you must have different identities to open multiple stores in IPC

	if ( storeCtx == NULL ) { TVERIFY(!"Thread couldn't open store, bailing out") ; return ; }

	const char * identity = inInfo->mbCreateStore ? NULL : "Guest" ;
	ISession *s1 = NULL ;

	while ( s1 == NULL ) 
	{ 
		//s1 = MVTApp::sDynamicLinkMvstore->startSession(storeCtx, identity, NULL /*password*/);
		s1 = MVStore::ISession::startSession(storeCtx, identity, NULL /*password*/);
		if ( s1 != NULL ) break ;

		if (inInfo->mbCreateStore) 
			TVERIFY( !"Fail to create session" ) ; 
		else
		{
			#if 0 // remove IPC		
			if ( !MVTApp::sDynamicLinkMvstore->isInProc() )
			{
				MVTestsPortability::threadSleep(MVTRand::getRange(1000,2000)); // Give first thread a second headstart
				continue ;
			}
			else
			{
				TVERIFY( !"Fail to create guest session" ) ; 
			}
			#endif
		}
		return ; 
	}

	if ( inInfo->mbCreateStore )
	{
		doStuffInStore( s1 ) ;
	}
	else
	{
		doGuestStuffInStore( s1 ) ;
	}

	TVERIFY( s1->getLocalStoreID() == inInfo->mStoreID ) ;

	char identCheck[64] ;
	TVERIFY( s1->getIdentityName( STORE_OWNER, identCheck, 64 ) == inInfo->mIdentity.length() ) ;
	TVERIFY( strcmp( identCheck, inInfo->mIdentity.c_str() ) == 0 ) ;

	s1->terminate() ;

	//MVTApp::sDynamicLinkMvstore->shutdown(storeCtx, true /* immediate */);
	shutdownStore(storeCtx);
	if ( isVerbose() )
	{
		if ( inInfo->mbCreateStore )
		{
			mLogger.out() <<"*****CREATOR CLOSE " << inInfo->mDir.c_str() <<endl;
		}
		else
		{
			mLogger.out() <<"*****GUEST CLOSE " << inInfo->mDir.c_str() << " - " << getThreadId() <<endl;
		}
	}
}

MVStoreKernel::StoreCtx * TestMultiStore::createStoreInDir(ThreadInfo * inCtx,const char * inDir, const char * inIdentity, unsigned short inStoreID)
{
	// Note: we can't use MVTApp::startStore helper because it assumes only one store open at a time
	MVTUtil::ensureDir( inDir ) ;

	//Flush any existing store files
	MVStoreKernel::StoreCtx *lStoreCtx = NULL;

	// Using different page sizes for the stores
#ifdef TEST_VARIABLE_PAGE_SIZE
	int pageSize = g_PageSizeOptions[inStoreID%5] ;	
#else
	int pageSize = g_MaxPageSize ;
#endif

	mOutputLock.lock() ; 
	mLogger.out() << "Creating store "<<  inDir << " Page size: " << pageSize << " NBuffers: " << NBUFFERS << " Total: " << (( pageSize * NBUFFERS ) / ( 1024. * 1024.)) << " MB" << endl ;
	mOutputLock.unlock() ;

	//const char * lAdditionalParams=inCtx->mbIOStr.empty()?NULL:inCtx->mbIOStr.c_str();
	StartupParameters const lSP(0, inDir, MAXFILES, NBUFFERS, DEFAULT_ASYNC_TIMEOUT, NULL, NULL, NULL /*password*/);  
	StoreCreationParameters const lSCP(NCTLFILES, pageSize, PAGESPEREXTENT, inIdentity, inStoreID, NULL /*password*/, true);

	// Do a mix of dumpload and regular store creations
	ISession * lLoadStore = NULL;

	//if (RC_OK != MVTApp::sDynamicLinkMvstore->createStore(lSCP,lSP,lStoreCtx,
	//		mUseDumpload?&lLoadStore:NULL,lAdditionalParams))
	if (RC_OK != createStore(lSCP,lSP,lStoreCtx,mUseDumpload?&lLoadStore:NULL))
	{
		string msg("Could not create MVStore ") ;
		msg += inDir ;
		TVERIFY2(0,msg.c_str()) ;
		return NULL ;
	}   

	if ( mUseDumpload )
	{
		TVERIFY( lLoadStore != NULL );
		if ( lLoadStore ) 
		{	
			// Do dumpstore kinds of things
			simulateDumpload(lLoadStore,inStoreID) ;
			lLoadStore->terminate() ; lLoadStore = NULL ; 		
		}

		if ( mThreadsPerStore == 1 ) 
		{
			// Close and reopen the store
			// We only do this if there are no guest thread, otherwise there is a risk
			// another thread will grab access before we are complete

			// REVIEW: IPC
			//MVTApp::sDynamicLinkMvstore->shutdown(lStoreCtx, true /* immediate */);
			shutdownStore(lStoreCtx);
			lStoreCtx = NULL ;

			// Reopen to get regular session
			//if (RC_OK != MVTApp::sDynamicLinkMvstore->openStore(lSP,lStoreCtx,lAdditionalParams))
			if (RC_OK != openStore(lSP,lStoreCtx))
			{
				string msg("Could not re-open existing MVStore ") ;
				msg += inDir ;
				TVERIFY2(0,msg.c_str()) ; // Unexpected
				return NULL ;
			}
		}
	}

	return lStoreCtx ;
}

MVStoreKernel::StoreCtx * TestMultiStore::openStoreInDir(const char * inDir)
{
	// Open existing store

	MVStoreKernel::StoreCtx *lStoreCtx = NULL;
	StartupParameters const lSP(0, inDir, MAXFILES, NBUFFERS, DEFAULT_ASYNC_TIMEOUT, NULL, NULL, NULL /*password*/);  

	while(1)
	{
		//RC rcopen = (RC)MVTApp::sDynamicLinkMvstore->openStore(lSP,lStoreCtx) ;
		RC rcopen = openStore(lSP,lStoreCtx) ;
		if ( rcopen == RC_NOACCESS )
		{
			// try again in a moment
			MVTestsPortability::threadSleep(MVTRand::getRange(250,2000)); 
		}
		else if ( rcopen == RC_NOTFOUND )
		{
			// Creator thread hasn't even had a chance to get in
			MVTestsPortability::threadSleep(MVTRand::getRange(1000,3000)); 
		}
		else if ( rcopen == RC_CORRUPTED )
		{
			// REVIEW: RC_CORRUPTED may occur on linux due to log files
			// loop to see if we are really stuck in that state
			MVTestsPortability::threadSleep(MVTRand::getRange(250,2000)); 
		}
		else if ( rcopen == RC_OK )
		{
			return lStoreCtx ;
		}
		else
		{
			TVERIFYRC2(rcopen,"Open existing store unexpected failure") ;
			return NULL ;
		}
	}

	return NULL ;
}

void TestMultiStore::simulateDumpload(ISession* session, unsigned short inStoreID)
{
	PropertyID propIds[5];
	MVTApp::mapURIs(session, "simulateDumpload.prop.", 5, propIds);
	const int startPage = 10;  
	const int pages = 5;
	int k;
	#if 1
	RC rc;
	for ( k = 0 ; k < pages; k++ )
	{
		TVERIFYRC(rc = session->reservePage( startPage+k )) ;
		if ( rc != RC_OK ) { mLogger.out() << "Page: " << startPage+k << endl ;  }
	}
	#endif
	for ( k = 0 ; k < pages; k++ )
	{
		PID lPID; INITLOCALPID(lPID); 			
		LOCALPID(lPID) = (uint64_t(inStoreID) << STOREID_SHIFT) + ((startPage+k)<<16) + k;
		Value val[2]; Tstring str;
		MVTApp::randomString(str,5,0,false);
		val[0].set(str.c_str());val[0].setPropID(propIds[0]);
		val[1].set(0);val[1].setPropID(propIds[1]);
		IPIN *pin = session->createUncommittedPIN(val,2,MODE_COPY_VALUES,&lPID);
		TVERIFYRC(session->commitPINs(&pin,1));
		pin->destroy();
	}	

	IdentityID i = session->loadIdentity( "Guest", NULL, 0, true ) ;
	TVERIFY( i != 0 ) ;
}

void TestMultiStore::doStuffInStore( ISession* inS  )
{
	// We can't do comprehensive coverage of all store features here, this would just be
	// for specific trouble areas and basic overview.  To get wide coverage the "bash" test should
	// be run in parallel
	
	// Create guest identity for the other threads
	IdentityID id = inS->storeIdentity( "Guest", NULL, true ) ;
	TVERIFY( id != 0 ) ;

	int i ;
	Value vals[3] ;
	string lstr ;

	PropertyID binary_id = MVTApp::getProp(inS,"binary_id") ;;
	PropertyID str_id = MVTApp::getProp(inS,"string_id") ;
	PropertyID int_id = MVTApp::getProp(inS,"int_id") ;
	PropertyID coll_id = MVTApp::getProp(inS,"collection_id") ;

	// Build limited range of strings
	string lStringPool[100] ;
	lStringPool[0] = "MatchMeString" ;
	for ( i = 1 ; i < 100 ; i++ )
	{
		MVTRand::getString( lStringPool[i], 1, 300 ) ;
	}

	// Define classes
	IStmt* lQ = inS->createStmt();
	unsigned char v0 = lQ->addVariable() ;
	lQ->setPropCondition(v0,&int_id,1) ;
	RC rc = defineClass(inS,"HasIntProp", lQ ) ;
	TVERIFY((rc==RC_OK||rc==RC_ALREADYEXISTS)) ;
	lQ->destroy() ;

	// Define family
	lQ = inS->createStmt();
	v0 = lQ->addVariable() ;

	Value args[2] ;
	args[0].setVarRef(v0,coll_id);
	args[1].setParam(0);

	IExprTree *lExpr = inS->expr(OP_EQ,2,args,CASE_INSENSITIVE_OP);
	lQ->addCondition(v0,lExpr);

	rc=defineClass(inS,"StringLookup", lQ ) ;
	TVERIFY(rc==RC_OK || rc==RC_ALREADYEXISTS) ;
	lQ->destroy() ;
	lExpr->destroy() ;

	// Add Pins
	uint64_t lCntString0 = 0 ;

	PID lAllPins[NUM_PINS];

	for ( i = 0 ; i < NUM_PINS ; i++ )
	{
		if ( i % 10 == 9 ) mLogger.out() << "." ;

		int strmSize = MVTRand::getRange( 10, 10000 ) ;
		if ( i % 50 == 0 ) strmSize = 0x20000 ; // Mix in a few really big streams		

		IStream *lStream = MVTApp::wrapClientStream(inS,new TestStringStream(strmSize,VT_BSTR));
		vals[0].set( lStream ) ; vals[0].property= binary_id ; vals[0].meta = META_PROP_NOFTINDEX | META_PROP_SSTORAGE;

		MVTRand::getString( lstr, 10, 200 ) ;
		vals[1].set( lstr.c_str() ) ; vals[1].property= str_id; 
		vals[2].set( i ) ; vals[2].property= int_id; 

		PID newpid ;
		TVERIFYRC(inS->createPIN(newpid,vals,1)) ;
		TVERIFY( inS->getLocalStoreID() == inS->getStoreID( newpid ) ) ;

		lAllPins[i] = newpid ;
	}	
	
	// Modify pins to add collections

	for ( i = 0 ; i < NUM_PINS ; i++ )
	{
		if ( i % 10 == 9 ) mLogger.out() << "." ;

		// Add a collection of strings
		for ( int k = 0 ; k < NUM_COLLECTION_ITEMS_PER_PIN ; k++ )
		{
			long lWhichString = MVTRand::getRange(1,99) ;		
			if ( k == 100 && ( i % 10 ) == 6 )
			{
				// Force a specific string so that we can test our query
				lWhichString = 0 ; 
				lCntString0++ ;
			}

			vals[0].set( lStringPool[lWhichString].c_str() ) ;  
			vals[0].property = coll_id; vals[0].op = OP_ADD ;
			vals[0].meta = META_PROP_NOFTINDEX ; /* prevent FT index but not family index */
			TVERIFYRC( inS->modifyPIN( lAllPins[i],vals,1 ) );
		}
	}

	// Run a query
	ClassID stringLookup = STORE_INVALID_CLASSID;
	TVERIFYRC(inS->getClassID("StringLookup", stringLookup )) ;
	lQ = inS->createStmt() ;

	Value stringToLookup ;
	stringToLookup.set( lStringPool[0].c_str() ) ;

	ClassSpec classInfo;
	classInfo.classID = stringLookup;
	classInfo.nParams = 1;
	classInfo.params = &stringToLookup;

	lQ->addVariable(&classInfo,1);		
	
	uint64_t cntQ ;
	TVERIFYRC(lQ->count(cntQ)) ;

	TVERIFY( lCntString0 == cntQ ) ;


	lQ->destroy() ;

	// Wait just to give more chance to see the state/RAM usage etc
	//MVTestsPortability::threadSleep(10000);			
}

void TestMultiStore::doGuestStuffInStore( ISession* inS  )
{
	// Run a query
	ClassID stringLookup = STORE_INVALID_CLASSID ;

	RC rc ;
	while(true)
	{
		rc = inS->getClassID("StringLookup", stringLookup ) ;
		if (rc==RC_OK)
			break ;
		// 
		#if 0 // remove temporarily
		if ( MVTApp::sDynamicLinkMvstore->isInProc() )
			break ;	// If not IPC we have exclusive access to file so can't spin
		#endif

		// IPC Guest thread is probably getting ahead of first thread
		MVTestsPortability::threadSleep(50);
	}
	TVERIFYRC(rc) ;

	CmvautoPtr<IStmt> lQ( inS->createStmt() );

	Value stringToLookup ;
	stringToLookup.set( "MatchMeString" ) ;

	ClassSpec classInfo;
	classInfo.classID = stringLookup;
	classInfo.nParams = 1;
	classInfo.params = &stringToLookup;

	lQ->addVariable(&classInfo,1);		
	
	uint64_t cntQ ;
	TVERIFYRC(lQ->count(cntQ)) ;
}
