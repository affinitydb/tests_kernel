/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

//
// Demonstrates several ways to implement a query that checks conditions on 4 different properties
// In the slowest approach it does a full scan query and evaluates an expression against each one.
// In the fastest approach it merges the results of lookups into 4 separate indexes (one for each property)
//
// for a variation that only tests performance (without any validation of expected results) run with "perf" argument
// tests testmultivarjoin perf

#include "app.h"
#include "mvauto.h" // MV auto pointers
#include "classhelper.h" 
#include "timedevent.h"
using namespace std;

#define CNT_SUB_QUERY 4 // Number of conditions on properties that we AND together
						// (note: if we want to change this value we also have to generalize 
						// some of the code of this test)
#define CNT_PINS 100

#define CNT_PINS_PERF 25000
#define CNT_EXTRA_PROPS 50  // For perf testing purposes we have to make the pins bigger

//#define SERIALIZE_Q 0 

class TestMultiVarJoin : public ITest
{
	enum QueryVariation
	{
		QV_ExplicitJoin=0,
		QV_Implicit,
		QV_FullScan,	// Note: when FT added in this isn't really Full Scan anymore
		QV_1Family,		// 1 Family + Expression
		QV_ImplicitWithClass,
		QV_ImplicitWithClassFirst,
		QV_ImplicitClassFamExpr,
		QV_ImplicitFamClassExpr,
		QV_ClassExpr,
		QV_ALL
	};

	static const char * sQDesc[QV_ALL];

	public:
		TEST_DECLARE(TestMultiVarJoin);

		// By convention all test names start with test....
		virtual char const * getName() const { return "testmultivarjoin"; }
		virtual char const * getDescription() const { return "13070 - compound query with more than two conditions to join. Optional Arg: [perf]"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual char const * getHelp() const { return ""; } 
		
		virtual int execute();
		virtual void destroy() { delete this; }
		
	protected:
		void doTest() ;

		void createMeta();
		void createPINs(int inCntPins,bool bLargePins=false);
		void doPerfTest();

		void doPerfPass(TimedEvent & te,const char * indesc, bool ft, bool orderby, int whichQ);

		void compareQueries(bool addFT, int orderByProp = -1);

		IExprNode * getQueryAsExpression();

		IStmt * getQuery(QueryVariation whichQ);

		IStmt * doExplicitJoin();
		IStmt * do1FamilyAndExpressions();
		IStmt * doFullScan();
		IStmt * doImplicitJoin();
		IStmt * doImplicitJoinWithClass(bool bClassFirst);
		IStmt * doImplicitClassFamExpr(bool bClassFirst);
		IStmt * doClassAndExpressions();

		IStmt * serializeQ(IStmt *q);

		void PrintExampleXPath();
	private:	
		ISession * mSession ;

		PropertyID mProps[CNT_SUB_QUERY];
		PropertyID mExtraProps[CNT_EXTRA_PROPS];
		PropertyID mFTProp;

		ClassID mFamilies[CNT_SUB_QUERY]; // Each sub query uses a different family
		string mFamilyNames[CNT_SUB_QUERY];
		ClassID mClass; // Extra class that includes all the test pins
		int	mRandLookups[CNT_SUB_QUERY]; // Random numbers used in all queries (so they produce equiv results)
};
TEST_IMPLEMENT(TestMultiVarJoin, TestLogger::kDStdOut);
const char * TestMultiVarJoin::sQDesc[] = { 
	"4 Family Join (Explicit)",
	"4 Family Join (Implicit)",
	"FullScan + Expression",
	"1 Family + Expression",
	"4 Family + Class (Implicit)",
	"Class + 4 Family (Implicit)",
	"Class + 1 Family + Expression (Implicit)",
	"1 Family + Class + Expression (Implicit)",
	"Class + Expression",
};

int TestMultiVarJoin::execute()
{
	bool bPerf=( (get_argc() >= 3) && (0 == strcmp("perf",get_argv(2))));

	if ( bPerf ) MVTApp::deleteStore();

	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }
	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	if (bPerf)
	{
		doPerfTest();
	}
	else
	{
		doTest() ;
	}

	mSession->terminate() ;
	MVTApp::stopStore() ;
	return RC_OK ;
}

