/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#define NUMBERPINS 1000
#define lAllClassNotifs (CLASS_NOTIFY_JOIN | CLASS_NOTIFY_LEAVE | CLASS_NOTIFY_CHANGE | CLASS_NOTIFY_DELETE | CLASS_NOTIFY_NEW)
using namespace std;

// Test for IStmt::count
// Note: there is also a INav::count method, not covered in this test.

//#define FT_AS_CLASS_QUERY // You cannot store a FT query as a class query.  Mark confirms
							 // this is by design

// Publish this test.
class TestCount : public ITest
{
	public:
		TEST_DECLARE(TestCount);
		virtual char const * getName() const { return "testcount"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of the IStmt count method"; }
		virtual bool isPerformingFullScanQueries() const { return true; } // Note: countPinsFullScan...
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testcount(ISession *session);		
};
TEST_IMPLEMENT(TestCount, TestLogger::kDStdOut);

// Implement this test.
int TestCount::execute()
{
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		testcount(session);
		session->terminate();
		MVTApp::stopStore();
	}
	else
	{
		return RC_FALSE;
	}

	return RC_OK;
}

void TestCount::testcount(ISession *session)
{
	Value val[3];
	int i;
	IPIN *pin;
	Tstring str, strcl, strcl1;
	MVTRand::getString(str,15,0,false);
	MVTRand::getString(strcl,10,0,false);
	MVTRand::getString(strcl1,20,0,false);

	PropertyID pids[3];
	MVTApp::mapURIs(session,"TestCount.testcount",3,pids);

	Value args[3];
	IStmt *query;
	unsigned char var;
	IExprTree *expr;
	ICursor *result;
	mLogger.out() << "Creating " <<  NUMBERPINS << " PINs...";
	for (i=0; i < NUMBERPINS; i++)
	{
		// Create many pins, all with the same string properties
		if(i%50 == 0) mLogger.out() << ".";
		pin = session->createPIN();
		val[0].set(str.c_str());val[0].setPropID(pids[0]);val[0].meta = META_PROP_FTINDEX;

		// Collection with two strings
		val[1].set(strcl.c_str());val[1].setPropID(pids[1]);val[1].eid=STORE_LAST_ELEMENT;val[1].op=OP_ADD;val[1].meta = META_PROP_FTINDEX;
		val[2].set(strcl1.c_str());val[2].setPropID(pids[1]);val[2].eid=STORE_LAST_ELEMENT;val[2].op=OP_ADD;val[2].meta = META_PROP_FTINDEX;

		RC rc = pin->modify(val,3);
		rc = session->commitPINs(&pin,1);
		pin->destroy();
		pin = NULL;
	}
	mLogger.out() << std::endl;

	mLogger.out() << "Running FullScan/FT/Class Queries to test IStmt->count()..." << std::endl;
	
	// First Query - create a full scan query looking for "str"
	query = session->createStmt();
	var = query->addVariable();
	args[0].setVarRef(0,pids[0]);
	args[1].set(str.c_str());
	expr = session->expr(OP_EQ,2,args);
	query->addCondition(var,expr);
	TVERIFYRC(query->execute(&result));

	// Confirm whether the result returned by IStmt::count
	int rescnt = MVTApp::countPinsFullScan(result,session);
	uint64_t rescnt1;
	query->count(rescnt1);

	TVERIFY(rescnt == (int)rescnt1) ;
	TVERIFY2(NUMBERPINS == rescnt1, "Q1 failure" ) ; // REVIEW Failure on linux (0 items found)

	// TEST CASE FOR ABORT in IStmt::count()
	int lRandAbort = MVTRand::getRange((int)rescnt/2, (int)rescnt);
	uint64_t lCount = 0;
	TVERIFY(RC_TIMEOUT == query->count(lCount, 0, 0, lRandAbort) && lCount == ~0ULL);
		
	query->destroy();
	result->destroy();
	expr->destroy();

	// Second query - look for the collection item strcl1
	query = session->createStmt();
	var = query->addVariable();
	query->setConditionFT(var,strcl1.c_str());

	TVERIFYRC(query->execute(&result));

	rescnt = MVTApp::countPinsFullScan(result,session);
	query->count(rescnt1);
	
	TVERIFY(rescnt == (int)rescnt1);
	TVERIFY2(rescnt1 == NUMBERPINS, "Q2 failure" ) ; 

	query->destroy();
	result->destroy();

	//Third Query - class query for "str" (based on the first query)
	ClassID cls = STORE_INVALID_CLASSID;
	IStmt *clsquery = session->createStmt();
	var = clsquery->addVariable();
	
	args[0].setVarRef(0,pids[0]);
	args[1].set(str.c_str());
	IExprTree *clsexpr = session->expr(OP_EQ,2,args);	
	clsquery->addCondition(var,clsexpr);
	defineClass(session,"counttest",clsquery,&cls);
	session->enableClassNotifications(cls,lAllClassNotifs); 
	clsexpr->destroy(); clsexpr = NULL ;
	clsquery->destroy(); clsquery = NULL ;

	//query
	SourceSpec cs;
	cs.objectID = cls;
	cs.nParams=0;
	cs.params=NULL;

	query = session->createStmt();
	var = query->addVariable(&cs,1);
	TVERIFYRC(query->execute(&result));

	rescnt = MVTApp::countPinsFullScan(result,session);
	query->count(rescnt1);

	TVERIFY(rescnt == (int)rescnt1);
	TVERIFY2(rescnt1 == NUMBERPINS, "Q3 failure" ) ; 

	query->destroy(); query=NULL ;
	result->destroy(); result=NULL ;

	// Fourth query - look for the collection item strcl with class query and FT search
	cls = STORE_INVALID_CLASSID ;

	clsquery = session->createStmt();
	var = clsquery->addVariable();
	TVERIFYRC(clsquery->setConditionFT(var,strcl.c_str(),0,&(pids[1]),1));

	//REVIEW: clone works ok in normal circumstance, but not for class
	IStmt * q2 = clsquery->clone() ; TVERIFY(q2!=NULL) ; q2->destroy() ;

#ifdef FT_AS_CLASS_QUERY 
	// If we were to support FT class queries

	//REVIEW: clone call fails (if (fClass && (vars==NULL||nVars!=1||vars->condFT!=NULL||vars->nCondProps==0&&vars->nCondIdx==0&&vars->nConditions==0)) )
	TVERIFYRC(defineClass(session,"counttest2",clsquery,&cls));

	SourceSpec cs2;
	cs2.objectID = cls;
	cs2.nParams=0;
	cs2.params=NULL;

	query = session->createStmt();
	var = query->addVariable(&cs2,1);
	result = query->execute();

	rescnt = MVTApp::countPinsFullScan(result,session);
	query->count(rescnt1);
	
	TVERIFY(rescnt == rescnt1);
	TVERIFY2(rescnt1 == NUMBERPINS, "Q4 failure" ) ; 

	query->destroy(); query=NULL ;
	result->destroy(); result=NULL ;
#else
	// Can't turn this query into a class query
// can now?	TVERIFY(RC_OK != defineClass(session,"counttest2",clsquery));
#endif
	clsquery->destroy();

	//TODO: Multiple threads querying and checking for count.
}
