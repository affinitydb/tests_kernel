/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

// 
// Coverage of 
//		IStmt::addVariable(const PID& pid,PropertyID propID,IExprTree *cond=NULL)
// which is special signature for searching a collection of PIN references
//

#include "app.h"
#include "mvauto.h"
#include "collectionhelp.h"

using namespace std;

#define TEST_QUERY_ASSTRING 1

// Publish this test.
class TestCollQuery : public ITest
{
	public:
		TEST_DECLARE(TestCollQuery);
		virtual char const * getName() const { return "testcollquery"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Query a collection - 7468"; }
		virtual bool isStandAloneTest()const {return true;}
		
		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void queryCollection( unsigned int cntElements );
		void queryCollectionWithDuplicates( unsigned int cntElements, bool bAllowDuplicates );
		
		void verifyQueryFoundPIN( IStmt * inQ, const PID & pidToSearchFor );

	private:
		ISession * mSession ;
		PropertyID mCollectionProp ;
		PropertyID mPinIDProp ;
};
TEST_IMPLEMENT(TestCollQuery, TestLogger::kDStdOut);

int TestCollQuery::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	mCollectionProp = MVTApp::getProp( mSession, "TestCollQuery_PinRefCollection" ) ;
	mPinIDProp = MVTApp::getProp( mSession, "TestCollQuery_PinID" ) ;

	queryCollection( 500 ) ;

	queryCollection( 1 ) ; // Edge case - only single PIN in the collection
	queryCollectionWithDuplicates( 1000, true ) ;
	queryCollectionWithDuplicates( 1000, false ) ;

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test

	return RC_OK  ;
}

void TestCollQuery::queryCollection( unsigned int cntElements )
{
	// Create PINs that will be referenced by another PIN
	vector<PID> referencedPins(cntElements);
	unsigned int i ;
	for ( i = 0 ; i < cntElements ; i++ )
	{
		// Put a unique value on each pin that could be queried for
		Value v ; v.set( i ) ; v.property = mPinIDProp ;
		TVERIFYRC(mSession->createPIN(referencedPins[i],&v,1));
	}

	// Create pin that points to all of the referencedPins
	PID pinWithColl ;
	TVERIFYRC(mSession->createPIN(pinWithColl,NULL,0));

	// Create the references 
	unsigned int k ;
	for ( k = 0 ; k < cntElements ; k++ )
	{
		Value ref ;	
		ref.set(referencedPins[k]) ; ref.op = OP_ADD ; ref.property = mCollectionProp ; 
		
		// REVIEW: doesn't seem to force it as a big collection
		ref.meta = META_PROP_SSTORAGE ;

		TVERIFYRC(mSession->modifyPIN( pinWithColl, &ref, 1 )) ;
	}
	
	// Sanity check
	IPIN * pin = mSession->getPIN(pinWithColl) ;
	MvStoreEx::CollectionIterator collection(pin,mCollectionProp);
	TVERIFY( collection.getSize() == cntElements ) ;
	pin = NULL ;

	/*
	Run query on the collection rather than entire store.  
	*/

	int indexOfPinToSearchFor = ( cntElements < 10 ) ? 0 : 10 ; 

	PID pidToSearchFor = referencedPins[indexOfPinToSearchFor] ; 

	CmvautoPtr<IStmt> lQ(mSession->createStmt()) ;
	unsigned char lVar = lQ->addVariable(pinWithColl,mCollectionProp) ; // Don't add the expr here

	// Expression to exaluate on each PIN referenced from the collection
	Value operands[2];	
	operands[0].setVarRef(0,mPinIDProp); 
	operands[1].set(indexOfPinToSearchFor) ;

	CmvautoPtr<IExprTree> lExpr(mSession->expr(OP_EQ,2,operands,0)) ;
	TVERIFYRC(lQ->addCondition(lVar,lExpr));

	verifyQueryFoundPIN( lQ, pidToSearchFor ) ;
}

