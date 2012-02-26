/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _STORE_TESTS_APP_H
#define _STORE_TESTS_APP_H

#include "tests.h"
#include "randutil.h"
#include "util.h"
#include "mvstoremessagereporter.h"
#include "syncTest.h"
#include "../../kernel/include/startup.h"

#include "MVTArgs.h"

#ifndef _MAX_PATH
#define _MAX_PATH 1024
#endif

#define PAGE_SHIFT 16 // Location of page in a PID
#define STOREID_SHIFT 48 // Location of StoreID in a PID
#define BUILDPID(storeid,page,pageslot) uint64_t(storeid)<<STOREID_SHIFT | (page)<<PAGE_SHIFT | (pageslot)
#define BUILDEID(storeID,index) uint32_t(byte(storeID>>8)^byte(storeID))<<24 | (index);

class MVTApp
{
	// Basic test execution infrastructure.
	public:
		class CompareTestsLexico { 
			public: bool operator()(ITest * p1, ITest * p2) const { 
						// Move the "testcustom" guys to the end of the list and
						// sort them numerically
						const char * p1Name = p1->getName() ;
						const char * p2Name = p2->getName() ;
						bool p1TestCustom = (0==strncasecmp( p1Name, "testcustom", 10 ));
						bool p2TestCustom = (0==strncasecmp( p2Name, "testcustom", 10 ));
						if ( p1TestCustom )
						{
							if ( !p2TestCustom )
							{
								return false ;
							}

							int p1Val = atoi(p1Name+10 /*strlen("testcustom")*/ );
							int p2Val = atoi(p2Name+10 );
							return p1Val < p2Val ;
						}
						else if ( p2TestCustom )
						{
							return true ;
						}
						return (strcasecmp(p1Name, p2Name) < 0); 
					} 
		};
		typedef std::set<ITest *, CompareTestsLexico> TSortedTests;
		typedef std::vector<ITest*> TTestList ;
	protected:
		TSortedTests mSortedTests;
		MVTArgs *pargs;

        // Moved all global variables into MVTApp scope
	protected:
		static bool bRandBuffer;
		static bool bReplicate;
		static bool bReplicSmoke;

		struct TestResult
		{
			const char *mTestName;
			const char *mTestDesc;
			int 		mResult;
			clock_t     mExecTime;
			TIMESTAMP   mTS;
		};

		struct TestSuiteCtx
		{
			// Represents the context of a suite of one or more tests that
			// will run in sequence.  They will all operate on
			// the same store (although the store may be flushed between executions)
			// and accumulate results in mTestResults.
			Tstring					mSuiteName ; // A name for the suite, e.g. "Smoke"

			Tstring					mDir;		// Directory for the store file
			Tstring					mLogDir;	// Option different directory for AfyDB.log files
			Tstring					mIdentity;	// Name of owner (IPC requires each store has diff id)
			Tstring					mPwd;		// Optional password
			Tstring					mIdentPwd;  // Password for mvstore owner
			unsigned short			mStoreID;	// Identifier of the store
			unsigned int			mPageSize;
			unsigned int   		    mKernelErrSize; // Number of errors to be displayed by kernel. 
			int						mNBuffer;	// Number of pages for buffer
			int						mNRepeat;	// Number of repetitions for a single test
			bool					mbArchiveLogs;	// STARTUP_ARCHIVE_LOGS so that logs are not erased
			bool					mbRollforward;
			bool					mbGetStoreCreationParam; //Call to getStoreCreationParameters() before running the test.
			bool					mbForceNew;
			bool					mbForceOpen;
			bool					mbForceClose;	// Flag correponding to force close.
			bool					mbTestDurability; // If true each store is dumped before shutdown, then reopened and diffed.
			bool					mbPrintStats;
			bool					mbSingleProcess; // If false each test is launched it is own process
			long 					mCntFailure;	// Summary of failed tests
			TIMESTAMP				mStartTime;	    // Test execution start time
			bool					mbSmoke ;	// Whether test should consider it is in a faster "smoke" execution mode
			bool					mbFlushStore, mbFlushStoreIfFullscan ; // Whether to erase store files between each execution
			bool					mbChild;		// Single test launched from another test suite process			
			int						mDelay;		// Delay before starting (multistore scenario)
			unsigned int			mSeed;		// Seed for tests to use
			bool					mbSeedSet;	// Seed value is set in command line
			Tstring					mIOInit;    // Configuration of i/o drivers
			Tstring					mClient;	// client host
			IStoreIO*             mIO;		// IO Profiler or other IO plugin
			int						argc ;			// Test specific arguments (only makes sense for Suites running a single test)
			char **					argv ;	

			long volatile			mStarted;		// ref count for openstore calls
			long volatile			mTestIndex;		// executing test index
			MVTestsPortability::Mutex	*mLock;			// Protect state during concurrent store open/closing
			AfyKernel::StoreCtx *mStoreCtx;		// Store context when the store is open
			THREADID				mThreadID;		// framework thread running this suite

			TTestList				mTests;			// Tests to run in this suite
			std::vector<TestResult> mTestResults;	// Results of execution

