/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

// Demonstates example of merge to perform an common app scenario : FT + order by date
// (where date is indexed in a family)
//
// It can also act as a basic performance test
//
// (for purpose of demonstration it does not attempt to cover many different scenarios
// see testmerge2 and other tests for more systematic approaches)

// Note: because pin generation takes so long this test will reuse an existing store
// in subsequent executions.  To allow this to happen the randomness has been a bit
// crippled.

//TIP: use this command to see the profiler analysis:
//tests testmergesimple -ioinit={stdio}{ioprofiler} -nbuf=2000

#include "app.h"
#include "mvauto.h" // MV auto pointers
#include "collectionhelp.h"			// If reading collections
#include "teststream.h"				// If using streams
using namespace std;

#define CNT_PINS 30000 // Note (maxw, Dec2010): Used to be 100000, but for practical reasons I reduced to 30000
#define STOP_START_STORE 1 // For more fair perf comparison, flush all buffers before each query

// WORDS_PER_PROP / CNT_UNIQUE_WORDS defines % of CNT_PINS to expect in result
#define CNT_UNIQUE_WORDS 200
#define WORDS_PER_PROP 100 // How many words each pin has, e.g is this an email or a category?

class TestMergeSimple : public ITest
{
	public:
		TEST_DECLARE(TestMergeSimple);

		// By convention all test names start with test....
		virtual char const * getName() const { return "testmergesimple"; }
		virtual char const * getDescription() const { return "Demonstation of single merge query"; }
		virtual char const * getHelp() const { return ""; } // Optional
		
		virtual int execute();
		virtual void destroy() { delete this; }
		
		// Note (maxw, Dec2010): it is a performance test... could run in a separate category/suite.
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Takes too long to run"; return false; }
		virtual bool isLongRunningTest() const { return true; } // Depends on CNT_PINS
	protected:
		void doTest() ;
		bool createMeta();
		void createPins();
		unsigned long doMerge(uint64_t startDate, uint64_t endDate, const char * inFT);
		unsigned long doSimpleQuery(uint64_t startDate, uint64_t endDate, const char * inFT);

		void stopStartStore();
	private:	
		ISession * mSession;
		ClassID mClassFamily;
		PropertyID mTextProp;
		PropertyID mDateProp;

		RandStrPool mStringPool ; // Fixed number of random strings
};
TEST_IMPLEMENT(TestMergeSimple, TestLogger::kDStdOut);

int TestMergeSimple::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }
	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;
	doTest() ;
	mSession->terminate() ;
	MVTApp::stopStore() ;
	return RC_OK ;
}

void TestMergeSimple::doTest()
{
	if (createMeta())
		createPins();

	// Searching entire range of dates, e.g. no pins should be filtered out
	// I think this is realistic with common app usage (e.g. we just want to sort
	// by date).  But this could also restrict a range of dates
	uint64_t startDate = 0, endDate=((uint64_t)~0ull);

	const char * randWord = mStringPool.getStrRef().c_str();

#if STOP_START_STORE	
	stopStartStore();
#endif

	mLogger.out() << "-------------------------------" << endl;

	// remove temporarily
	//MVTApp::sDynamicLinkMvstore->setIoParam(MVTApp::Suite().mStoreCtx,"profilereset","");

	long a,b;
	
	a = getTimeInMs();
	unsigned long cnt1 = doMerge(startDate,endDate,randWord);
	b = getTimeInMs();
	mLogger.out() << "Merge query found " << cnt1 << " results - " << b-a << " ms" << endl;

	// remove temporarily
	//MVTApp::sDynamicLinkMvstore->setIoParam(MVTApp::Suite().mStoreCtx,"profilereport","");
	//MVTApp::sDynamicLinkMvstore->setIoParam(MVTApp::Suite().mStoreCtx,"profilereset","");

	mLogger.out() << "-------------------------------" << endl;

	// Second execution (takes advantage of cached pages)
	a = getTimeInMs();
	unsigned long cnt1_b = doMerge(startDate,endDate,randWord);
	b = getTimeInMs();
	mLogger.out() << "Second run: Merge query found " << cnt1_b << " results - " << b-a << " ms" << endl;

	// remove temporarily
	//MVTApp::sDynamicLinkMvstore->setIoParam(MVTApp::Suite().mStoreCtx,"profilereport","");

	mLogger.out() << "-------------------------------" << endl;

#if STOP_START_STORE	
	stopStartStore();
#endif

	// remove temporarily
	//MVTApp::sDynamicLinkMvstore->setIoParam(MVTApp::Suite().mStoreCtx,"profilereset","");

	a = getTimeInMs();
	unsigned long cnt2 = doSimpleQuery(startDate,endDate,randWord);
	b = getTimeInMs();
	mLogger.out() << "Simple query found " << cnt2 << " results - " << b-a << " ms" << endl;

	// remove temporarily
	//MVTApp::sDynamicLinkMvstore->setIoParam(MVTApp::Suite().mStoreCtx,"profilereport","");
	//MVTApp::sDynamicLinkMvstore->setIoParam(MVTApp::Suite().mStoreCtx,"profilereset","");

	mLogger.out() << "-------------------------------" << endl;

	a = getTimeInMs();
	unsigned long cnt2_b = doSimpleQuery(startDate,endDate,randWord);
	b = getTimeInMs();
	mLogger.out() << "Second run: Simple query found " << cnt2_b << " results - " << b-a << " ms" << endl;

	// remove temporarily
	//MVTApp::sDynamicLinkMvstore->setIoParam(MVTApp::Suite().mStoreCtx,"profilereport","");

	// TODO - confirm PINs were same and in same order
	TVERIFY(cnt1==cnt2);
}

