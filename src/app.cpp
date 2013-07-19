/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#define mvlog_STACKTRACE_INTERNALS
#include "app.h"
#include "md5stream.h"
#include "serialization.h"
#include "mvauto.h"
#include <map>
#include <algorithm>

#if defined(WIN32)
	#include <shellapi.h> // ShellExecute
	#include <direct.h>	// getcwd
	#ifdef _DEBUG
		#include <crtdbg.h>
		#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
	#endif
#else
//	#include <dlfcn.h>
//	#include <sys/wait.h>
//	#include <sys/time.h>
#endif
long gMainThreadID = 0;

ITest::TTests * ITest::sTests = NULL;
long MVTApp::sCommandCrashWithinMsAfterStartup = 0;
long MVTApp::sCommandCrashWithinMsBeforeStartup = 0;
Afy::IAffinity * MVTApp::sReplicaStoreCtx = NULL;
//mvcore::DynamicLinkMvstore * MVTApp::sDynamicLinkMvstore = NULL;
MVTestsPortability::Tls MVTApp::sThread2Session;
MvStoreMessageReporter MVTApp::sReporter ;
Tstring MVTApp::mAppName;
int MVTApp::sNumStores = 1;
Afy::IAffinity * MVTApp::mStoreCtx = NULL;
StoreCreationParameters MVTApp::mSCP; 

bool MVTApp::bNoUI = false;
bool MVTApp::bVerbose = false ;
bool MVTApp::bRandBuffer = false;
bool MVTApp::bReplicate = false;
bool MVTApp::bReplicSmoke = false;
bool MVTApp::bRandomTests = false;

std::vector<MVTApp::TestSuiteCtx> MVTApp::mMultiStoreCxt;

static void setAssertOutput( bool bUseMsgBox )
{
#if WIN32
	if ( bUseMsgBox )
	{
		//REVIEW: It would be nicer to have both a message box
		//and an error logged.
		//You can pass _OUT_TO_MSGBOX|_OUT_TO_STDERR to _set_error_mode
		//and _CRTDBG_MODE_FILE | _CRTDBG_MODE_WNDW to _CrtSetReportMode
		//to achieve approximately that result, however it seems to
		//break the ability to ignore an assert
		_set_error_mode(_OUT_TO_MSGBOX);
	}
	else
	{
		_set_error_mode(_OUT_TO_STDERR); //(_OUT_TO_STDOUT doesn't exist)
	}	

	#ifdef _DEBUG
	// Controll _CrtDbgReport behavior
	int mode = ( bUseMsgBox ? _CRTDBG_MODE_WNDW : _CRTDBG_MODE_FILE ) ;

	_CrtSetReportMode( _CRT_ASSERT, mode ) ;
	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDOUT );
	_CrtSetReportMode( _CRT_ERROR, mode ) ;
	_CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDOUT );
	_CrtSetReportMode( _CRT_WARN, mode ) ;
	_CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
	#endif
#endif
}
 
static void durationToString(clock_t inDuration, std::string & outDT, bool bBreakDown = false)
{
	// There used to be code that would determine the individual duration as a string
	// like minutes, seconds etc.  However to allow easy sorting and comparison
	// plus remove a lot of code, this is just the result in seconds!

	double seconds = inDuration / 1000. ; // Millisecond to Second
	char lBuf1[64];

	if ( !bBreakDown )
	{
		// Leave it in seconds

		sprintf(lBuf1,"%.2lf",seconds);
	}
	else
	{
		int intSeconds = (int)seconds ;
		int hours = intSeconds/(60*60);
		int minutes = intSeconds/60 - hours*60;
		int remSeconds = intSeconds%60;

		if ( hours > 0 )
		{					
			sprintf(lBuf1,"%d hour%s, %d minute%s, %d second%s",
				hours, (hours>1?"s":""),
				minutes, (minutes>1?"s":""),
				remSeconds, (remSeconds>1?"s":"")
				);
		}
		else if ( minutes > 0 )
		{
			sprintf(lBuf1,"%d minute%s, %d second%s",
				minutes, (minutes>1?"s":""),
				remSeconds, (remSeconds>1?"s":""));
		}
		else
		{
			sprintf(lBuf1,"%.2lf seconds",seconds);
		}
	}
	outDT = lBuf1;
}
#ifdef WIN32
	#include "mvlog.h"
	#include <tlhelp32.h>
static void writeStackToDiskWin32( const char * inFilename ) 
{
	//(see 9370 for details)
	std::ofstream lOs(inFilename, std::ios::out); 

	DWORD lThisProc = GetProcessId(GetCurrentProcess());
	HANDLE const lToolSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (lToolSnapshot)
	{
		THREADENTRY32 lTE;
		memset(&lTE, 0, sizeof(lTE));
		lTE.dwSize = sizeof(lTE);
		if (Thread32First(lToolSnapshot, &lTE)) do
		{
			if (lThisProc != lTE.th32OwnerProcessID)
				continue ;

			lOs<<endl<<endl<<"-------------Thread "<<lTE.th32ThreadID<<"-------------"<<endl;

			HANDLE hThread = ::OpenThread(THREAD_ALL_ACCESS,false,lTE.th32ThreadID) ;

			CONTEXT lContext;
			lContext.ContextFlags = CONTEXT_FULL;
			
			if ( GetCurrentThreadId() != lTE.th32ThreadID )
				SuspendThread( hThread ) ;
			if ( !GetThreadContext(hThread,&lContext) )
				continue ;

			TRawStackTrace lRawTrace ;
#if defined(_M_X64) || defined(_M_IA64)
			bool const lIntact = false;		//???
#else
			bool const lIntact = mvlogGetStackTrace( lRawTrace, lContext.Eip, lContext.Esp, lContext.Ebp ) ;
#endif
			if (!lIntact)
				lOs << "(truncated)";

			TTextStackTrace lTextStack;
			mvlogTranslateStackTrace(lTextStack,lRawTrace);

			for (TTextStackTrace::iterator i = lTextStack.begin(); lTextStack.end() != i; i++)
				lOs << std::endl << "  " << (*i).c_str();

			#if 0
				if (lTE.th32ThreadID == gMainThreadID)
					MessageBox(NULL, "Produced main thread's stack trace", "Warning", MB_OK);
			#endif

			#if 0 // Note: While we're at it, make sure the thread stops where we think it stops...
				if ( GetCurrentThreadId() != lTE.th32ThreadID )
					ResumeThread( hThread ) ;
			#endif
			CloseHandle(hThread);

			memset(&lTE, 0, sizeof(lTE));
			lTE.dwSize = sizeof(lTE);
		} while (Thread32Next(lToolSnapshot, &lTE));
		CloseHandle(lToolSnapshot);
	}
}

// Handle intentional Crash without any question, when needed... 
LONG MVTAppUnhandledExceptionFilter(EXCEPTION_POINTERS *) 
{ 
	writeStackToDiskWin32( "crashstack.txt" ) ;
	return EXCEPTION_EXECUTE_HANDLER; 
}
#endif



MVTApp::TestSuiteCtx::TestSuiteCtx()
{		
	mIdentity = "allo";
	mNBuffer = 2000;
	mNRepeat = 1;
	mStoreID = 0x1000;
	mPageSize = 0x8000; // 32K
	mbSmoke = false ;
	mbFlushStore = false ;
	mbFlushStoreIfFullscan = false ;
	mKernelErrSize = 0;
	mThreadID = getThreadId();
	mbSingleProcess = true ;
	argc = 0 ;
	argv = NULL ;
	mStarted = 0 ;
	mTestIndex = 0 ;
	mStoreCtx = NULL;
	mbChild = false ;
	mbArchiveLogs = false ;
	mbRollforward = false ;
	mbForceNew = false ;
	mbGetStoreCreationParam = false;
	mbForceOpen = false;
	mbForceClose = false;
	mbTestDurability = false;
	mbPrintStats = false;
	mCntFailure = 0 ;
	getTimestamp(mStartTime);
	mDelay=1000;
	mSeed=0;
	mbSeedSet = false;
	mLock = new MVTestsPortability::Mutex ;
}
MVTApp::TestSuiteCtx::TestSuiteCtx(const TestSuiteCtx& rhs )
{
	*this = rhs;
	// Get private copy of each test
	for ( size_t i = 0 ; i < rhs.mTests.size() ; i++ )
	{
		mTests[i]=rhs.mTests[i]->newInstance() ;
	}
	mLock = new MVTestsPortability::Mutex ;
}
MVTApp::TestSuiteCtx::~TestSuiteCtx()
{
	delete(mLock) ;
	for ( size_t i = 0 ; i < mTests.size() ; i++ )
	{
		mTests[i]->destroy() ;
	}

	mTests.clear() ;
	mTestResults.clear() ;
}



MVTApp::MVTApp()
{
	sortTests();
}

int MVTApp::start(MVTArgs *args)
{
	// next 2 lines are for backward compatability...
	// On return larc,largv contains the parameters which have been read
	// from the file(s) specified as @file(x) within original command line.
	char ** largv=NULL; int largc; string lval; 
	
	args->get_paramOldFashion(largc,largv);
	pargs = args;
	
	pargs->get_param("pname",mAppName); 
	
	mMultiStoreCxt.resize(1);

	// Store uses current working directory if nothing specified.
	// Determine that immediately so that error messages are more meaningful
	char cwd[_MAX_PATH];
	Suite().mDir = getcwd(cwd, _MAX_PATH);
	Suite().mSeed = ((unsigned int)time(NULL) * 500) & 0xffff;
	bool bList=false, bVerbose=false;

	// Parse the misc options available
	// These arguments all start with "-" and are not the
	// arguments exposed by the test itself
	
	pargs->get_param("ident",Suite().mIdentity);
	pargs->get_param("storeid",Suite().mStoreID);
	pargs->get_param("pwd",Suite().mPwd);
	pargs->get_param("ipwd",Suite().mIdentPwd);
	pargs->get_param("pagesize",Suite().mPageSize);
	pargs->get_param("ioinit", Suite().mIOInit);
	pargs->get_param("client", Suite().mClient);

	if(Suite().mIOInit!="")Suite().mIOInit="ioinit="+Suite().mIOInit;
	
	pargs->get_param("dir",Suite().mDir);
	pargs->get_param("logdir",Suite().mLogDir);
	
	pargs->get_param("multistore",sNumStores);
	pargs->get_param("-randtests",bRandomTests);
	pargs->get_param("nbuf",Suite().mNBuffer);
	pargs->get_param("-rand",bRandBuffer); 

	Suite().mbSeedSet=pargs->get_param("seed",Suite().mSeed);
	
	if(pargs->param_present("-variant")) Suite().mbSingleProcess = false;
	
	pargs->get_param("delay",Suite().mDelay);
	pargs->get_param("crash",sCommandCrashWithinMsAfterStartup);
	pargs->get_param("crashduring",sCommandCrashWithinMsBeforeStartup);
	pargs->get_param("-replicate",bReplicate);
	pargs->get_param("-replicsmoke",bReplicSmoke);
	pargs->get_param("-child",Suite().mbChild);
	pargs->get_param("-archivelogs",Suite().mbArchiveLogs);
	pargs->get_param("-rollforward",Suite().mbRollforward);
	pargs->get_param("-forcenew",Suite().mbForceNew);
	pargs->get_param("-gscp",Suite().mbGetStoreCreationParam);
	pargs->get_param("-forceopen",Suite().mbForceOpen);
	pargs->get_param("-forceclose",Suite().mbForceClose);
	pargs->get_param("-durability",Suite().mbTestDurability);
	pargs->get_param("-printstats",Suite().mbPrintStats);
	pargs->get_param("-newstore",Suite().mbFlushStore);
	pargs->get_param("errlogsize",Suite().mKernelErrSize);
	pargs->get_param("-v",MVTApp::bVerbose);
	pargs->get_param("-noui",MVTApp::bNoUI);
	pargs->get_param("-list",bList);
	pargs->get_param("verbose",bVerbose);

	//MVTApp::sReporter.init(MVTApp::sDynamicLinkMvstore);

	// If "-forsmoke" is specified, let the test know it's run for smoke test,
	// so it can disable some experimental (not-yet-working) parts.
	pargs->get_param("-forsmoke",Suite().mbSmoke);	
		
	pargs->get_param("repeat",Suite().mNRepeat);
		
	if ( MVTApp::bNoUI )
	{
		setAssertOutput( false ) ;
	}

#if 0 //remove replication temporarily
	// In order to run the systematic replication test, we need to use ipc and not multistore.
	if (bReplicate && (sDynamicLinkMvstore->isInProc() || sNumStores>1))
		bReplicate = false;
#endif

	if (sCommandCrashWithinMsAfterStartup)
	{
		#ifdef WIN32
			// Crash without any question, when needed...
			::SetErrorMode(SEM_NOGPFAULTERRORBOX);
			::SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)MVTAppUnhandledExceptionFilter);
		#endif
		largc--;
	}

	// Option to crash during recovery, i.e. during the startup itself.
	if (sCommandCrashWithinMsBeforeStartup)
	{
		#ifdef WIN32
			// Crash without any question, when needed...
			::SetErrorMode(SEM_NOGPFAULTERRORBOX);
			::SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)MVTAppUnhandledExceptionFilter);
		#endif
		largc--;
	}

	// We have already processed all supported arguments starting in "-"
	// Remaining arguments are passed to the suite/test itself
	int iArg;
	for (iArg = largc - 1; iArg >= 0; iArg--)
	{
		if ( strlen(largv[iArg])==0  ) 
			largc--;
		else if ( largv[iArg][0] == '-' )
			largc--;
		else if ( strlen(largv[iArg])==2 && 
			((largv[iArg][0] == '\'' && largv[iArg][1] == '\'')||
			 (largv[iArg][0] == '\"' && largv[iArg][1] == '\"'))
			)
			largc--; // Ignore empty '' args, side effect of optional arg handling from bash scripts
	}
	
	if ( pargs->get_cntParamTotal() == 1 )
		return help(); // No Test Name specified

	// Execute pre-defined test-suite, if requested.
	if (0 == strcasecmp("all", pargs->get_arvByIndex(1)))
	{
		initSuite_all(true, pargs->get_param("starttest",lval)?lval.c_str():NULL); 
	}
	else if (0 == strcasecmp("smoke", pargs->get_arvByIndex(1)))
	{
		initSuite_smoke(false /*out of proc*/, pargs->get_param("starttest",lval)?lval.c_str():NULL); 
	}
	else if (0 == strcasecmp("smokeinproc", pargs->get_arvByIndex(1)))
	{
		initSuite_smoke(true /*out of proc*/, pargs->get_param("starttest",lval)?lval.c_str():NULL); 
	}
	else if (0 == strcasecmp("longrunning", pargs->get_arvByIndex(1)))
	{
		initSuite_longrunning() ;
	}	
	else if (0 == strcasecmp("standalone", pargs->get_arvByIndex(1)))
	{
		initSuite_standalone() ;
	}	
	else if (0 == strcasecmp("perf", pargs->get_arvByIndex(1)))
	{
		initSuite_perf() ;
	}
	else if (0 == strcasecmp("fromlist", pargs->get_arvByIndex(1)))
	{
		initSuite_fromlist();
	}
	else
	{
		// Find the test that corresponds to the user-provided arguments.
		ITest::TTests const & lTests = ITest::getTests();
		ITest::TTests::const_iterator i;
		ITest * lMatch = NULL;
		for (i = lTests.begin(); i != lTests.end(); i++)
		{
			ITest * const lTestIt = (*i);
			if (0 != strcasecmp(lTestIt->getName(), pargs->get_arvByIndex(1)))
				continue;

			lMatch = lTestIt;
			break;
		}
		//TestLogger lOutV(TestLogger::kDStdOutVerbose);

		if (lMatch==NULL)
		{
			if ( !bList ) 
				return help() ;
		}
		else
		{
			Suite().mTests.push_back(lMatch->newInstance()) ;

			//The following 2 lines for the backward compatability of the code
			Suite().argc = largc;
			Suite().argv = largv ;
		}
	}

	if (bList)
	{
		// Useful in conjunction with scripts like "runtestsfromlist.sh"
		// or to check if a test is valid
		for (int i = 0 ; i < (int)Suite().mTests.size() ; i++)
		{
			ITest * lTestIt = Suite().mTests[i];
			std::cout << lTestIt->getName() << " ";
			char const * lR;
			if ( bVerbose )
			{
				std::cout << (lTestIt->isLongRunningTest() ? "long " : "");
				std::cout << (lTestIt->includeInSmokeTest(lR) ? "smoke " : "");
				std::cout << (lTestIt->includeInLongRunningSmoke(lR) ? "smokelong " : "");
				std::cout << (lTestIt->includeInBashTest(lR) ? "bash " : "");
				std::cout << (lTestIt->includeInMultiStoreTests() ? "multistore " : "");
				std::cout << (lTestIt->includeInPerfTest() ? "perf " : "");
				std::cout << (lTestIt->isPerformingFullScanQueries() ? "fullscan " : "");
			}
			std::cout << endl;
		}			
		return 0;
	}

	if (bRandomTests && Suite().mTests.size() > 1)
	{
		srand(((unsigned int)time(NULL) * 500) & 0xffff);
		std::random_shuffle(Suite().mTests.begin(), Suite().mTests.end());
	}

	if (sNumStores > 1)
		prepareMultiStoreTests();

	int const lResult = executeTests();

	if (sNumStores > 1)
		makeMultiStoreReport(true);
	mMultiStoreCxt.clear();
	return lResult;
}

