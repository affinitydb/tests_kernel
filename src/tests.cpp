/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "tests.h"
#include "app.h"

void ITest::registerTest(ITest * pTest) 
{ 
	guarantyTests(); 
	sTests->push_back(pTest); 
}
void ITest::unregisterAllTests() 
{ 
	// Auto-destruction of the registered ITest instances
	if (sTests)
	{ 
		while (!sTests->empty()) 
		{ 
			#if 0
				cout << sTests->back()->getName() << endl;
			#endif
			sTests->back()->destroy(); 
			sTests->pop_back(); 
		} 
		delete sTests; 
	} 
}

// Implementation of ITest (base class for all tests)
int ITest::performTest(MVTArgs *p, long * outTime, TIMESTAMP * outTime2)
{
	int largc; char** largv=NULL; mpArgs = p; mpArgs->get_paramOldFashion(largc, largv);
	return performTest(largc,largv, outTime, outTime2);
}
int ITest::performTest(int argc, char * argv[], long * outTime, TIMESTAMP * outTime2)
{
	int lResult = RC_OK;

	mCntEvents = 0;
	mCntFailures = 0;
	std::ostream & out = std::cout;

	out << "-----" << getName() << " Starting---------" << endl;
	string val; mpArgs->get_allparamstr(&val);
    out << "Test arguments: " << val << endl;
	out << "Using seed: "<< mRandomSeed << endl;

	MVTApp::sReporter.reset();

	// Review (maxw): Just keep the most precise of the two.
	long const lBef = getTimeInMs();
	TIMESTAMP lBefTS; getTimestamp(lBefTS);
	
	for (int ii=0; ii<argc; ii++)
		mTestArgs.push_back(argv[ii]);
	lResult = execute();

	long const lAft = getTimeInMs();
	TIMESTAMP lAftTS;getTimestamp(lAftTS);

	// TVERIFY macros can signal test failure at any point
	if (lResult == RC_OK && mCntFailures > 0)
		{ lResult = RC_FALSE; }

	out << "-----" << getName() << " Finished";
	if (mCntEvents > 0)
		out << ": " << std::dec << mCntFailures << " failed events out of " << mCntEvents;
	out << endl;

	if (isStoreErrorFatal() &&  MVTApp::sReporter.wasErrorLogged())
	{
		out << "MVStore logged errors" << endl;
		lResult = RC_FALSE;
	}
	out << "Test status: ";
	out << ((lResult == RC_OK) ? "Succeeded! " : "Failed! ");
    out << " (" << std::dec << lAft - lBef << " ms, " << std::dec << (lAftTS-lBefTS)/1000 << " ms timestamp diff)" << std::endl;

	if (outTime != NULL) *outTime = lAft - lBef;
	if (outTime2 != NULL) *outTime2 = lAftTS - lBefTS;

	return lResult;
}

void ITest::TestEvent(const char* code, const char* extramsg, const char* filename, int lineNumber, bool success)
{   
	mOutputLock.lock();
	mCntEvents++;
	if (!success)
	{
		// TIP: if you want to stop your debugger at the point of
		// the first failure just put a breakpoint here (and equivalent
		// location below).  You could also add an assert statement.

		// Remember failure
		mCntFailures++;

		// Log the failure
		mLogger.out() << "*** failed *** " << code;
		if (strlen(extramsg) > 0)
			mLogger.out() << "," << extramsg;
		mLogger.out() << " (" << filename << ":" << lineNumber << ")\n" << endl;
	}
	mOutputLock.unlock();
}

void ITest::TestEventRC(const char* code, const char* extramsg, const char* filename, int lineNumber, RC result)
{   
	// Variation that prints out the RC code
	// Note: Keep this list in sync with Kernel
	static const char * rcCodes[] =
	{
		"RC_OK",
		"RC_NOTFOUND",
		"RC_ALREADYEXISTS",
		"RC_INTERNAL",
		"RC_NOACCESS",
		"RC_NORESOURCES",
		"RC_FULL",
		"RC_DEVICEERR",
		"RC_DATAERROR",
		"RC_EOF",
		"RC_TIMEOUT",
		"RC_REPEAT",
		"RC_CORRUPTED",
		"RC_CANCELED",
		"RC_VERSION",
		"RC_TRUE",
		"RC_FALSE",
		"RC_TYPE",
		"RC_DIV0",
		"RC_INVPARAM",
		"RC_READTX",
		"RC_OTHER",
		"RC_DEADLOCK",
		"RC_QUOTA",
		"RC_SHUTDOWN",
		"RC_DELETED",
		"RC_CLOSED",
		"RC_READONLY",
		"RC_NOSESSION",
		"RC_INVOP",
		"RC_SYNTAX",
		"RC_TOOBIG",
		"RC_PAGEFULL",
	};
	assert((1 + RC_PAGEFULL) == sizeof(rcCodes) / sizeof(rcCodes[0]));

	mOutputLock.lock(); 
	mCntEvents++;			
	if (result != RC_OK)
	{   
		mCntFailures++;
		const char * rcAsStr = (result >= 0 && size_t(result) < sizeof(rcCodes)/sizeof(rcCodes[0])) ? rcCodes[result] : "unknown rc!";
		mLogger.out() << "*** failed with RC " << result << "(" << rcAsStr << ") *** " << code;
		if (strlen(extramsg) > 0)
			mLogger.out() << "," << extramsg;
		mLogger.out() << " (" << filename << ":" << lineNumber << ")\n" << endl;
	}
	mOutputLock.unlock();
}

bool ITest::isVerbose() const
{
	return MVTApp::bVerbose;
}

bool ITest::isS3IO() const
{
	// Hopefully only a small subset of tests would ever need to consider this
	// because 99% of tests should work regardless of whether s3 is enabled.
	// It's only special operations like moving store files or checking files on disk
	// that might need to consider this.
	return NULL != strstr(MVTApp::Suite().mIOInit.c_str(), "{s3io");
}
