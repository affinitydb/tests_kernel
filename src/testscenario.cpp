/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

/*
testscenario contains a group of tests that cooperate to
cover persistence, store recovery, concurrency etc

Each "scenario" is a list of actions to perform when creating a new store.

You can run each step (scenario_gen/scenario_run/scenario_test) manually,
or else use "tests scenario" to run iterations of the entire process
with included crash (see app.cpp for details)

(tests used to be called recovery/gen/run/test but they have been renamed
to avoid confusion with testrecovery.cpp, and give more consistent names)
*/

#include "app.h"
#include "mvauto.h"
#include "collectionhelp.h"
#include <algorithm>
#include <bitset>

// For debugging the test itself
#define SMALL_DATA_SET 0

#define MAX_PINS 1024
#define MAX_PROPERTIES 256
#define BIG_COLLECTION_WORKAROUND 0 /* might cause roll-back errors? */
#define ENABLE_COLLECTIONS 0

#define TEST_10739 0

typedef std::bitset<MAX_PINS> Tbitsetpins;
typedef std::bitset<MAX_PROPERTIES> Tbitsetproperties;
class PITScenario;

#if SMALL_DATA_SET
	#undef MAX_PARAMETER_SIZE
	#define MAX_PARAMETER_SIZE 100
	#define MIN_ELEMENTS 2
	#define MAX_ELEMENTS 5 
#else
	#define MIN_ELEMENTS 5
	#define MAX_ELEMENTS 500
#endif

static inline bool verboseTestPass() { return true; } // For a more demonstrative demo execution...

static PropertyID mapProp(ISession* session,PropertyID in)
{
	// Map from abstract test property id to real store property id
#if TEST_10739
	return in;
#else
	char propname[64];
	sprintf(propname,"testscenario.%d", in);
	return MVTUtil::getProp(session,propname);
#endif
}

class TestScenario : public ITest
{		
	TEST_DECLARE(TestScenario);
	virtual char const * getName() const { return "scenario"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "run a few loops of the recovery sequence of {gen, run&fail, test}"; }
	
	virtual int execute();
	virtual void destroy() { delete this; }
	virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Special test that erases store"; return false; }
	virtual bool excludeInIPCSmokeTest(char const *& pReason) const { pReason = "Does create, crash of store. Not possible in IPC mode"; return true; }
};
TEST_IMPLEMENT(TestScenario, TestLogger::kDStdOut);

int TestScenario::execute()
{
	// This has been moved out of the app.cpp so that it is a test consistent
	// with all other tests.  It is slightly unusual in that it launches other tests

	// See also testrecovery.cmd for a script version of this batch process

	int lResult = RC_OK ;

	string cmd; stringstream testargs;
	MVTApp::buildCommandLine( MVTApp::Suite(), testargs );

	int cntIterations=4; // 4 loops iterations = <2 minutes on Black test machine
	if(!mpArgs->get_param("iter", cntIterations )) 
	{ 
		mLogger.out() << "No --iter parameter, using 4" << endl;
    }

	int i;
	for (i = 0; i < cntIterations; i++) 
	{
		mLogger.out() << endl << endl << "Beginning Recovery iteration " << i << endl << endl ;

		// 1) Generate a new (failing) script.
		mLogger.out() << "Generating..." << std::endl;

		cmd =string("scenario_gen --scenfile=scenario.dat --numthreads=3 --numoperations=300 --failure") + testargs.str();

		lResult = MVTUtil::executeProcess(MVTApp::mAppName.c_str(),cmd.c_str(),NULL,NULL,false,true );
		if (0 != lResult)
		{
			TVERIFY(!"Generation phase failed");
			break ;
		}

		// 2) Run the script (in debugger when we need to plant a failure).
		mLogger.out() <<  "Running..." << std::endl;
		cmd =string("scenario_run --scenfile=scenario.dat --pinfile=scenario.pins ") + testargs.str();
		lResult = MVTUtil::executeProcess(MVTApp::mAppName.c_str(), cmd.c_str(),NULL,NULL,false,true,true);
		if (0 != lResult)
		{
			TVERIFY(!"Run phase failed");
			break ;
		}

		// 3) Test that recovery works, i.e. the completed part of the script preceding
		//    the crash is indeed recovered.
		mLogger.out() <<  "Testing..." << std::endl;
		cmd =string("scenario_test --scenfile=scenario.dat --pinfile=scenario.pins ") + testargs.str();
		lResult = MVTUtil::executeProcess(MVTApp::mAppName.c_str(), cmd.c_str(),NULL,NULL,false,true);
		if (0 != lResult)
		{
			TVERIFY(!"Testing phase failed");
			break ;
		}
	}

	return lResult ;
}

class TestScenarioColl : public ITest
{		
	//Enabling collection
	TEST_DECLARE(TestScenarioColl);
	virtual char const * getName() const { return "collectionscenario"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "run a few loops of the recovery sequence of {gen, run&fail, test} with collections"; }
	
	virtual int execute();
	virtual void destroy() { delete this; }
	virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Special test that erases store"; return false; }
	virtual bool excludeInIPCSmokeTest(char const *& pReason) const { pReason = "Does create, crash of store. Not possible in IPC mode"; return true; }
};
TEST_IMPLEMENT(TestScenarioColl, TestLogger::kDStdOut);

int TestScenarioColl::execute()
{
	int lResult = RC_OK ;

	string cmd; stringstream testargs;

	int cntIterations=4; 
	if(!mpArgs->get_param("iter", cntIterations )) 
	{ 
		mLogger.out() << "No --iter parameter, using 4" << endl;
    }

	// Pick up any extra args that have been specified, e.g. crash timeout,
	// archive logs
	MVTApp::buildCommandLine( MVTApp::Suite(), testargs );

	int i;
	for (i = 0; i < cntIterations; i++) 
	{
		mLogger.out() << endl << endl << "Beginning Recovery iteration " << i << endl << endl ;

		// 1) Generate a new (failing) script.
		mLogger.out() << "Generating..." << std::endl;
		cmd =string("scenario_gen --scenfile=scenario.dat --numthreads=3 --numoperations=300 --failure --collections") + testargs.str();
		lResult = MVTUtil::executeProcess(MVTApp::mAppName.c_str(), cmd.c_str(),NULL,NULL,false,true);
		if (0 != lResult)
		{
			TVERIFY(!"Generation phase failed");
			break ;
		}

		// 2) Run the script (in debugger when we need to plant a failure).
		mLogger.out() <<  "Running..." << std::endl;
		cmd =string("scenario_run --scenfile=scenario.dat --pinfile=scenario.pins ") + testargs.str();
		lResult = MVTUtil::executeProcess(MVTApp::mAppName.c_str(), cmd.c_str(),NULL,NULL,false,true,true);
		if (0 != lResult)
		{
			TVERIFY(!"Run phase failed");
			break ;
		}

		// 3) Test that recovery works, i.e. the completed part of the script preceding
		//    the crash is indeed recovered.
		mLogger.out() <<  "Testing..." << std::endl;
		cmd =string("scenario_test --scenfile=scenario.dat --pinfile=scenario.pins ") + testargs.str();
		lResult = MVTUtil::executeProcess(MVTApp::mAppName.c_str(), cmd.c_str(),NULL,NULL,false,true);
		if (0 != lResult)
		{
			TVERIFY(!"Testing phase failed");
			break ;
		}
	}

	return lResult ;
}

// Publish this test (3 parts).
class TestScenarioBase : public ITest
{		
	public:
		virtual void destroy() { delete this; }
		Afy::IAffinity *mStoreCtx;
	protected:
		typedef std::vector<PITScenario *> TScenarii;
		void cleanup(TScenarii & pScenarii);
		bool parseScenarioFile(char const * pFileName, std::vector<PITScenario *> & pScenarii);
		bool parsePINsFile(char const * pFileName, std::vector<PITScenario *> & pScenarii);
};
class TestScenarioGen : public TestScenarioBase
{
	public:
		TEST_DECLARE(TestScenarioGen);
		virtual char const * getName() const { return "scenario_gen"; }
		virtual char const * getHelp() const { return "<scenfile> <numthreads> <numoperations> <failure> <bcollections>";}
	    virtual char const * getDescription() const { return "generates a new scenario file"; }
		
		virtual int execute();
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "need parameters, see help... "; return false; }
};
class TestScenarioRun : public TestScenarioBase
{
	public:
		TEST_DECLARE(TestScenarioRun);
		virtual char const * getName() const { return "scenario_run"; }
		virtual char const * getHelp() const { return "<scenfile> <pinfile>"; }
		virtual char const * getDescription() const { return "runs scenfile and produces pinfile"; }
		
		virtual int execute();
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "need parameters, see help... "; return false; }
};
class TestScenarioTest : public TestScenarioBase
{
	public:
		TEST_DECLARE(TestScenarioTest);
		virtual char const * getName() const { return "scenario_test"; }
		virtual char const * getHelp() const { return "<scenfile> <pinfile>"; }
		virtual char const * getDescription() const { return "tests if the db conforms to {scenfile, pinfile}"; }
		
