/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

using namespace std;

#define VERBOSE 0

// Publish this test.
class TestFamilyBasic : public ITest
{
	public:
		TEST_DECLARE(TestFamilyBasic);
		virtual char const * getName() const { return "testfamilybasic"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Overview of basic Family and Class behavior"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }

		virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual bool isStandAloneTest()const {return true;}
	protected:
		void runQuery( const char * desc, IStmt * inQ, const Value * params = NULL, unsigned int nParams = 0 ) ;
		void addPIN( int propXVal, char propYVal ) ;	
 	private:
		ISession * mSession ;
		PropertyID mPropX ;
		PropertyID mPropY ;
		PropertyID mPropIndex ; // Just to give a recognizable name to the pin

		vector<PID> mPINs ;

		ClassID mClass1 ;
		ClassID mFamily2 ;
		ClassID mFamily3 ;
		ClassID mFamily4 ;
};
TEST_IMPLEMENT(TestFamilyBasic, TestLogger::kDStdOut);

int TestFamilyBasic::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	string strRand ; MVTRand::getString( strRand, 20, 0 ) ; // used to randomize class names

	MVTApp::mapURIs(mSession,"TestFamilyBasic.propX",1,&mPropX) ;
	MVTApp::mapURIs(mSession,"TestFamilyBasic.propY",1,&mPropY) ;
	MVTApp::mapURIs(mSession,"TestFamilyBasic.propIndex",1,&mPropIndex) ;

	/*1*/ addPIN( 5,  'A') ;
	/*2*/ addPIN( 10, 'C') ;
	/*3*/ addPIN( 19, 'A') ;
	/*4*/ addPIN( 1,  'A') ;
	/*5*/ addPIN( 0,  'B') ;

	{
		CmvautoPtr<IStmt> lFullScanQ( mSession->createStmt() ) ;
		unsigned char lVar = lFullScanQ->addVariable() ;
		Value ops[2] ; 
		ops[0].setVarRef(0, mPropX ) ;
		ops[1].set( 10 ) ;
		CmvautoPtr<IExprTree> lE( mSession->expr( OP_GE /* >= */, 2, ops ) ) ;
		lFullScanQ->addCondition( lVar, lE ) ;

		runQuery( "Full Scan PropX >= 10", lFullScanQ ) ;

		// Build Class 1 based on this query
		CmvautoPtr<IStmt> lClassQ( lFullScanQ->clone() ) ;

		string strClass1 = "Class1." + strRand ;
		mClass1 = STORE_INVALID_CLASSID;
		TVERIFYRC(defineClass(mSession, strClass1.c_str(), lClassQ, &mClass1 ));
	}

	{
		//Use Class 1
		CmvautoPtr<IStmt> lQUsingClass( mSession->createStmt() ) ;

		SourceSpec cs ;
		cs.objectID = mClass1 ;
		cs.nParams = 0 ;
		cs.params = NULL ;

		lQUsingClass->addVariable( &cs, 1 ) ;

		runQuery( "All Pins with PropX >= 10", lQUsingClass ) ;
	}

	{
		// Class 1 with a filter
		// e.g. All pins with propX >= 10 and propY == 'A'

		CmvautoPtr<IStmt> lQUsingClass( mSession->createStmt() ) ;

		SourceSpec cs ;
		cs.objectID = mClass1 ;
		cs.nParams = 0 ;
		cs.params = NULL ;

		unsigned char lVar = lQUsingClass->addVariable( &cs, 1 ) ;

		Value ops[2] ; 
		ops[0].setVarRef(0, mPropY ) ;
		ops[1].set( 'A' ) ;
		CmvautoPtr<IExprTree> lE( mSession->expr( OP_EQ, 2, ops ) ) ;
		lQUsingClass->addCondition( lVar, lE ) ;

		runQuery( "All Pins in Class 1 and PropY == 'A'", lQUsingClass ) ;
	}