int MVTApp::help() const
{
	// Output help.
	std::cout << "  Afy Kernel Test Suite ";
	#ifdef _DEBUG
		std::cout << "[Debug]" << std::endl;
	#else
		std::cout << "[Release]" << std::endl;
	#endif
	std::cout << endl;
	std::cout << "  Used to launch tests and suites of tests" << endl;
	std::cout << "  Usage:" << std::endl;
	std::cout << mAppName.c_str() << " test_or_suite_name [testargs] [-opt=...] [-opt] " << endl;
	std::cout << "  Use no spaces for options that require a value, e.g. -buf=2000" << endl << endl;
	std::cout << "  Example: " << endl ;
	std::cout << "  tests testemailimport 5000 1 random -nbuf=2000 -v" << endl ;
	std::cout << "  -----" << endl ;

	TSortedTests::const_iterator i;
	for (i = mSortedTests.begin(); i != mSortedTests.end(); i++)
	{
		ITest * const lTestIt = (*i);

#if 0
#ifdef _DEBUG
		// To avoid problems in automated test scripts, especially case sensitive linux
		// enforce lowercase convention.  To fix just fix the test name
		const char *pName=lTestIt->getName() ;
		while(*pName!='\0') {
			if (tolower(*pName)!=*pName) {
				//assert(!"Test name should be all lower case");
				std::cout << "\n\n\nWARNING - name is not lower case " << lTestIt->getName() << "\n\n\n";
			}
			pName++;
		}
#endif
#endif

		std::cout << "  " << mAppName.c_str() << " ";
		std::cout << lTestIt->getName() << " " << lTestIt->getHelp() << std::endl;
		std::cout << "                ... ";
		std::cout << lTestIt->getDescription() << std::endl;
	}

	// Special commands.
	std::cout << "  -----" << std::endl;
	/*
	std::cout << "  " << mAppName.c_str() << " bash" << " [smoke|longrunning] [NumOfThreads] [StoreDirectory] [StoreOwnerIdentity] [Password]"<< std::endl;
	std::cout << "                ... run tests in random order in an endless loop" << std::endl;
	*/
	std::cout << "  " << mAppName.c_str() << " perf" << std::endl;
	std::cout << "                ... run performance tests " << std::endl;
	std::cout << "  " << mAppName.c_str() << " smoke [-starttest=test_to_start_at]" << std::endl;
	std::cout << "                ... run all tests once (alphabetical order)" << std::endl;
	std::cout << "  " << mAppName.c_str() << " smokeinproc" << std::endl;
	std::cout << "                ... run all tests once (alphabetical order), in-process" << std::endl;
	std::cout << "  " << mAppName.c_str() << " longrunning" << std::endl;
	std::cout << "                ... runs all tests classified as long running." << std::endl;
	std::cout << "  " << mAppName.c_str() << " standalone" << std::endl;
	std::cout << "                ... runs tests which require new store" << std::endl;
	std::cout << "  " << mAppName.c_str() << " fromlist" << std::endl;
	std::cout << "                ... runs all tests mentioned in list.txt file" << std::endl;
	std::cout << "  -----" << std::endl;
	std::cout << "  -ipc Run test in IPC mode" << std::endl;
	std::cout << "		-srvname=<SERVERNAME> Specify the server name to connect (if not running under default name)" << std::endl;
	std::cout << "  -dir=STOREDIR Directory to start (default is current directory)" << std::endl;
	std::cout << "  -logdir=optional alternative directory for the log files" << std::endl;
	std::cout << "  -nbuf=NUMBER [-rand] Number of buffers to start the store with. If '-rand' then nbuf will be randomized for multistore" << std::endl;
	std::cout << "  -pwd=WORD open or create password protected encrypted store" << std::endl;
	std::cout << "  -ipwd=WORD provide identity password" << std::endl;
	std::cout << "  -seed=NUMBER Seed the random generator with this arg, to allow reproducable tests.  Supported by most individual tests." << std::endl;
	std::cout << "  -repeat=NUMBER Perform the same test up to N times (stops after first failure)." << std::endl;
	std::cout << "  -multistore=n [-delay=x] [-variant] Runs tests on n number of stores parallelly in a single process" << std::endl;
	std::cout << "		... -variant Runs multistore tests in multiple processes" << std::endl;
	std::cout << "		... -delay=x Specifies time gap of x ms across multistore test invocation " << std::endl;	
	std::cout << "  -errlogsize=n (unit in MB). Restrict the Log file size, which is used for all the kernel reported errors/warnings " << std::endl; 
	std::cout << "  -newstore Erase store before starting test (or between tests of a suite)" << std::endl;
	std::cout << "	-randtests Randomizes the test lists (like bash test) " << std::endl;
	std::cout << "  -crash=TIME Test will crash TIME ms after starting " << std::endl;
	std::cout << "  -crashduring=TIME Crash will occur during startup\\recovery after TIME ms" << std::endl;
	std::cout << "  -forsmoke Only run the smoke portion of the test" << std::endl;
	std::cout << "  -wait Pause at completion until key pressed" << std::endl;
	std::cout << "  -v Verbose output" << std::endl;
	std::cout << "  -noui Don't block with assert msgbox or other dialogs" << std::endl; 
	std::cout << "  -list List tests in suite rather than executing them" << std::endl;
	std::cout << "  -----" << std::endl;
	std::cout << "  -ioinit=CONFIG Use s3, traceio or other drivers" << std::endl;
	std::cout << "          examples:"<<std::endl;
	std::cout << "          \"-ioinit={stdio}{s3io,user:abc,secret:12345,remotepath:'/www.pidocs.com/test'}\""<< std::endl;
	std::cout << "          \"-ioinit={stdio}{iotracer,tracelogio:1,pagesize:32768}{ioprofiler}\""<< std::endl;
	std::cout << "  -----" << std::endl;
	std::cout << "  -client=client host address (now ignored: the host address is now inferred from ident), use the clientapi library to connect over http" << std::endl;
	
	std::cout << "  -archivelogs, -forcenew, -forceopen,-forceclose, -rollforward, -printstats : Enable kernel STARTUP_ flags" << std::endl;
	std::cout << "  -durability : perform an additional durability test, at shutdown" << std::endl;
	std::cout << "  -----" << std::endl;
	std::cout << "These flags only work when store does not exist (and may be ignored by some tests):" << std::endl ;
	std::cout << "  -ident=NAME Set the identity (no spaces allowed)" << std::endl;
	std::cout << "  -storeid=NUMBER Set the identity (no spaces allowed)" << std::endl;
	std::cout << "  -pagesize=NUMBER (new store only, must be 4k-64k multiple of 2)" << std::endl;
	std::cout << " -gscp Calls GetStoreCreationParameter()" << std::endl;
	return 1;
}

void MVTApp::sortTests()
{
	ITest::TTests const & lTests = ITest::getTests();
	ITest::TTests::const_iterator i;
	for (i = lTests.begin(); i != lTests.end(); i++)
	{
		ITest * const lTestIt = (*i);
		mSortedTests.insert(lTestIt);
	}
}

void MVTApp::initSuite_all(bool inProc, const char * inStartAt)
{
	TestSuiteCtx & suite = Suite() ;
	suite.mbSmoke = false;	
	suite.mSuiteName = "all" ;
	suite.mbSingleProcess = sNumStores == 1 ? inProc : true;
	TSortedTests::const_iterator i = mSortedTests.begin();

	// Find the normal smoke tests
	for (; i != mSortedTests.end() ; i++)
	{
		ITest * const lTestIt = (*i);

		if ( MVTApp::bVerbose )
		{
			std::cout << std::endl;
			std::cout << "Smoke test: " << lTestIt->getName() << std::endl;
			std::cout << "Intention of the test: " << lTestIt->getDescription() << std::endl;
		}

		if (inStartAt != NULL)
		{
			if (0 != strcmp(lTestIt->getName(),inStartAt)) 
			{
				if ( MVTApp::bVerbose )
					std::cout <<lTestIt->getName()<<" comes before " << inStartAt << std::endl;
				continue ;
			}

			// Found it, now can start inserting
			inStartAt = NULL;
		}

		ITest * const lTestCpy = lTestIt->newInstance(); 
		suite.mTests.push_back(lTestCpy);
	}
}