			TestSuiteCtx() ;
			TestSuiteCtx(const TestSuiteCtx& rhs ) ;			
			~TestSuiteCtx() ;
		};
		static std::vector<TestSuiteCtx> mMultiStoreCxt;
		
		struct TestExecutionThreadCtxt
		{
			MVTApp				 * pThis ;
			MVTApp::TestSuiteCtx * pSuite ;
		} ;

		struct ThreadCrashCtxt
		{
			const char * mDirectory;
			bool mSingleProcess;
			long mTestIndex ;
			long mCrashTime ;
			ThreadCrashCtxt(const char *pDir, bool pProc, long pIndex, long pTime) : 
			mDirectory(pDir), mSingleProcess(pProc), mTestIndex(pIndex), mCrashTime(pTime) {};
		} ;

		static bool isSingleStore() { return mMultiStoreCxt.size() > 1 ; }
		static size_t numStores() { return mMultiStoreCxt.size() ; }
	public:
		MVTApp();
		int start(MVTArgs *args);

		static TestSuiteCtx & Suite(const char * inStoreDir=NULL) ; 
	protected:
		int help() const;
		void sortTests();

		void initSuite_all(bool inProc,const char* inStartAt);
		void initSuite_smoke(bool inProc,const char* inStartAt);
		void initSuite_perf();
		void initSuite_fromlist();
		void initSuite_longrunning();
		void initSuite_standalone();

		int executeTests() ; 
		int executeSuite(TestSuiteCtx & suite) ;

		bool canExecuteSmokeTest(ITest &, std::ostream& out);
		void makeHTMLResultFile(TestSuiteCtx &suite, bool bPopResults);
		void makeMultiStoreReport(bool bPopResults = false);
		void makeResultCSV(const char * inFilepath);
		void prepareMultiStoreTests();
		static THREAD_SIGNATURE threadCrash(void * pCtx);
		static THREAD_SIGNATURE threadExecuteSuite(/*TestExecutionThreadCtxt* */void * pCtx);

	// Library of services for all tests (don't hesitate to add stuff here).
	// TODO: publish already existing services from various tests (e.g. collection traversal etc.).
	public:
	    
		static long sCommandCrashWithinMsAfterStartup;
		static long sCommandCrashWithinMsBeforeStartup;
		static AfyKernel::StoreCtx * sReplicaStoreCtx;
		//static mvcore::DynamicLinkMvstore * sDynamicLinkMvstore;
		//static mvcore::DynamicLinkMvstore * smartPtr;
		static MVTestsPortability::Tls sThread2Session;
		static MvStoreMessageReporter sReporter;
		static Tstring mAppName; //argv[0]
		static bool bVerbose;
		static bool bNoUI; // Automated tests should not block on error, even serious assert etc
		static int sNumStores;
		static bool bRandomTests;
		static void printStoreCreationParam();
		static StoreCreationParameters mSCP;
		static AfyKernel::StoreCtx *mStoreCtx;
		static bool startStore(
			IStoreNet * pNetCallback = NULL, 
			IStoreNotification * pNotifier = NULL, 
			const char *pDirectory = NULL, 
			const char *pIdentity = NULL, 
			const char *pPassword = NULL,
			unsigned numbuffers=0, 
			unsigned short pStoreID=0, 
			unsigned int pPageSize=0, 
			const char * pIOStrOverride=NULL);

		static void dummyStartStore(unsigned int mode=0);
		static void stopStore();

		static RC createStoreWithDumpSession(ISession *& outSession, IStoreNet * pNetCallback = NULL, IStoreNotification * pNotifier = NULL);

		static bool isRunningSmokeTest() { return Suite().mbSmoke; }
		static int getNBuffers() { return Suite().mNBuffer; }
		enum eStartSessionFlags { kSSFTrackCurrentSession = (1 << 0), kSSFNoReplication = (1 << 1), };
		static ISession * startSession(AfyKernel::StoreCtx * = 0, char const * = 0, char const * = 0, long pFlags = kSSFTrackCurrentSession);
		static ISession * getSession() { return (ISession *)sThread2Session.get(); }
		static AfyDB::IStream * wrapClientStream(ISession * pSession, AfyDB::IStream * pClientStream);
		static AfyKernel::StoreCtx * getStoreCtx(const char * pIdentity = NULL);
		static unsigned int getPageSize() { return Suite().mPageSize ; }

		static bool deleteStore() ;
		static void buildCommandLine(TestSuiteCtx & suite,std::stringstream & cmdstr);
		static long getCurrentTestIndex(const char * pDirectory) { return Suite(pDirectory).mTestIndex; }
		static void enumClasses(ISession & pSession, std::vector<Tstring> * pNames = NULL, std::vector<uint32_t> * pIDs = NULL, std::vector<IStmt*> * pPredicates = NULL);

	public:

