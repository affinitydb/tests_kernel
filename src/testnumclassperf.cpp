/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

#include "mvauto.h"

// Test to look 
// 
// Scenarios:
// Size + Time with no index
// Size + Time with index from before
// Size + Time with index created afterwards
// Size after value change fragmentation
// Size after add/remove fragmentation

#define CNT_PINS 100000
#define CNT_VALUE_MODIFICATIONS CNT_PINS / 2
#define CNT_PIN_ADD CNT_PINS
#define CNT_PIN_REMOVE CNT_PINS

// Publish this test.
class TestNumClassPerf : public ITest
{
	public:
		TEST_DECLARE(TestNumClassPerf);
		virtual char const * getName() const { return "testnumclassperf"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Measure overhead/performance gain of numeric class/family"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }

		virtual bool includeInPerfTest() const { return false; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Under development, will take a long time to execute"; return false; }	
		virtual bool includeInBashTest(char const *& pReason) const { pReason = "Under development"; return false; }
		virtual bool isPerformingFullScanQueries() const { return true; }

	protected:
		void doTest();
	private:
		ISession * mSession;
};
TEST_IMPLEMENT(TestNumClassPerf, TestLogger::kDStdOut);

class PerfScenarioBase
{
public:
	PerfScenarioBase(ITest* inTest):mTest(inTest),mStart(0),mOut(&(mTest->getLogger().out())){}
	virtual ~PerfScenarioBase() {}
	void execute()
	{
		prepare();
		doTest();
		conclude();
	}
	
	virtual void doTest() = 0;

	virtual void prepare()
	{
		*mOut << endl << "---------------------------------" << endl 
			<< "Starting scenario : " << mDesc << endl;

		MVTApp::deleteStore();
		bool bStarted = MVTApp::startStore();
		TV_R(bStarted,mTest);

		mSession = MVTApp::startSession();
		TV_R( mSession != NULL,mTest );

		startTimer();  // Derived class should call this method
						// again if they do any preparations they don't want timed
	}

	virtual void conclude()
	{
		mSession->terminate(); 
		MVTApp::stopStore();

		*mOut << "Store size: " << getStoreFileSize() << endl;
	}

	long startTimer()
	{
		mStart = getTimeInMs();
		return mStart;
	}
	
	long endTimer()
	{
		return getTimeInMs() - mStart;
	}

	uint64_t getStoreFileSize()
	{
        // REVIEW: Store size isn't very meaningful because it is allocated in large 16 meg chunks and
        // actual numeric data is fairly small.  More meaningful would be size of the index, pin and other
        // pages inside the store
#ifdef WIN32
		// Todo: get number of bytes, e.g. from store file itself
		WIN32_FILE_ATTRIBUTE_DATA lData;
		::GetFileAttributesEx( STOREPREFIX DATAFILESUFFIX, GetFileExInfoStandard, &lData );

		return lData.nFileSizeLow + ((uint64_t)lData.nFileSizeHigh<<32);
#else
    	// not implemented
        return 0;
#endif
	}

	void setOutput( std::ostream& inStream )
	{
		mOut = &inStream;
	}

	ITest* mTest;
	string mDesc;
	ISession* mSession; 
	long mStart;
	std::ostream* mOut;
};

class IndexPerfBase : public PerfScenarioBase
{
public:
	IndexPerfBase(ITest* inTest) : PerfScenarioBase(inTest),mCntSpecific(0)
	{
	}
	
	virtual void prepare()
	{
		PerfScenarioBase::prepare();
		mNumberProp = MVTApp::getProp(mSession,"NumberProperty");
	}

	void createIndex( const char* inName = NULL )
	{
		startTimer();
		IStmt* lQ = mSession->createStmt();
		unsigned char v0 = lQ->addVariable();
		Value ops[2];
		ops[0].setVarRef(v0,mNumberProp);
		ops[1].setParam(0);
		IExprNode * lE = mSession->expr(OP_EQ,2,ops);
		lQ->addCondition(v0,lE);

		TVRC_R(ITest::defineClass(mSession,mFamilyName, lQ ),mTest);

		lQ->destroy();
		lE->destroy();

		*mOut << "Index creation time:" << endTimer() << endl;
	}

	void createClass()
	{
		// Class of all properties
		startTimer();
		IStmt* lQ = mSession->createStmt();
		unsigned char v0 = lQ->addVariable();
		lQ->setPropCondition(v0,&mNumberProp,1);

		TVRC_R(ITest::defineClass(mSession,mClassName, lQ ),mTest);

		lQ->destroy();

		*mOut << "Class creation time:" << endTimer() << endl;
	}

	IStmt * getPinsWithProp( )
	{
		// Use the class
		IStmt * q = mSession->createStmt();

		// See if a class exists to speed up the query
		ClassID lClassID = MVTApp::getClass(mSession,mClassName);			 

		if ( lClassID != STORE_INVALID_CLASSID )
		{
			SourceSpec classInfo;
			classInfo.objectID = lClassID;
			classInfo.nParams = 0;
			classInfo.params = NULL;

			q->addVariable(&classInfo,1);				
		}
		else
		{
			q->setPropCondition(q->addVariable(),&mNumberProp,1);
		}
		return q;
	}

	IStmt * getIndexSearchForNumber( int inNumber )
	{
		const char * lClassName = mFamilyName;

		ClassID cls = MVTApp::getClass(mSession, lClassName );
		TV_R( cls != STORE_INVALID_CLASSID,mTest );
		IStmt * q = mSession->createStmt();

		Value lNumberToLookup; lNumberToLookup.set( inNumber ); 
		lNumberToLookup.property = mNumberProp;

		SourceSpec classInfo;
		classInfo.objectID = cls;
		classInfo.nParams = 1;
		classInfo.params = &lNumberToLookup;

		q->addVariable(&classInfo,1);				
		return q;
	}

	IStmt * getNonIndexSearchForNumber( int inNumber )
	{
		IStmt* lQ = mSession->createStmt();
		unsigned char v0 = lQ->addVariable();
		Value ops[2];
		ops[0].setVarRef(v0,mNumberProp);
		ops[1].set( inNumber );
		IExprNode * lE = mSession->expr(OP_EQ,2,ops);
		lQ->addCondition(v0,lE);
		lE->destroy();
		return lQ;
	}

	void findNumber()
	{
		// Find the pins with some specific number in the store
		// This uses the index if available

		// Close and reopen store to flush Pages already in memory
		mSession->terminate(); 
		MVTApp::stopStore();  

		MVTApp::startStore();
		mSession = MVTApp::startSession();

		startTimer();

		IStmt * lQ;

		ClassID cls = MVTApp::getClass(mSession, mFamilyName );
		if ( cls != STORE_INVALID_CLASSID )
		{
			lQ = getIndexSearchForNumber( mSpecificNumber );
		}
		else
		{
			lQ = getNonIndexSearchForNumber( mSpecificNumber );
		}

		ICursor * lR = NULL;
		lQ->execute(&lR);

		uint64_t cnt = 0;
		IPIN * lPin = NULL;
		while( lPin = lR->next() )
		{
#ifdef VERBOSE
			MVTApp::output( lPin, *mOut, mSession );
#endif

			cnt++;
			TV_R( lPin->getValue( mNumberProp )->i == mSpecificNumber ,mTest);
			lPin->destroy();
		}

		TV_R( cnt == uint64_t(mCntSpecific), mTest );
		lR->destroy();
		lQ->destroy();

		*mOut << "Query time took " << endTimer() << endl;
	}

	void addPins( int cntPins )
	{
		startTimer();
		for ( int i = 0; i < cntPins; i++ )
		{
			int val = MVTRand::getRange( 0, 999 );

			if ( val == mSpecificNumber )
				mCntSpecific++;

			Value v; v.set( val ); v.property = mNumberProp;
			TVRC_R(mSession->createPIN(&v,1,NULL,MODE_PERSISTENT|MODE_COPY_VALUES),mTest);
		}
		*mOut << "Add PIN time: " << endTimer() << endl;
	}	

	void modifyPINValues()
	{
		// Perform updates to the PIN values (which will adjust the index)
		*mOut << "Afy size before value modifications: " << getStoreFileSize() << endl;
		startTimer(); 
		int cntModifsRemaining = CNT_VALUE_MODIFICATIONS;

		IStmt * lQ = getPinsWithProp();
		ICursor * lR = NULL;
		lQ->execute(&lR);

		while( cntModifsRemaining > 0 )
		{
			// Move ahead some random number of PINs
			int cntGap = MVTRand::getRange(0,10);

			IPIN * lPIN;
			while(true)
			{
				lPIN=lR->next();
				if ( lPIN==NULL )
				{
					lR->destroy();
					lQ->execute(&lR);  // Start at beginning again
					lPIN=lR->next();
				}

				assert(lPIN!=NULL); // Empty query? Algorithm can't work

				cntGap--;
				if ( cntGap > 0 )
					lPIN->destroy();
				else
					break;
			}

			// Change the value in the pin we found
			int curVal = lPIN->getValue(mNumberProp)->i;
			if ( curVal == mSpecificNumber )
				mCntSpecific--;

			int newVal = MVTRand::getRange(0,999);
			if ( newVal == mSpecificNumber )
				mCntSpecific++;

			Value lValue; lValue.set( newVal ); lValue.property = mNumberProp;
			lPIN->modify( &lValue, 1 );

			cntModifsRemaining--;
		}
		lR->destroy();
		lQ->destroy();

		*mOut << "Modify value time: " << endTimer() << endl;
	}

	void addRemovePINs()
	{
		*mOut << "Afy size before add/remove: " << getStoreFileSize() << endl;
		startTimer();

		int cntRemoveRemaining = CNT_PIN_REMOVE;
		int cntAddRemaining = CNT_PIN_ADD;
		assert( CNT_PIN_REMOVE == CNT_PIN_ADD ); // Currently done 1 for 1

		IStmt * lQ = getPinsWithProp();
		ICursor * lR = NULL;
		lQ->execute(&lR);

		while( cntRemoveRemaining > 0 )
		{
			// Move ahead some random number of PINs
			int cntGap = MVTRand::getRange(0,10);

			IPIN * lPIN;
			while(true)
			{
				lPIN=lR->next();
				if ( lPIN==NULL )
				{
					lR->destroy();
					lQ->execute(&lR); // Start at beginning again
					lPIN=lR->next();
				}

				assert(lPIN!=NULL); // Empty query? Algorithm can't work

				cntGap--;
				if ( cntGap > 0 )
					lPIN->destroy();
				else
					break;
			}

			// Change the value in the pin we found
			int curVal = lPIN->getValue(mNumberProp)->i;
			if ( curVal == mSpecificNumber )
				mCntSpecific--;

			mSession->deletePINs( &lPIN, 1, MODE_PURGE );

			cntRemoveRemaining--;

			// Also add a new PIN
			int val = MVTRand::getRange( 0, 999 );
			if ( val == mSpecificNumber )
				mCntSpecific++;
			Value v; v.set( val ); v.property = mNumberProp;
			TVRC_R(mSession->createPIN(&v,1,NULL,MODE_PERSISTENT|MODE_COPY_VALUES),mTest);
			cntAddRemaining--;
		}

		lR->destroy();
		lQ->destroy();
		*mOut << "Add remove value time: " << endTimer() << endl;
	}

	PropertyID mNumberProp;
	const static int mSpecificNumber = 789; /* specific number that will be present on some pins to verify our results*/
	int mCntSpecific;  

	static const char * mFamilyName; 
	static const char * mClassName;
};

const char *IndexPerfBase::mFamilyName = "NumberIndex";
const char *IndexPerfBase::mClassName = "HasNumber";

class NoIndex : public IndexPerfBase
{
public:
	NoIndex(ITest* inTest) : IndexPerfBase(inTest)
	{
		mDesc = "No Index";
	}

	virtual void doTest()
	{		
		addPins( CNT_PINS );
		findNumber();
		modifyPINValues();
		findNumber();
		addRemovePINs();
		findNumber();
	}
};

class WithIndex : public IndexPerfBase
{
public:
	WithIndex(ITest* inTest) : IndexPerfBase(inTest)
	{
		mDesc = "Index created before Pins";
	}

	virtual void doTest()
	{
		createIndex();
		addPins( CNT_PINS );
		findNumber();
		modifyPINValues();
		findNumber();
		addRemovePINs();
		findNumber();
	}
};

class DeferredIndex : public IndexPerfBase
{
public:
	DeferredIndex(ITest* inTest) : IndexPerfBase(inTest)
	{
		mDesc = "Index created after adding numbers";
	}

	virtual void doTest()
	{
		addPins( CNT_PINS );
		createIndex();
		findNumber();
		modifyPINValues();
		findNumber();
		addRemovePINs();
		findNumber();
	}
};


class WithClassAndIndex : public IndexPerfBase
{
public:
	WithClassAndIndex(ITest* inTest) : IndexPerfBase(inTest)
	{
		mDesc = "Class and Family created before Pins";
	}

	virtual void doTest()
	{
		createIndex();
		createClass(); 
		addPins( CNT_PINS );
		findNumber();
		modifyPINValues();
		findNumber();
		addRemovePINs();
		findNumber();
	}
};


class DeferredClassAndIndex : public IndexPerfBase
{
public:
	DeferredClassAndIndex(ITest* inTest) : IndexPerfBase(inTest)
	{
		mDesc = "Class and Family created after Pins";
	}

	virtual void doTest()
	{
		addPins( CNT_PINS );
		createIndex();
		createClass(); 
		findNumber();
		modifyPINValues();
		findNumber();
		addRemovePINs();
		findNumber();
	}
};

class WithClass : public IndexPerfBase
{
public:
	WithClass(ITest* inTest) : IndexPerfBase(inTest)
	{
		mDesc = "Class created before Pins (no family)";
	}

	virtual void doTest()
	{
		createClass(); 
		addPins( CNT_PINS );
		findNumber();
		modifyPINValues();
		findNumber();
		addRemovePINs();
		findNumber();
	}
};


int TestNumClassPerf::execute()
{
	doTest();
	return RC_OK;
}

void TestNumClassPerf::doTest()
{
	std::stringstream timerResults;

	mLogger.out() << "Tests with " << CNT_PINS << endl;
	
	vector<IndexPerfBase *> lTests;

	lTests.push_back(new NoIndex(this)); 
	lTests.push_back(new DeferredIndex(this)); 
	lTests.push_back(new WithIndex(this)); 
	lTests.push_back(new WithClassAndIndex(this)); 
	lTests.push_back(new DeferredClassAndIndex(this)); 
	lTests.push_back(new WithClass(this)); 

	for ( size_t i = 0; i < lTests.size(); i++ )
	{
		lTests[i]->setOutput(timerResults); 
		lTests[i]->execute();
		delete(lTests[i]); 
	}

	mLogger.out() << endl << endl << "Timing results" << endl;
	mLogger.out() << timerResults.str();
}