void MVTApp::initSuite_smoke(bool inProc, const char * inStartAt)
{
	TestSuiteCtx & suite = Suite() ;
	suite.mbSmoke = true;	
	suite.mbFlushStore = true; // Note (maxw, Nov2010): For all those years we had run 'smoke' on a dirty store; but recently we decided that the perf/stability fluctuations were counter-productive; in a near future, 'longrunning' will run on a dirty store.
	suite.mSuiteName = "smoke";
	suite.mbSingleProcess = sNumStores == 1?inProc:true;
	TSortedTests::const_iterator i = mSortedTests.begin();

	// Find the normal smoke tests
	for (; i != mSortedTests.end() ; i++)
	{
		ITest * const lTestIt = (*i);

		if ( MVTApp::bVerbose )
		{
			std::cout << std::endl;
			std::cout << "Smoke test: " << lTestIt->getName() << std::endl;
			std::cout << "Intention of the test: " << lTestIt->getDescription() << std::endl;
		}

		if (inStartAt != NULL )
		{
			if (0 != strcmp(lTestIt->getName(),inStartAt)) 
			{
				if ( MVTApp::bVerbose )
					std::cout <<lTestIt->getName()<<" comes before " << inStartAt << std::endl;
				continue ;
			}

			// Found it, now can start inserting
			inStartAt = NULL ;
		}

		if (lTestIt->isLongRunningTest())
		{
			if ( MVTApp::bVerbose )
				std::cout <<lTestIt->getName()<<" is a long running test. Please run tests with longrunning option" << std::endl;
			continue ;
		}
		if (lTestIt->isStandAloneTest())
		{
			if ( MVTApp::bVerbose )
				std::cout <<lTestIt->getName()<<" is a stand alone test. Please run tests with standalone option" << std::endl;
			continue ;
		}

		char const * lReason = NULL;
		if (!lTestIt->includeInSmokeTest(lReason))
		{
			if ( MVTApp::bVerbose )
			{
				std::cout << "  skipped (excluded from smoke test: ";
				if(lReason) std::cout << lReason;
				std::cout << ")" << std::endl;
			}
			continue ;
		}
		if (lTestIt->excludeInLessPageSizeTestSuits(lReason) >= suite.mPageSize )
		{
			if ( MVTApp::bVerbose )
				std::cout << "  skipped (excluded from smoke test: " << lReason << ")" << std::endl;
			continue ;
		}

#if 0 // remove IPC
		if(lTestIt->excludeInIPCSmokeTest(lReason) && !sDynamicLinkMvstore->isInProc())
		{
			if ( MVTApp::bVerbose )
				std::cout << "  skipped (excluded from IPC smoke test: " << lReason << ")" << std::endl;
			continue ;
		}
#endif		
		if(sNumStores > 1 && !lTestIt->includeInMultiStoreTests())
		{
			if ( MVTApp::bVerbose )
				std::cout << "  skipped (excluded from multistore smoke tests" << std::endl;
			continue ;
		}
		ITest * const lTestCpy = lTestIt->newInstance(); 
		suite.mTests.push_back(lTestCpy);
	}
	//Add all tests here when u want to run them after the normal tests have run e.g testdropclass etc
#if 1 
	static char const * const tests[] =
	{
			/*"testquickindexrebuild",*/"testdropclass"
	};
	for (unsigned int t=0; t<sizeof(tests) / sizeof(tests[0]); t++)
	{
		for (i = mSortedTests.begin(); i != mSortedTests.end()/* && sDynamicLinkMvstore->isInProc()*/; i++)
		{
			ITest * const lTestIt = (*i);
			if ( 0 == strcmp( lTestIt->getName(), tests[t] ))
			{
				if ( MVTApp::bVerbose )
					std::cout << "Adding special tests to end" << std::endl;

				ITest * const lTestCpy = lTestIt->newInstance(); 
				suite.mTests.push_back(lTestCpy);
				break ;
			}
		}
	}
#endif
	// Add the recovery test (however it erase 
	// the store files)
	bool lAddScenario = !bRandomTests && ((sNumStores == 1 && !suite.mbSingleProcess) || (sNumStores > 1 && !suite.mbSingleProcess));	
	for (i = mSortedTests.begin(); i != mSortedTests.end()/* && sDynamicLinkMvstore->isInProc()*/ && lAddScenario; i++)
	{
		ITest * const lTestIt = (*i);
		if ( 0 == strcmp( lTestIt->getName(), "scenario" ))
		{
			if ( MVTApp::bVerbose )
				std::cout << "Adding scenario recovery test to end" << std::endl;

			ITest * const lTestCpy = lTestIt->newInstance(); 
			suite.mTests.push_back(lTestCpy);
			break ;
		}
	}
}

void MVTApp::initSuite_perf()
{
	TestSuiteCtx & suite = Suite() ;
	suite.mbSmoke = true;	
	suite.mbFlushStore = true ;
	suite.mSuiteName = "perf" ;
	suite.mbSingleProcess = sNumStores == 1?false:true;
	suite.mKernelErrSize = 0;
	TSortedTests::const_iterator i = mSortedTests.begin();
	for (; i != mSortedTests.end() ; i++)
	{
		ITest * const lTestIt = (*i);
		char const * lReason = NULL;
		
		if(sNumStores > 1 && !lTestIt->includeInMultiStoreTests())
		{
			if ( MVTApp::bVerbose )
				std::cout << "  skipped (excluded from multistore perf tests" << std::endl;
			continue ;
		}

		if (lTestIt->includeInPerfTest() && (lTestIt->excludeInLessPageSizeTestSuits(lReason) < suite.mPageSize) )
		{
			if ( MVTApp::bVerbose )
			{
				std::cout << "Perf will include: " << lTestIt->getName() << std::endl;
			}

			ITest * const lTestCpy = lTestIt->newInstance(); 
			suite.mTests.push_back(lTestCpy);
		}
	}
}

void MVTApp::initSuite_longrunning()
{
	TestSuiteCtx & suite = Suite() ;
	suite.mSuiteName = "longrunning" ;
	suite.mbSmoke = false; // Tests don't need to run quickly as in a normal smoke
	suite.mbFlushStore = false; // Note (maxw, Nov2010): We decided to focus on 2 suites: 'smoke' is quick and runs on clean stores; 'longrunning' is slow and runs as much as possible on dirty stores.
	suite.mbFlushStoreIfFullscan = true;
	suite.mKernelErrSize = 0;
	suite.mbSingleProcess = sNumStores == 1?false:true;
	TSortedTests::const_iterator i;

	// First, include most of the std smoke suite, but running on dirty store; reserve tests doing fullscan queries for the end.
	char const * lReason = NULL;
	TTestList lReservedForTheEnd;
	for (i = mSortedTests.begin(); i != mSortedTests.end() ; i++)
	{
		ITest * const lTestIt = (*i);
		if (sNumStores > 1 && !lTestIt->includeInMultiStoreTests())
			continue;
		if (lTestIt->isLongRunningTest())
			continue;
		if (lTestIt->excludeInLessPageSizeTestSuits(lReason) >= suite.mPageSize)
			continue;
		if (!lTestIt->includeInSmokeTest(lReason))
			continue;

		// Ok, keep this test.
		if (lTestIt->isPerformingFullScanQueries())
			{ lReservedForTheEnd.push_back(lTestIt->newInstance()); continue; }
		if (MVTApp::bVerbose)
			std::cout << "Long Running will include smoke test: " << lTestIt->getName() << std::endl;
		suite.mTests.push_back(lTestIt->newInstance());
	}

	// Second, include the original longrunning suite, but reserve tests doing fullscan queries for the end.
	for (i = mSortedTests.begin(); i != mSortedTests.end() ; i++)
	{
		ITest * const lTestIt = (*i);
		if (sNumStores > 1 && !lTestIt->includeInMultiStoreTests())
		{
			if (MVTApp::bVerbose)
				std::cout << "  skipped (excluded from multistore longrunning tests" << std::endl;
			continue;
		}
		if (lTestIt->excludeInLessPageSizeTestSuits(lReason) >= suite.mPageSize)
			continue;
		if (!lTestIt->isLongRunningTest() || !lTestIt->includeInLongRunningSmoke(lReason))
			continue;

		// Ok, keep this test.
		if (lTestIt->isPerformingFullScanQueries())
			{ lReservedForTheEnd.push_back(lTestIt->newInstance()); continue; }
		if (MVTApp::bVerbose)
			std::cout << "Long Running will include longrunning test: " << lTestIt->getName() << std::endl;
		suite.mTests.push_back(lTestIt->newInstance());
	}

	// Last, include all tests that will require a clean store
	// (we do this mostly for performance reasons... otherwise the suite would be too long).
	TTestList::iterator iR;
	for (iR = lReservedForTheEnd.begin(); lReservedForTheEnd.end() != iR; iR++)
	{
		if (MVTApp::bVerbose)
			std::cout << "Long Running will include: " << (*iR)->getName() << " (on clean store)" << std::endl;
		suite.mTests.push_back(*iR);
	}
}

void MVTApp::initSuite_standalone()
{
	TestSuiteCtx & suite = Suite() ;
	suite.mSuiteName = "standalone" ;
	suite.mbSmoke = false;	// Tests don't need to run quickly as in a normal smoke
	suite.mbFlushStore = true ;
	suite.mKernelErrSize = 0;
	suite.mbSingleProcess = sNumStores == 1?false:true;
	TSortedTests::const_iterator i = mSortedTests.begin();
	for (; i != mSortedTests.end() ; i++)
	{
		ITest * const lTestIt = (*i);
		if(sNumStores > 1 && !lTestIt->includeInMultiStoreTests())
		{
			if ( MVTApp::bVerbose )
				std::cout << "  skipped (excluded from multistore standalone tests" << std::endl;
			continue ;
		}

		char const * lReason = NULL;
		if (lTestIt->isStandAloneTest() && (lTestIt->excludeInLessPageSizeTestSuits(lReason) < suite.mPageSize ))
		{
			if ( MVTApp::bVerbose )
			{
				std::cout << "Stand alone test will include: " << lTestIt->getName() << std::endl;
			}

			suite.mTests.push_back(lTestIt->newInstance());
		}
	}
}

void MVTApp::initSuite_fromlist()
{
	TestSuiteCtx & suite = Suite() ;
	suite.mbSmoke = true;	
	suite.mSuiteName = "fromlist" ;
	std::ifstream input;suite.mbSingleProcess = sNumStores == 1?false:true;
	input.open("list.txt",std::ios_base::in);
	if (!input)
	{
		std::cout<<"Input file list.txt not found"<<std::endl;
		input.close();
		exit(1);
	}
	else
	{
		while(!input.fail())
		{
			string testName;
			getline(input,testName);
			TSortedTests::const_iterator i = mSortedTests.begin();
			for (; i != mSortedTests.end() ; i++)
			{
				ITest * const lTestIt = (*i);
				if (0 == strcmp(lTestIt->getName(),testName.c_str()))
				{
					suite.mTests.push_back(lTestIt->newInstance());
					break;
				}
			};
		}
	}
	input.close();
}
int MVTApp::executeTests()
{
	assert(!mMultiStoreCxt.empty());

	if (mMultiStoreCxt.size()==1)
	{
		// No need for another thread
		return executeSuite(mMultiStoreCxt[0]);
	}
	else
	{
		// Launch suites in parallel
		HTHREAD * lThreads = (HTHREAD*)malloc(mMultiStoreCxt.size()*sizeof(HTHREAD));
		TestExecutionThreadCtxt * ctxts = (TestExecutionThreadCtxt*)malloc(mMultiStoreCxt.size()*sizeof(TestExecutionThreadCtxt));
		for (size_t j = 0 ; j < mMultiStoreCxt.size() ; j++)
		{
			ctxts[j].pThis = this ; ctxts[j].pSuite = &mMultiStoreCxt[j];
			createThread(&MVTApp::threadExecuteSuite, &(ctxts[j]), lThreads[j]);
			MVTestsPortability::threadSleep(mMultiStoreCxt[j].mDelay);
		}
		MVTestsPortability::threadsWaitFor((int)mMultiStoreCxt.size(), lThreads);
		free(lThreads);

		// Look for any failures
		for (size_t j = 0 ; j < mMultiStoreCxt.size() ; j++)
		{
			if (mMultiStoreCxt[j].mCntFailure>0)
				return 1 ;
		}
		return 0 ;
	}
}

int MVTApp::executeSuite(TestSuiteCtx & suite)
{
	// Execute a series of tests and remember the results
	// returns true for successful execution

	if ( suite.mTests.empty() )
	{
		std::cout << "Test framework error - no tests in suite" << endl;
	}

	if ( suite.mSuiteName.empty() ) 
	{
		if ( suite.mTests.size() == 1 )
			suite.mSuiteName = suite.mTests[0]->getName() ;
		else
			suite.mSuiteName = "test" ;
	}

	char lCSVResultPath[_MAX_PATH];
	sprintf(lCSVResultPath,
#ifdef WIN32
			"%s\\%s.csv", 
#else
			"%s/%s.csv", 
#endif
			suite.mDir.c_str(), 
			suite.mSuiteName.c_str() ) ;

	int lResult = 0;
	int lConsecutiveFailures = 0;
	for ( int i = 0 ; i < (int)suite.mTests.size() ; i++ )
	{
		ITest * lTestIt = suite.mTests[i] ;
		clock_t lTimeTaken = 0;
		TIMESTAMP lTS = 0;
		lResult = 0;
		suite.mTestIndex++;

		char const * lReason;
		unsigned int const lMinPageSize = lTestIt->excludeInLessPageSizeTestSuits(lReason);
		if (lMinPageSize >= suite.mPageSize)
			std::cout << "WARNING: " << lTestIt->getName() << " explicitly specifies that it shouldn't run at or below page size " << lMinPageSize << " (current=" << Suite().mPageSize << ")." << std::endl;

		if ( suite.mKernelErrSize )
		{
			MVTApp::sReporter.setLogSize(suite.mKernelErrSize,suite.mbSingleProcess);
			std::cout << "Log file size is set to"<<suite.mKernelErrSize<<"MB"<<std::endl;
		}

		// Note (maxw, Nov2010):
		//   We delete the store files between each test when the suite requires it,
		//   either systematically or only for those tests that perform full-scan queries.
		//   We also delete after 3 consecutive test failures, to prevent snowball effects
		//   from known failures; we let 3 tests run to highlight the severity of such
		//   snowball-effect failures, in our automated reports.
		if ( suite.mbFlushStore || ( suite.mbFlushStoreIfFullscan && lTestIt->isPerformingFullScanQueries() ) || lConsecutiveFailures >= 3 )
		{
			//{std::ofstream lOs("maxtst.txt", std::ios::app); lOs << std::endl << "***NEW***" << std::endl << std::flush;}
			lConsecutiveFailures = 0 ;
			bool bDeleteSuccess = MVTUtil::deleteStore(suite.mIOInit.c_str(),suite.mDir.c_str(),suite.mLogDir.c_str(),suite.mbArchiveLogs) ;
			if ( !bDeleteSuccess )
				std::cout << "ERROR deleting store files" << endl ;
		}

		if ( suite.mbSingleProcess )
		{
			if ( MVTApp::bVerbose )
			{
				std::cout << "Starting " << lTestIt->getName() << endl ;
				std::cout << "Description " << lTestIt->getDescription() << endl ;
			}

			lTestIt->setRandomSeed(suite.mSeed);

			for (int i=0; i<suite.mNRepeat && lResult==0; i++)
			{
				if ( MVTApp::bNoUI && !suite.mbChild )
				{
					//use try catch.  This isn't friendly for interactive debugging
					//of the test so we only do it when running in automated mode
					//and when this is not a subprocess already controlled by
					//an outer test suite process.
					try
					{
						lResult = lTestIt->performTest( pargs, (long int *) &lTimeTaken,&lTS ) ;
					}
					catch(...)
					{
						// Try to "swallow" exception so that we can report a failed test
						lResult = RC_FALSE;
						cout << "EXCEPTION Occurred" << endl << "Test status: Failed!" << endl;
					}
				}
				else
				{
					//lResult = lTestIt->performTest( suite.argc,suite.argv,&lTimeTaken,&lTS ) ;
					lResult = lTestIt->performTest(pargs,(long int *)&lTimeTaken,&lTS ) ;
				}

				if (!suite.mbSeedSet)
					lTestIt->setRandomSeed(suite.mSeed=((unsigned int)time(NULL) * 500) & 0xffff);
			}
		}
		else
		{
			// Launch separate process (to protect against exception)

			std::stringstream lParam;
			lParam << lTestIt->getName();
			lParam << " -forsmoke";

			buildCommandLine(suite,lParam);

			if ( MVTApp::bVerbose )
			{
				std::cout << "Launching " << mAppName << " " << lParam.str() << endl ;
				std::cout << lTestIt->getDescription() << endl ;
			}

			//Avoid totally freezing the machine
			bool bLowPriority = !isSingleStore() ;

			for (int i=0; i<suite.mNRepeat && lResult==0; i++)
			{
				//REVIEW: in this case lResult is an process return code, not an RC_ code
				//In both cases 0 is success, anything else failure
				lResult = MVTUtil::executeProcess(mAppName.c_str(), lParam.str().c_str(), &lTimeTaken, &lTS, bLowPriority,MVTApp::bVerbose);
				if (lResult==EXIT_CODE_KILL_SUITE)
					break;
			}
		}

		TestResult lTestResult = {lTestIt->getName(), lTestIt->getDescription(), lResult, lTimeTaken, lTS};
		suite.mTestResults.push_back(lTestResult);

		//We continue execution even beyond the first failure
		if (0 != lResult)
		{
			suite.mCntFailure++ ;
			lConsecutiveFailures++ ;
		}
		else
			lConsecutiveFailures = 0 ;

		//This special code is exception
		if (lResult==EXIT_CODE_KILL_SUITE)
			break;

		if (!suite.mbChild)
		{
			// Write incremental in-case of crash
			makeHTMLResultFile(suite,false);
			makeResultCSV( lCSVResultPath ) ;
		}
	}

	if (!suite.mbChild && suite.mTests.size() > 1)
	{
		// Final output, with popup
		// notes : set it to false, doesn't popup windows, Jan 17 2011
		makeHTMLResultFile(suite,false);

		if (lResult==EXIT_CODE_KILL_SUITE)
		{
			std::cout << "Killing entire suite after fatal test execution" << std::endl ;
			exit(EXIT_CODE_KILL_SUITE);
		}

		std::cout << "----Suite Execution complete: " << suite.mCntFailure << " Failures" << std::endl ;
	}

	return suite.mCntFailure ;
}

