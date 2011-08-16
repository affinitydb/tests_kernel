/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _STORE_TESTS_H
#define _STORE_TESTS_H

// For CMemoryState (memleak testing; in conjunction with "Use MFC in a Shared DLL").
#if 0 
#ifdef WIN32
#include <afxwin.h>
#endif
#endif

#ifdef WIN32
#pragma	warning(disable:4996)
#define	getcwd	_getcwd
#define	strdup	_strdup
#endif

// mvStore includes (needed by all tests).
#include "mvstoreapi.h"
#include "syncTest.h"

// For exceptional cases, e.g. special bug investigations
// a test can stop the current test suite and batch scripts by calling
// exit() with this special value.
#define EXIT_CODE_KILL_SUITE 13

// std includes (needed by infrastructure and most tests).
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <set>
#include <vector>
#include "MVTArgs.h"

using namespace MVStore;

typedef std::basic_string<char> Tstring;
typedef std::basic_string<wchar_t> Wstring;
typedef std::basic_ofstream<char> Tofstream;
typedef std::basic_ifstream<char> Tifstream;

#define MAX_LINE_SIZE 0x10000
#define MAX_PARAMETER_SIZE MAX_LINE_SIZE - 0x200
#define MAX_PROPERTIES_PER_PIN 50

#define CREATEPIN(session, PID, prop, nprop) if (RC_OK != (session)->createPIN((PID), (prop), (nprop))) assert(false)
PID const gInvalidPID = {STORE_INVALID_PID, STORE_INVALID_IDENTITY};
typedef IExprTree * TExprTreePtr;
#define EXPRTREEGEN(sessionptr) (sessionptr)->expr

/*
TVERIFY - Macro to perform a test "event", which is 
    validation of an expected condition.  It is like an ASSERT but
    executes the condition even in release.
	Any failure causes the whole test to fail, with details logged

Example trivial usage:
	TVERIFY( 0 == 0 ) ;  // Success
	TVERIFY( 0 == 1 ) ;  // Failure occurs

Example real usage:
	TVERIFY( x > 0 ) ;
*/
#define TVERIFY(f) do { TestEvent( #f, "", __FILE__, __LINE__, (f) ); } while (0) 

// Variation to add extra string information for the log.
#define TVERIFY2(f,s) do { TestEvent( #f, (s), __FILE__, __LINE__, (f) ); } while (0) 

// Convenient Variations for calling functions that return RC code.
#define TVERIFYRC(f) do { TestEventRC( #f, "", __FILE__, __LINE__, (f) ); } while (0) 
#define TVERIFYRC2(f,s) do { TestEventRC( #f, (s), __FILE__, __LINE__, (f) ); } while (0) 

/* "Redirected" TVERIFY
   For usage from helper objects or threads that are auxiliary to a test.
   Typical usage is to pass the "this" pointer of a test as part of the structure sent to any thread that it
   creates.  Then the threads can use these macros to perform tests and flag any error that they discover. */
#define TV_R(f,itest) do { (itest)->TestEvent( #f, "", __FILE__, __LINE__, (f) ); } while (0) 
#define TVRC_R(f,itest) do { (itest)->TestEventRC( #f, "", __FILE__, __LINE__, (f) ); } while (0) 

#ifdef WIN32
	static inline long getTimeInMs() { return ( 1000 / CLOCKS_PER_SEC ) * ::clock() ; }
#else
	static inline long getTimeInMs() { struct timespec ts; 
#ifdef Darwin
	struct timeval tv; gettimeofday(&tv, NULL);
	ts.tv_sec=tv.tv_sec; ts.tv_nsec=tv.tv_usec*1000;
#else
	::clock_gettime(CLOCK_REALTIME,&ts);
#endif	
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }
#endif

// An output stream that simply hides everything.
class NullStream : public std::basic_ostream<char>
{
	public:
		class NullStreamBuf : public std::basic_streambuf<char>
		{
			protected:
				int_type overflow(int_type) { return traits_type::eof(); }
				int	sync() { return 0; }
		};
		NullStream() : std::basic_ostream<char>( lBuffer = new NullStreamBuf()) {}
		~NullStream() { delete lBuffer;}
	private:
		NullStreamBuf *lBuffer;
};