		virtual int execute();
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "need parameters, see help... "; return false; }
};
TEST_IMPLEMENT(TestScenarioGen, TestLogger::kDStdOut);
TEST_IMPLEMENT(TestScenarioRun, TestLogger::kDStdOut);
TEST_IMPLEMENT(TestScenarioTest, TestLogger::kDStdOut);

// Set of PIN indices and PropertyIDs, used for test pass and random scenario generation.
class PITDomain
{
	public:
		typedef std::vector<Tbitsetproperties> TPINIndex2PropIDs;
	protected:
		Tbitsetpins mPINIndices, mDeadPINIndices, mUnavailablePINIndices;
		TPINIndex2PropIDs mPropIDs, mCollPropIDs, mDeadPropIDs;
	public:
		void addPINIndex(long pPINIndex) { mPINIndices.set(pPINIndex); mDeadPINIndices.reset(pPINIndex); mUnavailablePINIndices.set(pPINIndex); }
		void removePINIndex(long pPINIndex) { mPINIndices.reset(pPINIndex); mDeadPINIndices.set(pPINIndex); }
		void donePINIndex(long pPINIndex) { mPINIndices.reset(pPINIndex); mDeadPINIndices.reset(pPINIndex); }
		bool hasPIN(long pPINIndex, bool pDead = false) const { return pDead ? mDeadPINIndices.test(pPINIndex) : mPINIndices.test(pPINIndex); }
		int getNumPINs(bool pDead = false) const { return pDead ? (int)mDeadPINIndices.count() : (int)mPINIndices.count(); }
		long getFirstFreePINIndex() const { return getFirstUnset(mUnavailablePINIndices); }
		long getAnyPINIndex() const;
	public:
		void addPropID(long pPINIndex, PropertyID pPropID) { growTo(mPropIDs, pPINIndex + 1); growTo(mDeadPropIDs, pPINIndex + 1); mPropIDs[pPINIndex].set(pPropID); mDeadPropIDs[pPINIndex].reset(pPropID); }
		void removePropID(long pPINIndex, PropertyID pPropID) { growTo(mPropIDs, pPINIndex + 1); growTo(mDeadPropIDs, pPINIndex + 1); mPropIDs[pPINIndex].reset(pPropID); mDeadPropIDs[pPINIndex].set(pPropID); }
		void donePropID(long pPINIndex, PropertyID pPropID) { growTo(mPropIDs, pPINIndex + 1); growTo(mDeadPropIDs, pPINIndex + 1); mPropIDs[pPINIndex].reset(pPropID); mDeadPropIDs[pPINIndex].reset(pPropID); }
		bool hasPropID(long pPINIndex, PropertyID pPropID, bool pDead = false) const { if (mPropIDs.size() <= (size_t)pPINIndex) return pDead; return pDead ? mDeadPropIDs[pPINIndex].test(pPropID) : mPropIDs[pPINIndex].test(pPropID); }
		int getNumPropIDs(long pPINIndex, bool pDead = false) const { if (mPropIDs.size() <= (size_t)pPINIndex) return 0; return pDead ? (int)mDeadPropIDs[pPINIndex].count() : (int)mPropIDs[pPINIndex].count(); }
		long getFirstFreePropID(long pPINIndex, bool pDead = false) const { if (mPropIDs.size() <= (size_t)pPINIndex) return 0; return pDead ? getFirstUnset(mDeadPropIDs[pPINIndex]) : getFirstUnset(mPropIDs[pPINIndex]); }
		long getAnyPropID(long pPINIndex) const;
		long getAnyNonCollPropID(long pPINIndex) const;		
	public: // For Collection Properties
		void addCollPropID(long pPINIndex, PropertyID pPropID) { growTo(mCollPropIDs, pPINIndex + 1); mCollPropIDs[pPINIndex].set(pPropID); addPropID(pPINIndex, pPropID); }
		void removeCollPropID(long pPINIndex, PropertyID pPropID) { growTo(mCollPropIDs, pPINIndex + 1); mCollPropIDs[pPINIndex].reset(pPropID); removePropID(pPINIndex, pPropID); }
		void doneCollPropID(long pPINIndex, PropertyID pPropID) { growTo(mCollPropIDs, pPINIndex + 1); mCollPropIDs[pPINIndex].reset(pPropID); donePropID(pPINIndex, pPropID); }
		int getNumCollPropIDs(long pPINIndex) const { if (mCollPropIDs.size() <= (size_t)pPINIndex) return 0; return (int)mCollPropIDs[pPINIndex].count(); }
		long getAnyCollPropID(long pPINIndex) const;
	public:
		void print(std::ostream & pOs); // For debugging.
		void clear();
	protected:
		template <class T> static long getFirstUnset(T const & p) { long lR; for (lR = 0; lR < (long)p.size(); lR++) { if (!p.test(lR)) return lR; } return -1; }
		static void growTo(TPINIndex2PropIDs & pVector, size_t pSize) { for (size_t i = pVector.size(); i < pSize; i++) pVector.push_back(Tbitsetproperties()); }
};

// Context for the test pass.
class PITTestContext : public PITDomain
{
	public:
		enum ePass
		{
			kPForward = 0, // Forward pass, to collect all PINs and properties that survived until the end of the scenario.
			kPBackward, // Backward pass, to verify the state of those PINs in the database.
			kPLast
		};
	protected:
		ePass mPass;
		long mNextNewPINIndex;
	public:
		PITTestContext() : mPass(kPForward), mNextNewPINIndex(0) {}
		ePass getPass() const { return mPass; }
		void nextPass() { mPass = ePass(mPass + 1); assert(mPass < kPLast); }
		long nextNewPINIndex() { return mNextNewPINIndex++; }
};

// Operations that can be run in a scenario.
class PITOperation
{
	public:
		enum eType
		{
			kTNone = 0,
			kTBeginTransaction,
			kTCommit,
			kTRollback,
			kTCreatePIN,
			kTDeletePIN,
			kTAddProperty,
			kTDeleteProperty,
			kTUpdateProperty,
			kTGetProperty,
			kTAddCollection,
			kTUpdateCollection,
			kTDeleteCollection,
			kTTotal
			// Review: add more stuff (e.g. INewPIN support)
		};

	public:
		static char const * const sTypeName[kTTotal]; // Strings corresponding to eType.
		static eType const sWeighted[]; // Set of operations weighted for better random generation of scripts.
		static eType const sCollWeighted[]; // Separate set of operations for collections.
	protected:
		eType const mType; // The type of operation (see eType).
		size_t const mIndex; // The 0-based index of this operation in the scenario.
		Tstring const mParameters; // Parameters of this operation (depend on mType).
		int const mFailureIndex; // -1 if no failure desired for this operation, otherwise the index of the desired failure.
	protected:
		long mPINIndex; // For the testing pass only.

	public:
		PITOperation(eType pType, size_t pIndex, char const * pParameters, int pFailureIndex, ITest*);
		eType getType() const { return mType; }
		size_t getIndex() const { return mIndex; }
		Tstring const & getParameters() const { return mParameters; }
		int getFailureIndex() const { return mFailureIndex; }
		void run(PITScenario & pContext);
		void test(PITScenario & pContext, PITTestContext & pTestContext);
		void tokenize(Tstring &pStr, std::vector<Tstring> &pList, const char *pToken);
	public:
		static eType TypeFromName(char const * pTypeName);
	private:
		ITest * mTest;
};

// Scenario (single-threaded, runs in its own thread).
class PITScenario
{
	public:
		typedef std::vector<PITOperation *> TOperations;
		typedef std::vector<PID> TPIDs;
	public:
		static Tofstream * sOutputPINs; // The output stream of resulting PINs.
		static MVTestsPortability::Mutex * sLockOutputPINs; // To regulate shared access to the output stream of PINs.
		static MVTestsPortability::Mutex sLockOutputDbg;
		static bool sFailed;
	protected:
		TestLogger & mLogger; // Log output.
		long const mThreadAbstrID; // The "thread" id abstraction that identifies this scenario.
		Afy::IAffinity *mStoreCtx; // The store ctx.
		ISession * mSession; // The session through which this scenario is run.
		TOperations mOperations; // The operations to run in this scenario.
		TPIDs mPIDs; // The PINs resulting from those operations.
		TPIDs mPIDsTransaction; // The PINs resulting from the current transaction.
		Tbitsetpins mDeletedPIDs; // The deleted PINs.
		Tbitsetpins mDeletedPIDsTransaction; // The PINs deleted during the current transaction (might be rolled back).
		HTHREAD mThread; // The actual thread in which to run this scenario.
		bool mInTransaction; // Status of the run/test.
	public:
		PITScenario(TestLogger & pLogger, long pThreadAbstrID) : mLogger(pLogger), mThreadAbstrID(pThreadAbstrID), mStoreCtx(NULL), mSession(NULL), mThread(0), mInTransaction(false) {}
		~PITScenario();
		long getThreadAbstrID() const { return mThreadAbstrID; }
		void addOperation(PITOperation * pOp) { assert(!mThread); mOperations.push_back(pOp); }
		size_t getNumOperations() { return mOperations.size(); }
		void setStoreCtx(Afy::IAffinity *pStoreCtx){ mStoreCtx = pStoreCtx; }
	public:
		void run();
		bool test();
		void wait();
	public:
		ISession * getSession() const { return mSession; }
		bool failed() const { return sFailed; }
		void setFailed(char const * pMessage, PITOperation const * pOp);
		void outputMessage(char const * pMessage, PITOperation const * pOp, int pParamLen = -1) const;
		void beginTransaction() { assert(!mInTransaction); mInTransaction = true; }
		void endTransaction(bool pCommit);
		void registerPID(PID pPID);
		void unregisterPID(PID pPID);
		void runDeletePID(PID pPID);
		PID getPID(size_t pIndex) const { assert(!mDeletedPIDs.test(pIndex)); return mPIDs[pIndex]; }
	protected:
		static THREAD_SIGNATURE ThreadFunction(void * pThis) { ((PITScenario *)pThis)->ThreadFunctionImplementation(); return 0; }
		void ThreadFunctionImplementation();
};
Tofstream * PITScenario::sOutputPINs = NULL;
MVTestsPortability::Mutex * PITScenario::sLockOutputPINs = NULL;
MVTestsPortability::Mutex PITScenario::sLockOutputDbg;
bool PITScenario::sFailed = false;

