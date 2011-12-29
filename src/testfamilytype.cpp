/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

/*
Class families indexes are based on a single VT_ type
This test explores the implications for client callers.

-By default first pin establishes the type
-Class definition can establish the type via Value::setParam (not implemented)
-Kernel will attempt to convert values to the index type, e.g. string "100" can fit into int index
-Pins with types that can't be converted to the index type are ignored (without error) and the pin is not indexed
*/ 

using namespace std;

#define TEST_TOSTRING_9082 1

#define VERBOSE 0

// Publish this test.
class TestFamilyType : public ITest
{
	public:
		TEST_DECLARE(TestFamilyType);
		virtual char const * getName() const { return "testfamilytype"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Test index type"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }

		//virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Work in progress"; return false; }
	protected:
		void testIndexBeforePin() ;
		void testIndexForceInt() ;
		void testIndexAfterPin() ;
		void testForceType() ;
		void testStringIndex() ;

		ClassID createEqFamily(const char * inFamilyName, PropertyID inProp, ValueType inIndexType = VT_ANY) ;

		template<class T> void countIndexMatches(ClassID inIndex, PropertyID inProp, T inValToLookup, int inExpected) ;
	private:	
		ISession * mSession ;
		string mClassRand ; 
};
TEST_IMPLEMENT(TestFamilyType, TestLogger::kDStdOut);

int TestFamilyType::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;
	MVTRand::getString( mClassRand, 20, 0 ) ; // used to randomize class names

	testIndexBeforePin() ;
	testIndexAfterPin() ;
	testForceType() ;
	testStringIndex() ;

	mSession->terminate() ;
	MVTApp::stopStore() ;

	return RC_OK ;
}

void TestFamilyType::testIndexBeforePin()
{
	//
	// Create class before first pin
	//
	PropertyID prop1 = MVTUtil::getPropRand( mSession, "infamfirst" ) ;
	ClassID mFamilyFirst = createEqFamily( "FamFirst", prop1 ) ;
	
	// first pin doesn't establish type of index anymore
	int i=-1 ;
	Value v ; v.set( i ) ; v.property = prop1 ;
	PID pid1 ;
	TVERIFYRC( mSession->createPIN( pid1, &v, 1 ) ) ;

	// Add some doubles

	double dbls[] = { -1.02,
					  -1.9,
					  -2.1,
					  -0.8 } ;
	for (size_t i = 0; i < (sizeof( dbls )/sizeof(dbls[0])) ; i++)
	{
		v.set( dbls[i] ) ; v.property = prop1 ;
		TVERIFYRC( mSession->createPIN( pid1, &v, 1 ) ) ;
	}

	// String can be converted to -1
	PID pidConvertableStr ;
	v.set( "-1" ) ; v.property = prop1 ;
	TVERIFYRC( mSession->createPIN( pidConvertableStr, &v, 1 ) ) ;

	PID pidBadType ;
	v.set( "bogus" ) ; v.property = prop1 ;
	TVERIFYRC( mSession->createPIN( pidBadType, &v, 1 ) ) ;

	// Try out the family
	countIndexMatches(mFamilyFirst, prop1, -1 /*val to test*/, 2 /*expected matches -1 and "-1"*/) ; 
	countIndexMatches(mFamilyFirst, prop1, -2.1, 1) ;
	countIndexMatches(mFamilyFirst, prop1, -0.8, 1) ;
	countIndexMatches(mFamilyFirst, prop1, "bogus", 1) ;

	// removing pid shouldn't cause trouble
	TVERIFYRC( mSession->deletePINs( &pidBadType, 1, MODE_PURGE ) ) ;

	// Changing a type to an unsupported one should silently remove it from the index
	v.set( "Not a Number Anymore" ) ; v.property = prop1 ;
	TVERIFYRC( mSession->modifyPIN( pidConvertableStr, &v, 1 ) ) ;
	countIndexMatches(mFamilyFirst, prop1, -1, 1) ; 
}