// An output stream that uses windows' OutputDebugString.
#ifdef WIN32
class OutputDebugStream : public std::basic_ostream<char>
{
	public:
		class OutputDebugStreamBuf : public std::basic_streambuf<char>
		{
			protected:
				virtual int_type overflow(int_type c) { mBuf[mBufPtr++] = c; mBuf[mBufPtr] = 0; put_all(c == '\n'); return traits_type::not_eof(c); }
				virtual int	sync() { put_all(true); return 0; }
			private:
				char mBuf[0x100];
				int mBufPtr;
				void put_all(bool pForce) { if (pForce || mBufPtr >= 0x100 - 1) { ::OutputDebugString(mBuf); mBufPtr = 0; mBuf[0] = 0; } assert(pbase() == pptr()); }
			public:
				OutputDebugStreamBuf() : mBufPtr(0) { mBuf[0] = 0; }
		};
		OutputDebugStream() : std::basic_ostream<char>(lBuffer = new OutputDebugStreamBuf()) {}
		~OutputDebugStream() { delete lBuffer;}
	private:
		OutputDebugStreamBuf *lBuffer;
};
#endif

// The test framework's primary logging abstraction; allows to redirect output to various useful streams.
class TestLogger
{
	public:
		enum eDestination { kDNull, kDStdOut, kDStdOutVerbose, kDStdErr, kDOutputDebug, kDStrstr, };
	protected:
		eDestination mDestination;
		NullStream mNullOutput;
		#ifdef WIN32
			OutputDebugStream mOutputDebugOutput;
		#endif
		std::basic_ostringstream<char> mStrstrOutput;
		bool const mVerbose;
	public:
		TestLogger(eDestination pDestination = kDStdOut) : mDestination(pDestination), mVerbose(NULL != getenv("TESTS_VERBOSE")) {}
		eDestination getDestination() const { return mDestination; }
		void setDestination(eDestination pDestination) { mDestination = pDestination; }
		std::ostream & out()
		{
			switch (mDestination)
			{
				case kDNull: return mNullOutput;
				case kDStdOut: return std::cout << std::flush;
				case kDStdOutVerbose: if (mVerbose) return std::cout; return mNullOutput;
				case kDStdErr: return std::cerr;
				#ifdef WIN32
					case kDOutputDebug: return mOutputDebugOutput;
				#else
					case kDOutputDebug: return std::cout;
				#endif
				case kDStrstr: return mStrstrOutput;
				default: return std::cout;
			}
		}
		void print(char * pStr, ...)
		{
			static char lStr[0x2000];
			va_list	lArguments;
			va_start(lArguments, pStr);
			vsprintf(lStr, pStr, lArguments);
			va_end(lArguments);
			out() << lStr;
		}
};

// Base class (and interface) of all tests.
class ITest
{
	public:
		typedef std::vector<ITest *> TTests;
		typedef std::vector<char *> TTestArgs;

	protected:
		TestLogger mLogger;
		unsigned int mRandomSeed;
		int mCntEvents; // optional - count of TVERIFY calls
		int mCntFailures; // optional - count of failed TVERIFY calls
		MVStoreKernel::Mutex mOutputLock; // Serialize TVERIFY macros or calls to mLogger.out
		MVTArgs * mpArgs; // parsed command line parameters
		TTestArgs mTestArgs;