// PITDomain
long PITDomain::getAnyPINIndex() const
{
	size_t const lNumPINs = getNumPINs();
	if (0 == lNumPINs)
		return -1;
	size_t const lIndex = (size_t)(double(lNumPINs - 1) * rand() / RAND_MAX);
	size_t i;
	long lPINIndex;
	for (lPINIndex = 0; lPINIndex < (long)mPINIndices.size() && !mPINIndices.test(lPINIndex); lPINIndex++);
	for (i = 0; i < lIndex && lPINIndex < (long)mPINIndices.size(); lPINIndex++)
		if (mPINIndices.test(lPINIndex))
			{ i++; if (i == lIndex) break; }
	return lPINIndex;
}
long PITDomain::getAnyPropID(long pPINIndex) const
{
	size_t const lNumPropIDs = (pPINIndex >= 0 && (size_t)pPINIndex < mPropIDs.size()) ? getNumPropIDs(pPINIndex) : 0;
	if (0 == lNumPropIDs)
		return -1;
	size_t const lIndex = (size_t)(double(lNumPropIDs - 1) * rand() / RAND_MAX);
	size_t i;
	long lPropId;
	for (lPropId = 0; lPropId < (long)mPropIDs[pPINIndex].size() && !mPropIDs[pPINIndex].test(lPropId); lPropId++);
	for (i = 0; i < lIndex && lPropId < (long)mPropIDs[pPINIndex].size(); lPropId++)
		if (mPropIDs[pPINIndex].test(lPropId))
			{ i++; if (i == lIndex) break; }
	return lPropId;
}
long PITDomain::getAnyNonCollPropID(long pPINIndex) const
{
	size_t lNumPropIDs = (pPINIndex >= 0 && (size_t)pPINIndex < mPropIDs.size()) ? getNumPropIDs(pPINIndex) : 0;
	lNumPropIDs -= getNumCollPropIDs(pPINIndex);
	if (0 >= lNumPropIDs)
		return -1;
	bool fColl = getNumCollPropIDs(pPINIndex) != 0;
	TPINIndex2PropIDs lTmpPropIds = mPropIDs;
	size_t const lIndex = (size_t)(double(lNumPropIDs - 1) * rand() / RAND_MAX);
	size_t i;	
	long lPropId = 0;
	for(lPropId = 0; fColl && lPropId < (long)mCollPropIDs[pPINIndex].size() ; lPropId++)
		if(mCollPropIDs[pPINIndex].test(lPropId)) lTmpPropIds[pPINIndex].reset(lPropId);

	for (lPropId = 0; lPropId < (long)lTmpPropIds[pPINIndex].size() && !lTmpPropIds[pPINIndex].test(lPropId) ; lPropId++);
	for (i = 0; i < lIndex && lPropId < (long)lTmpPropIds[pPINIndex].size(); lPropId++)
		if (lTmpPropIds[pPINIndex].test(lPropId))
			{i++; if (i == lIndex) break;}

	return lPropId;
}
long PITDomain::getAnyCollPropID(long pPINIndex) const
{
	size_t const lNumPropIDs = (pPINIndex >= 0 && (size_t)pPINIndex < mCollPropIDs.size()) ? getNumCollPropIDs(pPINIndex) : 0;
	if (0 == lNumPropIDs)
		return -1;
	size_t const lIndex = (size_t)(double(lNumPropIDs - 1) * rand() / RAND_MAX);
	size_t i;
	long lPropId;
	for (lPropId = 0; lPropId < (long)mCollPropIDs[pPINIndex].size() && !mCollPropIDs[pPINIndex].test(lPropId); lPropId++);
	for (i = 0; i < lIndex && lPropId < (long)mCollPropIDs[pPINIndex].size(); lPropId++)
		if (mCollPropIDs[pPINIndex].test(lPropId))
			{ i++; if (i == lIndex) break; }
	return lPropId;
}
void PITDomain::print(std::ostream & pOs)
{
	pOs << "Outputing domain..." << std::endl;
	long lPINIndex;
	for (lPINIndex = 0; lPINIndex < (long)mPINIndices.size(); lPINIndex++)
	{
		if (!mPINIndices.test(lPINIndex))
			continue;
		pOs << "PIN " << lPINIndex << ":";
		if ((long)mPropIDs.size() <= lPINIndex)
		{
			pOs << std::endl;
			continue;
		}

		long lPropId;
		for (lPropId = 0; lPropId < (long)mPropIDs[lPINIndex].size(); lPropId++)
		{
			if (!mPropIDs[lPINIndex].test(lPropId))
				continue;
			pOs << "prop" << lPropId << " ";
		}

		pOs << std::endl;
	}
	pOs << "done." << std::endl;
}
void PITDomain::clear()
{
	mPropIDs.clear();
	mDeadPropIDs.clear();
	mPINIndices.reset();
	mDeadPINIndices.reset();
	mUnavailablePINIndices.reset();
}
// PITOperation
PITOperation::eType const PITOperation::sWeighted[] =
{ 
	kTBeginTransaction,
	kTCommit, kTCommit, kTCommit,
	kTRollback, kTRollback,
	kTCreatePIN, kTCreatePIN, kTCreatePIN, kTCreatePIN,
	kTDeletePIN,
	kTAddProperty, kTAddProperty, kTAddProperty, kTAddProperty, kTAddProperty, kTAddProperty, kTAddProperty, kTAddProperty, kTAddProperty, kTAddProperty, kTAddProperty, kTAddProperty,
	kTDeleteProperty, kTDeleteProperty,
	kTUpdateProperty, kTUpdateProperty, kTUpdateProperty, kTUpdateProperty, kTUpdateProperty, kTUpdateProperty, kTUpdateProperty, kTUpdateProperty, kTUpdateProperty, kTUpdateProperty, kTUpdateProperty, kTUpdateProperty,
	kTGetProperty, kTGetProperty, kTGetProperty, kTGetProperty, kTGetProperty, kTGetProperty, kTGetProperty, kTGetProperty,
	#if ENABLE_COLLECTIONS
		kTAddCollection, kTAddCollection,
		//kTUpdateCollection
		kTDeleteCollection
	#endif
};
// PITOperation :: For Collection scenarios
PITOperation::eType const PITOperation::sCollWeighted[] =
{ 
	kTBeginTransaction,
	kTCommit, kTCommit, kTCommit,
	kTRollback, kTRollback,
	kTCreatePIN, kTCreatePIN, kTCreatePIN, kTCreatePIN,
	kTDeletePIN,
	kTAddProperty,
	kTDeleteProperty,
	kTGetProperty, kTGetProperty, kTGetProperty, kTGetProperty,
	kTAddCollection, kTAddCollection, kTAddCollection, kTAddCollection, kTAddCollection, kTAddCollection, kTAddCollection, kTAddCollection,
	//kTUpdateCollection, kTUpdateCollection, kTUpdateCollection, kTUpdateCollection, kTUpdateCollection, kTUpdateCollection, kTUpdateCollection,
	kTDeleteCollection, kTDeleteCollection	
};