void TestFamilyType::testIndexForceInt()
{
	//
	// Create class before first pin
	//
	PropertyID prop1 = MVTUtil::getPropRand( mSession, "forceint" ) ;
	ClassID mFamilyFirst = createEqFamily( "FamFirst", prop1, VT_INT ) ;
	
	int i=-1 ;
	Value v ; v.set( i ) ; v.property = prop1 ;
	PID pid1 ;
	TVERIFYRC( mSession->createPIN( pid1, &v, 1 ) ) ;

	// Add some doubles. No truncation of decimal part (violates semantics)

	double dbls[] = { -1.02,
					  -1.9,
					  -2.1,
					  -0.8 } ;
	for (size_t i = 0; i < (sizeof( dbls )/sizeof(dbls[0])) ; i++)
	{
		v.set( dbls[i] ) ; v.property = prop1 ;
		TVERIFYRC( mSession->createPIN( pid1, &v, 1 ) ) ;
	}

	// String can be converted to -1
	PID pidConvertableStr ;
	v.set( "-1" ) ; v.property = prop1 ;
	TVERIFYRC( mSession->createPIN( pidConvertableStr, &v, 1 ) ) ;

	// String not indexed at all.  Overall transaction of committing the pin does not fail
	PID pidBadType ;
	v.set( "bogus" ) ; v.property = prop1 ;
	TVERIFYRC( mSession->createPIN( pidBadType, &v, 1 ) ) ;

	// Try out the family
	countIndexMatches(mFamilyFirst, prop1, -1 /*val to test*/, 2 /*expected matches -1 and "-1"*/) ; 
	countIndexMatches(mFamilyFirst, prop1, -2.1, 0) ;
	countIndexMatches(mFamilyFirst, prop1, -0.8, 0) ;
	countIndexMatches(mFamilyFirst, prop1, "bogus", 0) ;

	// removing the pid that wasn't really indexed shouldn't cause trouble
	TVERIFYRC( mSession->deletePINs( &pidBadType, 1, MODE_PURGE ) ) ;

	// Changing a type to an unsupported one should silently remove it from the index
	v.set( "Not a Number Anymore" ) ; v.property = prop1 ;
	TVERIFYRC( mSession->modifyPIN( pidConvertableStr, &v, 1 ) ) ;
	countIndexMatches(mFamilyFirst, prop1, -1, 3) ; 
}

void TestFamilyType::testIndexAfterPin()
{
	//
	// Pin already existing can establish the type
	//
	PropertyID prop1 = MVTUtil::getPropRand( mSession, "inpinfirst" ) ;
	
	// pin that will belong to family
	int i=100 ;
	Value v ; v.set( i ) ; v.property = prop1 ;
	PID pid1 ;
	TVERIFYRC( mSession->createPIN( pid1, &v, 1 ) ) ;

	// Create family.  pid1 should be categorized and establish the type
	ClassID mFamily = createEqFamily( "PinFirst", prop1 ) ;

	// String can be converted to a number
	v.set( "100" ) ; v.property = prop1 ;
	TVERIFYRC( mSession->createPIN( pid1, &v, 1 ) ) ;

	// Try out the family
	// (If we only got one match it would suggest that the type was VT_STR,
	// but two matches proves VT_INT)
	countIndexMatches(mFamily, prop1, 100 /*val to test*/, 2 /*expected matches 100 and "100"*/) ; 
}

void TestFamilyType::testForceType()
{
	//
	// Create class and explicitly specify the type 
	//
	PropertyID prop1 = MVTUtil::getPropRand( mSession, "infamfirst" ) ;
	ClassID mFamilyFirst = createEqFamily( "TypeForced", prop1, VT_DOUBLE ) ;
	
	// first pin is int but index should remain double
	int i=-1 ;
	Value v ; v.set( i ) ; v.property = prop1 ;
	PID pid1 ;
	TVERIFYRC( mSession->createPIN( pid1, &v, 1 ) ) ;

	// Add some doubles

	double dbls[] = { -1.02,  
					  -1.9,	  
					  -2.1,   
					  -0.8 } ;
	for (size_t i = 0 ; i < (sizeof( dbls )/sizeof(dbls[0])) ; i++)
	{
		v.set( dbls[i] ) ; v.property = prop1 ;
		TVERIFYRC( mSession->createPIN( pid1, &v, 1 ) ) ;
	}

	// String can be converted to -1.0
	v.set( "-1" ) ; v.property = prop1 ;
	TVERIFYRC( mSession->createPIN( pid1, &v, 1 ) ) ;

	// Try out the family
	countIndexMatches(mFamilyFirst, prop1, -1 /*val to test*/, 2 /*expected matches -1 and "-1"*/) ; 
	countIndexMatches(mFamilyFirst, prop1, -2, 0) ;
	countIndexMatches(mFamilyFirst, prop1, 0, 0) ;
}