	public:
		virtual char const * getName() const = 0;
		virtual char const * getHelp() const = 0;
		virtual char const * getDescription() const = 0;
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = ""; return true; }
		virtual bool excludeInIPCSmokeTest(char const *& pReason) const { pReason = ""; return false; }
		virtual bool isLongRunningTest() const { return false; /* returning true automatically excludes from smoke test */ }
		virtual bool includeInLongRunningSmoke(char const *& pReason) const { pReason = ""; return true; }
		virtual bool includeInBashTest(char const *& pReason) const { pReason = ""; return true; }
		virtual bool includeInPerfTest() const { return false; }
		virtual bool isStandAloneTest() const { return false; /* returning true automatically excludes from smoke test */ }
		virtual unsigned int excludeInLessPageSizeTestSuits(char const *& pReason) const { pReason = ""; return 0; /* returning the pagesize limit will exclude the test from any test suite run with a pagesize lesser */ }
		virtual bool includeInMultiStoreTests() const { return true; }
		virtual bool isPerformingFullScanQueries() const { return false; /* returning true means that this test will certainly run at least one full-scan query */ }
		virtual bool isStoreErrorFatal() const { return true; /* returning true means that any store error is considered fatal (the default) */ }

		virtual void setRandomSeed(unsigned int pRandomSeed) { mRandomSeed = pRandomSeed; srand(mRandomSeed); }

		/* Most tests override this for the implementation.  
		   Any return value other than RC_OK signals a failed test.
		   Another way to signal failed test is via a failed TVERIFY macro */
		virtual int execute() { return RC_FALSE; }

		virtual ITest * newInstance() = 0;
		virtual void destroy() = 0;
		TestLogger & getLogger() { return mLogger; }

		// Method called by the test framework to execute test
		int performTest(int argc = 0, char * argv[] = NULL, long * outTime = NULL, TIMESTAMP * outTime2 = NULL);
		int performTest(MVTArgs *p, long * outTime, TIMESTAMP * outTime2);

		// Tests can check this to see if they can use verbose output
		bool isVerbose() const; 

		// Check whether the tests are using s3 for storage
		bool isS3IO() const;

		// Called by TVERIFY macros (do not call directly)
		void TestEvent(const char* code, const char* extramsg, const char* filename, int lineNumber, bool success);
		void TestEventRC(const char* code, const char* extramsg, const char* filename, int lineNumber, RC result);
        
		// Called to get access to the command line
		virtual unsigned long get_argc() const { return (unsigned long)mTestArgs.size(); }
		virtual char * get_argv(unsigned long argc) const { return argc > mTestArgs.size() ? NULL : mTestArgs[argc]; }
	
		static void registerTest(ITest * pTest);
		static void unregisterAllTests();
		static TTests const & getTests() { guarantyTests(); return *sTests; }

	public:
		// mvStore helpers (implement common services that used to be in ISession)
		static RC defineClass(ISession *ses, const char *className, IStmt *qry, ClassID *pClsid=NULL, bool fSDelete=false)
		{
			Value vals[4]; unsigned cnt=0; RC rc=RC_OK;
			vals[0].set(className); vals[0].setPropID(PROP_SPEC_URI); cnt++;
			vals[1].set(qry); vals[1].setPropID(PROP_SPEC_PREDICATE); cnt++;
			if (fSDelete) {vals[cnt].set(unsigned(fSDelete?CLASS_SDELETE:0)); vals[cnt].setPropID(PROP_SPEC_CLASS_INFO); cnt++;}
			IPIN *pin=ses->createUncommittedPIN(vals,cnt,MODE_COPY_VALUES);
			if (pin==NULL) rc=RC_NORESOURCES;
			else {
				rc=ses->commitPINs(&pin,1);
				if (pClsid!=NULL && (rc==RC_OK || rc==RC_ALREADYEXISTS)) {
					const Value *clsid=pin->getValue(PROP_SPEC_CLASSID);
					if (clsid!=NULL && clsid->type==VT_URIID) *pClsid=clsid->uid; else rc=RC_OTHER;
				}
				pin->destroy();
			}
			return rc;
		}
		static RC updateClass(ISession *ses, const char *className, IStmt *qry, bool fIndex=true)
		{
			ClassID cid=STORE_INVALID_CLASSID; IPIN *pin=NULL;
			if (className==NULL) return ses->rebuildIndices();
			RC rc=ses->getClassID(className,cid);
			if (rc==RC_OK) {
				if (qry==NULL) rc=ses->rebuildIndices(&cid,1);
				else if ((rc=ses->getClassInfo(cid,pin))==RC_OK) {
					Value v; v.set(qry); v.setPropID(PROP_SPEC_PREDICATE);
					rc=pin->modify(&v,1);
				}
			}
			return rc;
		}
		static RC dropClass(ISession *ses,const char *className)
		{
			ClassID cid=STORE_INVALID_CLASSID; IPIN *pin=NULL;
			RC rc=ses->getClassID(className,cid);
			if (rc==RC_OK && (rc=ses->getClassInfo(cid,pin))==RC_OK) rc=pin->deletePIN();
			return rc;
		}

	protected:
		static TTests * sTests;
		static TTests * guarantyTests() { return sTests ? sTests : (sTests = new TTests); }
};

// Automatic (static) registration of tests.
template <class Test>
class TestRegistry
{
	public:
		TestRegistry(Test * pTest, TestLogger::eDestination pOutputDst) { pTest->getLogger().setDestination(pOutputDst); ITest::registerTest(pTest); }
};
#define TEST_DECLARE(classname) \
	public: \
		static TestRegistry<classname> sRegistry; \
		virtual ITest * newInstance() { classname * l = new classname(); l->getLogger().setDestination(mLogger.getDestination()); return l; }
#define TEST_IMPLEMENT(classname, outputdst) \
	TestRegistry<classname> classname::sRegistry(new classname(), outputdst);
#endif