void MVTApp::buildCommandLine(TestSuiteCtx & suite,std::stringstream & cmdstr)
{
	// Map the suite context into command line arguments, so that
	// a child process will build the same state.
	// This must be kept up to date with any new arguments added to the test 
	// framework

	// Normally only called when launching a smoke test, but individual tests
	// may sometimes need to launch separate tests.

#if 0 // remove IPC
	if (!sDynamicLinkMvstore->isInProc())
	{
		cmdstr << " -ipc";
		if(sDynamicLinkMvstore->getServerName()) cmdstr << " -srvname=" << sDynamicLinkMvstore->getServerName();
	}
#endif	

	cmdstr << " -ident=" << suite.mIdentity;
	cmdstr << " -storeid=" << suite.mStoreID;
	cmdstr << " -pagesize=" << suite.mPageSize;
	#ifdef WIN32
		cmdstr << " -dir=\"" << suite.mDir << "\"";
	#else
		cmdstr << " -dir=" << suite.mDir;
	#endif
	if ( !suite.mLogDir.empty() )
		cmdstr << " -logdir=" << suite.mLogDir;
	cmdstr << " -nbuf=" << suite.mNBuffer;
	cmdstr << " -child" ; // Notify that it is launched from this "master" process
	cmdstr << " -seed=" << suite.mSeed;

	if ( !suite.mPwd.empty() )
		cmdstr << " -pwd=" << suite.mPwd ;

	if ( !suite.mIdentPwd.empty() )
		cmdstr << " -ipwd=" << suite.mIdentPwd ;

	if ( MVTApp::bVerbose )
		cmdstr << " -v" ;

	if ( !suite.mIOInit.empty() )
		cmdstr << " -" << suite.mIOInit ;

	if ( MVTApp::bNoUI )
		cmdstr << " -noui" ; // important to prevent single assert from stalling smoke test
	if ( suite.mbArchiveLogs )
		cmdstr << " -archivelogs" ;
	if ( suite.mbForceNew )
		cmdstr << " -forcenew" ;
	if ( suite.mbForceOpen )
		cmdstr << " -forceopen" ;
	if ( suite.mbForceClose )
		cmdstr << " -forceclose" ;
	if ( suite.mbTestDurability )
		cmdstr << " -durability" ;
	if ( suite.mbRollforward )
		cmdstr << " -rollforward" ;
	if ( suite.mbPrintStats )
		cmdstr << " -printstats" ;
	if ( suite.mbGetStoreCreationParam )
		cmdstr << " -gscp" ;
	if (sCommandCrashWithinMsAfterStartup)
		cmdstr << " -crash=" << sCommandCrashWithinMsAfterStartup ;

	if (sCommandCrashWithinMsBeforeStartup)
		cmdstr << " -crashduring=" << sCommandCrashWithinMsBeforeStartup ;
}

THREAD_SIGNATURE MVTApp::threadExecuteSuite(void * pTestExecutionThreadCtxt)
{
	TestExecutionThreadCtxt * const lCtx = (TestExecutionThreadCtxt *)pTestExecutionThreadCtxt;

	// So that open store calls will find the correct context
	lCtx->pSuite->mThreadID = getThreadId() ; 
	lCtx->pThis->executeSuite(*lCtx->pSuite);

	return 0;
}

void MVTApp::prepareMultiStoreTests()
{
	// Duplicate the first ctxt to setup other parallel 
	// executions in separate stores
	assert(mMultiStoreCxt.size() == 1) ;
	assert(sNumStores > 1);

	char lDir[2064], lIden[64], lLogDir[2064];
	char *lSlash = NULL;
	#ifdef WIN32
		lSlash = "\\";
	#else
		lSlash = "//";
	#endif

	for ( int j = 1 ; j < sNumStores ; j++ )
	{
		mMultiStoreCxt.push_back( mMultiStoreCxt[0] ) ; // Duplicate
		TestSuiteCtx & newItem = mMultiStoreCxt[j] ;

		// Override destination dir, identity, storeid etc

		sprintf(lDir,"%s%sstore%d",mMultiStoreCxt[0].mDir.c_str(),lSlash,j);
		if(!mMultiStoreCxt[0].mLogDir.empty())
			sprintf(lLogDir,"%s%sstore%d",mMultiStoreCxt[0].mLogDir.c_str(),lSlash,j);
		sprintf(lIden,"Identity%d",j);

		newItem.mDir = lDir ;
		if(!mMultiStoreCxt[0].mLogDir.empty())
			newItem.mLogDir = lLogDir ;
		newItem.mIdentity = lIden ;
		newItem.mStoreID = mMultiStoreCxt[0].mStoreID + j;
		newItem.mSeed = mMultiStoreCxt[0].mSeed + j;

		// TODO: For s3io support the destination would also need to be fixed

		if(bRandBuffer) 
		{
			int baseBuffers = mMultiStoreCxt[0].mNBuffer ;
			int lNumBuffers ;
			int lOffset = randInRange(0, 10) * 100;
			if(randBool())
				lNumBuffers = baseBuffers + lOffset;
			else
				lNumBuffers = baseBuffers - lOffset;
			newItem.mNBuffer = lNumBuffers<100?baseBuffers:lNumBuffers;
		}

		// Jumble the ordering to get results similar to bashtest
		if (newItem.mTests.size()>1 && bRandomTests)
		{
			srand((((unsigned int)time(NULL) * 500) & 0xffff)+j);
			std::random_shuffle(newItem.mTests.begin(), newItem.mTests.end());
		}
		
		MVTUtil::ensureDir(lDir);
		if(!mMultiStoreCxt[0].mLogDir.empty())
			MVTUtil::ensureDir(lLogDir);
		//Flush any existing store files
		//MVTUtil::deleteStoreFiles(lDir) ;
	}
}


THREAD_SIGNATURE MVTApp::threadCrash(void * pThreadCrashCtxt)
{
	ThreadCrashCtxt * const lCtx = (ThreadCrashCtxt *)pThreadCrashCtxt;	
	MVTestsPortability::threadSleep(lCtx->mCrashTime);
	bool lContinue = true;
	if(lCtx->mSingleProcess && lCtx->mTestIndex != 0 && MVTApp::getCurrentTestIndex(lCtx->mDirectory) != lCtx->mTestIndex)
		lContinue = false;
	if(lContinue)
	{
		std::cout << "Crashing as requested...  " 
		#ifdef WIN32
			<< "see crashstack.txt" 
		#endif
			<< std::endl;
	//	HANDLE lh = OpenEvent(EVENT_ALL_ACCESS, FALSE, "dontcrash");
	//	if (WAIT_OBJECT_0 != WaitForSingleObject(lh, 0))
			throw 1; //"By all means, please crash!"; // Throwing number gives slightly clearer callstack
	//	else
	//		printf("skipped the requested crash, thanks to event dontcrash\n");
	}
	return 0;
}

void MVTApp::makeMultiStoreReport(bool bPopResults)
{
	if (!isSingleStore()) return;

	TIMESTAMP lReportCreationTS ; getTimestamp(lReportCreationTS);
	std::string lCmplTime = "unknown";
	char lCompl[32];
#ifdef WIN32
	SYSTEMTIME lNow ;
	GetLocalTime(&lNow);
	sprintf(lCompl,"%02u/%02u/%04u %02u:%02u:%02u",
		lNow.wMonth,lNow.wDay,lNow.wYear,lNow.wHour,lNow.wMinute,lNow.wSecond);		
	lCmplTime = lCompl;
#else
	time_t lTimeNow; tm lNow ;
	time(&lTimeNow);
	if ( NULL != localtime_r(&lTimeNow, &lNow) )
	{
		sprintf(lCompl,"%02u/%02u/%04u %02u:%02u:%02u",
			lNow.tm_mon+1,lNow.tm_mday,lNow.tm_year+1900,lNow.tm_hour,lNow.tm_min,lNow.tm_sec);		
		lCmplTime = lCompl;
	}
#endif

	char lResultPath[1024];

	#ifdef WIN32
		sprintf(lResultPath, "%s\\%s_ms.html", mMultiStoreCxt[0].mDir.c_str(), mMultiStoreCxt[0].mSuiteName.c_str()); 
	#else
		sprintf(lResultPath,"%s/%s_ms.html", mMultiStoreCxt[0].mDir.c_str(), mMultiStoreCxt[0].mSuiteName.c_str()); 
	#endif

	ofstream lFile;lFile.open(lResultPath,ios::out);
	size_t i;

	std::stringstream lHTML, lSummary, lTable, lResults;
	std::vector<Tstring> lStoreResults;

	lTable << "\n<table align=center font=Tahoma BORDER=2 CELLPADDING=3 CELLSPACING=1><caption><b><u><font size=2>MultiStore Tests Summary</u></b></caption>";
	lTable << "<tr font size=2><th align=center><font size=2>Identity</th><th align=center><font size=2>Directory</th><th align=center><font size=2>Seed</th><th align=center><font size=2>Total time taken</th>";
	lTable << "<th align=center><font size=2>Number of tests run</th><th align=center><font size=2>Number of tests passed</th><th align=center><font size=2>Number of test failures </th></tr>";
	for(i = 0; i < mMultiStoreCxt.size(); i++)
	{
		lTable << "<tr>";
		TestSuiteCtx &lSuite = mMultiStoreCxt[i];
		std::vector<TestResult> & lTResult = lSuite.mTestResults;

		Tstring lFailedTests = "";
		std::stringstream lStoreTable, lStoreResult;
		int lNumTests = (int)lTResult.size();		
		
		lStoreTable << "<td><font size=2>" << lSuite.mIdentity << "</td>";
		lStoreTable << "<td><font size=2>" << lSuite.mDir << "</td>";
		lStoreTable << "<td align=center><font size=2>" << lSuite.mSeed << "</td>";
		
		lStoreResult << "<br><br><table align=center font=Tahoma BORDER=2 CELLPADDING=3 CELLSPACING=1><caption><b><u><font size=2>" << lSuite.mIdentity << " Test Results</u></b></caption>";
		lStoreResult << "<tr bgcolor=""SILVER""><th align=""center""><font size=2>Name</th><th align=""center"">Description</th><th align=""center""><font size=2>Status</th><th align=""center""><font size=2>Time in ms ";
		lStoreResult << "</th><th align=""center""><font size=2>Time</th></tr>\n";
	
		clock_t lTotalTime = 0;
		int lNumFailedTests = 0;
		int j = 0;
		for(j = 0; j < lNumTests; j++)
		{
			char lExecTime[32];
			sprintf(lExecTime,"%ld",lTResult[j].mExecTime);	
			
			Tstring lDateTime = "";			
			durationToString(lTResult[j].mExecTime,lDateTime,true /*human friendly*/);			
		
			lTotalTime+=lTResult[j].mExecTime;
			if(lTResult[j].mResult) 
			{
				lStoreResult << "<tr bgcolor=""DARKSALMON"">";
				if(!lFailedTests.empty()) lFailedTests.append(",");
				lFailedTests += lTResult[j].mTestName;				
				lNumFailedTests++;
			}
			else
				lStoreResult << "</tr>" ;

			lStoreResult << "<td><font size=2>" << lTResult[j].mTestName << "</td>" ;
			lStoreResult << "<td><font size=2>" << lTResult[j].mTestDesc << "</td>" ;
			lStoreResult << "<td><font size=2>" << (lTResult[j].mResult?"<b>Fail</b>":"Pass") << "</td>" ;
			lStoreResult << "<td align=\"right\"><font size=2>" << lExecTime << "</td>" ;
			lStoreResult << "<td><font size=2>" << lDateTime << "</td>" ;
			lStoreResult << "</tr>\n" ;
		}
		lStoreResult << "</table>";
		lStoreResults.push_back(lStoreResult.str());

		Tstring lDuration = "";			
		durationToString(lTotalTime, lDuration,true /*human friendly*/);
		
		lStoreTable << "<td align=center><font size=2>" << lDuration.c_str() << "</td>";
		lStoreTable << "<td align=center><font size=2>" << lSuite.mTestResults.size() << "</td>";		
		lStoreTable << "<td align=center><font size=2>" << (lNumTests - lNumFailedTests) << "</td>";
		lStoreTable << "<td align=center><font size=2>" << lNumFailedTests;
		if(lNumFailedTests)
			lStoreTable << " <font size=2 face=Times color=#990000> [" << lFailedTests.c_str() << "]";
		lStoreTable << "</td>";

		lTable << lStoreTable.str() << "</tr>";
	}		
	lTable << "</table>";	
	
	for(i = 0; i < lStoreResults.size(); i++)
		lResults << lStoreResults[i];

	lSummary << "<font size=""2"" face=""Tahoma""> <u><b>Test Summary:</b></u> <ul>";

	// System Details
	char *lPlatform;
	#ifdef WIN32
		lPlatform = "Win32";
		char *lMachineName;
		lMachineName = getenv("COMPUTERNAME");
	#else
		lPlatform = "Linux";
		char lMachineNameStr[32];
		#include <unistd.h>
		gethostname(lMachineNameStr, 32);
		char *lMachineName = lMachineNameStr;
	#endif

	#if !defined(NDEBUG)
		char *lConfig = "debug";
	#else
		char *lConfig = "release";
	#endif

	lSummary << "<li> System details: " << lPlatform << ", " << lConfig 
		<< ", " << (lMachineName?lMachineName:"") << "</li>\n" ;

	lSummary << "<li> Test details: <ul>" ;
	lSummary << "<li>Stores: " << mMultiStoreCxt.size() << "</li>" ;
	lSummary << "<li>NBUFFERS: " << mMultiStoreCxt[0].mNBuffer << "</li>" ;
	lSummary << "<li>PAGESIZE:" << mMultiStoreCxt[0].mPageSize << "</li>" ;	
	lSummary << "<li>STORE_IFACE_VER: 0x" << hex << STORE_IFACE_VER <<dec<< "</li>" ;
	lSummary << "</ul></li>\n";

	double dTotal = (( lReportCreationTS - mMultiStoreCxt[0].mStartTime ) / 1000000. ) ;

	Tstring lDuration = "";			
	durationToString(clock_t(dTotal*1000),lDuration,true /*human friendly*/);


	lSummary << "<li> Completed at " << lCmplTime 
		<< "<font color=""RED""><li> Total time taken: " 
		<< lDuration.c_str()
		<< " ("
		<< dTotal
		<< " seconds)</li></font>";

	lHTML << "<html><head><title>Afy Test Results</title></head><body>"
		<< lSummary.str() 
		<< lTable.str() 
		<< lResults.str()
		<< "</body></html>";

	lFile << lHTML.str().c_str();
	lFile.close();
	#ifdef WIN32		
	if ( bPopResults )
		ShellExecute(NULL, NULL, lResultPath , NULL, NULL, SW_SHOWNORMAL);
	#endif
}