char const * const PITOperation::sTypeName[kTTotal] =
{
	"",
	"beginTransaction",
	"commit",
	"rollback",
	"createPINAndCommit",
	"deletePIN",
	"addProperty",
	"deleteProperty",
	"updateProperty",
	"getProperty",
	"addCollection",
	"updateCollection",
	"deleteCollection"
};
PITOperation::PITOperation(eType pType, size_t pIndex, char const * pParameters, int pFailureIndex, ITest* inTest)
	: mType(pType)
	, mIndex(pIndex)
	, mParameters(pParameters)
	, mFailureIndex(pFailureIndex)
	, mPINIndex(0)
	, mTest(inTest)
{
}
void PITOperation::run(PITScenario & pContext)
{
#if SMALL_DATA_SET
	// Verbose display of each step as it occurs
	pContext.outputMessage("running", this);
#endif

	if (mFailureIndex >= 0)
	{	
		pContext.outputMessage("setup failure", this);
		exit(pContext.failed()?1:0); // debuggerSetupCrash(NULL, mFailureIndex);
	}

	RC rc = RC_OK;
	ISession * const lSession = pContext.getSession();
	switch (mType)
	{
		case kTBeginTransaction:
			if (RC_OK != (rc=lSession->startTransaction())) {
				TVRC_R(rc,mTest);
				pContext.setFailed("run error", this);
			}
			else
				pContext.beginTransaction();
			break;
		case kTCommit:
			if (RC_OK != (rc= lSession->commit())) {
				TVRC_R(rc,mTest); pContext.setFailed("run error", this);
			}
			else
				pContext.endTransaction(true);
			break;
		case kTRollback:
			if (RC_OK != (rc=lSession->rollback())) {
				TVRC_R(rc,mTest); 
				pContext.setFailed("run error", this);
			}
			else
				pContext.endTransaction(false);
			break;
		case kTCreatePIN:
		{
			PID lPID;
			CREATEPIN(lSession, lPID, NULL, 0);
			if (STORE_INVALID_PID == LOCALPID(lPID))
				pContext.setFailed("run error", this);
			else
				pContext.registerPID(lPID);
			break;
		}
		case kTDeletePIN:
		{
			long lPIndex;
			assert(mParameters.length() > 0);
			sscanf(mParameters.c_str(), "%ld", &lPIndex);
			PID lPID = pContext.getPID(lPIndex);
			assert(STORE_INVALID_PID != LOCALPID(lPID));
			if (RC_OK != (rc=lSession->deletePINs(&lPID, 1))) {
				TVRC_R(rc,mTest);
				pContext.setFailed("run error", this);
			}
			else
				pContext.runDeletePID(lPID);
			break;
		}
		case kTAddProperty:
		{
			long lPIndex;
			PropertyID lPropId;
			char lString[MAX_PARAMETER_SIZE];
			lString[0] = 0;
			assert(mParameters.length() > 0);
			sscanf(mParameters.c_str(), "%ld %d %s", &lPIndex, &lPropId, &lString[0]);
			lPropId=mapProp(lSession,lPropId);
			PID const lPID = pContext.getPID(lPIndex);
			assert(STORE_INVALID_PID != LOCALPID(lPID));
			CmvautoPtr<IPIN> lPIN(lSession->getPIN(lPID));
			Value lV;
			SETVALUE(lV, lPropId, lString, OP_ADD);
			if (!lPIN.Get() || RC_OK != (rc=lPIN->modify(&lV, 1)))
			{
				TVRC_R(rc,mTest);
				mTest->getLogger().out() << "Modify failed pid " << std::hex << lPID.pid << " prop " << std::dec << lPropId << endl ;
				pContext.setFailed("run error", this);
			}
			break;
		}
		case kTDeleteProperty:
		case kTDeleteCollection:
		{
			long lPIndex;
			PropertyID lPropId;
			assert(mParameters.length() > 0);
			sscanf(mParameters.c_str(), "%ld %d", &lPIndex, &lPropId);
			lPropId=mapProp(lSession,lPropId);
			PID const lPID = pContext.getPID(lPIndex);
			assert(STORE_INVALID_PID != LOCALPID(lPID));
			CmvautoPtr<IPIN> lPIN(lSession->getPIN(lPID));
			Value lV;
			lV.setDelete(lPropId);
			if (RC_OK != (rc=lPIN->modify(&lV, 1)))
			{
				TVRC_R(rc,mTest);
				pContext.setFailed("run error", this);
			}
			break;
		}
		case kTUpdateProperty:
		{
			long lPIndex;
			PropertyID lPropId;
			char lString[MAX_PARAMETER_SIZE];
			lString[0] = 0;
			assert(mParameters.length() > 0);
			sscanf(mParameters.c_str(), "%ld %d %s", &lPIndex, &lPropId, &lString[0]);
			lPropId=mapProp(lSession,lPropId);
			PID const lPID = pContext.getPID(lPIndex);
			assert(STORE_INVALID_PID != LOCALPID(lPID));
			CmvautoPtr<IPIN> lPIN(lSession->getPIN(lPID));
			Value lV;
			SETVALUE(lV, lPropId, lString, OP_SET);
			if (!lPIN.Get() || RC_OK != (rc=lPIN->modify(&lV, 1)))
			{
				TVRC_R(rc,mTest);
				pContext.setFailed("run error", this);
			}
			break;
		}
		case kTGetProperty:
		{
			long lPIndex;
			PropertyID lPropId;
			assert(mParameters.length() > 0);
			sscanf(mParameters.c_str(), "%ld %d", &lPIndex, &lPropId);
			PID const lPID = pContext.getPID(lPIndex);
			assert(STORE_INVALID_PID != LOCALPID(lPID));
			CmvautoPtr<IPIN> lPIN(lSession->getPIN(lPID));
			lPropId=mapProp(lSession,lPropId);
			lPIN->getValue((PropertyID)lPropId);
			// TODO: If zealous, could test (like in the test pass).
			break;
		}
		case kTAddCollection:
		{
			long lPIndex;
			PropertyID lPropId;
			int lNumElements;
			char lString[MAX_PARAMETER_SIZE];
			lString[0] = 0;			
			std::vector<Tstring> lElements;
			assert(mParameters.length() > 0);
			sscanf(mParameters.c_str(), "%ld %d %d %s", &lPIndex, &lPropId, &lNumElements, &lString[0]);
			Tstring lElementStr = mParameters.substr(mParameters.find(lString), mParameters.length());
			tokenize(lElementStr,lElements,";");
			lPropId=mapProp(lSession,lPropId);
			PID const lPID = pContext.getPID(lPIndex);
			assert(STORE_INVALID_PID != LOCALPID(lPID));
			CmvautoPtr<IPIN> lPIN(lSession->getPIN(lPID));
			Value lV;
			SETVALUE(lV, lPropId, lElements[0].c_str(), OP_ADD);
			if (!lPIN.Get() || RC_OK != (rc=lPIN->modify(&lV, 1)))
			{
				TVRC_R(rc,mTest);
				pContext.setFailed("run error", this);
			}
			else
			{
				// Add remaining elements
				int k = 0;
				Value *lVal = new Value[lNumElements - 1];
				for(k = 0; k < lNumElements - 1; k++)
				{
					SETVALUE_C(lVal[k], lPropId, lElements[k+1].c_str(),OP_ADD, STORE_LAST_ELEMENT); 
				}
				// Workaround for BIG Collection problem
				#if !BIG_COLLECTION_WORKAROUND
					if (RC_OK != (rc=lPIN->modify(lVal, k)))
				#else
					lV.set(lVal,k); lV.setPropID(lPropId); lV.op = OP_ADD; lV.eid = STORE_LAST_ELEMENT;
					if (RC_OK != (rc=lPIN->modify(&lV, 1)))
				#endif
					{
						TVRC_R(rc,mTest);
						pContext.setFailed("run error", this);
					}
				delete[] lVal;
			}
			lElements.clear();
			break;
		}
		default:
			assert(false);
			break;
	}
}
void PITOperation::test(PITScenario & pContext, PITTestContext & pTestContext)
{
	ISession * const lSession = pContext.getSession();

	// In the forward pass we collect the PINs and properties that survive until the end of the scenario.
	if (PITTestContext::kPForward == pTestContext.getPass())
	{
		switch (mType)
		{
			case kTCreatePIN:
			{
				mPINIndex = pTestContext.nextNewPINIndex();
				pTestContext.addPINIndex(mPINIndex);
				break;
			}
			case kTDeletePIN:
			{
				long lPIndex;
				assert(mParameters.length() > 0);
				sscanf(mParameters.c_str(), "%ld", &lPIndex);
				pTestContext.removePINIndex(lPIndex);
				break;
			}
			case kTAddProperty:
			case kTAddCollection:
			{
				long lPIndex;
				PropertyID lPropId;
				assert(mParameters.length() > 0);
				sscanf(mParameters.c_str(), "%ld %d", &lPIndex, &lPropId);
				if(mType == kTAddCollection) pTestContext.addCollPropID(lPIndex, lPropId);
				else pTestContext.addPropID(lPIndex, lPropId);
				break;
			}
			case kTDeleteProperty:
			case kTDeleteCollection:
			{
				long lPIndex;
				PropertyID lPropId;
				assert(mParameters.length() > 0);
				sscanf(mParameters.c_str(), "%ld %d", &lPIndex, &lPropId);
				if(mType == kTDeleteCollection) pTestContext.removeCollPropID(lPIndex, lPropId);
				else pTestContext.removePropID(lPIndex, lPropId);
				break;
			}
			default:
				break;
		}
	}

	// In the backward pass we make sure that the db is in sync with the scenario.
	else
	{
		switch (mType)
		{
			case kTCreatePIN:
			{
				if (pTestContext.hasPIN(mPINIndex))
				{
					PID const lPID = pContext.getPID(mPINIndex);
					CmvautoPtr<IPIN> lPIN(lSession->getPIN(lPID));
					if (!lPIN.Get())
						pContext.setFailed("test error", this);
					else if (verboseTestPass())
						pContext.outputMessage("test succeeded: createPINAndCommit", this, 15);
				}
				break;
			}
			case kTDeletePIN:
			{
				long lPIndex;
				assert(mParameters.length() > 0);
				sscanf(mParameters.c_str(), "%ld", &lPIndex);
				if (pTestContext.hasPIN(lPIndex, true))
				{
					PID const lPID = pContext.getPID(lPIndex);
					CmvautoPtr<IPIN> lPIN(lSession->getPIN(lPID));
					if (lPIN.Get())
						pContext.setFailed("test error", this);
					else if (verboseTestPass())
						pContext.outputMessage("test succeeded: deletePIN", this, 15);
					pTestContext.donePINIndex(lPIndex);
				}
				break;
			}
			case kTAddProperty:
			case kTUpdateProperty:
			{
				long lPIndex;
				PropertyID lPropId;
				char lString[MAX_PARAMETER_SIZE];
				lString[0] = 0;
				assert(mParameters.length() > 0);
				sscanf(mParameters.c_str(), "%ld %d %s", &lPIndex, &lPropId, &lString[0]);
				if (pTestContext.hasPIN(lPIndex) && pTestContext.hasPropID(lPIndex, lPropId))
				{
					PropertyID lRealPropId=mapProp(lSession,lPropId);
					PID const lPID = pContext.getPID(lPIndex);
					assert(STORE_INVALID_PID != LOCALPID(lPID));
					CmvautoPtr<IPIN> lPIN(lSession->getPIN(lPID));
					Value const * lV = NULL;
					if (!lPIN.Get())
						pContext.setFailed("test error (pin not there)", this);
					else if (NULL == (lV = lPIN->getValue(lRealPropId)))
					{
						mTest->getLogger().out() << "Missing value on " << std::hex << lPID.pid << " prop " << std::dec << lPropId << endl ;
						pContext.setFailed("test error (value not there)", this);
					}
					else
					{
						size_t const lMinLen = min(strlen(lV->str), strlen(lString));
						if (0 != strcmp(lV->str, lString))
						{
							size_t i;
							for (i = 0; i < lMinLen && lString[i] == lV->str[i]; i++);
							std::basic_ostringstream<char> os;
							os << "test error (diff at character " << (long)i << " [" << (int)(unsigned char)lString[i] << "->" << (int)(unsigned char)lV->str[i] << "], ";
							os << "length [" << (long)strlen(lString) << "->" << (long)strlen(lV->str) << "], ";
							os << "actual value: " << lV->str << ")" << std::ends;
							pContext.setFailed(os.str().c_str(), this);
						}
						else if (verboseTestPass())
							pContext.outputMessage("test succeeded: modify property", this, 15);
					}
					pTestContext.donePropID(lPIndex, lPropId);
				}
				break;
			}
			case kTDeleteProperty:
			case kTDeleteCollection:
			{
				long lPIndex;
				PropertyID lPropId;
				assert(mParameters.length() > 0);
				sscanf(mParameters.c_str(), "%ld %d", &lPIndex, &lPropId);
				if (pTestContext.hasPIN(lPIndex) && pTestContext.hasPropID(lPIndex, lPropId, true))
				{
					PropertyID lRealPropId=mapProp(lSession,lPropId);
					PID const lPID = pContext.getPID(lPIndex);
					CmvautoPtr<IPIN> lPIN(lSession->getPIN(lPID));
					if (!lPIN.Get())
						pContext.setFailed("test error (pin not there)", this);
					else if (lPIN->getValue(lRealPropId))
						pContext.setFailed("test error (value still there)", this);
					else if (verboseTestPass())
						pContext.outputMessage("test succeeded: deleteProperty", this, 15);
					if (mType == kTDeleteCollection) pTestContext.doneCollPropID(lPIndex, lPropId);
					else pTestContext.donePropID(lPIndex, lPropId);
				}
				break;
			}
			case kTAddCollection:
			{
				long lPIndex;
				PropertyID lPropId;
				char lString[MAX_PARAMETER_SIZE];
				lString[0] = 0;
				int lNumElements = 0;
				assert(mParameters.length() > 0);
				sscanf(mParameters.c_str(), "%ld %d %d %s", &lPIndex, &lPropId, &lNumElements, &lString[0]);
				if (pTestContext.hasPIN(lPIndex) && pTestContext.hasPropID(lPIndex, lPropId))
				{
					std::vector<Tstring> lExpElements;
					Tstring lElementStr = mParameters.substr(mParameters.find(lString), mParameters.length());
					tokenize(lElementStr,lExpElements,";");			
					PID const lPID = pContext.getPID(lPIndex);
					assert(STORE_INVALID_PID != LOCALPID(lPID));
					CmvautoPtr<IPIN> lPIN(lSession->getPIN(lPID));
					Value const * lV = NULL;
					PropertyID lRealPropId=mapProp(lSession,lPropId);
					if (!lPIN.Get())
						pContext.setFailed("test error (pin not there)", this);
					else if (NULL == (lV = lPIN->getValue(lRealPropId)))
						pContext.setFailed("test error (value not there)", this);
					else
					{
						MvStoreEx::CollectionIterator lActElements(lPIN, lPropId) ;
						
						bool lSameSize = lNumElements == (int)lActElements.getSize();
 						bool lSuccess = true;

						for( const Value * lVal=lActElements.getFirst() ; 
							lVal != NULL ;
							lVal=lActElements.getNext())
						{
							int iT = lActElements.getCurrentPos() ;
							sprintf(lString, "%s", lExpElements[iT].c_str());
							assert(lVal->str != NULL);
							if (!lSameSize || 0 != strcmp(lVal->str, lString))
							{
								size_t i;
								size_t const lMinLen = min(strlen(lVal->str), strlen(lString));							
								for (i = 0; i < lMinLen && lString[i] == lVal->str[i]; i++);
								std::basic_ostringstream<char> os;
								os << "test error (diff at character " << (long)i << " [" << (int)(unsigned char)lString[i] << "->" << (int)(unsigned char)lVal->str[i] << "], ";
								os << "length [" << (long)strlen(lString) << "->" << (long)strlen(lVal->str) << "], ";
								os << "actual value: " << lVal->str << ")" << std::ends;
								pContext.setFailed(os.str().c_str(), this);
								lSuccess = false;
							}
						}
						if (lSuccess && verboseTestPass())
							pContext.outputMessage("test succeeded: modify property", this, 15);
					}
					pTestContext.doneCollPropID(lPIndex, lPropId);
				}
				break;
			}
			default:
				break;
		}
	}
}
void PITOperation::tokenize(Tstring &pStr, std::vector<Tstring> &pList, const char *pToken)
{
	Tstring lTmpStr = pStr;
	Tstring::size_type lPos = pStr.find(pToken);
	while(Tstring::npos != lPos)
	{
		Tstring lTmpVal = lTmpStr.substr(0, lPos);
		if(lTmpVal.length() > 0) pList.push_back(lTmpVal); 
		if(lPos + 1 < lTmpStr.length())
		{
			lTmpStr = lTmpStr.substr(lPos + 1, lTmpStr.length());
			lPos = lTmpStr.find(pToken);
		}
		else 
			break;
	}
	if(Tstring::npos == lPos && lTmpStr.length() > 0) pList.push_back(lTmpStr);
}
PITOperation::eType PITOperation::TypeFromName(char const * pTypeName)
{
	int i;
	for (i = 0; i < kTTotal; i++)
		if (0 == strcasecmp(pTypeName, sTypeName[i]))
			return eType(i);
	return eType(0);
}
#if 0 // Review (XXX): This type of approach would give more control over failure, but does not work this way (thunks).  Could be done with generic macro at beginning of some mvstore functions...
#define FAILURES_TXMGR \
	&AfyKernel::TxMgr::startAction, &AfyKernel::TxMgr::update, &AfyKernel::TxMgr::endAction