	{
		// Family 2 - PropY == $param0

		CmvautoPtr<IStmt> lFamilyQ( mSession->createStmt() ) ;
		unsigned char lVar = lFamilyQ->addVariable() ;

		Value ops[2] ; 
		ops[0].setVarRef(0, mPropY ) ;
		ops[1].setParam(0);
		CmvautoPtr<IExprTree> lE( mSession->expr( OP_EQ, 2, ops ) ) ;
		lFamilyQ->addCondition( lVar, lE ) ;

		Value paramVal ; paramVal.set( 'A' ) ;
		runQuery( "Full Scan Parameterized Query - PropY == 'A'", lFamilyQ, &paramVal, 1 ) ;

		string strFamily2 = "Family2." + strRand ;
		mFamily2 = STORE_INVALID_CLASSID ;
		TVERIFYRC(defineClass(mSession, strFamily2.c_str(), lFamilyQ, &mFamily2 ));
	}
	{
		CmvautoPtr<IStmt> lQUsingFamily( mSession->createStmt() ) ;

		Value paramVal ; paramVal.set( 'A' ) ;

		SourceSpec cs ;
		cs.objectID = mFamily2 ;
		cs.nParams = 1 ;
		cs.params = &paramVal ;

		lQUsingFamily->addVariable( &cs, 1 ) ;

		runQuery( "All Pins in Family2 with param0 = 'A'", lQUsingFamily ) ;
	}


	{
		// "Pins where PropY == 'A' and PropX > 1 sorted by PropX"
		// By using Family:
		// "Pins in family 2 with param0='A' and PropX > 1 sorted by PropX"
		CmvautoPtr<IStmt> lQFamilySorted( mSession->createStmt() ) ;

		Value paramVal ; paramVal.set( 'A' ) ;
		SourceSpec cs ;
		cs.objectID = mFamily2 ;
		cs.nParams = 1 ;
		cs.params = &paramVal ;

		unsigned char lVar = lQFamilySorted->addVariable( &cs, 1 ) ;

		// PropX > 1 condition

		Value ops[2] ; 
		ops[0].setVarRef(0, mPropX ) ;
		ops[1].set( 1 ) ;
		CmvautoPtr<IExprTree> lE( mSession->expr( OP_GT, 2, ops ) ) ;
		lQFamilySorted->addCondition( lVar, lE ) ;

		// Sort by PropX (Ascending is default)
		OrderSeg ord = {NULL, mPropX, ORD_DESC, 0, 0};
		lQFamilySorted->setOrder( &ord, 1) ;

		runQuery( "Pins in family 2 with param0='A' and PropX > 1 sorted by PropX", lQFamilySorted ) ;
	}

	{
		// Family with two variables
		// PropX >= param0 AND PropY >= param1
		//mFamily3
		CmvautoPtr<IStmt> lFamilyQ( mSession->createStmt() ) ;
		unsigned char lVar = lFamilyQ->addVariable() ;

		Value ops[2] ; 
		ops[0].setVarRef(0, mPropX ) ;
		ops[1].setParam(0);
		IExprTree* lE1 = mSession->expr( OP_GT, 2, ops ) ;

		ops[0].setVarRef(0, mPropY ) ;
		ops[1].setParam(1);
		IExprTree* lE2 = mSession->expr( OP_EQ, 2, ops ) ;

		ops[0].set( lE1 ) ;
		ops[1].set( lE2 );
		CmvautoPtr<IExprTree> lE( mSession->expr( OP_LAND, 2, ops ) ) ;

		lFamilyQ->addCondition( lVar, lE ) ;

		// Try it out
		Value paramVals[2] ; 
		paramVals[0].set( 1 ) ;
		paramVals[1].set( 'A' ) ;
		runQuery( "Full Scan Parameterized Query - PropY == 'A' AND PropX >= 1", lFamilyQ, paramVals, 2 ) ;

		string strFamily3 = "Family3." + strRand ;
		mFamily3 = STORE_INVALID_CLASSID ;
		TVERIFYRC(defineClass(mSession, strFamily3.c_str(), lFamilyQ, &mFamily3 ));
	}