void TestMultiVarJoin::createMeta()
{
	// Prepare "meta-data" used by all approaches

	mFTProp=MVTUtil::getPropRand(mSession,"testmultivarjoin.");

	size_t i ;
	for ( i = 0 ; i < CNT_SUB_QUERY ; i++ )
	{
		mProps[i]=MVTUtil::getPropRand(mSession,"testmultivarjoin.");

		// This is the lookup we will do on this property
		// e.g. looking for all pins with mProp[i] greater than this rand value
		mRandLookups[i]=MVTRand::getRange(0,500);

		//
		// Create a family to index each property
		// These families are used by some but not all queries
		//
		// Create query defining family so that index is created
		CmvautoPtr<IStmt> qFamily(mSession->createStmt());
		unsigned char v=qFamily->addVariable();

		Value qexpr[2];
		qexpr[0].setVarRef(0,mProps[i]);
		qexpr[1].setParam(0);

		// OP_GT isn't really realistic, doing an OP_IN range would 
		// be more broadly useful, but its good enough for this simple test
		CmvautoPtr<IExprNode> expr(mSession->expr(OP_GT,2,qexpr));

		TVERIFYRC(qFamily->addCondition(v,expr));

		mFamilies[i]=MVTUtil::createUniqueClass(mSession,"testmultivarjoin",qFamily,&(mFamilyNames[i]));
		TVERIFY(mFamilies[i]!=STORE_INVALID_CLASSID);
	}

	CmvautoPtr<IStmt> qClass(mSession->createStmt());
	unsigned char v=qClass->addVariable();
	qClass->setPropCondition(v,&mProps[0],1);

	mClass=MVTUtil::createUniqueClass(mSession,"testmultivarjoin",qClass);
	TVERIFY(mClass!=STORE_INVALID_CLASSID);
}