void * PITOperation::getFailure(eType pOp, int pIndex)
{
	void * lAddress = NULL;
	static HMODULE const lMvstore = (HMODULE)GetModuleHandle("affinity.dll");
	static void * const lLogMgr_insert = ::GetProcAddress(lMvstore, "?insert@LogMgr@AfyKernel@@QAA?AVLSN@2@W4LRType@2@KKZZ");
	switch (pOp)
	{
		case kTBeginTransaction:
		{
			static void * sOps[] = {&AfyKernel::TxMgr::startTx, lLogMgr_insert, FAILURES_TXMGR};
			return sOps[pIndex < (sizeof(sOps) / sizeof(sOps[0])) ? pIndex : 0];
		}
		case kTCommit:
		{
			static void * sOps[] = {&AfyKernel::TxMgr::commitTx, lLogMgr_insert, FAILURES_TXMGR};
			return sOps[pIndex < (sizeof(sOps) / sizeof(sOps[0])) ? pIndex : 0];
		}
		case kTRollback:
		{
			static void * sOps[] = {&AfyKernel::TxMgr::abortTx, lLogMgr_insert, FAILURES_TXMGR};
			return sOps[pIndex < (sizeof(sOps) / sizeof(sOps[0])) ? pIndex : 0];
		}
		case kTCreatePIN:
		case kTDeletePIN:
		case kTAddProperty:
		case kTDeleteProperty:
		case kTUpdateProperty:
		case kTGetProperty:
		{
			static void * sOps[] = {lLogMgr_insert, FAILURES_TXMGR};
			return sOps[pIndex < (sizeof(sOps) / sizeof(sOps[0])) ? pIndex : 0];
		}
		default:
			break;
	}
	return lAddress;
}
#endif