void MVTApp::makeHTMLResultFile(TestSuiteCtx &suite, bool inbPopResults /*whether to open in browser*/)
{
	if (suite.mTestResults.size() == 0) return;

	TIMESTAMP lReportCreationTS ; getTimestamp(lReportCreationTS);
	std::string lCmplTime = "unknown";
	char lCompl[32];
#ifdef WIN32
	SYSTEMTIME lNow ;
	GetLocalTime(&lNow);
	sprintf(lCompl,"%02u/%02u/%04u %02u:%02u:%02u",
		lNow.wMonth,lNow.wDay,lNow.wYear,lNow.wHour,lNow.wMinute,lNow.wSecond);		
	lCmplTime = lCompl;
#else
	time_t lTimeNow; tm lNow ;
	time(&lTimeNow);
	if ( NULL != localtime_r(&lTimeNow, &lNow) )
	{
		sprintf(lCompl,"%02u/%02u/%04u %02u:%02u:%02u",
			lNow.tm_mon+1,lNow.tm_mday,lNow.tm_year+1900,lNow.tm_hour,lNow.tm_min,lNow.tm_sec);		
		lCmplTime = lCompl;
	}
#endif

	char lResultPath[1024];

	std::vector<TestResult> & lTResult = suite.mTestResults;

	#ifdef WIN32
	sprintf(lResultPath, "%s\\%s.html", suite.mDir.c_str(), suite.mSuiteName.c_str()); 
	#else
	sprintf(lResultPath,"%s/%s.html", suite.mDir.c_str(), suite.mSuiteName.c_str()); 
	#endif

	ofstream lFile;lFile.open(lResultPath,ios::out);
	size_t i;

	std::stringstream lHTML, lSummary,lTable ;
	std::vector<Tstring> lFailedTests;

	lTable << "\n<table border=""3"" cellpadding=""5"" font=""Times New Roman""><caption><b><u>Afy Test Results</u></b></caption>";
	lTable << "<tr bgcolor=""SILVER""><th>No.</th><th align=""center"">Name</th><th align=""center"">Description</th><th align=""center"">Status</th><th align=""center"">Time in ms ";
	lTable << "</th><th align=""center"">Time</th></tr>\n";
	for(i = 0; i < lTResult.size(); i++)
	{
		char lExecTime[32];
		sprintf(lExecTime,"%ld",lTResult[i].mExecTime);			

		Tstring lDateTime = "";			
		durationToString(lTResult[i].mExecTime,lDateTime,true /*human friendly*/);

		if(lTResult[i].mResult) 
		{
			// Failure
			lTable << "<tr bgcolor=""DARKSALMON"">";
			lFailedTests.push_back(lTResult[i].mTestName);
		}
		else
			lTable << "<tr>";

		lTable << "<td>" << ((int)i+1) << "</td>" ; // Test index
		lTable << "<td>" << lTResult[i].mTestName << "</td>" ;
		lTable << "<td>" << lTResult[i].mTestDesc << "</td>" ;
		lTable << "<td>" << (lTResult[i].mResult?"<b>Fail</b>":"Pass") << "</td>" ;
		lTable << "<td align=\"right\">" << lExecTime << "</td>" ;
		lTable << "<td>" << lDateTime << "</td>" ;
		lTable << "</tr>\n" ;
	}
	lTable << "</table>";

	int lNumTests = (int)lTResult.size();
	int lNumFailedTests = (int) lFailedTests.size();

	lSummary << "<font size=""2"" face=""Tahoma""> <u><b>Test Summary:</b></u> <ul>";

	// System Details
	char *lPlatform;
	#ifdef WIN32
		lPlatform = "Win32";
		char *lMachineName;
		lMachineName = getenv("COMPUTERNAME");
	#else
		lPlatform = "Linux";
		char lMachineNameStr[32];
		#include <unistd.h>
		gethostname(lMachineNameStr, 32);
		char *lMachineName = lMachineNameStr;
	#endif

	#if !defined(NDEBUG)
		char *lConfig = "debug";
	#else
		char *lConfig = "release";
	#endif

	lSummary << "<li> System details: " << lPlatform << ", " << lConfig 
		<< ", " << (lMachineName?lMachineName:"") << "</li>\n" ;

	lSummary << "<li> Store details: <ul>" ;
	lSummary << "<li>Directory: " << suite.mDir << "</li>" ;
	lSummary << "<li>Identity: " << suite.mIdentity << "</li>" ;
	lSummary << "<li>NBUFFERS: " << suite.mNBuffer << "</li>" ;
	lSummary << "<li>PAGESIZE:" << suite.mPageSize << "</li>" ;
	lSummary << "<li>SEED:" << suite.mSeed << "</li>" ;
	lSummary << "<li>STORE_IFACE_VER: 0x" <<hex<< STORE_IFACE_VER <<dec<< "</li>" ;
	lSummary << "</ul></li>\n";

	double dTotal = (( lReportCreationTS - suite.mStartTime ) / 1000000. ) ;

	Tstring lDuration = "";			
	durationToString(clock_t(dTotal*1000),lDuration,true /*human friendly*/);


	lSummary << "<li> Completed at " << lCmplTime 
		<< "<font color=""RED""><li> Total time taken: " 
		<< lDuration.c_str()
		<< " ("
		<< dTotal
		<< " seconds)</li></font>" 
		<< "<b><li> Number of tests run: " << lNumTests << " </li></b>" ;

	lSummary << "<li> Number of tests passed: " << (lNumTests - lNumFailedTests) << " </li>" 
		<< "<b><li> Number of tests failed: " << lNumFailedTests << "</b>" ;
	
	for(i = 0 ; i < lFailedTests.size(); i++)
	{
		if(i == 0) 
			lSummary << "<font size=""3"" face=""Times"" color=""#990000""> [";

		lSummary << lFailedTests[i].c_str();

		if(i == lFailedTests.size() - 1) 
			lSummary << "] </font>" ;
		else 
			lSummary << ", ";
	}
	lSummary << "</ul></font>";

	lHTML << "<html><head><title>Afy Test Results</title></head><body>"
		<< lSummary.str() 
		<< lTable.str() 
		<< "</body></html>";

	lFile << lHTML.str().c_str();
	lFile.close();
	#ifdef WIN32		
	if ( inbPopResults && !isSingleStore())
		ShellExecute(NULL, NULL, lResultPath , NULL, NULL, SW_SHOWNORMAL);
	#endif
}

void MVTApp::makeResultCSV( const char * inFilepath )
{
	TestSuiteCtx & suite = Suite() ;

	FILE * f = fopen( inFilepath, "w" ) ;
		
	fprintf( f, "RunId, TestName, StartTime, EndTime, Elapsed, Comment\n" ) ;

	for(size_t i = 0; i < suite.mTestResults.size(); i++)
	{
		TestResult & lResult = suite.mTestResults[i] ;

		fprintf( f, "%s,%s,%d,%ld,%ld,%s\n", 
			"1", // RunID - not used at the moment
			lResult.mTestName, 
			0, 
			lResult.mExecTime, 
			lResult.mExecTime, 
			lResult.mResult ? "Fail" : "Pass" ) ;			
	}

	fclose( f ) ;
}

class PushSession
{
	protected:
		ISession & mBw, & mFw;
	public:
		PushSession(ISession & pBw, ISession & pFw) : mBw(pBw), mFw(pFw) { mBw.detachFromCurrentThread(); mFw.attachToCurrentThread(); }
		~PushSession() { mFw.detachFromCurrentThread(); mBw.attachToCurrentThread(); }
};