		// --begin obsolete methods--
		//
		// NOTE: THESE ARE BEING MOVED TO randutil.h
		// please use the equivalents there.  These will be removed
		//
		static Tstring & randomString(Tstring & pS, int pMin = 1, int pMax = MAX_PARAMETER_SIZE, bool pWords = true, bool keepcase = true) { return MVTRand::getString(pS,pMin,pMax,pWords,keepcase); }
		static Wstring & randomString(Wstring & pS, int pMin = 1, int pMax = MAX_PARAMETER_SIZE, bool pWords = true, bool keepcase = true) { return MVTRand::getString(pS,pMin,pMax,pWords,keepcase); }
		static int randInRange( int min, int max ) { return MVTRand::getRange( min, max ) ; }
		static bool randBool() { return MVTRand::getBool() ; }
		static uint64_t generateRandDateTime(ISession *pSession, bool bAllowFuture=false) { return MVTRand::getDateTime(pSession,bAllowFuture) ; }

		// These are being moved to util.h
		static void mapURIs(ISession *pSession, const char * pPropName, int pNumProps, PropertyID *pPropIDs) { return MVTUtil::mapURIs( pSession, pPropName, pNumProps, pPropIDs ) ; }
		static void mapURIs(ISession *pSession, const char * pPropName, int pNumProps, URIMap *pPropMaps) { return MVTUtil::mapURIs( pSession, pPropName, pNumProps, pPropMaps ) ; }
		static void mapStaticProperty(ISession *pSession, const char * pPropName, URIMap &pPropMap) { MVTUtil::mapStaticProperty( pSession, pPropName, pPropMap ) ; }
		static void mapStaticProperty(ISession *pSession, const char * pPropName, PropertyID &pPropID) { MVTUtil::mapStaticProperty( pSession, pPropName, pPropID ) ; } 
		static int countPinsFullScan(ISession * session) { return MVTUtil::countPinsFullScan( session ) ; }
		static int countPinsFullScan(ICursor * result, ISession * session) { return MVTUtil::countPins( result, session ) ; }
		static size_t getCollectionLength(Value const & pV) { return MVTUtil::getCollectionLength( pV ) ; }
		static void registerTestPINs(std::vector<PID> &pTestPINs, PID * pPIDs, const int pNumPIDs = 1) { MVTUtil::registerTestPINs(pTestPINs, pPIDs, pNumPIDs ) ; }
		static void registerTestPINs(std::vector<IPIN *> &pTestPINs, IPIN **pPINs, const int pNumPINs = 1) { MVTUtil::registerTestPINs(pTestPINs, pPINs, pNumPINs ) ; }
		static void unregisterTestPINs(std::vector<IPIN *> &pTestPINs, ISession *pSession){ MVTUtil::unregisterTestPINs( pTestPINs, pSession ) ; }
		static void unregisterTestPINs(std::vector<PID> &pTestPIDs, ISession *pSession){ MVTUtil::unregisterTestPINs( pTestPIDs, pSession ) ;  }
		static PropertyID getProp(ISession* inS, const char* inName) { return MVTUtil::getProp( inS, inName ) ; }
		static ClassID getClass(ISession* inS, const char* inClass) { return MVTUtil::getClass( inS, inClass ) ; }
		static char * myToUTF8(wchar_t const * pStr, size_t pLen, uint32_t & pBogus) { return MVTUtil::myToUTF8( pStr, pLen, pBogus) ; }
		static char *toUTF8(const wchar_t *ustr,uint32_t ilen,uint32_t& olen) { return MVTUtil::toUTF8(ustr,ilen,olen); }
		static wchar_t *toUNICODE(const char *str,uint32_t ilen,uint32_t& olen) { return MVTUtil::toUNICODE(str,ilen,olen) ; }
		static void getCurrentTime(Tstring & pTime) { MVTUtil::getCurrentTime( pTime ) ; }
		static bool equal(IPIN const & p1, IPIN const & p2, ISession & pSession, bool pIgnoreEids = false) { return MVTUtil::equal( p1, p2, pSession, pIgnoreEids ) ; }
		static bool equal(Value const & pVal1, Value const & pVal2, ISession & pSession, bool pIgnoreEids = false) { return MVTUtil::equal( pVal1, pVal2, pSession, pIgnoreEids ) ; }
		static void outputComparisonFailure(PID const & pPID, IPIN const & p1, IPIN const & p2, std::ostream & pOs);

		static void output(Value const & pV, std::ostream & pOs, ISession * pSession = NULL) { MVTUtil::output(pV,pOs,pSession) ; }
		static void output(IPIN const & pPIN, std::ostream & pOs, ISession * pSession = NULL) { MVTUtil::output(pPIN,pOs,pSession) ; }
		static void output(const PID & pid, std::ostream & pOs, ISession * pSession) { MVTUtil::output(pid,pOs,pSession) ; }
		static void output(const IStoreNotification::NotificationEvent & event, std::ostream & pOs, ISession * pSession = NULL) { MVTUtil::output(event,pOs,pSession) ; }
		static void outputTab(std::ostream & pOs, int pLevel) { for (int i = 0; i < pLevel; i++) pOs << "  "; }

		// ---end obsolete methods---

	public:
		static bool compareReplicaStore(TestLogger & pLogger);
};

#endif