// PITScenario
PITScenario::~PITScenario()
{
	assert(!mSession && !mThread);
	size_t i;
	for (i = 0; i < mOperations.size(); i++)
		delete mOperations[i];
	mOperations.clear();
}
void PITScenario::run()
{
	assert(!mThread);
	createThread(&PITScenario::ThreadFunction, this, mThread);
}
bool PITScenario::test()
{
	mSession = MVTApp::startSession();

	// Discard all operations past the failure point (if any in this scenario).
	long i;
	bool lFailure = false;
	for (i = 0; i < (long)mOperations.size(); i++)
	{
		if (mOperations[i]->getFailureIndex() >= 0)
			lFailure = true;
		if (lFailure)
		{
			delete mOperations[i];
			mOperations[i] = NULL;
		}
	}

	// Discard all rolled-back or never completed transactions.
	PITOperation::eType lBraceOpener = PITOperation::kTNone;
	for (i = (long)mOperations.size() - 1; i >= 0; i--)
	{
		if (!mOperations[i])
			continue;

		PITOperation::eType const lType = mOperations[i]->getType();
		if (PITOperation::kTRollback == lType || PITOperation::kTCommit == lType)
			lBraceOpener = lType;

		if (PITOperation::kTBeginTransaction == lType)
		{
			long j;
			bool lDone = (PITOperation::kTRollback != lBraceOpener && PITOperation::kTNone != lBraceOpener);
			for (j = i; !lDone && j < (long)mOperations.size(); j++)
			{
				if (!mOperations[j])
					continue;
				lDone = (mOperations[j]->getType() == lBraceOpener);
				delete mOperations[j];
				mOperations[j] = NULL;
			}
			lBraceOpener = PITOperation::kTNone;
		}
	}

	// Forward pass to collect all relevant PINs and properties.
	PITTestContext lTestContext;
	for (i = 0; i < (long)mOperations.size() && !failed(); i++)
		if (mOperations[i])
			mOperations[i]->test(*this, lTestContext);

	// Backward pass to test their state in the database.
	lTestContext.nextPass();
	for (i = (long)mOperations.size() - 1; i >= 0 && !failed(); i--)
		if (mOperations[i])
			mOperations[i]->test(*this, lTestContext);

	// Cleanup.
	mSession->terminate();
	mSession = NULL;
	return !lFailure;
}
void PITScenario::wait()
{
	MVTestsPortability::threadsWaitFor(1, &mThread);
	#ifdef WIN32
		CloseHandle(mThread);
	#endif
	mThread = 0;
}
void PITScenario::setFailed(char const * pMessage, PITOperation const * pOp)
{
	sFailed = true;
	outputMessage(pMessage, pOp);
}
void PITScenario::outputMessage(char const * pMessage, PITOperation const * pOp, int pParamLen) const
{
	MVTestsPortability::MutexP const lLock(&sLockOutputDbg);
	mLogger.out() << "{scenario} [T" << mThreadAbstrID << "] ";
	mLogger.out() << pMessage << " ";
	mLogger.out() << PITOperation::sTypeName[pOp->getType()] << " ";
	if (pParamLen < 0)
		mLogger.out() << pOp->getParameters().c_str() << " ";
	else if (pParamLen > 0)
	{
		if ((size_t)pParamLen > pOp->getParameters().length())
			mLogger.out() << pOp->getParameters().c_str() << " ";
		else
			mLogger.out() << pOp->getParameters().substr(0, pParamLen).c_str() << "... ";
	}
	mLogger.out() << "op#" << (long)pOp->getIndex() << std::endl;
}
void PITScenario::endTransaction(bool pCommit)
{
	assert(mInTransaction);
	mInTransaction = false;

	if (!pCommit)
	{
		TPIDs::iterator i;
		for (i = mPIDsTransaction.begin(); mPIDsTransaction.end() != i; i++)
			if (STORE_INVALID_PID != LOCALPID((*i)))
				unregisterPID(*i);
		mDeletedPIDs &= ~mDeletedPIDsTransaction;
	}
	mPIDsTransaction.clear();
	mDeletedPIDsTransaction.reset();
}
void PITScenario::registerPID(PID pPID)
{
	assert(STORE_INVALID_PID != LOCALPID(pPID));
	//assert(mPIDs.end() == (std::find<TPIDs::iterator, PID>(mPIDs.begin(), mPIDs.end(), pPID)));
	TPIDs::iterator const i = std::find<TPIDs::iterator, PID>(mPIDs.begin(), mPIDs.end(), gInvalidPID);
	if (mPIDs.end() != i)
	{
		(*i) = pPID;
		TPIDs::iterator::difference_type const lIndex = i - mPIDs.begin();
		mDeletedPIDs.reset(lIndex);
	}
	else
	{
		mPIDs.push_back(pPID);
		mDeletedPIDs.reset(mPIDs.size() - 1);
	}
	if (mInTransaction)
		mPIDsTransaction.push_back(pPID);

	if (sLockOutputPINs)
	{
		sLockOutputPINs->lock();
		(*sOutputPINs) << mThreadAbstrID << " " << LOCALPID(pPID) << std::endl;
		sLockOutputPINs->unlock();
	}
}
void PITScenario::unregisterPID(PID pPID)
{
	assert(STORE_INVALID_PID != LOCALPID(pPID));
	assert(!mInTransaction);
	TPIDs::iterator const i = std::find<TPIDs::iterator, PID>(mPIDs.begin(), mPIDs.end(), pPID);
	assert(mPIDs.end() != i);
	(*i) = gInvalidPID;

	if (sLockOutputPINs)
	{
		sLockOutputPINs->lock();
		(*sOutputPINs) << "*" << mThreadAbstrID << " " << LOCALPID(pPID) << std::endl;
		sLockOutputPINs->unlock();
	}
}
void PITScenario::runDeletePID(PID pPID)
{
	// Note: Should only be invoked during the run pass...
	assert(STORE_INVALID_PID != LOCALPID(pPID));
	TPIDs::iterator i;
	int lIndex;
	for (i = mPIDs.begin(), lIndex = 0; i != mPIDs.end(); i++, lIndex++)
	{
		if ((*i) != pPID)
			continue;
		mDeletedPIDs.set(lIndex);

		if (mInTransaction)
			mDeletedPIDsTransaction.set(lIndex);
	}
}
void PITScenario::ThreadFunctionImplementation()
{
	mSession = MVTApp::startSession(mStoreCtx);

	size_t i;
	for (i = 0; i < mOperations.size(); i++)
		mOperations[i]->run(*this);
	
	mSession->terminate();
	mSession = NULL;
}

// TestScenario
void TestScenarioBase::cleanup(TScenarii & pScenarii)
{
	size_t i;
	for (i = 0; i < pScenarii.size(); i++)
		delete pScenarii[i];
	pScenarii.clear();
}
bool TestScenarioBase::parseScenarioFile(char const * pFileName, std::vector<PITScenario *> & pScenarii)
{
	// Open the file and extract its scenarii.
	// Each line in the file is of the form: thread,operation,parameters
	Tifstream is(pFileName);
	if ( !is.is_open() )
	{
		cerr << "Invalid filename " << pFileName << endl ;
		return false; 
	}

	Tstring lLineS;
	int lFailureIndex = -1;
	char lLine[MAX_LINE_SIZE];
	while (!is.eof())
	{
		// Get the next line.
		lLine[0] = 0;
		is.getline(lLine, MAX_LINE_SIZE);
		if (0 == strlen(lLine))
			continue;

		// Determine if this operation must crash (starting with '*'), or if it's a comment.
		bool lCrash = false;
		int lStartOfLine = 0;
		if (!isdigit(lLine[0]))
		{
			if (lLine[0] == '*')
			{
				lCrash = true;
				lStartOfLine++;
			}
			else if (lLine[0] == '!')
			{
				sscanf(&lLine[1], "%d", &lFailureIndex);
				continue;
			}
			else // Allow to comment out a line in a scenario, for testing purposes...
				continue;
		}

		// Extract the fields.
		long lThreadAbstrID;
		char lOperation[128];
		char * lParameters = "";
		lOperation[0] = 0;
		sscanf(&lLine[lStartOfLine], "%ld %s", &lThreadAbstrID, lOperation);
		char * lOpOff = strstr(lLine, lOperation);
		if (lOpOff - lLine + strlen(lOperation) + 1 < strlen(lLine))
			lParameters = lOpOff + strlen(lOperation) + 1;

		// Determine the type of operation.
		PITOperation::eType const lOType = PITOperation::TypeFromName(lOperation);
		if (PITOperation::kTNone == lOType)
		{
			mLogger.out() << "{scenario} Unknown operation: " << lLine << std::endl;
			continue;
		}

		// Determine the scenario correpsonding to that thread, or create one.
		size_t lScenario;
		for (lScenario = 0; lScenario < pScenarii.size(); lScenario++)
			if (pScenarii[lScenario]->getThreadAbstrID() == lThreadAbstrID)
				break;
		if (lScenario >= pScenarii.size())
			pScenarii.push_back(new PITScenario(mLogger, lThreadAbstrID));

		// Create the operation and register it in the scenario.
		PITOperation * const lOp = new PITOperation(lOType, pScenarii[lScenario]->getNumOperations(), lParameters, lCrash ? lFailureIndex : -1, this);
		pScenarii[lScenario]->addOperation(lOp);
	}
	return (pScenarii.size() > 0);
}
bool TestScenarioBase::parsePINsFile(char const * pFileName, std::vector<PITScenario *> & pScenarii)
{
	// Open the file and extract the PIDs created by our scenario.
	// Each line in the file is of the form: thread,PID
	Tifstream lIs(pFileName);
	char lLine[MAX_LINE_SIZE];
	while (!lIs.eof())
	{
		// Get the next line.
		lLine[0] = 0;
		lIs.getline(lLine, MAX_LINE_SIZE);
		if (0 == strlen(lLine))
			continue;

		// Handle removed PINs (due to rollback/delete).
		bool lRemove = false;
		int lStartOfLine = 0;
		if (!isdigit(lLine[0]))
		{
			lRemove = true;
			lStartOfLine++;
		}

		// Extract the fields.
		long lThreadAbstrID;
		PID lPID;
		INITLOCALPID(lPID);
		// The implementation of sscanf(...) is more strict under OSX 
		// see 'man sscanf' for details
		sscanf(&lLine[lStartOfLine], "%ld %lld", &lThreadAbstrID, &LOCALPID(lPID));

		// Add the PID to the right scenario.
		bool lHandled = false;
		size_t lScenario;
		for (lScenario = 0; lScenario < pScenarii.size(); lScenario++)
		{
			if (pScenarii[lScenario]->getThreadAbstrID() == lThreadAbstrID)
			{
				lHandled = true;
				if (lRemove)
					pScenarii[lScenario]->unregisterPID(lPID);
				else
					pScenarii[lScenario]->registerPID(lPID);
			}
		}
		if (!lHandled)
			return false;
	}
	return true;
}