#if 0
class ReplicationEventProcessor
{
	public:
		static ReplicationEventProcessor * sTheReplicationEventProcessor;
		static void * sTheOrgToken;
		static void * sTheDstToken;
	protected:
		IReplicationController * mOrg, * mDst;
		Afy::IAffinity * mStoreCtxReplica;
	protected:
		HTHREAD mThread;
		long volatile mFinishing;
	public:
		ReplicationEventProcessor()
			: mOrg(NULL), mDst(NULL), mStoreCtxReplica(NULL), mFinishing(0)
		{
			createThread(&ReplicationEventProcessor::threadDeliverCIDs, this, mThread);
		}
		~ReplicationEventProcessor()
		{
			INTERLOCKEDI(&mFinishing);
			#ifdef WIN32
				::WaitForSingleObject(mThread, INFINITE);
				::CloseHandle(mThread);
			#else
				int const lErr = ::pthread_join(mThread, NULL);
				if (0 != lErr)
					fprintf(stderr, "pthread_join failed with code %d", lErr);
			#endif
		}
		void setOrg(IReplicationController * pOrg) { mOrg = pOrg; }
		void setDst(IReplicationController * pDst) { mDst = pDst; }
		void setStoreCtxReplica(Afy::IAffinity * pStoreCtxReplica) { mStoreCtxReplica = pStoreCtxReplica; }
		virtual bool sendEvents(ReplicationID const *pRIDs,size_t pNumRIDs)
		{
			if (!mOrg || !mDst)
				{ assert(false); return false; }
			bool lRet = false;
			for(size_t idx = 0 ; idx < pNumRIDs;idx++)
			{
				ISession * lSessionOrg = NULL; // MVTApp::getSession();
				bool lNewSession = false;
				if (!lSessionOrg)
					{ lSessionOrg = MVTApp::startSession(0, 0, 0, false); lNewSession = true; }
				if (!lSessionOrg)
					{ assert(false); return false; }
				unsigned char * lData;
				TSerVersion txver =0;/*review what shld be passed*/
				TSerVersion opver =0;/*review what shld be passed*/
				size_t lDataSize = mOrg->getContentData(*lSessionOrg, pRIDs[idx], txver,opver/*review*/,&lData);
				size_t lDataStart = 0;
				{
					// Note: getContentData now returns a header in front of each CID...
					unsigned long lBlobSize;
					ReplicationID lRID;
					std::istringstream lIs((char *)lData);
					lIs >> std::hex >> lRID;
					lIs >> std::dec >> lBlobSize; lIs.get();
					assert(lRID == pRIDs[idx]);
					lDataStart = (size_t)lIs.tellg();
				}
				lDataSize -= lDataStart;
				unsigned char * lDataCpy = new unsigned char[lDataSize];
				memcpy(lDataCpy, lData + lDataStart, lDataSize);
				lSessionOrg->free(lData);
				if (lNewSession)
					lSessionOrg->terminate();
				else
					lSessionOrg->detachFromCurrentThread();

				ISession * lSessionDst = MVTApp::startSession(mStoreCtxReplica, 0, 0, false);
				if (lSessionDst)
				{
					lSessionDst->setInterfaceMode(lSessionDst->getInterfaceMode() | ITF_REPLICATION);
					lRet = mDst->receiveEvent(*lSessionDst, pRIDs[idx], lDataCpy, lDataSize);
					lSessionDst->terminate();
				}
				else
					assert(false);

				delete [] lDataCpy;
				if (!lNewSession)
					lSessionOrg->attachToCurrentThread();
			}
			return lRet;
		}
		virtual void release() { delete this; }
		bool waitForCompletion(TestLogger & pLogger)
		{
			if (!mOrg || !mDst)
				{ assert(false); return false; }
			
			ISession * const lOriginalS = MVTApp::startSession(MVTApp::Suite().mStoreCtx);
			lOriginalS->detachFromCurrentThread();
			ISession * const lReplicaS = MVTApp::startSession(MVTApp::sReplicaStoreCtx);
			lReplicaS->detachFromCurrentThread();
			lOriginalS->attachToCurrentThread();

			bool lCompleted = false;
			int lAttempts = 0;
			ReplicationID lAttemptsAt = 0, lLastSent = 0, lLastReceived = 0;
			TVersionVector lVVOriginal, lVVReplica;
			do
			{
				lLastSent = mOrg->getLastSent(*lOriginalS);
				unsigned int const lStoreID = lOriginalS->getLocalStoreID();

				PushSession const lCtx(*lOriginalS, *lReplicaS);
				lLastReceived = mDst->getLastReceived(*lReplicaS, lStoreID);

				if (lLastSent > lLastReceived)
				{
					if (lLastReceived > lAttemptsAt)
						{ lAttemptsAt = lLastReceived; lAttempts = 0; }
					else
						lAttempts++;
					MVTestsPortability::threadSleep(500);
				}
				else
					lCompleted = true;
			} while (!lCompleted && lAttempts < 10);

			if (!lCompleted)
				pLogger.out() << "WARNING: last received CID (" << std::hex << lLastReceived << ") never caught up with last sent CID (" << lLastSent << ")!" << std::endl;

			lOriginalS->terminate();
			lReplicaS->attachToCurrentThread();
			lReplicaS->terminate();
			return lCompleted;
		}
		virtual RET_CODE getSource(const char *, std::string &) {return 0l;}
		virtual RET_CODE getSerializedRemoteValue(ISession *, const char *, Afy::PID const &, const char *, Afy::ElementID, Afy::IStream **) {return 0;}
	protected:
		static THREAD_SIGNATURE threadDeliverCIDs(void * pThis) { ((ReplicationEventProcessor *)pThis)->threadDeliverCIDsImpl(); return 0; }
		void threadDeliverCIDsImpl()
		{
			ISession * const lOriginalS = MVTApp::startSession(MVTApp::Suite().mStoreCtx);
			lOriginalS->detachFromCurrentThread();
			ISession * const lReplicaS = MVTApp::startSession(MVTApp::sReplicaStoreCtx);
			lReplicaS->detachFromCurrentThread();

			while (!mFinishing)
			{
				ReplicationID lLastSent = 0, lLastReceived = 0;
				unsigned int lStoreID = 0;
				{
					lOriginalS->attachToCurrentThread();
					lLastSent = mOrg->getLastSent(*lOriginalS);
					lStoreID = lOriginalS->getLocalStoreID();
					lOriginalS->detachFromCurrentThread();
				}
				{
					lReplicaS->attachToCurrentThread();
					lLastReceived = mDst->getLastReceived(*lReplicaS, lStoreID);
					lReplicaS->detachFromCurrentThread();
				}

				ReplicationID iCID;
				for (iCID = lLastReceived + 1; iCID <= lLastSent; iCID++)
					sendEvents(&iCID, 1);
				MVTestsPortability::threadSleep(500);
			}
		}
};
ReplicationEventProcessor * ReplicationEventProcessor::sTheReplicationEventProcessor = NULL;
void * ReplicationEventProcessor::sTheOrgToken = NULL;
void * ReplicationEventProcessor::sTheDstToken = NULL;
#endif

// #define NBUFFERS 2000 : Not used anymore. Check 'sDefaultNumberBuffers'
#define	NTHREADS 20
#define	NOPS 1000
#define	NCTLFILES 0
#define	NLOGFILES 1
#define	PAGESIZE 0x8000
#define	PAGESPEREXTENT 0x200

#ifndef STARTUP_DELETE_LOG_ON_SHUTDWON
	#define STARTUP_DELETE_LOG_ON_SHUTDWON 0
#endif

MVTApp::TestSuiteCtx & MVTApp::Suite( const char * inDir ) 
{ 
	if ( mMultiStoreCxt.empty() )
	{
		assert(!"Test suite not initialized");
		return mMultiStoreCxt[666] ; // Crash time
	}
	else if ( mMultiStoreCxt.size()==1 )
	{
		return mMultiStoreCxt[0]; 
	}
	else
	{
		// Attempt a match by thread id or directory
		// As any test can launch its own threads this may
		// not success - it would probably be best for each
		// ITest object to point back to its the Suite object
		// it was launched from

		THREADID lThreadID = getThreadId();
		for(int i = 0; i < (int)mMultiStoreCxt.size(); i++)
		{
			if(inDir!=NULL && 0 == strcmp(inDir,mMultiStoreCxt[i].mDir.c_str()))
			{
				return mMultiStoreCxt[i] ;
			}

			if(mMultiStoreCxt[i].mThreadID == lThreadID)
			{
				return mMultiStoreCxt[i] ;
			}
		}

		// Test may have launched multiple threads, 
		// in which case they shouldn't really use this feature.
		// Revert to first
		return mMultiStoreCxt[0];
	}
}

void MVTApp::printStoreCreationParam()
{
	//RC rc = (RC)sDynamicLinkMvstore->getStoreCreationParameters(mSCP,mStoreCtx);
	RC rc = getStoreCreationParameters(mSCP,mStoreCtx);
	if ( RC_OK == rc )
	{
		std::cout<<"Startup Parameters..\n";
		std::cout<<"Records:"<<mSCP.nControlRecords<<std::endl;
		std::cout<<"Page Size:"<<mSCP.pageSize<<std::endl;
		std::cout<<"FileExtentSize:"<<mSCP.fileExtentSize<<std::endl;
		std::cout<<"StoreID:"<<mSCP.storeId<<std::endl;
		std::cout<<"MaxSize:"<<mSCP.maxSize<<std::endl;	
	}
	else if( RC_QUOTA == rc )
	{
		std::cout << "store allocation quota exceeded\n";
	}
	else
	{
		std::cout << "Problem in getting store creation parameters" << std::endl;
	}
}
bool MVTApp::startStore(
	IService * pNetCallback, 
	IStoreNotification * pNotifier, 
	// Following args are almost never needed, 
	// because they can be set from the cmd line args, e.g. Suite() struct
	// They are used for specialized tests
	const char *pDirectory,  
	const char *pIdentity, 
	const char *pPassword,
	unsigned numbuffers, 
	unsigned short pStoreID,
	unsigned int pPageSize,
	const char *pIOStrOverride)
{
	// Check if this store is already open

	// If multi-store tests running there can be multiple contexts to figure out
	// TODO: test should know its own context
	
	TestSuiteCtx & suite = Suite( pDirectory ) ;

	suite.mLock->lock() ;
	if (suite.mStarted > 0)
	{
		// REVIEW: To try to force tests to run concurrently in a single
		// store we need to fake any calls to open store to no-ops.

		// Perhaps this should be handled at a higher level, e.g. remove
		// the requirement for normal tests to open their own stores,
		// so this funciton can be simplified

		assert(suite.mStoreCtx!=NULL);
		
		// Test problem if they want to set new notifications
		if ( pNetCallback != NULL || pNotifier != NULL )
		{
			cout << "Cannot set notifier callbacks - store already open" << endl ;
		}

		if (pDirectory && (0!=strcmp(pDirectory,suite.mDir.c_str())))
		{
			assert(!"pDir not supported for concurrent tests");
			cout << "Different is already open for this test.  Call openStore directly" << endl ;
		}

		suite.mLock->unlock() ;
		return (!pNetCallback && !pNotifier);
	}

	if (sCommandCrashWithinMsBeforeStartup)
	{
		std::cout << "... scheduled a crash at startup:" << sCommandCrashWithinMsBeforeStartup << " ..." << std::endl;
		HTHREAD lThreadCrash;
		ThreadCrashCtxt *lCtx = new ThreadCrashCtxt(suite.mDir.c_str(), suite.mbSingleProcess, suite.mTestIndex, sCommandCrashWithinMsBeforeStartup);
		createThread(&MVTApp::threadCrash, lCtx, lThreadCrash);
	}

	#if 0 // Review: This has been broken...
		if (bReplicate)
			MVTApp::deleteStore();
	#endif

	const char * lDirectory = pDirectory?pDirectory:suite.mDir.c_str() ;
	const char * lLogDirectory = suite.mLogDir.empty()?lDirectory:suite.mLogDir.c_str();
	const char * lPassword = pPassword?pPassword:suite.mPwd.c_str(); /* We don't set password by default to "testpwd" anymore */
	const char * lIdentity = pIdentity?pIdentity:suite.mIdentity.c_str();
	if(suite.mPwd.empty() && pPassword) suite.mPwd = pPassword;
	if(suite.mIdentity.empty() && lIdentity) suite.mIdentity = lIdentity; 
	const char * lAdditionalParams=pIOStrOverride;
	if ( lAdditionalParams == NULL && !suite.mIOInit.empty() )
		lAdditionalParams = suite.mIOInit.c_str();

	const int lNumBuffers = numbuffers?numbuffers:suite.mNBuffer;
	unsigned int mode = 0;
	if ( suite.mbArchiveLogs )
		mode |= STARTUP_ARCHIVE_LOGS ;
	if ( suite.mbForceNew )
		mode |= STARTUP_FORCE_NEW ;
	if ( suite.mbRollforward )
		mode |= STARTUP_ROLLFORWARD ;
	if ( suite.mbForceOpen )
		mode |= STARTUP_FORCE_OPEN ;
	if ( suite.mbPrintStats )
		mode |= STARTUP_PRINT_STATS ;


	StartupParameters const lSP(mode, lDirectory, DEFAULT_MAX_FILES, lNumBuffers, 
		DEFAULT_ASYNC_TIMEOUT, pNetCallback, pNotifier,lPassword,lLogDirectory);

	const unsigned short lStoreID = pIdentity?pStoreID:suite.mStoreID ;
	const unsigned int lPageSize = suite.mPageSize ;
	StoreCreationParameters lSCP(NCTLFILES, lPageSize, PAGESPEREXTENT, lIdentity, lStoreID, lPassword, false); 
	lSCP.fEncrypted = ((lPassword != NULL) && (strlen(lPassword)>0));

	Afy::IAffinity *& lStoreCtx = suite.mStoreCtx;
	mSCP = lSCP; mStoreCtx = lStoreCtx;
	RC rc = RC_NOACCESS;
	while (rc==RC_NOACCESS)
	{
		//if (RC_OK != ( rc = (RC)sDynamicLinkMvstore->openStore(lSP, lStoreCtx, lAdditionalParams, lSCP.storeId)))
		if (RC_OK != ( rc = openStore(lSP, lStoreCtx)))
		{
			suite.mStoreCtx = NULL ; // REVIEW: store returns pointer to deleted obj
			if (rc == RC_NOACCESS )
			{
				std::cout << "No access error opening store" << std::endl;
				suite.mLock->unlock() ;
				MVTestsPortability::threadSleep(1000);
			//	return false;
			}
			else if (rc == RC_OTHER )
			{
				// Avoid trying to create store if i/o stack creation didn't work
				std::cout << "Cannot open store (perhaps IO related)" << std::endl;
				suite.mLock->unlock() ;
				return false;
			}
			//else if (RC_OK != sDynamicLinkMvstore->createStore(lSCP, lSP, lStoreCtx,0, lAdditionalParams ))
			else if (RC_OK != createStore(lSCP, lSP, lStoreCtx,0))
			{
				// Normal clients might not automatically create a new store when it fails to open existing store
				// But for tests it is often convenient to run either from an existing store or with a new store
				suite.mStoreCtx = NULL ; 
				std::cout << "Could not create store!" << std::endl;
				suite.mLock->unlock() ;
				return false;
			}
			else
			{
				std::cout << "Created new store. Identity: " << lIdentity << " PageSize: " << lPageSize << " NBUFFERS: " << lNumBuffers << " StoreID: " << lStoreID << " Password: " << lPassword << " Location: " << (lDirectory?lDirectory:"unknown") << endl ;
				if ( !suite.mIdentPwd.empty() ) 
				{
					// Temporary session to establish to password for new store
					//ISession * setPwdSession=sDynamicLinkMvstore->startSession(suite.mStoreCtx,suite.mIdentity.c_str(),NULL);
					ISession * setPwdSession=suite.mStoreCtx->startSession(suite.mIdentity.c_str(),NULL);
					if ( setPwdSession ) {
						rc = setPwdSession->changePassword(STORE_OWNER,NULL/*no previous pwd*/,suite.mIdentPwd.c_str());
						setPwdSession->terminate();
						if ( rc != RC_OK ) { std::cout << "Failed to set identity password" << endl; }
						else { std::cout << "Set identity password " << suite.mIdentPwd << endl; }
					}
				}
			}
		}
		else
		{
			std::cout << "Opened existing store. Owner: " << lIdentity << " NBUFFERS: " << lNumBuffers << endl ;
		}
	}

#if 0	
	if (bReplicate && !sReplicaStoreCtx)
	{
		// Review: No way to destroy the repControllers!?!

		// Create replica directory, if not already there.
		std::string lReplicaDirectory = lDirectory;
		#ifdef WIN32
			lReplicaDirectory += "\\replica";
			if (GetFileAttributes( lReplicaDirectory.c_str() ) == INVALID_FILE_ATTRIBUTES)
			{
				string lCmd = "/C mkdir ";
				lCmd += lReplicaDirectory;
				MVTUtil::executeProcess("cmd.exe", lCmd.c_str());
			}
		#else
			lReplicaDirectory += "/replica";
			string lCmd = "bash -c \"mkdir ";
			lCmd += lReplicaDirectory;
			lCmd += "\"";
			system(lCmd.c_str());
		#endif

		// Delete the store that is there, if relevant
		// (we always want to diff test by test with this mode).
		MVTUtil::deleteStoreFiles(lReplicaDirectory.c_str());

		// Create replica store.		
		StartupParameters const lSP(0, lReplicaDirectory.c_str(), DEFAULT_MAX_FILES, suite.mNBuffer, DEFAULT_ASYNC_TIMEOUT);
		StoreCreationParameters const lSCP(NCTLFILES, suite.mPageSize, PAGESPEREXTENT, "replica", suite.mStoreID + 0x1000); 
		if (RC_OK != sDynamicLinkMvstore->openStore(lSP, sReplicaStoreCtx, NULL, lSCP.storeId) && 
			RC_OK != sDynamicLinkMvstore->createStore(lSCP, lSP, sReplicaStoreCtx))
				printf("Couldn't open replica store!\n");
		else
			std::cout << "Opened Replica store " << lReplicaDirectory.c_str() << std::endl;

		// Register sendEvent callback for the local store.
		ReplicationEventProcessor::sTheReplicationEventProcessor = new ReplicationEventProcessor;
		IReplicationNotificationHandler * lReplicationHandlerI1 = NULL;
		ReplicationEventProcessor::sTheOrgToken = sDynamicLinkMvstore->repCreateStoreNotificationHandler(lStoreCtx, lIdentity, lDirectory, lStoreID, lReplicationHandlerI1);
		IReplicationController * const lController1 = sDynamicLinkMvstore->repCreateController(ReplicationEventProcessor::sTheOrgToken);
		ReplicationEventProcessor::sTheReplicationEventProcessor->setOrg(lController1);

		// Create controller for replica store, and connect everything together.
		IReplicationNotificationHandler * lReplicationHandlerI2 = NULL;
		ReplicationEventProcessor::sTheDstToken = sDynamicLinkMvstore->repCreateStoreNotificationHandler(sReplicaStoreCtx, "replica", lReplicaDirectory.c_str(), suite.mStoreID + 0x1000, lReplicationHandlerI2);
		IReplicationController * lController2 = sDynamicLinkMvstore->repCreateController(ReplicationEventProcessor::sTheDstToken);
		ReplicationEventProcessor::sTheReplicationEventProcessor->setDst(lController2);
		ReplicationEventProcessor::sTheReplicationEventProcessor->setStoreCtxReplica(sReplicaStoreCtx);
	}
#endif

	if (sCommandCrashWithinMsAfterStartup)
	{
		std::cout << "... scheduled a crash:" << sCommandCrashWithinMsAfterStartup << " ..." << std::endl;
		HTHREAD lThreadCrash;
		ThreadCrashCtxt *lCtx = new ThreadCrashCtxt(suite.mDir.c_str(), suite.mbSingleProcess, suite.mTestIndex, sCommandCrashWithinMsAfterStartup);
		createThread(&MVTApp::threadCrash, lCtx, lThreadCrash);
	}

	suite.mStarted++;
	assert(suite.mStarted==1);
	assert(suite.mStoreCtx!=NULL);

	suite.mLock->unlock() ;
	return true;
}