void TestFamilyType::testStringIndex()
{
	// Explore type conversion behavior of a string index

	PropertyID prop1 = MVTUtil::getPropRand( mSession, "stringprop" ) ;

	// (Case sensitive OP_EQ string index)
	ClassID mFamilyFirst = createEqFamily( "StringIndex", prop1 /*, VT_STR*/ ) ;

	Value v ; 
	
	v.set( "First Data" ) ; v.property = prop1 ;
	PID pid ;
	TVERIFYRC( mSession->createPIN( pid, &v, 1 ) ) ;
	
	countIndexMatches(mFamilyFirst, prop1, "First Data", 1) ; 
}


ClassID TestFamilyType::createEqFamily
(
	const char * inFamilyName, 
	PropertyID inProp, 
	ValueType inIndexType /*= VT_ANY*/
)
{
	// Family - inProp == $param0
	CmvautoPtr<IStmt> lFamilyQ( mSession->createStmt() ) ;
	unsigned char lVar = lFamilyQ->addVariable() ;

	Value ops[2] ; 
	ops[0].setVarRef(0, inProp ) ;
	ops[1].setParam(0,inIndexType);

	CmvautoPtr<IExprTree> lE( mSession->expr( OP_EQ, 2, ops ) ) ;
	lFamilyQ->addCondition( lVar, lE ) ;
	
	// Make sure query can survive persistence to/freom string format, including the param type
	char * qString = lFamilyQ->toString() ;
#if VERBOSE
	mLogger.out() << "Family Query: " << endl << qString << endl ;
#endif
	CmvautoPtr<IStmt> lQRecreated( mSession->createStmt( qString ) );

#if TEST_TOSTRING_9082
	TVERIFY( lQRecreated.IsValid());
#endif

	if (lQRecreated.IsValid())
	{
		char * qString2 = lQRecreated->toString() ;
	#if VERBOSE
		mLogger.out() << "Family Query Recreated: " << endl << qString2 << endl ;
	#endif
		mSession->free( qString2 ) ;
	}
	else
	{
		// Temporary workaround to 9082 - use the original family
		lQRecreated.Attach( lFamilyQ.Detach() ) ;
	}
	mSession->free( qString ) ;

	string strFamily = "Family." ;
	strFamily += inFamilyName ;
	strFamily += mClassRand ;
	
	ClassID ret = STORE_INVALID_CLASSID;
	TVERIFYRC(defineClass(mSession, strFamily.c_str(), lQRecreated, &ret ));
	return ret;
}

template<class T>
void TestFamilyType::countIndexMatches(ClassID inIndex, PropertyID inProp, T inVal, int inExpected)
{
#if VERBOSE		
	mLogger.out() << "Scanning for pins with property " << inProp << " eq " << inVal << endl ;
#endif
	// Check how many matches for a specific int in an index
	CmvautoPtr<IStmt> lQUsingFamily( mSession->createStmt() ) ;

	Value paramVal ; paramVal.set( inVal ) ;

	ClassSpec cs ;
	cs.classID = inIndex ;
	cs.nParams = 1 ;
	cs.params = &paramVal ;

	lQUsingFamily->addVariable( &cs, 1 ) ;

	uint64_t cnt ;
	TVERIFYRC(lQUsingFamily->count( cnt )) ;

	if ( (int)cnt != inExpected )
	{
		TVERIFY( (int)cnt == inExpected ) ; 
		mLogger.out() << "Expected " << inExpected << " but got " << cnt << endl ;
	}

	if ( cnt == 0 ) 
		return ;

	ICursor* lC = NULL;
	TVERIFYRC(lQUsingFamily->execute( &lC, NULL, 0 ));
	CmvautoPtr<ICursor> lR(lC);
	if ( !lR.IsValid() ) { TVERIFY(false); return ; }

	IPIN * pResult ;
	unsigned long cntVerify = 0 ;
	while ( NULL != ( pResult = lR->next() ) )
	{
		// NOTE: although index only has one value type,
		// the retrieved, persisted value could be of a different (but compatible) type
		const Value * pIndex = pResult->getValue( inProp ) ;
		if ( pIndex == NULL ) 
		{ 
			TVERIFY(!"No index prop") ; pResult->destroy() ; continue ; 
		}
#if VERBOSE		
		MVTApp::output( *pIndex, mLogger.out(), mSession ) ;
#endif
		cntVerify++ ;
		pResult->destroy() ;
	}
	TVERIFY( cnt == cntVerify ) ; 
}