// Implement this test (3 parts).
int TestScenarioGen::execute()
{
	size_t lNumThreads;   // = atoi(pNumThreads);
	size_t lNumOperations; // = atoi(pNumOperations);
	bool fColls;   // = atoi(pCollections)!=0;
	bool lFailure; // = (pFailure == strstr(pFailure, "fail"));
    string pScFileName; 
	
	if(!mpArgs->get_param("scenfile", pScFileName))
	{
		pScFileName="scenario.dat";
		mLogger.out() << "No --scenfile parameter, defaulting to " << pScFileName << endl;
	}
	if(!mpArgs->get_param("numthreads", lNumThreads))
	{
		lNumThreads=3;
		mLogger.out() << "No --numthreads parameter, defaulting to " << (int)lNumThreads << endl;
	}

	if(!mpArgs->get_param("numoperations",lNumOperations))
	{
		lNumOperations=300;
		mLogger.out() << "No --numoperations parameter, defaulting to " << (int)lNumOperations << endl;
	}

	if(!mpArgs->get_param("--collections", fColls))
	{
		fColls=false;
		mLogger.out() << "No --collections parameter - not using collections" << endl;
	}
	
	if(!mpArgs->get_param("--failure", lFailure))
	{
		lFailure=false;
		mLogger.out() << "No --failure parameter, scenario will not crash" << endl;
	}
	
	
	Tofstream lOs(pScFileName.c_str(), std::ios::ate);
	lOs << "# Randomly generated scenario: " << (long)lNumThreads << " threads, " << (long)lNumOperations << " ops" << (lFailure ? ", with failure." : ".") << std::endl;
	if (!lNumThreads || !lNumOperations)
	{
		mLogger.out() << "{scenario} Wrong parameters for gen!" << std::endl;
		return 1;
	}

	size_t const lFailureIndex = (size_t)(100.0 * rand() / RAND_MAX);
	size_t const lCrashingOp = lNumOperations > 10 ? (5 + (size_t)(double(lNumOperations - 5) * rand() / RAND_MAX)) : lNumOperations - 1;
	if (lFailure)
		lOs << "!" << (long)lFailureIndex << " (failure index)" << std::endl;

	Tstring lRandomS;
	size_t lT;
	for (lT = 0; lT < lNumThreads; lT++)
	{
		PITDomain lDomain, lUndoDomain;
		size_t lOutOfTransaction = 0;
		static size_t const lOutOfTransactionThreshold = 5;
		bool lInTransaction = false;
		size_t lO;
		static long const sNumWeighted = fColls?(sizeof(PITOperation::sCollWeighted) / sizeof(PITOperation::sCollWeighted[0])):(sizeof(PITOperation::sWeighted) / sizeof(PITOperation::sWeighted[0]));
		for (lO = 1; lO < lNumOperations; lO++)
		{
			// Choose the next operation randomly.
			long const lWIndex = (long)(double(sNumWeighted - 1.0) * rand() / RAND_MAX);
			assert(lWIndex >= 0 && lWIndex < sNumWeighted);
			PITOperation::eType lType = (0 != lDomain.getNumPINs()) ? (fColls?PITOperation::sCollWeighted[lWIndex]:PITOperation::sWeighted[lWIndex]) : PITOperation::kTCreatePIN;
			assert(lType > PITOperation::kTNone && lType < PITOperation::kTTotal);

			// Make sure begin-end of transactions are matched correctly (no reentrance for the moment).
			if (PITOperation::kTBeginTransaction == lType && lInTransaction)
			{
				lType = PITOperation::kTCommit;
				lInTransaction = false;
			}
			else if (PITOperation::kTCommit == lType || PITOperation::kTRollback == lType)
			{
				if (!lInTransaction)
					lType = PITOperation::kTBeginTransaction;
				lInTransaction = false;
			}

			// Threshold for number of out-of-transaction operations.
			if (!lInTransaction)
			{
				lOutOfTransaction++;
				if (lOutOfTransaction > lOutOfTransactionThreshold)
					lType = PITOperation::kTBeginTransaction;
			}

			// Account for new transaction.
			if (PITOperation::kTBeginTransaction == lType)
			{
				lInTransaction = true;
				lOutOfTransaction = 0;
			}

			switch (lType)
			{
				case PITOperation::kTBeginTransaction:
				case PITOperation::kTCommit:
					if (0 == lT && lFailure && lCrashingOp == lO)
						lOs << "*";
					lOs << (long)lT << " " << PITOperation::sTypeName[lType] << std::endl;
					lUndoDomain = lDomain;
					break;
				case PITOperation::kTRollback:
					if (0 == lT && lFailure && lCrashingOp == lO)
						lOs << "*";
					lOs << (long)lT << " " << PITOperation::sTypeName[lType] << std::endl;
					lDomain = lUndoDomain;
					lUndoDomain.clear();
					break;
				case PITOperation::kTCreatePIN:
				{
					if (0 == lT && lFailure && lCrashingOp == lO)
						lOs << "*";
					lOs << (long)lT << " " << PITOperation::sTypeName[lType] << std::endl;
					int const lNextPIN = lDomain.getFirstFreePINIndex();
					lDomain.addPINIndex(lNextPIN);
					break;
				}
				case PITOperation::kTDeletePIN:
				{
					long const lPINIndex = lDomain.getAnyPINIndex();
					assert(lPINIndex >= 0);
					if (0 == lT && lFailure && lCrashingOp == lO)
						lOs << "*";
					lOs << (long)lT << " " << PITOperation::sTypeName[lType] << " " << lPINIndex << std::endl;
					lDomain.removePINIndex(lPINIndex);
					break;
				}
				case PITOperation::kTAddProperty:
				{
					long const lPINIndex = lDomain.getAnyPINIndex();
					assert(lPINIndex >= 0);
					#if 1
						// Basic: Properties added in sequential order.
						long const lPropID = lDomain.getFirstFreePropID(lPINIndex);
					#else
						// Variation 1: Properties added in random order.
						long lPropID, lTrials = 0;
						do { lPropID = (long)(1000.0 * rand() / RAND_MAX); lTrials++; } while (lTrials < 10 && lDomain.hasPropID(lPINIndex, lPropID));
						if (lDomain.hasPropID(lPINIndex, lPropID))
							lPropID = lDomain.getFirstFreePropID(lPINIndex);
					#endif
					if (lDomain.getNumPropIDs(lPINIndex) < MAX_PROPERTIES_PER_PIN && lPropID != -1)
					{
						lDomain.addPropID(lPINIndex, lPropID);
						#if 0
							MVTRand::getString(lRandomS,1,MAX_PARAMETER_SIZE,false,true);
						#elif SMALL_DATA_SET
							MVTRand::getString(lRandomS,1,25,true,true);
						#else
							MVTRand::getString(lRandomS);
						#endif
						if (0 == lT && lFailure && lCrashingOp == lO)
							lOs << "*";
						lOs << (long)lT << " " << PITOperation::sTypeName[lType] << " ";
						lOs << lPINIndex << " ";
						lOs << 100 + lPropID << " ";
						lOs << lRandomS.c_str() << std::endl;
					}
					else
						lO--;
					break;
				}
				case PITOperation::kTUpdateProperty:
				{
					long const lPINIndex = lDomain.getAnyPINIndex();
					assert(lPINIndex >= 0);
					long const lPropID = lDomain.getAnyNonCollPropID(lPINIndex);
					if (lPropID >= 0)
					{
						#if 0
							MVTRand::getString(lRandomS,1,MAX_PARAMETER_SIZE,false,true);
						#elif SMALL_DATA_SET
							MVTRand::getString(lRandomS,1,25,true,true);
						#else
							MVTRand::getString(lRandomS);
						#endif
						if (0 == lT && lFailure && lCrashingOp == lO)
							lOs << "*";
						lOs << (long)lT << " " << PITOperation::sTypeName[lType] << " ";
						lOs << lPINIndex << " ";
						lOs << 100 + lPropID << " ";
						lOs << lRandomS.c_str() << std::endl;
					}
					else
						lO--;
					break;
				}
				case PITOperation::kTDeleteProperty:
				{
					long const lPINIndex = lDomain.getAnyPINIndex();
					assert(lPINIndex >= 0);
					long const lPropID = lDomain.getAnyNonCollPropID(lPINIndex);
					if (lPropID >= 0)
					{
						if (0 == lT && lFailure && lCrashingOp == lO)
							lOs << "*";
						lOs << (long)lT << " " << PITOperation::sTypeName[lType] << " ";
						lOs << lPINIndex << " ";
						lOs << 100 + lPropID << std::endl;
						lDomain.removePropID(lPINIndex, lPropID);
					}
					else
						lO--;
					break;
				}
				case PITOperation::kTGetProperty:
				{
					long const lPINIndex = lDomain.getAnyPINIndex();
					assert(lPINIndex >= 0);
					long const lPropID = lDomain.getAnyPropID(lPINIndex);
					if (lPropID >= 0)
					{
						if (0 == lT && lFailure && lCrashingOp == lO)
							lOs << "*";
						lOs << (long)lT << " " << PITOperation::sTypeName[lType] << " ";
						lOs << lPINIndex << " ";
						lOs << 100 + lPropID << std::endl;
					}
					else
						lO--;
					break;
				}
				case PITOperation::kTAddCollection:
				{
					long const lPINIndex = lDomain.getAnyPINIndex();
					assert(lPINIndex >= 0);
					#if 1
						// Basic: Properties added in sequential order.
						long const lPropID = lDomain.getFirstFreePropID(lPINIndex);
					#else
						// Variation 1: Properties added in random order.
						long lPropID, lTrials = 0;
						do { lPropID = (long)(1000.0 * rand() / RAND_MAX); lTrials++; } while (lTrials < 10 && lDomain.hasPropID(lPINIndex, lPropID));
						if (lDomain.hasPropID(lPINIndex, lPropID))
							lPropID = lDomain.getFirstFreePropID(lPINIndex);
					#endif
					if (lDomain.getNumPropIDs(lPINIndex) < MAX_PROPERTIES_PER_PIN && lPropID != -1)
					{
						lDomain.addCollPropID(lPINIndex, lPropID);
						std::vector<Tstring> lCollElements;
						int i = 0, lNumElements = MVTRand::getRange(MIN_ELEMENTS,MAX_ELEMENTS);/*fColls?MVTRand::getRange(MIN_ELEMENTS,MAX_ELEMENTS):MVTRand::getRange(3,5);*/
						int lMaxElementSize = (int)(int(MAX_PARAMETER_SIZE)/lNumElements);
						// And the seperator used?
						#if 0
							lMaxElementSize-=lNumElements; 
						#endif
						for(i = 0 ; i< lNumElements; i++)							
						{
							MVTRand::getString(lRandomS, int(lMaxElementSize/6), lMaxElementSize, true, true);
							lCollElements.push_back(lRandomS);
						}

						if (0 == lT && lFailure && lCrashingOp == lO)
							lOs << "*";
						lOs << (long)lT << " " << PITOperation::sTypeName[lType] << " ";
						lOs << lPINIndex << " ";
						lOs << 100 + lPropID << " ";
						lOs << lNumElements;
						for(i = 0; i < lNumElements; i++)
						    lOs << " " << lCollElements[i].c_str() << ";";
						lOs << std::endl;
						lCollElements.clear();
					}
					else
						lO--;
					break;
				}
				case PITOperation::kTDeleteCollection:
				{
					long const lPINIndex = lDomain.getAnyPINIndex();
					assert(lPINIndex >= 0);
					long const lPropID = lDomain.getAnyCollPropID(lPINIndex);
					if (lPropID >= 0)
					{
						if (0 == lT && lFailure && lCrashingOp == lO)
							lOs << "*";
						lOs << (long)lT << " " << PITOperation::sTypeName[lType] << " ";
						lOs << lPINIndex << " ";
						lOs << 100 + lPropID << std::endl;
						lDomain.removeCollPropID(lPINIndex, lPropID);
					}
					else
						lO--;
					break;
				}
				
				default:
					assert(false);
					break;
			}
		}
		if (lInTransaction)
			lOs << (long)lT << " " << PITOperation::sTypeName[PITOperation::kTCommit] << std::endl;
	}
	return 0;
}