RC MVTApp::createStoreWithDumpSession(ISession *& outSession, IService * pNetCallback, IStoreNotification * pNotifier)
{
	// Special variation of MVTApp::startStore for tests that run with a simulate a store dumpload
	// Warning: some code duplication with startStore - should it just be a special mode?

	outSession = NULL;

	TestSuiteCtx & suite = Suite() ;

	suite.mLock->lock() ;
	if (suite.mStarted > 0)
	{
		return RC_FALSE; // Not supporting multi-store attack on this
	}

	const char * lDirectory = suite.mDir.c_str() ;
	const char * lLogDirectory = suite.mLogDir.empty()?lDirectory:suite.mLogDir.c_str();
	const char * lPassword = suite.mPwd.c_str(); /* We don't set password by default to "testpwd" anymore */
	const char * lIdentity = suite.mIdentity.c_str();
	//const char * lAdditionalParams=suite.mIOInit.empty()?NULL:suite.mIOInit.c_str();

	const int lNumBuffers = suite.mNBuffer;
	unsigned int mode = 0 ;
	if ( suite.mbArchiveLogs )
		mode |= STARTUP_ARCHIVE_LOGS ;
	if ( suite.mbForceNew )
		mode |= STARTUP_FORCE_NEW ;
	if ( suite.mbForceOpen )
		mode |= STARTUP_FORCE_OPEN ;
	if ( suite.mbRollforward )
		mode |= STARTUP_ROLLFORWARD ;
	if ( suite.mbPrintStats )
		mode |= STARTUP_PRINT_STATS ;


	StartupParameters const lSP(mode, lDirectory, DEFAULT_MAX_FILES, lNumBuffers, 
		DEFAULT_ASYNC_TIMEOUT, pNetCallback, pNotifier,lPassword,lLogDirectory);

	const unsigned short lStoreID = suite.mStoreID ;
	const unsigned int lPageSize = suite.mPageSize ;
	StoreCreationParameters lSCP(NCTLFILES, lPageSize, PAGESPEREXTENT, lIdentity, lStoreID, lPassword, false); 
	lSCP.fEncrypted = ((lPassword != NULL) && (strlen(lPassword)>0));

	Afy::IAffinity *& lStoreCtx = suite.mStoreCtx;

	RC rc ;
	//if (RC_OK != ( rc = (RC)sDynamicLinkMvstore->createStore(lSCP, lSP, lStoreCtx,&outSession, lAdditionalParams )))
	if (RC_OK != ( rc = createStore(lSCP, lSP, lStoreCtx, &outSession)))
	{
		suite.mStoreCtx = NULL ; 
		if (rc == RC_NOACCESS )
		{
				std::cout << "No access error creating store" << std::endl;
			suite.mLock->unlock() ;
			return rc;
		}
		else if (rc == RC_OTHER )
		{
			// Avoid trying to create store if i/o stack creation didn't work
			std::cout << "Cannot create store (perhaps IO related)" << std::endl;
			suite.mLock->unlock() ;
			return rc;
		}
	}
	else
	{
		std::cout << "Created new store. Identity: " << lIdentity << " PageSize: " << lPageSize << " NBUFFERS: " << lNumBuffers << " StoreID: " << lStoreID << " Password: " << lPassword << " Location: " << (lDirectory?lDirectory:"unknown") << endl ;
	}

	assert(outSession!=NULL);

	suite.mStarted++;
	assert(suite.mStarted==1);
	assert(suite.mStoreCtx!=NULL);

	suite.mLock->unlock() ;

	return RC_OK;
}

void MVTApp::stopStore()
{
	if (bReplicate)
	{
		TestLogger lOutV(TestLogger::kDStdOut);

		// Wait at least for replic.cpp's sBatchingTimeoutInMs,
		// to make sure we diff after all replication events were sent.
		// Review: Formalize this better...
		#if 0
		lOutV.out() << std::endl << "Waiting for completion of replication..." << std::endl;
		ReplicationEventProcessor::sTheReplicationEventProcessor->waitForCompletion(lOutV);

		// Diff the resulting stores, in replica mode.
		if (bReplicate && !compareReplicaStore(lOutV))
		#endif
			lOutV.out() << "  Failed!" << std::endl;
	}

	if ( Suite().mbGetStoreCreationParam )
	{
		MVTApp::printStoreCreationParam();
	}
	TestSuiteCtx & suite = Suite() ;
	suite.mLock->lock() ;

	if (suite.mStarted==0)
	{   
		// Store wasn't opened, or it wasn't opened properly by
		// MVTApp::startStore 
		assert(!"Try to stopStore that wasn't opened") ; // Store not open
		suite.mLock->unlock() ;
		return;
	}

	assert(suite.mStarted > 0) ; // Store not open
	assert(suite.mStoreCtx != NULL) ;

#if 1
	typedef std::map<PID, std::string> TMD5s;
	TMD5s lMD5s;
	if (suite.mbTestDurability)
	{
		printf("\n\n{\nProducing durability snapshot before shutdown...\n\n");
		Afy::IAffinity * const lStoreCtx = Suite().mStoreCtx;
		ISession * const lSession = lStoreCtx ? startSession(lStoreCtx) : NULL;
		ICursor * lCursor;
		if (lSession && RC_OK == CmvautoPtr<IStmt>(lSession->createStmt("SELECT *;"))->execute(&lCursor))
		{
			CmvautoPtr<ICursor> lCursorA(lCursor);
			Md5Stream lMd5S;
			unsigned char lMd5[16];
			MvStoreSerialization::ContextOutComparisons lSerCtx(lMd5S, *lSession);
			IPIN * lPIN;
			for (lPIN = lCursor->next(); NULL != lPIN; lPIN = lCursor->next())
			{
				PID const lPID = lPIN->getPID();
				MvStoreSerialization::OutComparisons::pin(lSerCtx, *lPIN);
				lMd5S.flush_md5(lMd5);
				lPIN->destroy();

				std::ostringstream lOs;
				for (size_t iC = 0; iC < sizeof(lMd5); iC++)
					lOs << std::hex << std::setw(2) << std::setfill('0') << (int)lMd5[iC];
				lOs << std::ends;
				lMD5s[lPID] = lOs.str();
			}
		}
		if (lSession)
			lSession->terminate();
	}
#endif

	if (suite.mStarted > 1)
	{
		if (suite.mbForceClose)	
			//if(RC_OK == sDynamicLinkMvstore->shutdown(suite.mStoreCtx,true))
			if(RC_OK == suite.mStoreCtx->shutdown())
				suite.mStarted--;

		suite.mLock->unlock() ;
		return;
	}

#if 0 // remove replication temporarily
	if (ReplicationEventProcessor::sTheOrgToken)
		{ sDynamicLinkMvstore->repDestroyStoreNotificationHandler(ReplicationEventProcessor::sTheOrgToken); ReplicationEventProcessor::sTheOrgToken = NULL; }
	if (ReplicationEventProcessor::sTheDstToken)
		{ sDynamicLinkMvstore->repDestroyStoreNotificationHandler(ReplicationEventProcessor::sTheDstToken); ReplicationEventProcessor::sTheDstToken = NULL; }
	if (sReplicaStoreCtx)
	{
		if(RC_OK == sDynamicLinkMvstore->shutdown(sReplicaStoreCtx, true)){suite.mStarted--; sReplicaStoreCtx = NULL; }
	}
	else
	if(RC_OK == sDynamicLinkMvstore->shutdown(suite.mStoreCtx, bReplicate|suite.mbForceClose ? true : false))
		suite.mStarted--;
#endif 

	if(RC_OK == suite.mStoreCtx->shutdown())
		suite.mStarted--;
	
	suite.mStoreCtx=NULL;
	assert(suite.mStarted==0);

#if 1
	if (suite.mbTestDurability)
	{
		printf("\nTesting durability snapshot after shutdown...\n\n");
		bool lDurabilityOk = true;
		StartupParameters const lSP(0, suite.mDir.c_str(), DEFAULT_MAX_FILES, suite.mNBuffer, DEFAULT_ASYNC_TIMEOUT, NULL, NULL, suite.mPwd.c_str(), suite.mLogDir.c_str());
		Afy::IAffinity * lStoreCtx = NULL;
		size_t const lNumChecked = lMD5s.size();
		if (RC_OK == openStore(lSP, lStoreCtx))
		{
			ISession * const lSession = lStoreCtx ? startSession(lStoreCtx) : NULL;
			ICursor * lCursor;
			if (lSession && RC_OK == CmvautoPtr<IStmt>(lSession->createStmt("SELECT *;"))->execute(&lCursor))
			{
				CmvautoPtr<ICursor> lCursorA(lCursor);
				Md5Stream lMd5S;
				unsigned char lMd5[16];
				MvStoreSerialization::ContextOutComparisons lSerCtx(lMd5S, *lSession);
				IPIN * lPIN;
				for (lPIN = lCursor->next(); NULL != lPIN; lPIN = lCursor->next())
				{
					PID const lPID = lPIN->getPID();
					MvStoreSerialization::OutComparisons::pin(lSerCtx, *lPIN);
					lMd5S.flush_md5(lMd5);
					lPIN->destroy();

					std::ostringstream lOs;
					for (size_t iC = 0; iC < sizeof(lMd5); iC++)
						lOs << std::hex << std::setw(2) << std::setfill('0') << (int)lMd5[iC];
					lOs << std::ends;

					TMD5s::iterator iM = lMD5s.find(lPID);
					if (lMD5s.end() == iM)
						{ printf("***\n*** ERROR: pin "_LX_FM" not found in pre-shutdown snapshot!\n***\n", lPID.pid); lDurabilityOk = false; }
					else
					{
						if ((*iM).second != lOs.str())
							{ printf("***\n*** ERROR: pin "_LX_FM" different after shutdown!\n***\n", lPID.pid); lDurabilityOk = false; }
						lMD5s.erase(iM);
					}
				}
				if (lMD5s.size() > 0)
					{ printf("***\n*** ERROR: %d pins not found after shutdown!\n***\n", lMD5s.size()); lDurabilityOk = false; }
			}
			else
				{ printf("***\n*** ERROR: could not obtain session/cursor for durability check!\n***\n"); lDurabilityOk = false; }
			if (lSession)
				lSession->terminate();
			lStoreCtx->shutdown();
		}
		else
			{ printf("***\n*** ERROR: could not reopen store for durability check!\n***\n"); lDurabilityOk = false; }
		if (!lDurabilityOk)
		{
			MVTestsPortability::threadSleep(5000);
			exit(EXIT_CODE_KILL_SUITE);
		}
		printf("\nVerified durability of %u pins.\n}\n\n", lNumChecked);
	}
#endif

	suite.mLock->unlock() ;
}

