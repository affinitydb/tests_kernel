/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

// Simple tests to bash on specific features of the store
// Can be used in conjunction with "-crash" option
//
// Each component is exposed as a separate "scenario" so that we
// have clear idea what was happening when a crash occurred.
//
// To pick a time for -crash option, first run the test normally
// and look for the "starting Deletion" time and total test time.
// If you set the crash times in the period that deletions occur 
// then you are bashing on the deletion recovery.
//

#include "app.h"
#include "mvauto.h"
#include "teststream.h"

using namespace std;

// Publish this test.
class TestBashPinDelete : public ITest
{
	public:
		TEST_DECLARE(TestBashPinDelete);
		virtual char const * getName() const { return "bashdelete"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Bash at pin deletion. Args: scenario number, deletion technique"; }
		virtual int execute();
		virtual void destroy() { delete this; }

		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Designed for stress testing"; return false; }
		virtual bool isPerformingFullScanQueries() const { return true; }

	protected:
		void createSimple() ;
		void createFTString() ;
		void createFTStringBigColl() ;
		void deleteTestPins() ;
		void createIndexedPins() ;
		void createPifsSSV() ;

		PID create2PropPin( const char * inProp1, const char * inProp2 = NULL ) ;
		unsigned long search2PropPin( char* inSearch, bool bProp1 = true, bool bProp2 = false, unsigned int flags = 0 ) ;

	private:
		ISession * mSession ;
		PropertyID mPropForDelete ; // Prop used to find pins for delete
		PropertyID mProp2 ;
		long mTestStart ;
		long mCntPins ;
		long mDeleteScenario ;
};
TEST_IMPLEMENT(TestBashPinDelete, TestLogger::kDStdOut);

int TestBashPinDelete::execute()
{	
	int scenario; bool pparsing = true;
	
	if(!mpArgs->get_param("inscenario",scenario)){
		mLogger.out() << "Problem with --inscenario parameter initialization!" << endl;
		pparsing = false;
	}
	
	if(!mpArgs->get_param("indeletescenario",mDeleteScenario)){
		mLogger.out() << "Problem with --indeleteScenario parameter initialization!" << endl;
		pparsing = false;
	}
	
	if(!pparsing){
	   mLogger.out() << "Parameter initialization problems! " << endl; 
	   mLogger.out() << "Expects:  ./tests bashdelete --inscenario={int} --indeleteScenario={int}  " << endl; 
			
	   return 1;
	}
	
	mCntPins=0; // Set by each test
	mTestStart = getTimeInMs();

	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	// Creates unique properties so that the exact number of FT matches can be confirmed
	MVTApp::mapURIs(mSession,"TestBashPinDelete.prop1",1,&mPropForDelete) ;
	
	if ( scenario == 0 )
		createSimple() ;
	else if ( scenario == 1 ) 
		createFTString() ;
	else if ( scenario == 2 || scenario == 8902 )
		createFTStringBigColl() ;
	else if ( scenario == 3 )
		createPifsSSV() ;
	else if ( scenario == 4 )
		createIndexedPins() ;
	else if ( scenario == 5 )
	{
		TVERIFYRC(mSession->startTransaction()) ;
		mLogger.out() << "Giant transaction for all pins" << endl ;
		createSimple() ;
		TVERIFYRC(mSession->commit()) ;
	}


	deleteTestPins() ;

	mSession->terminate(); 
	MVTApp::stopStore();  
	return RC_OK  ;
}

void TestBashPinDelete::createSimple()
{
	// Andrew note: passed recovery bash test

	mCntPins = 10000 ;
	mLogger.out() << "createSimple - Creating " << mCntPins << " simple pins" << endl ;
	static const int cntVals = 1 ;
	Value vals[cntVals] ;
	vals[0].set(0);vals[0].property=mPropForDelete;
	for ( long i = 0 ; i < mCntPins ; i++ )
		TVERIFYRC(mSession->createPIN(vals, cntVals, NULL, MODE_PERSISTENT|MODE_COPY_VALUES)) ;
}

void TestBashPinDelete::createFTString()
{
	// Andrew note: passed recovery bash test
	mCntPins = 1000 ;
	mLogger.out() << "createFTString - Creating " << mCntPins << " pins with ft indexed strings" << endl ;

	static const int cntVals = 2 ;
	Value vals[cntVals] ;

	vals[0].set(0);vals[0].property=mPropForDelete;

	// This is a bash test, not perf, so random words are ok
	string str = MVTRand::getString2( 50, 1000, true /*break into words*/ ) ;
	vals[1].set( str.c_str() ) ; vals[1].property=MVTApp::getProp( mSession, "TestBashPinDelete::createFTString" ) ;

	for ( long i = 0 ; i < mCntPins ; i++ )
		TVERIFYRC(mSession->createPIN(vals, cntVals, NULL, MODE_PERSISTENT|MODE_COPY_VALUES)) ;
}

void TestBashPinDelete::createFTStringBigColl()
{
	// Andrew note: passed recovery bash test
	mCntPins = 1 ; 
	mLogger.out() << "createFTStringBigColl - Creating " << mCntPins << " pins with collections of ft indexed strings" << endl ;

	static const int cntVals = 1 ; 
	static const int cntStrings = 400 ;
	Value vals[cntVals] ;

	vals[0].set(0);vals[0].property=mPropForDelete;
	PropertyID stringCollProp = MVTApp::getProp( mSession, "TestBashPinDelete::createFTStringBigColl" ) ;

	Value stringElements[cntStrings] ;

	vector<string> strBucket ;
	strBucket.resize( cntStrings ) ;

	long i ;
	for ( i = 0 ; i < cntStrings ; i++ )
	{
		// This is a bash test, not perf, so random words are ok
		strBucket[i] = MVTRand::getString2( 50, 1000, true /*break into words*/ ) ;

		const char * str = strBucket[i].c_str() ;
		stringElements[i].set( str ) ; 
		stringElements[i].property=stringCollProp;
		//stringElements[i].meta = META_PROP_SSTORAGE ; // #8357 required META_PROP_SSTORAGE
		stringElements[i].op = OP_ADD ;
	}

	for ( i = 0 ; i < mCntPins ; i++ )
	{
		IPIN *pin;
		TVERIFYRC(mSession->createPIN(vals, 1, &pin, MODE_PERSISTENT|MODE_COPY_VALUES|MODE_NO_EID)) ;

		// REVIEW: what is best way to build collection?
		long k ;
		for ( k = 0 ; k < cntStrings ; k++ )
		{
			RC rc ;
			TVERIFYRC( rc = mSession->modifyPIN(pin->getPID(), &(stringElements[k]), 1, MODE_NO_EID ) ) ;
			if ( rc != RC_OK )
			{
				mLogger.out() << "Adding string " << k << " val " << stringElements[k].str << endl ;
				break ;
			}
		}
	}
}

void TestBashPinDelete::createIndexedPins()
{
	// TODO: try to bash on crashes during creation or deletion of index entries
}

void TestBashPinDelete::createPifsSSV()
{
	// We don't need to do a OP_EDIT style creation
	// because testlargeblob can be used to bash on creation time crashes
	mCntPins = 50 ;

	int strmsize = 1024*MVTRand::getRange(1,200) ; // must be able to span more the 1 page
	IStream *lStream = MVTApp::wrapClientStream(mSession,new TestStringStream(strmsize,VT_BSTR));

	mLogger.out() << "createPifsSSV - Creating " << mCntPins << " pifs pins (size :" << strmsize << ")" << endl ;

	static const int cntVals = 1 ;
	Value vals[cntVals] ;
	vals[0].set(lStream);vals[0].property=mPropForDelete;
	for ( long i = 0 ; i < mCntPins ; i++ )
	{
		lStream->reset() ;
		TVERIFYRC(mSession->createPIN(vals, cntVals, NULL, MODE_PERSISTENT|MODE_COPY_VALUES)) ;
	}
}

void TestBashPinDelete::deleteTestPins()
{
	// Show time to help decide when to crash
	long startDeleteTime = getTimeInMs() - mTestStart ;
	mLogger.out() << "At " << startDeleteTime << "ms starting Deletion... " << endl ;

	CmvautoPtr<IStmt> lQ( mSession->createStmt(mDeleteScenario==0?STMT_DELETE:STMT_UPDATE) ) ;
	unsigned char var = lQ->addVariable(); 
	Value args[1];
	args[0].setVarRef(0,mPropForDelete);
	CmvautoPtr<IExprNode> expr(mSession->expr(OP_EXISTS,1,args));
	lQ->addCondition(var,expr);

	if ( mDeleteScenario == 1 )
	{
		Value removeProp ; removeProp.setDelete(mPropForDelete) ;
		lQ->setValues(&removeProp,1);
	}
	uint64_t cntDel; 
	TVERIFYRC(lQ->execute(NULL,0,0,~0,0,MODE_PURGE,&cntDel));
	TVERIFY(cntDel==uint64_t(mCntPins));
}