int TestScenarioRun::execute()
{
	// Parse the file for scenarii.
    string pScFileName; string pPINsFileName; 

	if(!mpArgs->get_param("scenfile",pScFileName))
	{
		pScFileName="scenario.dat";
		mLogger.out() << "No --scenfile parameter, defaulting to " << pScFileName << endl;
    }
	if(!mpArgs->get_param("pinfile",pPINsFileName))
	{
		pPINsFileName="scenario.pins";
		mLogger.out() << "No --pinfile parameter, defaulting to " << pPINsFileName << endl;
    }
		
	TestLogger lOutV(TestLogger::kDStdOutVerbose);
	lOutV.out() << "{scenario} Reading scenarii..." << std::endl;
	TScenarii lScenarii;
	
	if (!parseScenarioFile(pScFileName.c_str(), lScenarii))
	{
		mLogger.out() << "{scenario} Could not parse the scenario file <" << pScFileName << ">" << std::endl;
		return 1;
	}

	// Produce an output file for resulting PIDs.
	Tofstream lOs(pPINsFileName.c_str(), std::ios::ate);
	MVTestsPortability::Mutex lLockOs;
	PITScenario::sOutputPINs = &lOs;
	PITScenario::sLockOutputPINs = &lLockOs;

	// Open the store.
	bool lSuccess = true;
	lOutV.out() << "{scenario} Opening mvstore..." << std::endl;
	if (MVTApp::startStore())
	{
		mStoreCtx = MVTApp::getStoreCtx();

		// Run the scenarii.
		lOutV.out() << "{scenario} Running scenarii..." << std::endl;
		size_t i;
		for (i = 0; i < lScenarii.size(); i++)
		{
			lScenarii[i]->setStoreCtx(mStoreCtx);
			lScenarii[i]->run();
		}
		for (i = 0; i < lScenarii.size(); i++)
		{
			lScenarii[i]->wait();
			lSuccess = lSuccess && !lScenarii[i]->failed();
		}

		// Cleanup.
		lOutV.out() << "{scenario} Cleanup..." << std::endl;
		cleanup(lScenarii);
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }

	lOutV.out() << "{scenario} Done." << std::endl;
	return lSuccess ? 0 : 1;
}

int TestScenarioTest::execute()
{
	// Parse the file for scenarii.
    string pScFileName; string pPINsFileName; 

	if(!mpArgs->get_param("scenfile",pScFileName))
	{
		pScFileName="scenario.dat";
		mLogger.out() << "No --scenfile parameter, defaulting to " << pScFileName << endl;
    }
	if(!mpArgs->get_param("pinfile",pPINsFileName))
	{
		pPINsFileName="scenario.pins";
		mLogger.out() << "No --pinfile parameter, defaulting to " << pPINsFileName << endl;
    }
	
	TestLogger lOutV(TestLogger::kDStdOutVerbose);
	lOutV.out() << "{scenario} Reading scenarii..." << std::endl;
	TScenarii lScenarii;
	if (!parseScenarioFile(pScFileName.c_str(), lScenarii))
	{
		mLogger.out() << "{scenario} Could not parse the scenario file <" << pScFileName << ">" << std::endl;
		return 1;
	}

	// Parse the file for the PINs generated while running the scenarii.
	lOutV.out() << "{scenario} Reading PINs..." << std::endl;
	if (!parsePINsFile(pPINsFileName.c_str(), lScenarii))
	{
		mLogger.out() << "{scenario} Could not parse the PINs file <" << pPINsFileName << ">" << std::endl;
		return 1;
	}

	// Backup the store (but not feasible if running against s3)
	if ( !isS3IO())
		MVTUtil::backupStoreFiles() ;

	// Open the store.
	bool lSuccess = true;
	lOutV.out() << "{scenario} Opening mvstore..." << std::endl;
	if (MVTApp::startStore())
	{
		// Test the scenarii.
		lOutV.out() << "{scenario} Testing scenarii..." << std::endl;
		size_t i;
		bool lContinueTesting = true; // Review: Assumes that if there was a programmed failure, it was in the first scenario...
		for (i = 0; i < lScenarii.size() && lSuccess && lContinueTesting; i++)
		{
			lContinueTesting = lScenarii[i]->test();
			lSuccess = lSuccess && !lScenarii[i]->failed();
		}

		// Cleanup.
		lOutV.out() << "{scenario} Cleanup..." << std::endl;
		cleanup(lScenarii);
		MVTApp::stopStore();
	}

	lOutV.out() << "{scenario} Done." << std::endl;
	return lSuccess ? 0 : 1;
}