	{
		// "Pins where PropY == 'A' and PropX > 1"
		// By using Family 3:
		// "Pins in family 3 with param0=1 and param1='A'"
		CmvautoPtr<IStmt> lQFamily3( mSession->createStmt() ) ;

		Value paramVals[2] ; 
		paramVals[0].set( 1 ) ;
		paramVals[1].set( 'A' ) ;

		SourceSpec cs ;
		cs.objectID = mFamily3 ;
		cs.nParams = 2 ;
		cs.params = paramVals ;

		lQFamily3->addVariable( &cs, 1 ) ;

		runQuery( "Pins in family 3 with param0='A' and param1 > 1", lQFamily3 ) ;
	}

	{
		// Family based on Class
		// "Pins from Class1 where PropY == $param0"
		CmvautoPtr<IStmt> lQFamilyBasedOnClass( mSession->createStmt() ) ;

		// Pins from Class1
		SourceSpec cs ;
		cs.objectID = mClass1 ;
		cs.nParams = 0 ;
		cs.params = NULL ;

		unsigned char lVar = lQFamilyBasedOnClass->addVariable( &cs, 1 ) ;

		// Indexed by PropY
		Value ops[2] ; 
		ops[0].setVarRef(0, mPropY ) ;
		ops[1].setParam( 0 ) ;
		CmvautoPtr<IExprTree> lE( mSession->expr( OP_EQ, 2, ops ) ) ;
		lQFamilyBasedOnClass->addCondition( lVar, lE ) ;

		string strFamily4 = "Family4." + strRand ;
		mFamily4 = STORE_INVALID_CLASSID ;
		TVERIFYRC(defineClass(mSession, strFamily4.c_str(), lQFamilyBasedOnClass, &mFamily4));
	}

	{
		// "Pins where PropY == 'A' and PropX > 1"
		// By using Family 4:
		// "Pins in family 4 with param0=A"
		CmvautoPtr<IStmt> lQFamily4( mSession->createStmt() ) ;

		Value paramVals[1] ; 
		paramVals[0].set( 'A' ) ;

		SourceSpec cs ;
		cs.objectID = mFamily4 ;
		cs.nParams = 1 ;
		cs.params = paramVals ;

		lQFamily4->addVariable( &cs, 1 ) ;

		runQuery( "Pins in family 4 with param0='A'", lQFamily4 ) ;
	}

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test

	return RC_OK  ;
}

void TestFamilyBasic::addPIN( int propXVal, char propYVal )
{
	PID pid; 
	Value vals[3] ;
	vals[0].set( (int)(1+mPINs.size()) ) ; vals[0].property = mPropIndex ; 
	vals[1].set( propXVal ) ; vals[1].property = mPropX ; 
	vals[2].set( propYVal ) ; vals[2].property = mPropY ; 
	TVERIFYRC(mSession->createPINAndCommit( pid, vals, 3 ) );	
	mPINs.push_back( pid ) ;
}

void TestFamilyBasic::runQuery( const char * desc, IStmt * inQ, const Value * params, unsigned int nParams )
{
	mLogger.out() << "----------------------------\nQuery results for " << desc << endl << endl ;
	ICursor* lC = NULL;
	TVERIFYRC(inQ->execute(&lC, params, nParams ));
	CmvautoPtr<ICursor> lR(lC);

	mLogger.out() << endl ;

#if VERBOSE
	char * queryStr = inQ->toString() ;
	mLogger.out() << queryStr ;
	mSession->free( queryStr ) ;
#endif

	if ( !lR.IsValid() ) { TVERIFY(false); return ; }

	IPIN * pResult ;
	while ( NULL != ( pResult = lR->next() ) )
	{
		const Value * pIndex = pResult->getValue( mPropIndex ) ;
		if ( pIndex == NULL ) { TVERIFY(!"No index prop") ; pResult->destroy() ; continue ; }
			
		mLogger.out() << "\t" << pIndex->i << endl ;

		pResult->destroy() ;
	}
}