void TestMultiVarJoin::createPINs(int inCntPins, bool bLargePins)
{
	mLogger.out() << "-----------------Creating " << inCntPins << " pins-------------" << endl;

	int i;
	for ( i = 0 ; i < inCntPins ; i++ )
	{
		Value vals[CNT_SUB_QUERY];

		for ( size_t k = 0 ; k < CNT_SUB_QUERY ; k++ )
		{
			vals[k].set( MVTRand::getRange(0,1000) );
			vals[k].property=mProps[k];
		}

		PID pid;IPIN *pin;
		TVERIFYRC(mSession->createPIN(vals,CNT_SUB_QUERY,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
		pid = pin->getPID();
		pin->destroy();
		Value strVal;

		// When FT turned on then only 80% of the pins will match
		if ( MVTRand::getRange(1,100) < 80 )
			strVal.set( "findme" ); 
		else
			strVal.set( "missme" ); 
		
		strVal.property=mFTProp;
		TVERIFYRC(mSession->modifyPIN(pid,&strVal,1));

		if ( bLargePins )
		{
			// Bloat out the pins so they are spread over more pin pages
			// (But not optimized out to SSV pages)
			static const int largePropLen=127;
			char blah[largePropLen];
			memset(blah,'z',largePropLen-1); blah[largePropLen-1]=0;

			size_t k ;
			for ( k = 0 ; k < CNT_EXTRA_PROPS ; k++ )
			{
				Value v; v.set(blah); v.property=mExtraProps[k];
				TVERIFYRC(mSession->modifyPIN(pid,&v,1));
			}
		}
	}
}

void TestMultiVarJoin::doTest()
{
	// Normal, systematic test that confirms all expected results

	//
	// Prepare pins
	//
	createMeta();
	createPINs(CNT_PINS);

	//Start with no ft and no orderby
	compareQueries(false,-1);

	// Repeat with FT
	compareQueries(true,-1);

	// Orderby different properties (all should make use of one of the available indexes)
	compareQueries(true,0);
	compareQueries(true,1);
	compareQueries(true,3);
}

void TestMultiVarJoin::doPerfTest()
{
	// Performance variation - many pins with multiple passed of a single query
	// Only counting is performed - no pin iteration or correctness validation
	//

	// Prepare pins
	//
	createMeta();

	size_t i ;
	for ( i = 0 ; i < CNT_EXTRA_PROPS ; i++ )
	{
		mExtraProps[i]=MVTUtil::getPropRand(mSession,"testmultivarjoin.extra.");
	}

	createPINs(CNT_PINS_PERF,true /*large pins*/);

	TimedEvent te(mLogger.out()); // This object accumulates as timing results

	// It takes rather long to perform all the queries, 
	// so we list the ones that we want here
	QueryVariation perfQueries[] = { 
		QV_FullScan, 
		QV_1Family, 
		QV_ExplicitJoin, 
		QV_Implicit, 
		QV_ALL } ; // End token

	for ( int i = 0 ; perfQueries[i] != QV_ALL ; i++ )
	{
		// Results are sorted by description and it seems better to group
		// results in a certain ordering, hence the magic prefix
		char sortingprefix[5]; strcpy(sortingprefix,"a1- "); sortingprefix[1]='1'+i; 

		QueryVariation queryType=perfQueries[i];
		string msg(sortingprefix); msg.append(sQDesc[queryType]);
		doPerfPass(te,msg.c_str(),false/*ft*/,false/*orderby*/,queryType);

		// Other variations (e.g. with FT, orderby)

		sortingprefix[0]='b'; msg=sortingprefix; 
		msg.append(sQDesc[queryType]); msg.append(" with FT");
		doPerfPass(te,msg.c_str(),true/*ft*/,false/*orderby*/,queryType);

		// Disabled because orderby not really measured
		sortingprefix[0]='c'; msg=sortingprefix; 
		msg.append(sQDesc[queryType]);msg.append(" with Order");
		doPerfPass(te,msg.c_str(),false/*ft*/,true/*orderby*/,queryType);

		sortingprefix[0]='d'; msg=sortingprefix; 
		msg.append(sQDesc[queryType]);msg.append(" with FT+Order");
		doPerfPass(te,msg.c_str(),true/*ft*/,true/*orderby*/,queryType);
	}

	PrintExampleXPath();

	te.historyReport(true/*print individual times also*/);
}

void TestMultiVarJoin::doPerfPass(TimedEvent & te,const char * indesc, bool ft, bool orderby,int /*QueryVariation*/ whichQ)
{
	// Run a particular query against the data
	// Repeat 5 times to get average
	int i; 
	for ( i = 0 ; i < 5 ; i++ )
	{

		// Stop and start store so that pages are flushed
		// (But not included in the timing!)
		mSession->terminate() ;
		MVTApp::stopStore() ;
		TVERIFY(MVTApp::startStore()) ;
		mSession = MVTApp::startSession();
		TVERIFY( mSession != NULL ) ;

		// Only run the fast query, and only get a count
		// This should run entirely on index pages

		CmvautoPtr<IStmt> q(getQuery((QueryVariation)whichQ));

		te.start(indesc);

		if (ft)
			TVERIFYRC(q->addConditionFT(0 /*All queries have at least var0*/,"findme",0,&mFTProp,1));

		if ( orderby )
		{
			OrderSeg ord = {NULL,mProps[0],0,0,0};
			TVERIFYRC(q->setOrder( &ord,1)); // Doing prop 0, but any should work
		}

		uint64_t cnt=0;
#if 0
		// This ignores orderby
		TVERIFYRC(q->count(cnt));
		TVERIFY(cnt>0);
#else
		// Have to pull the sorted pin ids rather than just getting a result
		// By getting pin ids we are not forcing the pin data to be loaded
		ICursor* lC = NULL;
		TVERIFYRC(q->execute(&lC));
		CmvautoPtr<ICursor> r(lC);
		PID pid;
		while( RC_OK==r->next(pid))
		{
			cnt++;
		}

#endif
		te.end();
		mLogger.out() << cnt << " pins" << endl;
	}
}

void TestMultiVarJoin::compareQueries(bool addFT, int orderByProp)
{
	//
	// Run all the queries and compare to ensure they return teh same results.
	//
	// FT and orderby conditions are optionally added to each raw query
	//

	mLogger.out() << "-------------Running queries-------------" << (addFT ? "With FT" : "") <<" ";
	if ( orderByProp>-1) mLogger.out() << "Sort by prop " << mProps[orderByProp] ;
	mLogger.out() << endl;

	static const int cntQueries=QV_ALL;
	IStmt * q[cntQueries];

	int i ;
	for ( i = 0 ; i < cntQueries ; i++ )
	{
		q[i]=getQuery((QueryVariation)i);
	}

	if ( addFT )
	{
		for ( i = 0 ; i < cntQueries ; i++ )
		{
			TVERIFYRC(q[i]->addConditionFT(0 /*All queries have at least var0*/,"findme",0,&mFTProp,1));

			// REVIEW: in this test scenario, even for multi-var query the FT result should
			// be the same no matter what variable it is assigned to (e.g. as an additional filtering
			// of the same set of pins)
		}		
	}

	if ( orderByProp > -1 )
	{
		// When doing implicit join scenario we can efficiently orderby any of the properties
		OrderSeg ord={NULL,mProps[orderByProp],0,0,0};
		for ( i = 0 ; i < cntQueries ; i++ )
		{
			TVERIFYRC(q[i]->setOrder(&ord,1));		// var ???
		}		
	}

	uint64_t cnts[cntQueries];
	for ( i = 0 ; i < cntQueries ; i++ )
	{
		TVERIFYRC(q[i]->count(cnts[i]));
		mLogger.out() << "Query \"" << sQDesc[i] << "\" returned " << cnts[i] << " pins" << endl;
		if ( i>0 ) TVERIFY( cnts[i]==cnts[0] ); // All results should match full scan
	}

	//
	// Actually compare the pins as well
	//
	ClassHelper::TPidSet pinSets[cntQueries];
	for ( i = 0 ; i < cntQueries ; i++ )
	{
		ClassHelper::populateSet(q[i],pinSets[i],mLogger.out(),CNT_PINS*2);

		// All queries should returned the same pins
		if ( i>0 ) 
		{
			TVERIFY2( pinSets[i].size()==pinSets[0].size(), "Query returned different number of pins" ); 
			TVERIFY2( pinSets[i]==pinSets[0], "Query returned different pins" ); 
		}

		// Actually check that the pin value matches the conditions that all queries test
		ClassHelper::TPidSet::iterator it=pinSets[i].begin();
		for ( ; it != pinSets[i].end() ; it++ )
		{
			PID pid = *it;
			CmvautoPtr<IPIN> pin(mSession->getPIN(pid));

			for ( int k = 0 ; k < CNT_SUB_QUERY ; k++ )
			{
				if ( pin->getValue(mProps[k])->i <= mRandLookups[k] )
				{
					TVERIFY(!"Pin doesn't belong in query") ;
					mLogger.out() << "PIN: " << hex << pid.pid << " Bad Property Index: " << dec <<  k 
						<< " (Value " << pin->getValue(mProps[k])->i << " is not > then " << mRandLookups[k] 
						<< endl;
					break;
				}
			}
			if ( addFT ) 
				TVERIFY(strcmp("findme",pin->getValue(mFTProp)->str)==0);
		}
	}

	for ( i = 0 ; i < cntQueries ; i++ )
	{
		q[i]->destroy();
	}
}

IStmt * TestMultiVarJoin::getQuery(QueryVariation whichQ)
{
	switch(whichQ)
	{
	case(QV_ExplicitJoin): return doExplicitJoin();
	case(QV_Implicit): return doImplicitJoin();
	case(QV_FullScan): return doFullScan();
	case(QV_1Family): return do1FamilyAndExpressions();
	case(QV_ImplicitWithClass): return doImplicitJoinWithClass(false);
	case(QV_ImplicitWithClassFirst): return doImplicitJoinWithClass(true);
	case(QV_ImplicitClassFamExpr): return doImplicitClassFamExpr(true);
	case(QV_ImplicitFamClassExpr): return doImplicitClassFamExpr(false);
	case(QV_ClassExpr): return doClassAndExpressions();
	default: assert(false);
	}
	return NULL;
}

IStmt * TestMultiVarJoin::doImplicitJoin()
{

	// Use new 13067 (m2.4) query syntax.  There is an implicit 
	// intersection ("AND") between each family item.
	//
	// This would be the RECOMMENDED way to combine the index queries

	IStmt * joinQ = mSession->createStmt();

	// Fill in SourceSpec structure for each family condition that the PIN should meet
	SourceSpec families[CNT_SUB_QUERY];
	Value paramsToFamilies[CNT_SUB_QUERY];

	size_t i ;	
	for ( i = 0 ; i < CNT_SUB_QUERY ; i++ )
	{		
		paramsToFamilies[i].set(mRandLookups[i]);  // Value to check OP_GT against
		paramsToFamilies[i].property=mProps[i];
		
		families[i].objectID=mFamilies[i];
		families[i].nParams=1;
		families[i].params=&paramsToFamilies[i];

	}

	// No explicit calls to addVariable are necessary
	unsigned char singleVarWithAllFamilies;
	singleVarWithAllFamilies=joinQ->addVariable(families,CNT_SUB_QUERY);

	return serializeQ(joinQ);
}

IStmt * TestMultiVarJoin::doImplicitJoinWithClass(bool bClassFirst)
{
	// As doImplicit but adding regular class as a condition
	IStmt * joinQ = mSession->createStmt();

	// Fill in SourceSpec structure for each family condition that the PIN should meet
	SourceSpec families[CNT_SUB_QUERY+1];
	Value paramsToFamilies[CNT_SUB_QUERY];

	size_t i ;
	size_t familypos=0; 
	size_t classpos=CNT_SUB_QUERY;

	if ( bClassFirst)
	{
		classpos=0;
		familypos=1;
	}

	families[classpos].objectID=mClass;
	families[classpos].nParams=0;
	families[classpos].params=NULL;

	for ( i = 0 ; i < CNT_SUB_QUERY ; i++,familypos++ )
	{		
		paramsToFamilies[i].set(mRandLookups[i]);  // Value to check OP_GT against
		paramsToFamilies[i].property=mProps[i];
		
		families[familypos].objectID=mFamilies[i];
		families[familypos].nParams=1;
		families[familypos].params=&paramsToFamilies[i];
	}

	// No explicit calls to addVariable are necessary
	unsigned char singleVarWithAllFamilies;
	singleVarWithAllFamilies=joinQ->addVariable(families,CNT_SUB_QUERY+1);

	return serializeQ(joinQ);
}

IStmt * TestMultiVarJoin::doExplicitJoin()
{
	// Use new m2.4 query syntax
	// 
	// Each variable represents a separate family lookup, and we intersect all the results together
	// 
	// ( mFamilies[0]($v0) QRY_INTERSECT mFamilies[1]($v1) ) QRY_INTERSECT
	// ( mFamilies[2]($v2) QRY_INTERSECT mFamilies[3]($v2) ) 

	unsigned char baseVars[CNT_SUB_QUERY];
	IStmt * joinQ = mSession->createStmt();

	size_t i ;	
	for ( i = 0 ; i < CNT_SUB_QUERY ; i++ )
	{		
		Value minVal;
		minVal.set(mRandLookups[i]); 
		minVal.property=mProps[i];
		
		SourceSpec cs;
		cs.objectID=mFamilies[i];
		cs.nParams=1;
		cs.params=&minVal;
		baseVars[i]=joinQ->addVariable(&cs,1);
	}
	
	// REVIEW: hardcoded for only 4 base queries, could be generalized to join arbitrary
	// number (up to maximum 255 variables!)
	joinQ->setOp(baseVars,4,QRY_INTERSECT);

	return serializeQ(joinQ);
}

IStmt * TestMultiVarJoin::doFullScan()
{
	// Full scan query + filtering using an expression tree
	// that matches the family expressions.

	// Compare with raw query (no joins)
	IStmt * rawQ=mSession->createStmt();

	CmvautoPtr<IExprNode> exprTreeRoot(getQueryAsExpression());
	
	// Use a full scan query
	unsigned char rawVar = rawQ->addVariable();
	rawQ->addCondition(rawVar,exprTreeRoot);
	return serializeQ(rawQ);
}

IStmt * TestMultiVarJoin::do1FamilyAndExpressions()
{
	// Use first family to get a subset of pins, then filter other pins according to 
	// the raw expression that defines intersection of all the family values
	//
	// For test simplicity we leave the first family also as part of the expression,
	// but only the other expressions are "needed" (e.g. that condition will 
	// always be true)

	IStmt * lQ=mSession->createStmt();

	CmvautoPtr<IExprNode> exprTreeRoot(getQueryAsExpression());

	Value minValFirstFam;
	minValFirstFam.set(mRandLookups[0]); 
	minValFirstFam.property=mProps[0];

	SourceSpec csFirstFamily;
	csFirstFamily.objectID=mFamilies[0];
	csFirstFamily.nParams=1;
	csFirstFamily.params=&minValFirstFam;

	unsigned char rawVar = lQ->addVariable(&csFirstFamily,1);

	TVERIFYRC(lQ->addCondition(rawVar,exprTreeRoot));
	return serializeQ(lQ);
}

IStmt * TestMultiVarJoin::doClassAndExpressions()
{
	// Like do1FamilyAndExpressions but the expression runs against the
	// class of all pins rather than a family.  E.g. the family indexes are not 
	// involved at all.  Like other expression based examples all pins must be loaded

	IStmt * lQ=mSession->createStmt();

	CmvautoPtr<IExprNode> exprTreeRoot(getQueryAsExpression());

	SourceSpec csClass;
	csClass.objectID=mClass;
	csClass.nParams=0;
	csClass.params=NULL;

	unsigned char v = lQ->addVariable(&csClass,1);

	TVERIFYRC(lQ->addCondition(v,exprTreeRoot));
	return serializeQ(lQ);
}

IStmt * TestMultiVarJoin::doImplicitClassFamExpr(bool bClassFirst)
{
	// Like do1FamilyAndExpressions but combine a family and a class
	// The class includes all pins so it should have no impact on the results.
	// The family does a partial filtering, but the expression is needed to 
	// replace the other CNT_SUB_QUERY-1 families
	IStmt * lQ=mSession->createStmt();

	CmvautoPtr<IExprNode> exprTreeRoot(getQueryAsExpression());

	Value minValFirstFam;
	minValFirstFam.set(mRandLookups[0]); 
	minValFirstFam.property=mProps[0];

	SourceSpec cs[2];

	int indexClass=bClassFirst?0:1;
	int indexFamily=bClassFirst?1:0;

	cs[indexFamily].objectID=mFamilies[0];
	cs[indexFamily].nParams=1;
	cs[indexFamily].params=&minValFirstFam;

	cs[indexClass].objectID=mClass;
	cs[indexClass].nParams=0;
	cs[indexClass].params=NULL;

	unsigned char rawVar = lQ->addVariable(cs,2);

	TVERIFYRC(lQ->addCondition(rawVar,exprTreeRoot));
	return serializeQ(lQ);
}

IExprNode * TestMultiVarJoin::getQueryAsExpression()
{
	// This expression tree makes no reference to families but evalutes to the same results
	// e.g.
	//  ( mProp[0]>mRandLookup[0] OP_LAND mProp[1]>mRandLookup[1] ) OP_LAND  
	//  ( mProp[2]>mRandLookup[2] OP_LAND mProp[3]>mRandLookup[3] ) 
	//
	// Performance note: Evaluation of such an expression implies that the PIN must be 
	// loaded and its values examined!

	IExprNode *baseExpr[CNT_SUB_QUERY];
	Value childPair[CNT_SUB_QUERY];

	size_t i;
	for ( i = 0 ; i < CNT_SUB_QUERY ; i++ )
	{
		Value qexpr[2];
		qexpr[0].setVarRef(0,mProps[i]);
		qexpr[1].set(mRandLookups[i]); // Lookup same value as we do in the join version
		baseExpr[i] = mSession->expr(OP_GT,2,qexpr);
		childPair[i].set(baseExpr[i]);
	}	

	IExprNode *parentExpr[2];
	parentExpr[0] = mSession->expr(OP_LAND,2,childPair);
	parentExpr[1] = mSession->expr(OP_LAND,2,childPair+2);

	Value parentPair[2];
	parentPair[0].set(parentExpr[0]);
	parentPair[1].set(parentExpr[1]);

	return mSession->expr(OP_LAND,2,parentPair);
}

IStmt * TestMultiVarJoin::serializeQ(IStmt *q)
{
	// Dump to string then recreate query
	// (to confirm that serialization code supports this query)
#ifdef SERIALIZE_Q
	char * queryStr=q->toString();

	if ( isVerbose() )
		mLogger.out() << "--------" << endl << queryStr << endl;

	IStmt * qNew = mSession->createStmt(queryStr);

	if ( qNew == NULL )
	{
		TVERIFY(!"Could not round trip query to string");
		return q;
	}

	mSession->free(queryStr);

	q->destroy();
	return qNew;
#else
	return q;
#endif
}

void TestMultiVarJoin::PrintExampleXPath()
{
	// For comparison in xpathcli or other tools, print xpath
	// that shows what families and values we were using
	// This is only one of the different variations, in theory 
	// we could print out xpath for all queries but some of them can't
	// be expressed yet and this mega query shows all the important info
	// for the implicit + ft
	stringstream xpath; 
	xpath<<"fn:eval-count(\"/pin[";
	size_t i ;	
	for ( i = 0 ; i < CNT_SUB_QUERY ; i++ )
	{		
		xpath<<"pin is "<<mFamilyNames[i]<<"("<<mRandLookups[i]<<")";

		// Unconditional because of the ft following
		xpath<< " and ";
	}

	xpath<<"pin ftcontains \'findme\'";
	xpath<<"]\")";

	mLogger.out() << xpath.str() << endl;
}