bool TestMergeSimple::createMeta()
{
	// Create properties, random data and family used by test
	// returns false if the store already contains this data,
	// (in which case we can assume it also contains the pins)

	srand(0); // Trick to ensure the words are the same even in multiple runs
	mStringPool.Init(CNT_UNIQUE_WORDS/*different words*/, 
		5, 10,  /*word length*/
		false /*no spaces in each word*/);
	srand(mRandomSeed);

	// Not using getRandProp in order to allow the same store to be used more than
	// once.  These functions only create if necessary
	mTextProp=MVTUtil::getProp(mSession,"TestMergeSimple_text");
	mDateProp=MVTUtil::getProp(mSession,"TestMergeSimple_date");

	mClassFamily=MVTUtil::getClass(mSession,"TestMergeSimple_datefamily");
	if ( mClassFamily != STORE_INVALID_CLASSID )
	{
		// Class has already been defined, e.g. store is already ready for the test
		return false;
	}

	// Create index over the date property
	CmvautoPtr<IStmt> qClassFamilyDef(mSession->createStmt());
	unsigned char v=qClassFamilyDef->addVariable();
	
	Value exprVals[2];
	exprVals[0].setVarRef(0,mDateProp);
	exprVals[1].setParam(0);

	CmvautoPtr<IExprTree> expr(mSession->expr(OP_IN,2,exprVals));
	qClassFamilyDef->addCondition(v,expr);

	TVERIFYRC(defineClass(mSession,"TestMergeSimple_datefamily", qClassFamilyDef));
	mClassFamily=MVTUtil::getClass(mSession,"TestMergeSimple_datefamily");
	return true;
}

void TestMergeSimple::createPins()
{
	typedef std::vector<IPIN *> TPINs;
	int const lBatchSize = 100;
	for (int i=0 ; i<CNT_PINS/lBatchSize; i++ )
	{
		TPINs lPINs;
		for (int j=0 ; j<lBatchSize; j++ )
		{
			Value v[2]; 
			
			v[0].setDateTime(MVTRand::getDateTime(mSession,true/*allow future*/)); 
			v[0].property=mDateProp;		// For index and class membership
			
			// Pick several words from the pool and make a fake sentence
			// If any of them match the FT word that the query looks for then
			// the pin will match
			string strRandWords;
			for ( int k = 0 ; k < WORDS_PER_PROP ; k++ )
			{
				strRandWords += mStringPool.getStr();
				strRandWords += " " ;
			}

			v[1].set(strRandWords.c_str()); 
			v[1].property=mTextProp; // For FT lookup

			IPIN * lP = mSession->createPIN(v,2,MODE_COPY_VALUES);
			if (lP)
				lPINs.push_back(lP);
		}
		TVERIFY(lPINs.size() == size_t(lBatchSize));
		TVERIFYRC(mSession->commitPINs(&lPINs[0], lPINs.size()));
		for (TPINs::iterator iP = lPINs.begin(); lPINs.end() != iP; iP++)
			(*iP)->destroy();
		lPINs.clear();
		mLogger.out() << "." << std::flush;
	}
	mLogger.out() << endl << "Created " << CNT_PINS << " pins" << endl;
}

unsigned long TestMergeSimple::doMerge(uint64_t startDate, uint64_t endDate, const char * inFT)
{
	// Demonstration of a query that does a QRY_INTERSECT between
	// an Date range index scan and an FT search

	CmvautoPtr<IStmt> q(mSession->createStmt());

	unsigned char vL, vR;
	
	{
		Value range;
		Value minmax[2];
		minmax[0].setDateTime(startDate);
		minmax[1].setDateTime(endDate);

		range.setRange(minmax);
		range.property=mDateProp;

		SourceSpec cs={mClassFamily,1,&range};
		vL = q->addVariable(&cs, 1);
	}

	{
		vR = q->addVariable();
		q->addConditionFT(vR,inFT);
	}

	q->setOp(vL,vR,QRY_INTERSECT);

	// Expected to be optimized with no extra pin loading or sorting
	// because the family we use gives results in this order
	OrderSeg ord={NULL,mDateProp,0,0,0};
	TVERIFYRC(q->setOrder(&ord,1));

	uint64_t cnt = 0;
	TVERIFYRC(q->count(cnt));

	return (unsigned long)cnt;
}

unsigned long TestMergeSimple::doSimpleQuery(uint64_t startDate, uint64_t endDate, const char * inFT)
{
	// For comparison sake, the same query with no merge operation

	CmvautoPtr<IStmt> q(mSession->createStmt());

	Value range;
	Value minmax[2];
	minmax[0].setDateTime(startDate);	
	minmax[1].setDateTime(endDate);

	range.setRange(minmax);
	range.property=mDateProp;

	SourceSpec cs={mClassFamily,1,&range};
	unsigned char singleVar = q->addVariable(&cs, 1);

	q->addConditionFT(singleVar,inFT);

	uint64_t cnt = 0;
	TVERIFYRC(q->count(cnt));

	return (unsigned long)cnt;
}

void TestMergeSimple::stopStartStore()
{
	MVTApp::sReporter.enable(false); // Reduce logged message
	mSession->terminate();
	MVTApp::stopStore();
	MVTApp::startStore();
	mSession = MVTApp::startSession();
	MVTApp::sReporter.enable(true);
}