void TestCollQuery::verifyQueryFoundPIN( IStmt * inQ, const PID & pidToSearchFor )
{
	// Confirms that a query that only expects single PIN match really 
	// returns the correct PIN

#if TEST_QUERY_ASSTRING
	// Covert to and from a string before using it
	char * strQ = inQ->toString() ;
	CmvautoPtr<IStmt> QCopy(mSession->createStmt(strQ));
	if (!QCopy.IsValid())
	{
		TVERIFY(!"Could not recreate query from string") ;
		mLogger.out() << "Original query: " << strQ << endl ;
		QCopy.Attach(inQ->clone()) ;
	}
	mSession->free(strQ);
#else
	CmvautoPtr<IStmt> QCopy( inQ->clone() ) ;
#endif

	uint64_t cnt = 0 ;
	TVERIFYRC(QCopy->count(cnt,NULL,0,~0 /*,MODE_NON_DISTINCT */)); 
	TVERIFY(cnt==1); mLogger.out() << cnt << endl ; 

	ICursor * lR = NULL;
	TVERIFYRC(QCopy->execute(&lR,0,0,~0,0 /*,MODE_NON_DISTINCT*/)) ;
	bool bFoundPINInResults = false ;
	if ( lR != NULL )
	{
		IPIN* lP ;
		while( NULL != ( lP = lR->next() ) )
		{		
			PID pidFound = lP->getPID() ;

			TVERIFY( pidFound == pidToSearchFor ) ;
			if ( pidFound == pidToSearchFor ) 
				bFoundPINInResults = true ;
		}
		lR->destroy() ;
	}
	TVERIFY( bFoundPINInResults ) ;
}

void TestCollQuery::queryCollectionWithDuplicates( unsigned int cntElements, bool bAllowDuplicates )
{
	// More advanced variation where there are extra properties and duplicates
	// in the collection
	vector<PID> referencedPins(cntElements);
	unsigned int i ;
	for ( i = 0 ; i < cntElements ; i++ )
	{
		TVERIFYRC(mSession->createPIN(referencedPins[i],NULL,0));
	}

	Value bogus ; bogus.set( "something to ignore in collection" ) ; 
	bogus.property = mCollectionProp ;
	//bogus.meta = META_PROP_SSTORAGE ;

	PID pinWithColl ;
	TVERIFYRC(mSession->createPIN(pinWithColl,&bogus,1));

	unsigned int k ;
	for ( k = 0 ; k < cntElements ; k++ )
	{
		// Add each pin reference twice to see about duplicate matches
		Value refs[2] ;	
		refs[0].set(referencedPins[k]) ; refs[0].op = OP_ADD ; refs[0].property = mCollectionProp ;
		refs[1].set(referencedPins[k]) ; refs[1].op = OP_ADD ; refs[1].property = mCollectionProp ;
		TVERIFYRC(mSession->modifyPIN( pinWithColl, refs, 2 )) ;
	}

	// Sanity check
	IPIN * pin = mSession->getPIN(pinWithColl) ;
	MvStoreEx::CollectionIterator collection(pin,mCollectionProp);
	TVERIFY( collection.getSize() == (1+cntElements*2) ) ;
	pin = NULL ;

	/*
	Run query on the collection rather than entire store.  
	*/
	PID pidToSearchFor = referencedPins[10] ; assert( cntElements > 10 ) ;

	IStmt * lQ = mSession->createStmt() ;
	unsigned char lVar = lQ->addVariable(pinWithColl,mCollectionProp) ; // Don't add the expr here

	Value operands[2];
		
	PropertyID propPinID = PROP_SPEC_PINID ;	
	operands[0].setVarRef(0,propPinID); 
	operands[1].set(pidToSearchFor); // Look for the 10th pin

	IExprTree * lExpr = mSession->expr(OP_EQ,2,operands,0) ;
	TVERIFYRC(lQ->addCondition(lVar,lExpr));

	verifyQueryFoundPIN( lQ, pidToSearchFor );

	// coverage for the MODE_NON_DISTINCT flag which permits the store to 
	// return the duplicate PINs
	unsigned long lAllowDuplicates = /*bAllowDuplicates ? MODE_NON_DISTINCT : */0 ;

	uint64_t cnt = 0 ;
	TVERIFYRC(lQ->count(cnt,NULL,0,~0,lAllowDuplicates)); 

	TVERIFY(cnt==((lAllowDuplicates!=0)?2:1)); mLogger.out() << cnt << endl ; 

	ICursor * lR = NULL;
	TVERIFYRC(lQ->execute(&lR,NULL,0,~0u,0,lAllowDuplicates)) ;
	bool bFoundPINInResults = false ;
	if ( lR != NULL )
	{
		IPIN* lP ;
		while( NULL != ( lP = lR->next() ) )
		{		
			PID pidFound = lP->getPID() ;

			TVERIFY( pidFound == pidToSearchFor ) ;
			if ( pidFound == pidToSearchFor ) 
				bFoundPINInResults = true ;
		}
		lR->destroy() ;
	}
	TVERIFY( bFoundPINInResults ) ;

	lExpr->destroy() ;
	lQ->destroy() ;
}
