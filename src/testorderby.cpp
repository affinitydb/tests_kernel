/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <float.h> // Numeric ranges
#include <limits.h>
#include "mvauto.h"
using namespace std;

#ifdef WIN32
#define	strncasecmp	_strnicmp
#endif

// Doesn't seem to be avilable on linux
#ifndef LLONG_MAX
#define	LLONG_MAX	9223372036854775807LL
#endif
#ifndef ULLONG_MAX
#define	ULLONG_MAX	0xffffffffffffffffLL
#endif

#define REALLY_VERBOSE 0

#define TEST_QUERY_ASSTRING 0

//#define SORTING_OF_BOOLS // known limitation that you can't sort by boolean properties
						   // because booleans don't have clear ordering

//#define COUNT_PINS 50000 // EXTERNAL SORT KICKS IN, but SLOW - enable this when testing it
#define COUNT_PINS 1000

#define MAX_ORDERING_DEPTH 4 // Store supports sorting by many properties,
							// but in practice even with 10,000 pins it is hard to get
							// to a huge depth.  E.g. to even consider the 4th ordering
							// criteria the first 3 must be completely identical

#define CNT_RANDOM_ORDERINGS 5 // How many different combinations of properties to try out

//Test of the IStmt::setOrderBy method

// Publish this test.
class TestOrderBy : public ITest
{
	public:
		TEST_DECLARE(TestOrderBy);
		virtual char const * getName() const { return "testorderby"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "tests the working of ORDER_BY"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual bool includeInPerfTest() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void doTest();
		void populateStore(ISession *session);
		bool validateResults(unsigned long cntExpected,IStmt *Q,size_t cntOrderBy, const OrderSeg *order) ;
		int compareValues( const Value * val, const Value * nextval, unsigned short order ) ;
		int comparePins( IPIN * current, IPIN * next, size_t cntOrderBy, const OrderSeg *order ) ;
		IStmt * getClassQuery(DataEventID incls, ISession* session );
		void getRandomOrdering( OrderSeg *order ) ;
		bool validateBasicExprResults(unsigned long cntExpected, IStmt *Q ) ;
		void testPinIDOrdering(ISession* session);
		void testOrderByIndex(ISession * inSession, int inCntPins) ;

		enum Props
		{
			propStrNoCase, // Test ORD_NCASE
			propInt, // Random int
			propStr,
			propFlt,
			propDbl,
			propUInt,
			propI64,
			propUI64,
			propPinIdx, // Each pin created has an index
			propBool,
			propDate,
			propFTStr, // Used as part of the actual query
			propPartialInt, // Random int that is only present on some of the pins
			propSpecCreated,
			propSpecUpdated,
			propSpecCreatedBy,
			propCount
		} ;

		ISession * mSession ;
		PropertyID mPropids[propCount];
		size_t mMaxDepth ; // Diagnosis or how many of the properties actually were used
						   // when comparing pins.  E.g. no use sorting by many properties if the
						   // first property is already unique
};

// For debugging purposes, keep in sync with Props enum
char * propNames[] = { 
"str_nocase",
"rand int",
"str",
"flt",
"dbl",
"uint",
"i64",
"ui64",
"pin index",
"bool",
"date",
"FT_str",
"partial_int",
"create_date",
"update_date",
"created_by"} ;

TEST_IMPLEMENT(TestOrderBy, TestLogger::kDStdOut);

// Implement this test.
int	TestOrderBy::execute()
{
	if (MVTApp::startStore())
	{
		doTest() ;
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	return RC_OK  ;
}

void TestOrderBy::doTest()
{
	mSession =MVTApp::startSession();
	if(NULL == mSession) {
		mLogger.out()<<"Failed to start session\n";
		return;
	}
	testOrderByIndex(mSession, 50) ;
	testOrderByIndex(mSession, 2000) ;

	populateStore(mSession); 

	Tstring lStr; MVTRand::getString(lStr,10,10,false,false);
	// Create a class which will exclude
	// some of the pins we create
	// (( PinIndex >= 2 && PinIndex <= (COUNT_PINS - 2) ) && propInt >= -5)
	DataEventID	lCLSID = STORE_INVALID_CLASSID;
	{
		IStmt * lQ	= mSession->createStmt();
		unsigned char const var = lQ->addVariable();
		TExprTreePtr lE;
		{
			Value val[2];

			// 1: MyIndex >= param1
			val[0].setVarRef(0,mPropids[propPinIdx]);
			val[1].set(2);
			TExprTreePtr lE1 = EXPRTREEGEN(mSession)(OP_GE, 2, val, 0);

			// 2: MyIndex <= param2
			val[0].setVarRef(0,mPropids[propPinIdx]);
			val[1].set(COUNT_PINS-2);
			TExprTreePtr lE2 = EXPRTREEGEN(mSession)(OP_LE, 2, val, 0);

			// 3: 1	&& 2
			val[0].set(lE1);
			val[1].set(lE2);
			TExprTreePtr lE3 = EXPRTREEGEN(mSession)(OP_LAND, 2, val, 0);

			// 4: MyRandom >= param3
			val[0].setVarRef(0,mPropids[propInt]);
			val[1].set(-5);
			TExprTreePtr lE4 = EXPRTREEGEN(mSession)(OP_GE, 2, val, 0);

			// 5: 3	&& 4
			val[0].set(lE3);
			val[1].set(lE4);
			lE = EXPRTREEGEN(mSession)(OP_LAND, 2, val, 0);				
		}
		lQ->addCondition(var,lE);
		
		char lB[100];	
		// REVIEW: , character was removed from this class name because
		// that is illegal and breaks query parsing
		sprintf(lB,"TestOrderBy_MyComplexQuery%s_%d", lStr.c_str(), 1);
		TVERIFYRC(defineClass(mSession,lB, lQ, &lCLSID));
		lE->destroy();
		lQ->destroy();
	}

	//Determine expected number of pins by running query with no ordering
	IStmt * lQNoOrder = getClassQuery(lCLSID,mSession);
	uint64_t expectedCount = 0 ;
	TVERIFYRC(lQNoOrder->count( expectedCount )) ;
	lQNoOrder->destroy() ;
	mLogger.out() << "Sorting " << expectedCount << " pins" << endl ;

	// Systematically run each property as the single ordering criteria
	for (int i = 0; i < propCount; i++)
	{		
		if (isVerbose())
			mLogger.out() << std::endl << "Query ordered by " << propNames[i] << std::endl;


		CmvautoPtr<IStmt> lQ(getClassQuery(lCLSID,mSession));

		// 50% chance of being ORD_DESC
		OrderSeg lOrder={NULL,mPropids[i],uint8_t(ORD_NULLS_BEFORE|(i == propStrNoCase?ORD_NCASE:0)|(MVTRand::getBool()?0:ORD_DESC)),0,0};
		TVERIFYRC(lQ->setOrder(&lOrder,1));

		const char *lQStr = lQ->toString();	TVERIFY( lQStr != NULL ) ;
#if REALLY_VERBOSE > 1
		mLogger.out() << lQStr << endl ;
#endif		
		TVERIFY(validateResults((unsigned long)expectedCount,lQ,1,&lOrder)) ;
	}

	// Try out some random combinations of properties
	// (it would take too long to do every single possible combination)
	for ( int i = 0 ; i < CNT_RANDOM_ORDERINGS ; i++ )
	{
		if (isVerbose())
			mLogger.out() << std::endl << "Query ordered by " ;

		OrderSeg order[ MAX_ORDERING_DEPTH ] ;
		getRandomOrdering( order ) ;

		CmvautoPtr<IStmt> lQ(getClassQuery(lCLSID,mSession));
		lQ->setOrder( order, MAX_ORDERING_DEPTH);

		const char *lQStr = lQ->toString();	TVERIFY( lQStr != NULL ) ;
	#if REALLY_VERBOSE 
		mLogger.out() << lQStr << endl ;
	#endif		
		TVERIFY(validateResults((unsigned long)expectedCount,lQ,MAX_ORDERING_DEPTH,order)) ;
	}

	{
		// Basic Expression
		if (isVerbose())
			mLogger.out() << endl << "Ordering by basic expression..." << endl ;

		Value val[2];
		val[0].setVarRef(0,mPropids[propPinIdx]);
		val[1].setVarRef(0,mPropids[propInt]);			
		TExprTreePtr lE = EXPRTREEGEN(mSession)(OP_MUL, 2, val, 0);

		CmvautoPtr<IStmt> lQ(getClassQuery(lCLSID,mSession));
		OrderSeg ord = {lE,STORE_INVALID_URIID,0,0,0};
		TVERIFYRC(lQ->setOrder(&ord,1));
		lE->destroy() ;

		const char *lQStr = lQ->toString();	TVERIFY( lQStr != NULL ) ;
#if REALLY_VERBOSE
		mLogger.out() << lQStr << endl ;
#endif		
		TVERIFY( validateBasicExprResults((unsigned long)expectedCount, lQ ) ) ;
	}

	testPinIDOrdering(mSession) ;

	// Confirm PINID sorting with pins from second session
	mSession->terminate(); mSession = NULL ;
	ISession * const session2 =	MVTApp::startSession();
	if (session2)
	{
		testPinIDOrdering(session2) ;
		session2->terminate();
	}
	else 
		mLogger.out()<<"Failed to get a session\n";
}

void TestOrderBy::testPinIDOrdering(ISession* session)
{
	// Check ordering by PROP_SPEC_PINID, across pins from at least 2 sessions...
	mLogger.out() << "Query ordered by PROP_SPEC_PINID" << endl;

	PropertyID groupingProp = MVTApp::getProp(session, "testPinIDOrdering"); 

	Value val ; val.set(true) ; val.property = groupingProp;
	PID pid;
	CREATEPIN(session, &pid, &val, 1);

	PID specificPid ;
	CREATEPIN(session, &specificPid, &val, 1);

	CREATEPIN(session, &pid, &val, 1);
	CREATEPIN(session, &pid, &val, 1);
	
	IStmt * const lQ = session->createStmt();
	unsigned char lVar = lQ->addVariable();
	OrderSeg lSortBy = {NULL, PROP_SPEC_PINID, 0, 0, 0};
	lQ->setOrder( &lSortBy, 1);
	lQ->setPropCondition(lVar,&groupingProp,1); // Only sort the pins created in this function (see 5938)	
	ICursor * lR = NULL;
	TVERIFYRC(lQ->execute(&lR));
	IPIN * lPrev = NULL;
	IPIN * lNext;
	bool bFoundSpecificPID =  false ;
	int iPin;
	for (iPin = 0; NULL != (lNext = lR->next()); iPin++)
	{
		if (lNext->getPID() == specificPid ) 
			bFoundSpecificPID = true ;

		if (lPrev && !(lPrev->getPID() < lNext->getPID()))
		{
			mLogger.out() << "at index #" << std::dec << iPin << std::hex << " -> prev: " << lPrev->getPID().pid << " next: " << lNext->getPID().pid << endl;
			TVERIFY(false);
		}
		if (lPrev)
			lPrev->destroy();
		lPrev = lNext;
	}
	if (lPrev)
		lPrev->destroy();
	lR->destroy();
	lQ->destroy();

	// Sanity check without the orderby
	IStmt * lQ2 = session->createStmt();
	lQ2->setPropCondition(lQ2->addVariable(),&groupingProp,1);
	uint64_t cntPinNoOrder ;
	lQ2->count( cntPinNoOrder ) ;
	lQ2->destroy();

	if ( (int)cntPinNoOrder != iPin )
	{
		stringstream lErr ; lErr << "Wrong number of PINs returned when sorted by PINID. Expected " << cntPinNoOrder << " got " << iPin ;
		TVERIFY2(0,lErr.str().c_str()) ;
	}
}

IStmt * TestOrderBy::getClassQuery(DataEventID incls, ISession* session)
{
	IStmt * lQ = session->createStmt();
	SourceSpec lCS;
	lCS.objectID = incls;
	lCS.nParams = 0;
	lCS.params = NULL;
	unsigned char const var1 = lQ->addVariable(&lCS, 1);

	lQ->setConditionFT(var1,"archer");

	return lQ ;
}

void TestOrderBy::getRandomOrdering( OrderSeg * order )
{
	// Pick some non-repeated properties to order by
	// Also randomize the ordering ascending/descending flags
	for ( int k = 0 ; k < MAX_ORDERING_DEPTH ; k++ )
	{
		order[k].expr = NULL; order[k].lPrefix = 0; order[k].var = 0;
		// Pick a random property, but make sure
		// we haven't already picked it
		int rnd;
		bool bValidRnd = false ;
		do
		{
			bValidRnd = true ;
			rnd = MVTRand::getRange(0,propCount-1) ;					
#ifndef SORTING_OF_BOOLS
			if ( rnd == propBool ) 
			{
				bValidRnd = false ;
				continue ;
			}
#endif
			for ( int j = 0 ; j < k ; j++ )
			{
				if ( order[j].pid == mPropids[rnd] )
				{
					bValidRnd = false ;
					break ; // Already picked this one
				}
			}
		} while(!bValidRnd) ;

		order[k].pid = mPropids[rnd];
		order[k].flags = MVTRand::getBool()?0:ORD_DESC ;				// may be ASC OR DESC
		order[k].flags |= MVTRand::getBool()?ORD_NULLS_BEFORE:ORD_NULLS_AFTER;		// may be ORD_NULLS_AFTER or ORD_NULLS_BEFORE
		if (isVerbose())
			mLogger.out() << propNames[rnd] << "," ;
	}
	if (isVerbose())
		mLogger.out() << endl ;
}

bool TestOrderBy::validateResults
(
	unsigned long cntExpected, // Expected number of pins in queyr
	IStmt *Q,				   // The query to test
	size_t cntOrderBy,		   // Number of properties that decide the pin order
	const OrderSeg *order		   // Properties that order the pins
)
{
	// General method for validating that pins in a query 
	// are ordered according to some specific set of property values

#if TEST_QUERY_ASSTRING
	char * strQ = Q->toString() ;
	CmvautoPtr<IStmt> QCopy(mSession->createStmt(strQ));
	if (!QCopy.IsValid())
	{
		TVERIFY(!"Could not recreate query from string") ;
		mLogger.out() << "Original query: " << strQ << endl ;
		QCopy.Attach(Q->clone()) ;
	}
	mSession->free(strQ);
#else
	CmvautoPtr<IStmt> QCopy( Q->clone() ) ;
#endif

	uint64_t cnt ;
	TVERIFYRC(QCopy->count( cnt )) ;
	TVERIFY( cnt == cntExpected ) ; 
	TVERIFY( !MVTUtil::findDuplicatePins( QCopy, mLogger.out() ) );

	ICursor* lC = NULL;
	TVERIFYRC(QCopy->execute(&lC));
	CmvautoPtr<ICursor> res(lC);

	mMaxDepth = 0 ;
	
	IPIN * current = res->next();
	IPIN * next = NULL;

	while( next = res->next() )
	{
		int compResult = comparePins( current, next, cntOrderBy, order ) ; 
		if ( compResult < 0 )
		{
			current->destroy() ; next->destroy() ;
			return false ;
		}

		// Move forward a step
		current->destroy() ;
		current = next ;
	}

	if ( current != NULL ) current->destroy() ;

		
	// This extra info helps judge the quality of the test and data
	if (isVerbose())
	{
		if ( mMaxDepth == cntOrderBy )
		{
			mLogger.out() << "Some pins were equal" << endl ; // This is perfectly ok, but unlikely if sorting
															// by many properties
		}
		else
			mLogger.out() << "It reached depth " << (int)mMaxDepth+1 << endl ;
	}
	return true ;
}

int TestOrderBy::comparePins( IPIN * current, IPIN * next, size_t cntOrderBy, const OrderSeg * order )
{
	TVERIFY( cntOrderBy > 0  && current != NULL && next != NULL ) ;
	size_t currentProp = 0 ;
	while( currentProp < cntOrderBy  )
	{
		const Value * valCur = current->getValue( order[currentProp].pid ) ;
		const Value * valNext = next->getValue( order[currentProp].pid ) ;

		int compResult = 0 ;
		if (  valCur == NULL || valNext == NULL )
		{
			// Only this property should ever be missing
			TVERIFY(order[currentProp].pid == mPropids[propPartialInt]) ;
		}

		compResult = compareValues( valCur, valNext, order[currentProp].flags ) ;

		if ( compResult != 0 ) return compResult ;

		// Values are identical so we need to look at the next property
		currentProp++ ;
		if ( mMaxDepth < currentProp ) mMaxDepth = currentProp ;
	}

	// If we reach here the Pins are completely equal for all considered properties
	return 0 ;
}

// Helper to do low level comparison between values
template<class T> int comp( T v1, T v2 )
{
	if ( v1 == v2 ) return 0 ;
	else if ( v1 < v2 ) return 1 ;
	else return -1 ;
}

// Negative means nextval comes before val
// 0 means equal
// Positive means nextval comes after val
int TestOrderBy::compareValues( const Value * val, const Value * nextval, unsigned short order )
{
	int ret = -1 ;
	if (  val == NULL || nextval == NULL )
	{
		// Special case for missing properties
		if ( val == nextval )
			return 0 ; // Both NULL
		else
			if (order&ORD_NULLS_AFTER)
				return nextval ? -1 : 1;
			else if (order&ORD_NULLS_BEFORE)
				return nextval ? 1 : -1;
			else
				// if NULL flag isn't specified, it's default order. currently is ORD_NULLS_AFTER
				return nextval ? -1 : 1;
	}				

#if REALLY_VERBOSE
	MVTApp::output( *val, mLogger.out(), NULL ) ; // Very verbose listing of each element as they are compared
#endif

	TVERIFY( val->type == nextval->type ) ;

	switch( val->type )
	{
	case(VT_STRING): 
		if ( order & ORD_NCASE )
			ret = strncasecmp(nextval->str,val->str,strlen(nextval->str));
		else
			ret = strcmp(nextval->str,val->str) ; 
		break;

#ifdef SORTING_OF_BOOLS
	case(VT_BOOL): ret = comp( val->b, nextval->b ) ;	break;
#else
	case(VT_BOOL): ret = 0 ;	break;
#endif

	case(VT_INT): ret = comp( val->i, nextval->i ) ; break;
	case(VT_UINT): ret = comp( val->ui, nextval->ui ) ; break;
	case(VT_INT64): ret = comp( val->i64, nextval->i64 ) ; break;
	case(VT_UINT64): case(VT_DATETIME): ret = comp(val->ui64,nextval->ui64) ; break;
	case(VT_FLOAT): ret = comp(val->f,nextval->f) ; break;
	case(VT_DOUBLE): ret = comp(val->d,nextval->d) ; break;
	case(VT_REF): ret = comp(val->uid,nextval->uid) ; break;			//???????
	case(VT_IDENTITY): ret = comp(val->iid,nextval->iid) ; break;
	default:
		TVERIFY(false) ;
		break ;
	}

	if ((order&ORD_DESC)!=0) 
		ret = -ret ;

	if ( ret < 0 )
	{
		// Comparison failure
		mLogger.out() << "These values are out of order:" ;
		if ((order&ORD_DESC)!=0) 
			mLogger.out() << " (descending) " ;
		mLogger.out() << endl ;
		MVTApp::output( *val, mLogger.out(), NULL ) ;
		MVTApp::output( *nextval, mLogger.out(), NULL ) ;
	}

	return ret ;
}

bool TestOrderBy::validateBasicExprResults
(
	unsigned long cntExpected, // Expected number of pins in queyr
	IStmt *Q				   // The query to test
)
{
	// Go through the results and make sure that the ordering makes
	// sense by calculating the same expression
	uint64_t cnt ;
	TVERIFYRC(Q->count( cnt )) ;
	TVERIFY( cnt == cntExpected ) ; 
	TVERIFY( !MVTUtil::findDuplicatePins( Q, mLogger.out() ));

	ICursor* lC = NULL;
	TVERIFYRC(Q->execute(&lC));
	CmvautoPtr<ICursor> res(lC);

	bool bRet = true ;

	IPIN * current = res->next();
	IPIN * next = NULL;

	while( next = res->next() )
	{
		// Ordering Expression is index * int
		int cPinIdx = current->getValue( mPropids[propPinIdx] )->i ;
		int cInt = current->getValue( mPropids[propInt] )->i ;
		int nPinIdx = next->getValue( mPropids[propPinIdx] )->i ;
		int nInt = next->getValue( mPropids[propInt] )->i ;

		int cOrder = cPinIdx * cInt ;
		int nOrder = nPinIdx * nInt ;

		if ( cOrder > nOrder )
		{
			mLogger.out() << "Error: Expected " << cOrder << " <  " << nOrder << endl ;
			bRet = false ;
			if (!isVerbose())
			{
				// Stop the comparisons
				current->destroy() ; next->destroy() ;
				return false ; 
			}
		}

		// Move forward a step
		current->destroy() ;
		current = next ;
	}

	if ( current != NULL ) current->destroy() ;
	return bRet ;
}

void TestOrderBy::populateStore(ISession *session){
	
	MVTApp::mapURIs(session,"TestOrderBy.prop",sizeof(mPropids)/sizeof(mPropids[0]),mPropids);
	mPropids[propSpecCreated] = PROP_SPEC_CREATED; // Hardcoded special properties.  Store sets their values automatically.
	mPropids[propSpecUpdated] = PROP_SPEC_UPDATED;
	mPropids[propSpecCreatedBy] = PROP_SPEC_CREATEDBY;

	for (int i = 0; i < COUNT_PINS; i++)
	{
		PID pid;
		Value val[propCount];
		int pos = 0 ; // We don't necessarily fill in every val for each pin

		int iNegator = MVTRand::getBool() ? -1 : 1 ; // Make sure to include some negative numbers in the 
													// types that can support it

		SETVALUE(val[pos], mPropids[propPinIdx], i, OP_SET); pos++;

		SETVALUE(val[pos], mPropids[propInt], iNegator * MVTRand::getRange(0,9), OP_SET); pos++;
		SETVALUE(val[pos], mPropids[propBool], MVTRand::getBool(), OP_SET); pos++;
		SETVALUE(val[pos], mPropids[propFlt], (float)(iNegator * 100.5 * rand() /RAND_MAX), OP_SET); pos++;

		// Pick out of a selection of about 100 possible values, near the outer range of 
		// the capacity of each type
		SETVALUE(val[pos], mPropids[propUInt], (unsigned int)((UINT_MAX/100)*MVTRand::getRange(0,100)), OP_SET); pos++;
		SETVALUE(val[pos], mPropids[propDbl], (double)(iNegator*(DBL_MAX/100)*MVTRand::getRange(0,100)), OP_SET); pos++;
		val[pos].setI64((int64_t)(iNegator*(LLONG_MAX/100)*MVTRand::getRange(0,100))); SETVATTR(val[6], mPropids[propI64], OP_SET); pos++;
		val[pos].setU64(((ULLONG_MAX/100)*MVTRand::getRange(0,100))); SETVATTR(val[pos], mPropids[propUI64], OP_SET);	pos++;

		val[pos].setDateTime(MVTRand::getDateTime( session )); val[pos].property = mPropids[propDate] ; pos++;

		if ( MVTRand::getRange(0,10) > 0 )
		{
			SETVALUE(val[pos], mPropids[propFTStr], "archer", OP_SET); val[pos].meta = META_PROP_FTINDEX; pos++;
		}
		else
		{
			SETVALUE(val[pos], mPropids[propFTStr], "different string", OP_SET); pos++;
		}

		Tstring str;
		MVTRand::getString(str, 20, 10, false);
		SETVALUE(val[pos], mPropids[propStr], str.c_str(), OP_SET); pos++;

		Tstring str2;
		MVTRand::getString(str2, 2, 0, false); // short little strings so ordering is really obvious
		str2[0] = MVTRand::getBool() ? 'a' : 'A' ; // Emphasis apparent string sorting problem

		SETVALUE(val[pos], mPropids[propStrNoCase], str2.c_str(), OP_SET); pos++;

		// 10% of the pins are missing this property
		if ( MVTRand::getRange(0,9) > 0 )
		{
			SETVALUE(val[pos], mPropids[propPartialInt], iNegator * MVTRand::getRange(0,9), OP_SET); pos++;
		}

		val[pos].setDateTime(0) ; val[pos].property = PROP_SPEC_CREATED ; pos++ ;
		val[pos].setDateTime(0) ; val[pos].property = PROP_SPEC_UPDATED ; pos++ ;
		val[pos].setIdentity(STORE_OWNER) ; val[pos].property = PROP_SPEC_CREATEDBY ; pos++ ;
		
		CREATEPIN(session, &pid, val, pos);		
			
		if ( i % ( COUNT_PINS / 10 ) == 0 ) 
		{
		   MVTestsPortability::threadSleep(100);	 // Just to give changing _CREATED_ time stamps
		   mLogger.out() << "." ;
	  	}
	}
}

void TestOrderBy::testOrderByIndex( ISession * inSession, int inCntPins )
{
	// Self-contained portion of the test
	// Order by should be using index to speed up calculations, in both ascending and descending order
	PropertyID dateProp ;
	MVTApp::mapURIs( inSession, "TestOrderBy_IndexedDate", 1, &dateProp ) ;

	// Create index (note: similar class also investigated in testpidocs7.cpp)
	CmvautoPtr<IStmt> classQ( inSession->createStmt() ) ;
	unsigned char var = classQ->addVariable() ;

	Value operands[2] ;
	operands[0].setVarRef(0,dateProp) ;
	operands[1].setParam(0) ;  
	CmvautoPtr<IExprNode> e(inSession->expr( OP_IN, 2, operands )) ;
	TVERIFYRC(classQ->addCondition(var,e));

	string randClassName ;
	
	RC rc ;
	do 
	{
		randClassName = MVTRand::getString2( 20,20,false ) ;
		randClassName += "_TestOrderBy" ;
		rc = defineClass(inSession, randClassName.c_str(), classQ) ;
	}
	while( rc == RC_ALREADYEXISTS ) ; // Problem when seed reused

	DataEventID dateClass = STORE_INVALID_CLASSID;
	TVERIFYRC(inSession->getDataEventID( randClassName.c_str(), dateClass)) ;

	//
	// Create pins for class
	//
	mLogger.out() << "Create " << inCntPins << " pins for sorting" << endl ;
	int i , j;
	vector<uint64_t> dateValue;
	
	//generating distinct & random dates
	dateValue.push_back(MVTRand::getDateTime( inSession, true /* allow future */));
	for ( i = 1 ; i < inCntPins ;  )
	{
		uint64_t dt = MVTRand::getDateTime( inSession, true /* allow future */);
		for( j = 0 ; j < i ; j++ )
		{
			if(dt == dateValue[j]) break;
		}
		if(j == i) 
		{
			dateValue.push_back(dt);
			i++;
		}
	}
	for ( i = 0 ; i < inCntPins ; i++ )
	{
		// Create pins with distinct and random dates
		Value v ;
		v.setDateTime( dateValue[i] ) ;
		v.property = dateProp ;

		TVERIFYRC(inSession->createPIN(&v, 1, NULL, MODE_PERSISTENT|MODE_COPY_VALUES));
	}
	//
	// Create query to use index
	//
	CmvautoPtr<IStmt> allPins( inSession->createStmt() ) ;

	Value allDateRange[2] ;
	allDateRange[0].setDateTime(0);
	allDateRange[1].setDateTime((uint64_t)~0);

	Value classParam ;
	classParam.setRange(allDateRange);	
	
	SourceSpec cs ;
	cs.objectID = dateClass ;
	cs.nParams = 1 ;
	cs.params = &classParam ;
	allPins->addVariable( &cs, 1 ) ;

	//
	// Check with ascending ordering
	//

	OrderSeg ord={NULL,dateProp,0,0,0}; // Ascending order
	TVERIFYRC( allPins->setOrder( &ord, 1) ) ;

	// Removing duplicate pins requires sorting by pin id so
	// it might disable the use of the index
	unsigned long mode = 0;		//MODE_NON_DISTINCT; obsolete

	uint64_t cnt ;
	TVERIFYRC(allPins->count( cnt,0,0,~0,mode)) ;
	TVERIFY( (int)cnt == inCntPins ) ;

	vector<PID> ascendingOrderPins(inCntPins)  ;
	IPIN * pin ;

	mLogger.out() << "----Ascending Order ";
	long lStartTime = getTimeInMs() ;

	ICursor* lC = NULL;
	TVERIFYRC(allPins->execute(&lC,0,0,~0,0,mode));
	CmvautoPtr<ICursor> ascendingResults(lC);
	size_t pos = 0 ;
	while( NULL != ( pin = ascendingResults->next() ) ) 
	{
#if REALLY_VERBOSE
		mLogger.out() << std::hex << pin->getPID() << endl;
#endif
		ascendingOrderPins[pos] = pin->getPID() ;
		pin->destroy() ;
		pos++ ;
	}
	TVERIFY( (int)pos == inCntPins ) ;

	long lEndTime = getTimeInMs() - lStartTime ;
	mLogger.out() << "Enumeration time (ms)" << lEndTime << endl ;

	//
	// DESCENDING ORDER
	//

	ord.flags = ORD_DESC ;

	TVERIFYRC(allPins->setOrder( &ord, 1 )) ;
	TVERIFYRC(allPins->count( cnt,0,0,~0,mode)) ;
	TVERIFY( (int)cnt == inCntPins ) ;

	mLogger.out() << "----Decending Order ";
	lStartTime = getTimeInMs() ;
	TVERIFYRC(allPins->execute(&lC,0,0,~0,0,mode));
	CmvautoPtr<ICursor> descendingResults(lC);

	bool bErrorSuppress = false ;
	pos = 0 ;
	while( NULL != ( pin = descendingResults->next() ) ) 
	{
#if REALLY_VERBOSE
			mLogger.out() << pin->getPID() << endl;
#endif

		// Expect reverse of previous
		if ( pos < (size_t)inCntPins )
		{
			TVERIFY( ascendingOrderPins[ inCntPins - pos - 1 ] == pin->getPID() ) ;
		}
		else
		{
			if (!bErrorSuppress)
			{
				//leaving endless loop for the moment....

				//TVERIFY2(0,"More results than expected, stopping scan" ) ;
				//bErrorSuppress = true ;

				mLogger.out() << "Extra pin: " << std::hex << pin->getPID() ;

				// Figure out which pin it is - this shows the pattern of repeat
				for ( int k = 0 ; k < inCntPins ; k++ )
				{
					if ( ascendingOrderPins[k] == pin->getPID() )
					{
						mLogger.out() << " Duplicate of position " << std::dec << (inCntPins - k - 1) << endl ;
						break ;
					}
				}	
			}			
		}
		pin->destroy() ;
		pos++ ;	
	}	
	if ( (int)pos != inCntPins ) { mLogger.out() << "Expected matches " << inCntPins << " got " << (int)pos << endl; } 

	// REVIEW: The timing of the two queries doesn't really prove the 
    // use of the index but if one timing was way off it would certainly be interesting
	lEndTime = getTimeInMs() - lStartTime ;
	mLogger.out() << "Enumeration time (ms)" << lEndTime << endl ;
}