ISession * MVTApp::startSession(Afy::IAffinity * pStoreCtx, char const * pIdentity, char const * pPassword, long pFlags)
{
	if (  pStoreCtx == NULL )
	{
		TestSuiteCtx & suite = Suite() ;
 		if ( pPassword == NULL && pIdentity == NULL )
		{
			// REVIEW: Identity should always be passed, otherwise the login process is skipped
			pIdentity = suite.mIdentity.c_str();
			pPassword = !suite.mIdentPwd.empty() ? suite.mIdentPwd.c_str() :
				suite.mPwd.c_str() ;  // REVIEW: is store password provided then 
									// it seems necessary to use it?
							// but what if both were provided?					
		}

		pStoreCtx = suite.mStoreCtx;
	}
	else
	{
		for(size_t j = 0; j < mMultiStoreCxt.size(); j++)
		{
			TestSuiteCtx & suite = mMultiStoreCxt[j];
			if(suite.mStoreCtx == pStoreCtx && (pIdentity == NULL || strcmp(suite.mIdentity.c_str(), pIdentity) == 0 ) &&  pPassword == NULL && (!suite.mIdentPwd.empty() || !suite.mPwd.empty()))
			{
				pPassword = !suite.mIdentPwd.empty() ? suite.mIdentPwd.c_str() : suite.mPwd.c_str() ;
				break;
			}
		}		
	}
	//ISession * const lSession = sDynamicLinkMvstore->startSession(pStoreCtx, pIdentity, pPassword);
	ISession * const lSession = pStoreCtx->startSession(pIdentity, pPassword);
	/* assert(lSession); disabled because TestIdentity Expects failure */

	//For Replication smoke tests which runs select kernel tests in IPC mode on a normal mv node installation (i.e. local node to hosting node replication)
	if(bReplicSmoke)
	{
		//lSession->setURIBase("http://vmware.com/core/");
		lSession->setInterfaceMode(lSession->getInterfaceMode() | ITF_DEFAULT_REPLICATION);
	}

	if (lSession)
	{
		// Note: Setting ITF_DEFAULT_REPLICATION is at the very least needed when bReplicate is set, but is also needed if we want to replicate pins created by the tests, using another mvstoreserver client process to handle the replication.
		if (0 == (pFlags & kSSFNoReplication))
			lSession->setInterfaceMode(lSession->getInterfaceMode() | ITF_DEFAULT_REPLICATION);
		if (0 != (pFlags & kSSFTrackCurrentSession))
			sThread2Session.set(lSession);
	}
	return lSession;
}

Afy::IStream * MVTApp::wrapClientStream(ISession * pSession, Afy::IStream * pClientStream)
{
	// Note: We don't use nkadaptor.h's definition in mvstore/tests, because we don't want to
	//		 impose the link dependency on mvstoreclient.dll in the inproc case...
#if 0 // remove IPC	
	if (sDynamicLinkMvstore->isInProc())
		return pClientStream;
	typedef Afy::IStream * (*TWrapStream)(Afy::ISession *, Afy::IStream *);
	static TWrapStream sWrapStream = NULL;
	#ifdef WIN32
		if (!sWrapStream)
		{
			void * const lMvStoreClient = ::LoadLibrary("affinityclient.dll");
			sWrapStream = lMvStoreClient ? (TWrapStream)::GetProcAddress((HMODULE)lMvStoreClient, "clientWrapStream") : NULL;
		}
	#else
		if (!sWrapStream)
		{
			void * const lMvStoreClient = dlopen("libaffinityclient.so", RTLD_LAZY);
			sWrapStream = lMvStoreClient ? (TWrapStream)dlsym(lMvStoreClient, "clientWrapStream") : NULL;
		}
	#endif
	if (sWrapStream)
		return (*sWrapStream)(pSession, pClientStream);
	assert(false);
#endif
	return pClientStream;
}

bool MVTApp::deleteStore()
{
	// Pull info from the current Suite() to delete the associated store file (which should
	// be closed)

	// HOWEVER IF AN INDIVIDUAL TESTS WORK WITH MULTIPLE STORES THEN THIS CANNOT WORK
	// WE WOULD NEED TO KNOW ALSO WHICH DIRECTORIES ON S3 TO LOOK AT, e.g. the ioinit string
	// should be different for each

	return MVTUtil::deleteStore(
			Suite().mIOInit.c_str(), 
			Suite().mDir.c_str(), 
			Suite().mLogDir.c_str(), 
			Suite().mbArchiveLogs) ;
}

Afy::IAffinity * MVTApp::getStoreCtx(const char * pIdentity)
{
	if(pIdentity == NULL)
	{
		return MVTApp::Suite().mStoreCtx ;
	}
	else
	{
		for(size_t j = 0; j < mMultiStoreCxt.size(); j++)
			if(strcmp(pIdentity,mMultiStoreCxt[j].mIdentity.c_str()) == 0)
			{
				return mMultiStoreCxt[j].mStoreCtx;
			}			
	}

	return NULL;
}

bool MVTApp::compareReplicaStore(TestLogger & pLogger)
{
	return false;
#if 0
	pLogger.out() << "Performing store comparison after replication..." << std::endl << std::flush;

	Afy::IAffinity * lStoreCtx = Suite().mStoreCtx ;

	if (!lStoreCtx || !sReplicaStoreCtx)
		{ assert(false); return false; }

	ISession * const lOriginalS = startSession(lStoreCtx);
	lOriginalS->detachFromCurrentThread();
	ISession * const lReplicaS = startSession(sReplicaStoreCtx);
	lReplicaS->detachFromCurrentThread();
	lOriginalS->attachToCurrentThread();

	IDumpStore * lDS;
	if (RC_OK != lOriginalS->dumpStore(lDS))
		return false;

	Md5Stream lOriginalMd5S, lReplicaMd5S;
	unsigned char lOriginalMd5[16], lReplicaMd5[16];
	MvStoreSerialization::ContextOutComparisons lSerCtxOriginal(lOriginalMd5S, *lOriginalS), lSerCtxReplica(lReplicaMd5S, *lReplicaS);

	//size_t iProgress = 0;
	bool lSuccess = true;
	IPIN * lOriginalP, * lReplicaP;
	while (RC_OK == lDS->getNextPIN(lOriginalP) && lOriginalP)
	{
		PID const lPID = lOriginalP->getPID();
		MvStoreSerialization::OutComparisons::pin(lSerCtxOriginal, *lOriginalP);
		lOriginalMd5S.flush_md5(lOriginalMd5);

		// Switch to the replica session and diff.
		{
			PushSession const lPS(*lOriginalS, *lReplicaS);
			lReplicaP = lReplicaS->getPIN(lPID);
			if (lReplicaP)
			{
				MvStoreSerialization::OutComparisons::pin(lSerCtxReplica, *lReplicaP);
				lReplicaMd5S.flush_md5(lReplicaMd5);
				if (0 != memcmp(lOriginalMd5, lReplicaMd5, sizeof(lOriginalMd5) / sizeof(lOriginalMd5[0])))
				{
					lSuccess = false;
					pLogger.out() << "  ERROR: Pin " << std::hex << lPID.pid << " was different on the replica" << std::endl;
					pLogger.out() << "  -> on the replica:" << std::endl;
					{
						PushSession const lPS(*lReplicaS, *lOriginalS);
						MVTApp::output(*lOriginalP, pLogger.out(), lOriginalS);
					}
					pLogger.out() << "  -> on the original:" << std::endl;
					MVTApp::output(*lReplicaP, pLogger.out(), lReplicaS);
				}
				else
					pLogger.out() << "  OK: pin " << std::hex << lPID.pid << std::endl << std::flush;
			}
			else
			{
				lSuccess = false;
				pLogger.out() << "  ERROR: Couldn't find pin " << std::hex << lPID.pid << std::endl << std::flush;
			}

			if (lReplicaP)
				lReplicaP->destroy();
		}

		lOriginalP->destroy();
	}

	pLogger.out() << std::endl;
	if (lSuccess)
		pLogger.out() << "  Store comparison successful!" << std::endl;

	lDS->destroy();
	lOriginalS->terminate();

	lReplicaS->attachToCurrentThread();
	lReplicaS->terminate();
	return lSuccess;
#endif
}

void MVTApp::outputComparisonFailure(PID const & pPID, IPIN const & p1, IPIN const & p2, std::ostream & pOs)
{
	pOs << "comparison for pin " << std::hex << pPID.pid << " failed!" << std::endl;
	pOs << "  expected:" << std::endl;
	MVTApp::output(p1, pOs);
	pOs << "  but got:" << std::endl;
	MVTApp::output(p2, pOs);
}

void MVTApp::enumClasses(ISession & pSession, std::vector<Tstring> * pNames, std::vector<uint32_t> * pIDs, std::vector<IStmt*> * pPredicates)
{
	CmvautoPtr<IStmt> lQ(pSession.createStmt());
	SourceSpec lCS;
	lCS.objectID = CLASS_OF_CLASSES;
	lCS.nParams = 0; lCS.params = NULL;
	lQ->addVariable(&lCS, 1);
	ICursor* lC = NULL;
	lQ->execute(&lC);
	CmvautoPtr<ICursor> lR(lC);
	if (!lR.IsValid())
		return;
	IPIN * lP;
	for (lP = lR->next(); NULL != lP; lP = lR->next())
	{
		if (pNames) {
			const Value *uri=lP->getValue(PROP_SPEC_OBJID); char buf[200]; size_t lbuf=sizeof(buf);
			if (uri!=NULL && uri->type==VT_URIID && pSession.getURI(uri->uid,buf,lbuf)==RC_OK && lbuf!=0)
				pNames->push_back(buf);
		}
		if (pIDs)
			pIDs->push_back(lP->getValue(PROP_SPEC_OBJID)->ui);
		if (pPredicates)
			pPredicates->push_back(lP->getValue(PROP_SPEC_PREDICATE)->stmt);
		lP->destroy();
	}
}

// Entry point.
int main(int argc, char * argv[])
{
//	{std::ofstream lOs("maxtst.txt", std::ios::app); lOs << std::endl << "---" << std::endl << std::flush;}
//	CreateEvent(NULL, FALSE, FALSE, "dontcrash");
	MVTArgs * pArgs = new MVTArgs(argc,argv);
/*	cout << "Double checking:" << endl;
	for(int ii = 0; ii < argc; ii++)
		cout << ii << ". " << argv[ii] << endl;
	
	string val;
	pArgs->get_allparamstr(&val);
	cout << "All argv at start: " << val << endl; 
*/	
	gMainThreadID = (long int)getThreadId();
    
	//
	// By default output asserts to output and open message box
	// The message box allows ignoring the assert or attaching debugger
	// (use -noui flag to override this default)
	//
	setAssertOutput(true) ;

	// Produce a CRT leak report.
	#if 0
		#if defined(WIN32) && !defined(NDEBUG)
			_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
		#endif
		new char[10]; // A leak, just to validate leak detection...
	#endif

	bool lIPC = false, lClientAPI = false;
	bool lWaitOnComplete = false ;
	Tstring lServerName = "";

	int i;
	for (i = argc - 1; i >= 0 ; i--)
	{
		if (0 == strncmp(argv[i], "-ipc",strlen("-ipc")) ) { lIPC=true; }
		else if (0 == strncmp(argv[i], "-wait",strlen("-wait")) ) { lWaitOnComplete=true; }
		else if (argv[i] == strstr(argv[i], "-client") ) { lClientAPI=true; }
		else if( 0 == strncmp(argv[i], "-srvname",strlen("-srvname"))) { lServerName = argv[i] + strlen("-srvname="); }
	}

	//MVTApp::sDynamicLinkMvstore = new mvcore::DynamicLinkMvstore(!lIPC, lIPC?lServerName.empty()?NULL:lServerName.c_str():NULL, lClientAPI);

	int const lRet = MVTApp().start(pArgs);

	MVTApp::sReporter.term() ;
	ITest::unregisterAllTests();

	//delete MVTApp::sDynamicLinkMvstore;
	//MVTApp::sDynamicLinkMvstore = NULL;

	if ( lWaitOnComplete )
		getchar() ; // Block to see results
    
	delete pArgs;
	return lRet;
}

#ifndef WIN32
	volatile long MVTestsPortability::Mutex::fInit = 0;
	pthread_mutexattr_t MVTestsPortability::Mutex::mutexAttrs;
	MVTestsPortability::Mutex::Mutex()
	{
		while (fInit<=0)
			if (cas(&fInit,-1,0)!=0) threadYield();
			else {
				pthread_mutexattr_init(&mutexAttrs);
				pthread_mutexattr_settype(&mutexAttrs,PTHREAD_MUTEX_DEFAULT);
				pthread_mutexattr_setpshared(&mutexAttrs,PTHREAD_PROCESS_PRIVATE);
				//pthread_mutexattr_setprotocol(&mutexAttrs,PTHREAD_PRIO_NONE);
				fInit=1; break;
			}
		pthread_mutex_init(&mutex,&mutexAttrs);
	}
#endif
