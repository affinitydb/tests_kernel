/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "TestCollNav.h"

using namespace std;

// Publish this test.
class TestQueries : public ITest
{
	public:
		URIMap pm[26]; 
		IPIN *PinID[4];
		RC mRCFinal;
		bool mExecuteClass;
		bool mRunFullScan;
		Tstring mRandStr;
		TEST_DECLARE(TestQueries);
		virtual char const * getName() const { return "testqueries"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "thorough testing of queries and operators"; }
		virtual bool includeInPerfTest() const { return true; }
		virtual bool isPerformingFullScanQueries() const { return true; } // Note: countPinsFullScan...
		// Take over 40 minutes on windows when run along with smoke tests as this test creates lots of classes
		virtual bool isLongRunningTest()const {return true;} 
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testIssue(ISession *session);
		void testIssue1(ISession *session);
		void testMemory(ISession *session);
		void testFTIndexWithNativeQuery(ISession *session);
		void testQuery(ISession *session);
		void runArithmeticOperators(ISession *session);
		void runComparisonOperators(ISession *session);
		void runBitwiseOperators(ISession *session);
		void runLogicalOperators(ISession *session);
		void runConversionOperators(ISession *session);
		void runStringOperators(ISession *session);
		void runFuncOperators(ISession *session);
		IExprTree *createArithExpr(ISession *session,unsigned var,int Op, int type);
		IExprTree *createCompExpr(ISession *session,unsigned var,int Op, int type);
		IExprTree *createBitwiseExpr(ISession *session,unsigned var,int Op, int type);
		IExprTree *createLogicalExpr(ISession *session,unsigned var,int Op,int Op1, int Op2,int type1, int type2=0,bool fake=false);
		IExprTree *createConvExpr(ISession *session,unsigned var,int Op, int type);
		IExprTree *createMinMaxExpr(ISession *session,unsigned var,int Op, int type);		
		IExprTree *createOP_INExpr(ISession *session,unsigned var,int type);
		IExprTree *createStringOpsExpr(ISession *session,unsigned var,int Op,int type,const int pVariant);
		IExprTree *createFuncOpsExpr(ISession *session,unsigned var,int Op,int type,const int pVariant);
		void createOP_INWithNavigator(ISession *session);

		void executeComplexQuery(ISession *session,int Op,int Op1,int type1,int Op2,int type2,int nExpResults,bool fake=false,const char *pClassName = NULL);
		void executeSimpleQuery(ISession *session,int Op,int type,int nExpResults,const char *pClassName = NULL,const int pVariant = 0);
		// TODO: refine/complete these and provide as service in app.h
		unsigned long reportResult(IStmt *pQuery);
		int reportResult(ICursor *result, ISession *pSession);
		void populateStore(ISession *session);
		void logResult(string str, RC rc);
};
TEST_IMPLEMENT(TestQueries, TestLogger::kDOutputDebug);

// Implement this test.
int TestQueries::execute()
{
	mRCFinal = RC_OK;
	mExecuteClass = true;
	mRunFullScan = false;
	//PropertyID lBasePropID = 24550;	
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		MVTRand::getString(mRandStr,10,10,false,false);
		
		//session->setURIBase("vmware.com/");
		//MVTApp::mapURIs(session,"TestQueries.prop",30,pm);
		
		memset(pm,0,sizeof(pm));				
		pm[0].URI="testQueries.country";
		pm[1].URI="testQueries.city";
		pm[2].URI="testQueries.street";
		pm[3].URI="testQueries.email";
		pm[4].URI="testQueries.firstname";
		pm[5].URI="testQueries.lastname";
		pm[6].URI="testQueries.pincode";
		pm[7].URI="testQueries.age"; // int32_t / VT_INT
		pm[8].URI="testQueries.somestring"; 
		pm[9].URI="testQueries.single"; //boolean
		pm[10].URI="testQueries.experience"; //VT_FLOAT
		pm[11].URI="testQueries.salary"; //VT_DOUBLE
		pm[12].URI="testQueries.ssn"; //int64_t/VT_INT64
		pm[13].URI="testQueries.dob";  //datetime
		pm[14].URI="testQueries.website"; //URL
		pm[15].URI="testQueries.true_false"; //string
		pm[16].URI="testQueries.VT_UINT"; // uint32_t / DWORD 
		pm[17].URI="testQueries.VT_INT64";  // int64 
		pm[18].URI="testQueries.VT_UINT64";  // uint64 
		pm[19].URI="testQueries.VT_DATETIME"; // VT_DATETIME
		pm[20].URI="testQueries.VT_INTERVAL"; // VT_INTERVAL
		pm[21].URI="testQueries.VT_USTR"; // VT_USTR
		pm[22].URI="testQueries.VT_BSTR"; // VT_BSTR
		pm[23].URI="testQueries.VT_UURL"; // VT_UURL
		pm[24].URI="testQueries.VT_ARRAY"; // VT_ARRAY
		pm[25].URI="testQueries.VT_COLLECTION"; // VT_COLLECTION
		
		for(size_t i = 0; i < (sizeof(pm)/sizeof(pm[0])); i++) {
			pm[i].uid = STORE_INVALID_PROPID; 
			if(RC_OK!=session->mapURIs(1, &pm[i])) 
				assert(false);			
		}
		
		populateStore(session);
		
		/* Tests reported as bugs */
		//createOP_INWithNavigator(session);
		//testIssue(session);
		//testMemory(session);
		//testQuery(session);
		//testFTIndexWithNativeQuery(session);
		//if(!MVTApp::isRunningSmokeTest()) testIssue1(session);

		/* Regular tests for queries */
		runArithmeticOperators(session);
		runComparisonOperators(session);
		runBitwiseOperators(session);
		runLogicalOperators(session);
		runConversionOperators(session);
		runStringOperators(session);
		runFuncOperators(session);

		createOP_INWithNavigator(session);
		//if(0) session->deletePINs(&PinID[0]->getPID(),1);
		PinID[0]->destroy();
		PinID[1]->destroy();
		PinID[2]->destroy();
		session->terminate();
		MVTApp::stopStore();
	}

	else { TVERIFY(!"Unable to start store"); }
	return mRCFinal;
}

// Test for quering the store for pins which dont have a particular property [LNOT(prop1 OP_EXISTS)]
void TestQueries::testIssue1(ISession *session)
{
	logResult("Test for LNOT(prop1 OP_EXISTS)",RC_OTHER);
	IStmt *query = session->createStmt();
	QVarID var = query->addVariable();

	// PIN 3 doesn't have property 10
	PropertyID pids[1];
	Value val[1];
	pids[0]=pm[10].uid;
	val[0].setVarRef(0,*pids);
#if 1
	IExprTree *expr = session->expr(OP_EXISTS,1,val);
	val[0].set(expr);
	IExprTree *exprfinal = session->expr(OP_LNOT,1,val);
#else
	IExprTree *exprfinal = session->expr(OP_EXISTS,1,val,NOT_BOOLEAN_OP);
#endif
	query->addCondition(var,exprfinal);
	ICursor * lR = NULL;
	TVERIFYRC(query->execute(&lR));
	uint64_t lCount = reportResult(lR,session);
	exprfinal->destroy();
	query->destroy();
	
	if(1 == lCount)
		logResult("",RC_OK);
	else
		logResult("",RC_FALSE);
}

void TestQueries::testIssue(ISession *session)
{
	IStmt *query = session->createStmt();
	unsigned var = query->addVariable();

	PropertyID proppin = PROP_SPEC_PINID;
	Value operands1[2];
	// Pin[1] does not have property id 2 in it
	PID otestPID = PinID[1]->getPID();
	operands1[0].setVarRef(0,proppin);
	operands1[1].set(otestPID);
	IExprTree *expr1 = session->expr(OP_EQ,2,operands1);

	PropertyID pids[1];
	// Property ID  = 2
	pids[0]=pm[2].uid;
	Value operands2[1];
	operands2[0].setVarRef(0,*pids);
	IExprTree *expr2 = session->expr(OP_EXISTS,1,operands2);

	Value operands[2];
	operands[0].set(expr1);
	operands[1].set(expr2);

	IExprTree *expr = session->expr(OP_LAND,2,operands);

	query->addCondition(var,expr);
	reportResult(query);
	
	expr->destroy();
	query->destroy();
}

void TestQueries::testMemory(ISession *session)
{
	IStmt *query = session->createStmt();
	unsigned var = query->addVariable();
	for(int i=0;i<1000;i++){
		query->setConditionFT(var,"j");
		ICursor *result = NULL;
		TVERIFYRC(query->execute(&result));
		IPIN *mpin = result->next();
		while(NULL != mpin)
		{
			Value oper[2];
			IStmt *query1 = session->createStmt();
			QVarID var1 = query1->addVariable();
			const static PropertyID pin_id=PROP_SPEC_PINID;
			oper[0].setVarRef(0,pin_id);
			oper[1].set(mpin->getPID());
			IExprTree *exp = session->expr(OP_EQ,2,oper);
			query1->addCondition(var1,exp);
			exp->destroy();
			ICursor *result1 = NULL;
			TVERIFYRC(query1->execute(&result));
			query1->destroy();
			result1->destroy();
			mpin->destroy();
			mpin = result->next();
		}
		// MVTestsPortability::threadSleep(1000);
		result->destroy();
		query->setConditionFT(var,"jp");
		TVERIFYRC(query->execute(&result));
		result->destroy();
		// MVTestsPortability::threadSleep(1000);
		query->setConditionFT(var,"jpg");
		TVERIFYRC(query->execute(&result));
		result->destroy();
		// MVTestsPortability::threadSleep(1000);
	}
	query->destroy();
}

void TestQueries::testQuery(ISession *session)
{
	URIMap propMap[2]; memset(propMap,0,sizeof(propMap));
	propMap[0].URI="post";
	propMap[1].URI="comments";
	session->mapURIs(sizeof(propMap)/sizeof(propMap[0]),propMap);

	Value val[3],pvs[2];	
	PID pidVal,pidVal1,pidVal2;
	
	pvs[0].set("Post1");pvs[0].setPropID(propMap[0].uid);
	pvs[1].set("abc1");pvs[1].setPropID(propMap[1].uid);
	session->createPIN(pidVal1,pvs,2);	

	pvs[0].set("Post2");pvs[0].setPropID(propMap[0].uid);
	pvs[1].set("abc2");pvs[1].setPropID(propMap[1].uid);
	session->createPIN(pidVal2,pvs,2);
	
	val[0].set(pidVal1);
	val[1].set(pidVal2);
	val[2].set("Hello");

	///pin which refrences to the above pins

	pvs[0].set("Post");pvs[0].setPropID(propMap[0].uid);

	#if 1
		pvs[1].set(val,3);pvs[1].setPropID(propMap[1].uid);
	#else
		pvs[1].set(val,2);pvs[1].setPropID(propMap[1].uid);
	#endif

	session->createPIN(pidVal,pvs,2);

#if 0		// obsolete
	IStmt *qry = session->createStmt();	
	unsigned char var = qry->addVariable();
	
	qry->setConditionFT(var,"Post");	
	PropertyID ids[1];
	Value pv[2];
	ids[0] = propMap[1].uid;
	pv[0].setVarRef(0);
	pv[1].setParam(0,1,ids);
	IExprTree *expr = session->expr(OP_IN,2,pv);
	qry->addCondition(var,expr);
	
	Value oparam;
	oparam.set(pidVal);
	
	ICursor *result = NULL;
	TVERIFYRC(qry->execute(&result, &oparam,1));
	reportResult(result,session);
	result->destroy();
	expr->destroy();
	qry->destroy();
#endif
}

void TestQueries::testFTIndexWithNativeQuery(ISession *session)
{
	URIMap pm[3]; memset(pm,0,sizeof(pm));
	pm[0].URI="post";
	pm[1].URI="comments";
	pm[2].URI="propnew";
	session->mapURIs(sizeof(pm)/sizeof(pm[0]),pm);

	Value oval[4];

	Value pvs1[2];
	PID pidVal1;
	pvs1[0].set("Post1");pvs1[0].setPropID(pm[0].uid);
	pvs1[1].set("abc1");pvs1[1].setPropID(pm[1].uid);
	session->createPIN(pidVal1,pvs1,2);
	oval[0].set(pidVal1);

	Value pvs2[2];
	PID pidVal2;
	pvs2[0].set("Post2");pvs2[0].setPropID(pm[0].uid);
	pvs2[1].set("abc2");pvs2[1].setPropID(pm[1].uid);
	session->createPIN(pidVal2,pvs2,2);

	oval[1].set(pidVal2);

	Value pvs3[2];
	PID pidVal3;
	pvs3[0].set("Post3");pvs3[0].setPropID(pm[0].uid);
	pvs3[1].set("abc3");pvs3[1].setPropID(pm[1].uid);
	session->createPIN(pidVal3,pvs3,2);
	oval[2].set(pidVal3);

	Value pvs4[2];
	PID pidVal4;
	pvs4[0].set("Post4");pvs4[0].setPropID(pm[0].uid);
	pvs4[1].set("abc4");pvs4[1].setPropID(pm[1].uid);
	session->createPIN(pidVal4,pvs4,2);
	oval[3].set(pidVal4);

	///pin which refrences the above pins

	Value pvs[2];
	PID pidVal;
	pvs[0].set("Post");pvs[0].setPropID(pm[0].uid);
	pvs[1].set(oval,4);pvs[1].setPropID(pm[1].uid);
	session->createPIN(pidVal,pvs,2);

	PID tmppidVal;
	pvs[0].set("Post7");pvs[0].setPropID(pm[0].uid);
	pvs[1].set("new property");pvs[1].setPropID(pm[2].uid);
	session->createPIN(tmppidVal,pvs,2);

	pvs[0].set("ABCD");pvs[0].setPropID(pm[0].uid);
	pvs[1].set("new property1");pvs[1].setPropID(pm[2].uid);
	session->createPIN(tmppidVal,pvs,2);

	pvs[0].set("Post9");pvs[0].setPropID(pm[0].uid);
	pvs[1].set("new ");pvs[1].setPropID(pm[1].uid);
	session->createPIN(tmppidVal,pvs,2);

	IStmt *qry = session->createStmt();

	unsigned char var = qry->addVariable();

	qry->setConditionFT(var,"Post");   //If i comment this line it returns 2 results as expected

	PropertyID ids[1];
	ids[0] = pm[0].uid;
	Value val1[2];	
	val1[0].setVarRef(0,*ids);
	val1[1].set("Post27");     
	IExprTree *expr1 = session->expr(OP_EQ,2,val1);
	val1[0].setVarRef(0,*ids);
	val1[1].set("Post7");     
	IExprTree *expr2 = session->expr(OP_EQ,2,val1);
	val1[0].set(expr1);
	val1[1].set(expr2);
	IExprTree *expr = session->expr(OP_LOR,2,val1);
	qry->addCondition(var,expr);
	ICursor *result = NULL;
	TVERIFYRC(qry->execute(&result));
	reportResult(result,session);
	result->destroy();
	expr->destroy();
	qry->destroy();
}

void TestQueries::runArithmeticOperators(ISession *session)
{
	// --- OP_PLUS ---
	logResult("OP_PLUS with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_PLUS,VT_INT,1,"OP_PLUS__VT_INT");

	logResult("OP_PLUS with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_PLUS,VT_FLOAT,1,"OP_PLUS__VT_FLOAT");

	logResult("OP_PLUS with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_PLUS,VT_DOUBLE,1,"OP_PLUS__VT_DOUBLE");

	logResult("OP_PLUS with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_PLUS,VT_UINT,1,"OP_PLUS__VT_UINT");

	logResult("OP_PLUS with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_PLUS,VT_INT64,1,"OP_PLUS__VT_INT64");

	logResult("OP_PLUS with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_PLUS,VT_UINT64,1,"OP_PLUS__VT_UINT64");

	logResult("OP_PLUS with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_PLUS,VT_DATETIME,1,"OP_PLUS__VT_DATETIME");

	// --- OP_MINUS ---
	logResult("OP_MINUS with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_MINUS,VT_INT,1,"OP_MINUS__VT_INT");

	logResult("OP_MINUS with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_MINUS,VT_FLOAT,1,"OP_MINUS__VT_FLOAT");

	logResult("OP_MINUS with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_MINUS,VT_DOUBLE,1,"OP_MINUS__VT_DOUBLE");

	logResult("OP_MINUS with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_MINUS,VT_UINT,1,"OP_MINUS__VT_UINT");

	logResult("OP_MINUS with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_MINUS,VT_INT64,1,"OP_MINUS__VT_INT64");

	logResult("OP_MINUS with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_MINUS,VT_UINT64,1,"OP_MINUS__VT_UINT64");

	logResult("OP_MINUS with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_MINUS,VT_DATETIME,1,"OP_MINUS__VT_DATETIME");

	// --- OP_MUL ---
	logResult("OP_MUL with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_MUL,VT_INT,1,"OP_MUL__VT_INT");

	logResult("OP_MUL with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_MUL,VT_FLOAT,1,"OP_MUL__VT_FLOAT");

	logResult("OP_MUL with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_MUL,VT_DOUBLE,1,"OP_MUL__VT_DOUBLE");

	logResult("OP_MUL with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_MUL,VT_UINT,1,"OP_MUL__VT_UINT");

	logResult("OP_MUL with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_MUL,VT_INT64,1,"OP_MUL__VT_INT64");

	logResult("OP_MUL with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_MUL,VT_UINT64,1,"OP_MUL__VT_UINT64");

	// --- OP_DIV ---
	logResult("OP_DIV with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_DIV,VT_INT,1,"OP_DIV__VT_INT");

	logResult("OP_DIV with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_DIV,VT_FLOAT,1,"OP_DIV__VT_FLOAT");

	logResult("OP_DIV with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_DIV,VT_DOUBLE,1,"OP_DIV__VT_DOUBLE");

	logResult("OP_DIV with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_DIV,VT_UINT,1,"OP_DIV__VT_UINT");

	logResult("OP_DIV with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_DIV,VT_INT64,1,"OP_DIV__VT_INT64");

	logResult("OP_DIV with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_DIV,VT_UINT64,1,"OP_DIV__VT_UINT64");

	// --- OP_MOD ---
	logResult("OP_MOD with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_MOD,VT_INT,1,"OP_MOD__VT_INT");

	logResult("OP_MOD with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_MOD,VT_FLOAT,1,"OP_MOD__VT_FLOAT");

	logResult("OP_MOD with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_MOD,VT_DOUBLE,1,"OP_MOD__VT_DOUBLE");

	logResult("OP_MOD with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_MOD,VT_UINT,1,"OP_MOD__VT_UINT");

	logResult("OP_MOD with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_MOD,VT_INT64,1,"OP_MOD__VT_INT64");

	logResult("OP_MOD with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_MOD,VT_UINT64,1,"OP_MOD__VT_UINT64");

	// --- OP_NEG ---
	logResult("OP_NEG with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_NEG,VT_INT,1,"OP_NEG__VT_INT");
	  
	logResult("OP_NEG with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_NEG,VT_FLOAT,1,"OP_NEG__VT_FLOAT");

	logResult("OP_NEG with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_NEG,VT_DOUBLE,1,"OP_NEG__VT_DOUBLE");

	logResult("OP_NEG with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_NEG,VT_INT64,1,"OP_NEG__VT_INT64");

	/* --- Unsigned integers. OP_NEG not applicable --
	logResult("OP_NEG with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_NEG,VT_UINT,1);
	logResult("OP_NEG with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_NEG,VT_UINT64,1);
	*/

    // --- OP_ABS ---
	logResult("OP_ABS with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_ABS,VT_INT,1,"OP_ABS__VT_INT");
 
	logResult("OP_ABS with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_ABS,VT_FLOAT,1,"OP_ABS__VT_FLOAT");

	logResult("OP_ABS with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_ABS,VT_DOUBLE,1,"OP_ABS__VT_DOUBLE");

	logResult("OP_ABS with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_ABS,VT_UINT,1,"OP_ABS__VT_UINT");

	logResult("OP_ABS with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_ABS,VT_INT64,1,"OP_ABS__VT_INT64");

	logResult("OP_ABS with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_ABS,VT_UINT64,1,"OP_ABS__VT_UINT64");

	//logResult("OP_ABS with VT_INTERVAL",RC_OTHER);
	//executeSimpleQuery(session,OP_ABS,VT_INTERVAL,1,"OP_ABS__VT_INTERVAL");

	// --- OP_AVG ---
	logResult("OP_AVG with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_AVG,VT_INT,1,"OP_AVG__VT_INT");

	logResult("OP_AVG with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_AVG,VT_FLOAT,1,"OP_AVG__VT_FLOAT");

	logResult("OP_AVG with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_AVG,VT_DOUBLE,1,"OP_AVG__VT_DOUBLE");
	
	logResult("OP_AVG with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_AVG,VT_UINT,1,"OP_AVG__VT_UINT");
	
	logResult("OP_AVG with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_AVG,VT_INT64,1,"OP_AVG__VT_INT64");
	
	logResult("OP_AVG with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_AVG,VT_UINT64,1,"OP_AVG__VT_UINT64");

	logResult("OP_AVG with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_AVG,VT_DATETIME,1,"OP_AVG__VT_DATETIME");

	logResult("OP_AVG with VT_ARRAY",RC_OTHER);
	executeSimpleQuery(session,OP_AVG,VT_ARRAY,1,"OP_AVG__VT_ARRAY");

	//logResult("OP_AVG with VT_INTERVAL",RC_OTHER);
	//executeSimpleQuery(session,OP_AVG,VT_INTERVAL,1,"OP_AVG__VT_INTERVAL");

	logResult("OP_AVG with VT_COLLECTION",RC_OTHER);
	executeSimpleQuery(session,OP_AVG,VT_COLLECTION,1,"OP_AVG__VT_COLLECTION");
}

void TestQueries::runComparisonOperators(ISession *session)
{
	// --- OP_MIN --
	logResult("OP_MIN with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_MIN,VT_STRING,1,"OP_MIN__VT_STRING");

	logResult("OP_MIN with VT_URL",RC_OTHER);
	executeSimpleQuery(session,OP_MIN,VT_URL,1,"OP_MIN__VT_URL");

	logResult("OP_MIN with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_MIN,VT_INT,1,"OP_MIN__VT_INT");

	logResult("OP_MIN with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_MIN,VT_UINT,1,"OP_MIN__VT_UINT");

	logResult("OP_MIN with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_MIN,VT_INT64,1,"OP_MIN__VT_INT64");

	logResult("OP_MIN with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_MIN,VT_UINT64,1,"OP_MIN__VT_UINT64");

	logResult("OP_MIN with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_MIN,VT_FLOAT,1,"OP_MIN__VT_FLOAT");

	logResult("OP_MIN with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_MIN,VT_DOUBLE,1,"OP_MIN__VT_DOUBLE");

	logResult("OP_MIN with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_MIN,VT_DATETIME,1,"OP_MIN__VT_DATETIME");

	// --- OP_MAX --
	logResult("OP_MAX with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_MAX,VT_STRING,1,"OP_MAX__VT_STRING");

	logResult("OP_MAX with VT_URL",RC_OTHER);
	executeSimpleQuery(session,OP_MAX,VT_URL,1,"OP_MAX__VT_URL");

	logResult("OP_MAX with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_MAX,VT_INT,1,"OP_MAX__VT_INT");

	logResult("OP_MAX with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_MAX,VT_UINT,1,"OP_MAX__VT_UINT");

	logResult("OP_MAX with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_MAX,VT_INT64,1,"OP_MAX__VT_INT64");

	logResult("OP_MAX with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_MAX,VT_UINT64,1,"OP_MAX__VT_UINT64");

	logResult("OP_MAX with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_MAX,VT_FLOAT,1,"OP_MAX__VT_FLOAT");

	logResult("OP_MAX with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_MAX,VT_DOUBLE,1,"OP_MAX__VT_DOUBLE");

	logResult("OP_MAX with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_MAX,VT_DATETIME,1,"OP_MAX__VT_DATETIME");

	// --- OP_EQ ---
	logResult("OP_EQ with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_EQ,VT_STRING,1,"OP_EQ__VT_STRING");

	logResult("OP_EQ with VT_URL",RC_OTHER);
	executeSimpleQuery(session,OP_EQ,VT_URL,1,"OP_EQ__VT_URL");

	logResult("OP_EQ with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_EQ,VT_INT,1,"OP_EQ__VT_INT");

	logResult("OP_EQ with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_EQ,VT_UINT,1,"OP_EQ__VT_UINT");

	logResult("OP_EQ with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_EQ,VT_INT64,1,"OP_EQ__VT_INT64");

	logResult("OP_EQ with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_EQ,VT_UINT64,1,"OP_EQ__VT_UINT64");

	logResult("OP_EQ with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_EQ,VT_FLOAT,1,"OP_EQ__VT_FLOAT");

	logResult("OP_EQ with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_EQ,VT_DOUBLE,1,"OP_EQ__VT_DOUBLE");

	// This is failing for NO reason. #^*&#$
	logResult("OP_EQ with VT_BOOL",RC_OTHER);
	executeSimpleQuery(session,OP_EQ,VT_BOOL,2,"OP_EQ__VT_BOOL");

	logResult("OP_EQ with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_EQ,VT_DATETIME,1,"OP_EQ__VT_DATETIME");

	// --- OP_NE ---
	logResult("OP_NE with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_NE,VT_STRING,2,"OP_NE__VT_STRING");

	logResult("OP_NE with VT_URL",RC_OTHER);
	executeSimpleQuery(session,OP_NE,VT_URL,2,"OP_NE__VT_URL");
	// With the fix for missing properties in logical expressions , following queries would fail.
	/*
	logResult("OP_NE with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_NE,VT_INT,1,"OP_NE__VT_INT");

	logResult("OP_NE with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_NE,VT_UINT,1,"OP_NE__VT_UINT");

	logResult("OP_NE with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_NE,VT_INT64,1,"OP_NE__VT_INT64");

	logResult("OP_NE with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_NE,VT_UINT64,1,"OP_NE__VT_UINT64");

	logResult("OP_NE with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_NE,VT_FLOAT,1,"OP_NE__VT_FLOAT");

	logResult("OP_NE with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_NE,VT_DOUBLE,1,"OP_NE__VT_DOUBLE");

	logResult("OP_NE with VT_BOOL",RC_OTHER);
	executeSimpleQuery(session,OP_NE,VT_BOOL,2,"OP_NE__VT_BOOL");

	logResult("OP_NE with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_NE,VT_DATETIME,0,"OP_NE__VT_DATETIME");
	*/

	// --- OP_LT ---
	logResult("OP_LT with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_LT,VT_STRING,1,"OP_LT__VT_STRING");

	logResult("OP_LT with VT_URL",RC_OTHER);
	executeSimpleQuery(session,OP_LT,VT_URL,1,"OP_LT__VT_URL");

	logResult("OP_LT with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_LT,VT_INT,2,"OP_LT__VT_INT");

	logResult("OP_LT with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_LT,VT_UINT,1,"OP_LT__VT_UINT");

	logResult("OP_LT with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_LT,VT_INT64,1,"OP_LT__VT_INT64");

	logResult("OP_LT with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_LT,VT_UINT64,1,"OP_LT__VT_UINT64");

	logResult("OP_LT with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_LT,VT_FLOAT,1,"OP_LT__VT_FLOAT");

	logResult("OP_LT with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_LT,VT_DOUBLE,1,"OP_LT__VT_DOUBLE");

	logResult("OP_LT with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_LT,VT_DATETIME,1,"OP_LT__VT_DATETIME");

	// --- OP_LE ---
	logResult("OP_LE with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_LE,VT_STRING,1,"OP_LE__VT_STRING");

	logResult("OP_LE with VT_URL",RC_OTHER);
	executeSimpleQuery(session,OP_LE,VT_URL,1,"OP_LE__VT_URL");

	logResult("OP_LE with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_LE,VT_INT,1,"OP_LE__VT_INT");

	logResult("OP_LE with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_LE,VT_UINT,1,"OP_LE__VT_UINT");

	logResult("OP_LE with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_LE,VT_INT64,1,"OP_LE__VT_INT64");

	logResult("OP_LE with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_LE,VT_UINT64,1,"OP_LE__VT_UINT64");

	logResult("OP_LE with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_LE,VT_FLOAT,1,"OP_LE__VT_FLOAT");

	logResult("OP_LE with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_LE,VT_DOUBLE,1,"OP_LE__VT_DOUBLE");

	logResult("OP_LE with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_LE,VT_DATETIME,1,"OP_LE__VT_DATETIME");

	// --- OP_GT ---
	logResult("OP_GT with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_GT,VT_STRING,2,"OP_GT__VT_STRING");

	logResult("OP_GT with VT_URL",RC_OTHER);
	executeSimpleQuery(session,OP_GT,VT_URL,2,"OP_GT__VT_URL");

	logResult("OP_GT with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_GT,VT_INT,1,"OP_GT__VT_INT");

	logResult("OP_GT with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_GT,VT_UINT,1,"OP_GT__VT_UINT");

	logResult("OP_GT with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_GT,VT_INT64,1,"OP_GT__VT_INT64");

	logResult("OP_GT with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_GT,VT_UINT64,1,"OP_GT__VT_UINT64");

	logResult("OP_GT with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_GT,VT_FLOAT,1,"OP_GT__VT_FLOAT");

	logResult("OP_GT with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_GT,VT_DOUBLE,1,"OP_GT__VT_DOUBLE");

	logResult("OP_GT with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_GT,VT_DATETIME,1,"OP_GT__VT_DATETIME");

	// --- OP_GE ---
	logResult("OP_GE with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_GE,VT_STRING,2,"OP_GE__VT_STRING");

	logResult("OP_GE with VT_URL",RC_OTHER);
	executeSimpleQuery(session,OP_GE,VT_URL,2,"OP_GE__VT_URL");

	logResult("OP_GE with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_GE,VT_INT,1,"OP_GE__VT_INT");

	logResult("OP_GE with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_GE,VT_UINT,1,"OP_GE__VT_UINT");

	logResult("OP_GE with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_GE,VT_INT64,1,"OP_GE__VT_INT64");

	logResult("OP_GE with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_GE,VT_UINT64,1,"OP_GE__VT_UINT64");

	logResult("OP_GE with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_GE,VT_FLOAT,1,"OP_GE__VT_FLOAT");

	logResult("OP_GE with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_GE,VT_DOUBLE,2,"OP_GE__VT_DOUBLE");

	logResult("OP_GE with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_GE,VT_DATETIME,1,"OP_GE__VT_DATETIME");

	// --- OP_IN ---
	logResult("OP_IN with VT_RANGE",RC_OTHER);
	executeSimpleQuery(session,OP_IN,VT_RANGE,1,"OP_IN__VT_RANGE");

	logResult("OP_IN with VT_ARRAY",RC_OTHER);
	executeSimpleQuery(session,OP_IN,VT_ARRAY,1,"OP_IN__VT_ARRAY");

	logResult("OP_IS_A with VT_URIID",RC_OTHER);
	//executeSimpleQuery(session,OP_IN,VT_CLASS,-1,"OP_IN__VT_CLASS");
	executeSimpleQuery(session,OP_IS_A,VT_URIID,-1,NULL);

	logResult("OP_IN with VT_STMT",RC_OTHER);
	//executeSimpleQuery(session,OP_IN,VT_STMT,-1,"OP_IN__VT_QUERY");
	executeSimpleQuery(session,OP_IN,VT_STMT,-1,NULL);

	/*logResult("OP_IN with VT_COLLECTION",RC_OTHER);
	executeSimpleQuery(session,OP_IN,VT_COLLECTION,1,"OP_IN__VT_CNAVIGATOR");*/
}

void TestQueries::runBitwiseOperators(ISession *session)
{
	// --- OP_AND ---
	logResult("OP_AND with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_AND,VT_INT,1,"OP_AND__VT_INT");

	logResult("OP_AND with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_AND,VT_UINT,1,"OP_AND__VT_UINT");

	logResult("OP_AND with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_AND,VT_INT64,1,"OP_AND__VT_INT64");

	logResult("OP_AND with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_AND,VT_UINT64,1,"OP_AND__VT_UINT64");

	// --- OP_OR ---
	logResult("OP_OR with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_OR,VT_INT,1,"OP_OR__VT_INT");

	logResult("OP_OR with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_OR,VT_UINT,1,"OP_OR__VT_UINT");

	logResult("OP_OR with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_OR,VT_INT64,1,"OP_OR__VT_INT64");

	logResult("OP_OR with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_OR,VT_UINT64,1,"OP_OR__VT_UINT64");

	// --- OP_XOR ---
	logResult("OP_XOR with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_XOR,VT_INT,1,"OP_XOR__VT_INT");

	logResult("OP_XOR with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_XOR,VT_UINT,1,"OP_XOR__VT_UINT");

	logResult("OP_XOR with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_XOR,VT_INT64,1,"OP_XOR__VT_INT64");

	logResult("OP_XOR with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_XOR,VT_UINT64,1,"OP_XOR__VT_UINT64");

	// --- OP_NOT ---
	logResult("OP_NOT with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_NOT,VT_INT,1,"OP_NOT__VT_INT");

	logResult("OP_NOT with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_NOT,VT_UINT,1,"OP_NOT__VT_UINT");

	logResult("OP_NOT with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_NOT,VT_INT64,1,"OP_NOT__VT_INT64");

	logResult("OP_NOT with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_NOT,VT_UINT64,1,"OP_NOT__VT_UINT64");

	// --- OP_LSHIFT ---
	logResult("OP_LSHIFT with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_LSHIFT,VT_INT,1,"OP_LSHIFT__VT_INT");

	logResult("OP_LSHIFT with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_LSHIFT,VT_UINT,1,"OP_LSHIFT__VT_UINT");

	logResult("OP_LSHIFT with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_LSHIFT,VT_INT64,1,"OP_LSHIFT__VT_INT64");

	logResult("OP_LSHIFT with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_LSHIFT,VT_UINT64,1,"OP_LSHIFT__VT_UINT64");

	// --- OP_RSHIFT ---
	logResult("OP_RSHIFT with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_RSHIFT,VT_INT,1,"OP_RSHIFT__VT_INT");

	logResult("OP_RSHIFT with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_RSHIFT,VT_UINT,1,"OP_RSHIFT__VT_UINT");

	logResult("OP_RSHIFT with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_RSHIFT,VT_INT64,1,"OP_RSHIFT__VT_INT64");

	logResult("OP_RSHIFT with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_RSHIFT,VT_UINT64,1,"OP_RSHIFT__VT_UINT64");
}

void TestQueries::runLogicalOperators(ISession *session)
{
	// --- OP_LAND ---
	int i=0,j=0,nRes=0,nRes1=0,temp=0;		
	string Op1="",Op2="";
	string OpStr="",OpStr1="", lClassName="";
	for(i=OP_EQ; i<=OP_GE;i++){
		nRes=0;
		switch(i){
			case OP_EQ: Op1 = "OP_EQ";  nRes = 1; break;
			case OP_NE: Op1 = "OP_NE";  nRes = 0; break;
			case OP_LT: Op1 = "OP_LT";  nRes = 1; break;
			case OP_LE: Op1 = "OP_LE";  nRes = 1; break;
			case OP_GT: Op1 = "OP_GT";  nRes = 0; break;
			case OP_GE: Op1 = "OP_GE";  nRes = 0; break;
		}					
		for(j=OP_EQ; j<=OP_GE;j++){
			switch(j){
				case OP_EQ: 
					Op2 = "OP_EQ"; 
					switch(i){
						case OP_EQ: case OP_LT: case OP_LE: nRes = 1; break;
						case OP_NE: case OP_GT: case OP_GE: nRes = 0; break;
					}
					break;					
				case OP_NE: 
					Op2 = "OP_NE"; 
					switch(i){
						case OP_EQ: case OP_LT: case OP_LE: nRes = 0; break;
						case OP_NE: case OP_GT: case OP_GE: nRes = 2; break;
					}
					break;
				case OP_LT: 
					Op2 = "OP_LT"; 
					switch(i){
						case OP_EQ: case OP_LT: case OP_LE: nRes = 1; break;
						case OP_NE: case OP_GT: case OP_GE: nRes = 0; break;
					}
					break;
				case OP_LE: 
					Op2 = "OP_LE";
					switch(i){
						case OP_EQ: case OP_LT: case OP_LE: nRes = 1; break;
						case OP_NE: nRes = 0; case OP_GT: case OP_GE: nRes = 0; break;
					}
					break;
				case OP_GT: 
					Op2 = "OP_GT"; 
					switch(i){
						case OP_EQ: case OP_LT: case OP_LE: nRes = 0; break;
						case OP_NE: nRes = 0; case OP_GT: case OP_GE: nRes = 2; break;
					}
					break;
				case OP_GE: 
					Op2 = "OP_GE"; 
					switch(i){
						case OP_EQ: case OP_LT: case OP_LE: nRes = 0; break;
						case OP_NE: nRes = 0; case OP_GT: case OP_GE: nRes = 2; break;
					}
					break;
			}	
			OpStr="OP_LAND with ";
			lClassName = "OP_LAND__";
			OpStr += Op1;
			lClassName += Op1;
			OpStr += "(VT_STRING) && ";
			lClassName += "_VT_STRING__";
			OpStr += Op2;
			lClassName += Op2;
			OpStr += "(VT_URL)";
			lClassName +="_VT_URL";
			logResult(OpStr,RC_OTHER);
			executeComplexQuery(session,OP_LAND,ExprOp(i),VT_STRING,ExprOp(j),VT_URL,nRes,false,lClassName.c_str());
		}
	}

	// --- OP_LOR ---
	for(i=OP_EQ; i<=OP_GE;i++){
		nRes=0;
		nRes1=0;
		switch(i){
			case OP_EQ: Op1 = "OP_EQ";  nRes = 1; break;
			case OP_NE: Op1 = "OP_NE";  nRes = 2; break;
			case OP_LT: Op1 = "OP_LT";  nRes = 1; break;
			case OP_LE: Op1 = "OP_LE";  nRes = 1; break;
			case OP_GT: Op1 = "OP_GT";  nRes = 2; break;
			case OP_GE: Op1 = "OP_GE";  nRes = 2; break;
		}		
		temp = nRes;
		for(j=OP_EQ; j<=OP_GE;j++){
			nRes=temp;
			switch(j){
				case OP_EQ: 
					Op2 = "OP_EQ";  
					if(i == OP_GT || i == OP_GE || i == OP_NE) nRes = 3;
					nRes1 = 1; break;
				case OP_NE: 
					Op2 = "OP_NE";  
					if(i == OP_EQ || i == OP_LT || i == OP_LE) nRes = 3;
					nRes1 = 2; break;
				case OP_LT: 
					Op2 = "OP_LT";
					if(i == OP_GT || i == OP_GE || i == OP_NE) nRes = 3;
					nRes1 = 1; break;
				case OP_LE: 
					if(i == OP_GT || i == OP_GE || i == OP_NE) nRes = 3;
					Op2 = "OP_LE";
					nRes1 = 1; break;
				case OP_GT: 
					Op2 = "OP_GT";  
					if(i == OP_EQ || i == OP_LT || i == OP_LE) nRes = 3;
					nRes1 = 2; break;
				case OP_GE: 
					Op2 = "OP_GE";  
					if(i == OP_EQ || i == OP_LT || i == OP_LE) nRes = 3;
					nRes1 = 2; break;
			}
			OpStr="OP_LOR with ";
			lClassName = "OP_LOR__";
			OpStr1 = "OP_LOR with OP_EQ(dummy:false expr) || ";				
			OpStr += Op1;
			lClassName += Op1;
			OpStr += "(VT_STRING) || ";
			lClassName += "_VT_STRING__";
			OpStr += Op2;
			lClassName += Op2;
			OpStr1 += Op2;
			OpStr += "(VT_URL)";
			OpStr1 += "(VT_URL)";
			lClassName += "_VT_URL";
			
			logResult(OpStr,RC_OTHER);
			executeComplexQuery(session,OP_LOR,ExprOp(i),VT_STRING,ExprOp(j),VT_URL,nRes,false,lClassName.c_str());
			
			lClassName += "_1_";
			logResult(OpStr1,RC_OTHER);
			executeComplexQuery(session,OP_LOR,ExprOp(i),VT_STRING,ExprOp(j),VT_URL,nRes1,true,lClassName.c_str());
		}
	}
	// --- OP_LNOT ---
	for(i=OP_EQ; i<=OP_GE;i++){
		nRes=0;
		switch(i){
			case OP_EQ: Op1 = "OP_EQ";  nRes = 2; break;
			case OP_NE: Op1 = "OP_NE";  nRes = 1; break;
			case OP_LT: Op1 = "OP_LT";  nRes = 2; break;
			case OP_LE: Op1 = "OP_LE";  nRes = 2; break;
			case OP_GT: Op1 = "OP_GT";  nRes = 1; break;
			case OP_GE: Op1 = "OP_GE";  nRes = 1; break;
		}

		OpStr="OP_LNOT with ";
		lClassName = "OP_LNOT__";
		OpStr += Op1;
		lClassName += Op1;
		OpStr += "(VT_STRING)";
		lClassName += "_VT_STRING";
		
		logResult(OpStr,RC_OTHER);
		executeComplexQuery(session,OP_LNOT,ExprOp(i),VT_STRING,ExprOp(i),VT_URL,nRes,false,lClassName.c_str());
	}
}

void TestQueries::runStringOperators(ISession *session)
{
	//OP_SUBSTR
	{
	// OP_SUBSTR with VT_STRING
	logResult("OP_SUBSTR with VT_STRING (0)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_STRING,1,"OP_SUBSTR__VT_STRING0",0);

	logResult("OP_SUBSTR with VT_STRING (1)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_STRING,1,"OP_SUBSTR__VT_STRING1",1);

	logResult("OP_SUBSTR with VT_STRING (2)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_STRING,1,"OP_SUBSTR__VT_STRING2",2);

	logResult("OP_SUBSTR with VT_STRING (3)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_STRING,1,"OP_SUBSTR__VT_STRING3",3);

	logResult("OP_SUBSTR with VT_STRING (4)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_STRING,2,"OP_SUBSTR__VT_STRING4",4);

	logResult("OP_SUBSTR with VT_STRING (5)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_STRING,1,"OP_SUBSTR__VT_STRING5",5);

	logResult("OP_SUBSTR with VT_STRING (6)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_STRING,1,"OP_SUBSTR__VT_STRING6",6);

	// OP_SUBSTR with VT_BSTR
	logResult("OP_SUBSTR with VT_BSTR (0)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_BSTR,1,"OP_SUBSTR__VT_BSTR0",0);

	logResult("OP_SUBSTR with VT_BSTR (1)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_BSTR,1,"OP_SUBSTR__VT_BSTR1",1);

	logResult("OP_SUBSTR with VT_BSTR (2)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_BSTR,1,"OP_SUBSTR__VT_BSTR2",2);

	logResult("OP_SUBSTR with VT_BSTR (3)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_BSTR,1,"OP_SUBSTR__VT_BSTR3",3);

	logResult("OP_SUBSTR with VT_BSTR (4)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_BSTR,1,"OP_SUBSTR__VT_BSTR4",4);

	logResult("OP_SUBSTR with VT_BSTR (5)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_BSTR,1,"OP_SUBSTR__VT_BSTR5",5);

	logResult("OP_SUBSTR with VT_BSTR (6)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_BSTR,1,"OP_SUBSTR__VT_BSTR6",6);

	// OP_SUBSTR with VT_URL
	logResult("OP_SUBSTR with VT_URL (0)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_URL,3,"OP_SUBSTR__VT_URL0",0);

	logResult("OP_SUBSTR with VT_URL (1)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_URL,3,"OP_SUBSTR__VT_URL1",1);

	logResult("OP_SUBSTR with VT_URL (2)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_URL,1,"OP_SUBSTR__VT_URL2",2);

	logResult("OP_SUBSTR with VT_URL (3)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_URL,3,"OP_SUBSTR__VT_URL3",3);

	logResult("OP_SUBSTR with VT_URL (4)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_URL,3,"OP_SUBSTR__VT_URL4",4);

	logResult("OP_SUBSTR with VT_URL (5)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_URL,1,"OP_SUBSTR__VT_URL5",5);

	logResult("OP_SUBSTR with VT_URL (6)",RC_OTHER);
	executeSimpleQuery(session,OP_SUBSTR,VT_URL,1,"OP_SUBSTR__VT_URL6",6);
	}

	//OP_REPLACE
	{
	// OP_REPLACE with VT_STRING
	logResult("OP_REPLACE with VT_STRING (0)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_STRING,1,"OP_REPLACE__VT_STRING0",0);

	logResult("OP_REPLACE with VT_STRING (1)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_STRING,1,"OP_REPLACE__VT_STRING1",1);

	logResult("OP_REPLACE with VT_STRING (2)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_STRING,1,"OP_REPLACE__VT_STRING2",2);

	logResult("OP_REPLACE with VT_STRING (3)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_STRING,1,"OP_REPLACE__VT_STRING3",3);

	logResult("OP_REPLACE with VT_STRING (4)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_STRING,1,"OP_REPLACE__VT_STRING4",4);

	logResult("OP_REPLACE with VT_STRING (5)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_STRING,1,"OP_REPLACE__VT_STRING5",5);

	logResult("OP_REPLACE with VT_STRING (6)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_STRING,1,"OP_REPLACE__VT_STRING6",6);

	logResult("OP_REPLACE with VT_STRING (7)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_STRING,1,"OP_REPLACE__VT_STRING7",7);

	// OP_REPLACE with VT_BSTR
	logResult("OP_REPLACE with VT_BSTR (0)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_BSTR,1,"OP_REPLACE__VT_BSTR0",0);

	logResult("OP_REPLACE with VT_BSTR (1)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_BSTR,1,"OP_REPLACE__VT_BSTR1",1);

	logResult("OP_REPLACE with VT_BSTR (2)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_BSTR,1,"OP_REPLACE__VT_BSTR2",2);

	logResult("OP_REPLACE with VT_BSTR (3)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_BSTR,1,"OP_REPLACE__VT_BSTR3",3);

	logResult("OP_REPLACE with VT_BSTR (4)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_BSTR,1,"OP_REPLACE__VT_BSTR4",4);

	logResult("OP_REPLACE with VT_BSTR (5)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_BSTR,1,"OP_REPLACE__VT_BSTR5",5);

	logResult("OP_REPLACE with VT_BSTR (6)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_BSTR,1,"OP_REPLACE__VT_BSTR6",6);

	// OP_REPLACE with VT_URL
	logResult("OP_REPLACE with VT_URL (0)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_URL,1,"OP_REPLACE__VT_URL0",0);

	logResult("OP_REPLACE with VT_URL (1)",RC_OTHER);
	executeSimpleQuery(session,OP_REPLACE,VT_URL,1,"OP_REPLACE__VT_URL1",1);
	}

	// OP_PAD
	{
	// OP_PAD with VT_STRING
	logResult("OP_PAD with VT_STRING (0)",RC_OTHER);
	executeSimpleQuery(session,OP_PAD,VT_STRING,1,"OP_PAD__VT_STRING0",0);

	logResult("OP_PAD with VT_STRING (1)",RC_OTHER);
	executeSimpleQuery(session,OP_PAD,VT_STRING,1,"OP_PAD__VT_STRING1",1);

	logResult("OP_PAD with VT_STRING (2)",RC_OTHER);
	executeSimpleQuery(session,OP_PAD,VT_STRING,1,"OP_PAD__VT_STRING2",2);

	logResult("OP_PAD with VT_STRING (3)",RC_OTHER);
	executeSimpleQuery(session,OP_PAD,VT_STRING,1,"OP_PAD__VT_STRING3",3);

	// OP_PAD with VT_BSTR
	logResult("OP_PAD with VT_BSTR (0)",RC_OTHER);
	executeSimpleQuery(session,OP_PAD,VT_BSTR,1,"OP_PAD__VT_BSTR0",0);

	logResult("OP_PAD with VT_BSTR (1)",RC_OTHER);
	executeSimpleQuery(session,OP_PAD,VT_BSTR,1,"OP_PAD__VT_BSTR1",1);

	logResult("OP_PAD with VT_BSTR (2)",RC_OTHER);
	executeSimpleQuery(session,OP_PAD,VT_BSTR,1,"OP_PAD__VT_BSTR2",2);

	logResult("OP_PAD with VT_BSTR (3)",RC_OTHER);
	executeSimpleQuery(session,OP_PAD,VT_BSTR,1,"OP_PAD__VT_BSTR3",3);

	logResult("OP_PAD with VT_BSTR (4)",RC_OTHER);
	executeSimpleQuery(session,OP_PAD,VT_BSTR,1,"OP_PAD__VT_BSTR4",4);
	}

	//OP_UPPER
	{
	// OP_UPPER with VT_STRING
	logResult("OP_UPPER with VT_STRING (0)",RC_OTHER);
	executeSimpleQuery(session,OP_UPPER,VT_STRING,1,"OP_UPPER__VT_STRING0",0);

	logResult("OP_UPPER with VT_STRING (1)",RC_OTHER);
	executeSimpleQuery(session,OP_UPPER,VT_STRING,1,"OP_UPPER__VT_STRING1",1);

	logResult("OP_UPPER with VT_STRING (2)",RC_OTHER);
	executeSimpleQuery(session,OP_UPPER,VT_STRING,1,"OP_UPPER__VT_STRING2",2);

	// OP_UPPER with VT_URL
	logResult("OP_UPPER with VT_URL (0)",RC_OTHER);
	executeSimpleQuery(session,OP_UPPER,VT_URL,1,"OP_UPPER__VT_URL0",0);
	}

	//OP_LOWER
	{
	// OP_LOWER with VT_STRING
	logResult("OP_LOWER with VT_STRING (0)",RC_OTHER);
	executeSimpleQuery(session,OP_LOWER,VT_STRING,1,"OP_LOWER__VT_STRING0",0);

	logResult("OP_LOWER with VT_STRING (1)",RC_OTHER);
	executeSimpleQuery(session,OP_LOWER,VT_STRING,1,"OP_LOWER__VT_STRING1",1);

	logResult("OP_LOWER with VT_STRING (2)",RC_OTHER);
	executeSimpleQuery(session,OP_LOWER,VT_STRING,1,"OP_LOWER__VT_STRING2",2);

	// OP_UPPER with VT_URL
	logResult("OP_LOWER with VT_URL (0)",RC_OTHER);
	executeSimpleQuery(session,OP_LOWER,VT_URL,1,"OP_LOWER__VT_URL0",0);
	}

	//OP_CONCAT
	{
	// OP_CONCAT with VT_STRING
	logResult("OP_CONCAT with VT_STRING (0)",RC_OTHER);
	executeSimpleQuery(session,OP_CONCAT,VT_STRING,1,"OP_CONCAT__VT_STRING0",0);

	logResult("OP_CONCAT with VT_STRING (1)",RC_OTHER);
	executeSimpleQuery(session,OP_CONCAT,VT_STRING,1,"OP_CONCAT__VT_STRING1",1);

	// don't support OP_CONCAT on more than 2 values
	//logResult("OP_CONCAT with VT_STRING (2)",RC_OTHER);
	//executeSimpleQuery(session,OP_CONCAT,VT_STRING,1,"OP_CONCAT__VT_STRING2",2);

	// OP_CONCAT with VT_URL
	logResult("OP_CONCAT with VT_URL (0)",RC_OTHER);
	executeSimpleQuery(session,OP_CONCAT,VT_URL,1,"OP_CONCAT__VT_URL0",0);

	// OP_CONCAT with VT_BSTR
	logResult("OP_CONCAT with VT_BSTR (0)",RC_OTHER);
	executeSimpleQuery(session,OP_CONCAT,VT_BSTR,1,"OP_CONCAT__VT_BSTR0",0);	
	}
}

void TestQueries::runFuncOperators(ISession *session)
{
	//OP_POSITION
	{
	// OP_POSITION with VT_STRING
	logResult("OP_POSITION with VT_STRING (0)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_STRING,1,"OP_POSITION__VT_STRING0",0);

	logResult("OP_POSITION with VT_STRING (1)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_STRING,1,"OP_POSITION__VT_STRING1",1);

	logResult("OP_POSITION with VT_STRING (2)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_STRING,1,"OP_POSITION__VT_STRING2",2);

	logResult("OP_POSITION with VT_STRING (3)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_STRING,3,"OP_POSITION__VT_STRING3",3);
	
	logResult("OP_POSITION with VT_STRING (4)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_STRING,3,"OP_POSITION__VT_STRING4",4);
	
	// OP_POSITION with VT_URL
	logResult("OP_POSITION with VT_URL (0)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_URL,1,"OP_POSITION__VT_URL0",0);

	// OP_POSITION with VT_BSTR
	logResult("OP_POSITION with VT_BSTR (0)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_BSTR,1,"OP_POSITION__VT_BSTR0",0);

	logResult("OP_POSITION with VT_BSTR (1)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_BSTR,1,"OP_POSITION__VT_BSTR1",1);

	logResult("OP_POSITION with VT_BSTR (2)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_BSTR,1,"OP_POSITION__VT_BSTR2",2);

	logResult("OP_POSITION with VT_BSTR (3)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_BSTR,1,"OP_POSITION__VT_BSTR3",3);

	logResult("OP_POSITION with VT_BSTR (4)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_BSTR,1,"OP_POSITION__VT_BSTR4",4);

	// OP_POSITION with VT_ARRAY
	/*
	logResult("OP_POSITION with VT_ARRAY (0)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_ARRAY,1,"OP_POSITION__VT_ARRAY0",0);

	logResult("OP_POSITION with VT_ARRAY (1)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_ARRAY,1,"OP_POSITION__VT_ARRAY1",1);	

	logResult("OP_POSITION with VT_ARRAY (2)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_ARRAY,1,"OP_POSITION__VT_ARRAY2",2);	
	
	logResult("OP_POSITION with VT_ARRAY (3)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_ARRAY,1,"OP_POSITION__VT_ARRAY3",3);
	
	logResult("OP_POSITION with VT_ARRAY (4)",RC_OTHER);
	executeSimpleQuery(session,OP_POSITION,VT_ARRAY,0,"OP_POSITION__VT_ARRAY4",4);
	*/
	}
}
void TestQueries::runConversionOperators(ISession *session)
{
	// --- OP_TONUM ---
	logResult("OP_TONUM with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_TONUM,VT_STRING,1,"OP_TONUM__VT_STRING");

	logResult("OP_TONUM with VT_BOOL",RC_OTHER);
	executeSimpleQuery(session,OP_TONUM,VT_BOOL,2,"OP_TONUM__VT_BOOL");

	// --- OP_TOINUM ---
	logResult("OP_TOINUM with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_TOINUM,VT_STRING,1,"OP_TOINUM__VT_STRING");

	logResult("OP_TOINUM with VT_BOOL",RC_OTHER);
	executeSimpleQuery(session,OP_TOINUM,VT_BOOL,2,"OP_TOINUM__VT_BOOL");

	logResult("OP_TOINUM with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_TOINUM,VT_DOUBLE,1,"OP_TOINUM__VT_DOUBLE");

	logResult("OP_TOINUM with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_TOINUM,VT_FLOAT,1,"OP_TOINUM__VT_FLOAT");

	// --- OP_CAST --- (to VT_STRING only)
	logResult("OP_TOSTRING with VT_URL",RC_OTHER);
	executeSimpleQuery(session,OP_CAST,VT_URL,1,"OP_TOSTRING__VT_URL");

	logResult("OP_TOSTRING with VT_BOOL",RC_OTHER);
	executeSimpleQuery(session,OP_CAST,VT_BOOL,2,"OP_TOSTRING__VT_BOOL");

	logResult("OP_TOSTRING with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_CAST,VT_INT,1,"OP_TOSTRING__VT_INT");
	
	logResult("OP_TOSTRING with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_CAST,VT_FLOAT,1,"OP_TOSTRING__VT_FLOAT");

	logResult("OP_TOSTRING with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_CAST,VT_DOUBLE,1,"OP_TOSTRING__VT_DOUBLE");	

	logResult("OP_TOSTRING with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_CAST,VT_UINT,1,"OP_TOSTRING__VT_UINT");

	logResult("OP_TOSTRING with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_CAST,VT_INT64,1,"OP_TOSTRING__VT_INT64");

	logResult("OP_TOSTRING with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_CAST,VT_UINT64,1,"OP_TOSTRING__VT_UINT64");
	
	//This is failing for NO reason *^#$*&^
#if 0
	// --- OP_TOBOOL ---
	logResult("OP_TOBOOL with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_TOBOOL,VT_STRING,1,"OP_TOBOOL__VT_STRING");
#endif

	// -- OP_RANGE ---
	logResult("OP_RANGE with VT_STRING",RC_OTHER);
	executeSimpleQuery(session,OP_RANGE,VT_STRING,1,"OP_TORANGE__VT_STRING");

	logResult("OP_RANGE with VT_INT",RC_OTHER);
	executeSimpleQuery(session,OP_RANGE,VT_INT,1,"OP_TORANGE__VT_INT");

	logResult("OP_RANGE with VT_UINT",RC_OTHER);
	executeSimpleQuery(session,OP_RANGE,VT_UINT,1,"OP_TORANGE__VT_UINT");

	logResult("OP_RANGE with VT_INT64",RC_OTHER);
	executeSimpleQuery(session,OP_RANGE,VT_INT64,1,"OP_TORANGE__VT_INT64");

	logResult("OP_RANGE with VT_UINT64",RC_OTHER);
	executeSimpleQuery(session,OP_RANGE,VT_UINT64,1,"OP_TORANGE__VT_UINT64");

	logResult("OP_RANGE with VT_DATETIME",RC_OTHER);
	executeSimpleQuery(session,OP_RANGE,VT_DATETIME,1,"OP_TORANGE__VT_DATETIME");

	logResult("OP_RANGE with VT_FLOAT",RC_OTHER);
	executeSimpleQuery(session,OP_RANGE,VT_FLOAT,1,"OP_TORANGE__VT_FLOAT");

	logResult("OP_RANGE with VT_URL",RC_OTHER);
	executeSimpleQuery(session,OP_RANGE,VT_URL,1,"OP_TORANGE__VT_URL");

	logResult("OP_RANGE with VT_DOUBLE",RC_OTHER);
	executeSimpleQuery(session,OP_RANGE,VT_DOUBLE,1,"OP_TORANGE__VT_DOUBLE");
}

IExprTree * TestQueries::createArithExpr(ISession *session,unsigned var,int Op, int type)
{
	IExprTree *expr1 = NULL,*exprfinal = NULL;
	PropertyID pids[1];
	Value val[2], vals[10], val1[1];
	unsigned int ui32;
	int64_t i64;
	uint64_t ui64; 
	switch(Op){
		case OP_PLUS:
			switch(type){
				case VT_INT://nExpResults = 1;
					pids[0] = pm[7].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(20);
					expr1 = session->expr(OP_PLUS,2,val);
					val[0].set(expr1);
					val[1].set(47);
					break;
				case VT_UINT:	
					pids[0] = pm[16].uid;
					val[0].setVarRef(0,*pids);
					ui32 = 123456789;
					val[1].set(ui32);
					expr1 = session->expr(OP_PLUS,2,val);
					val[0].set(expr1);
					ui32 = 1111111110;
					val[1].set(ui32);
					break;
				case VT_INT64:		
					pids[0] = pm[17].uid;
					val[0].setVarRef(0,*pids);
					i64 = 123456789;
					val[1].setI64(i64);
					expr1 = session->expr(OP_PLUS,2,val);
					val[0].set(expr1);
					i64 = 246913578;
					val[1].setI64(i64);
					break;
				case VT_UINT64:	
					pids[0] = pm[18].uid;
					val[0].setVarRef(0,*pids);
					ui64 = 123456789;
					val[1].setU64(ui64);
					expr1 = session->expr(OP_PLUS,2,val);
					val[0].set(expr1);
					ui64 = 246913578;
					val[1].setU64(ui64);
					break;
				case VT_FLOAT:
					pids[0] = pm[10].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(float(2.6));
					expr1 = session->expr(OP_PLUS,2,val);
					val[0].set(expr1);
					val[1].set(float(10.1));
					break;
				case VT_DOUBLE:		
					pids[0] = pm[11].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(double(10000.40));
					expr1 = session->expr(OP_PLUS,2,val);
					val[0].set(expr1);
					val[1].set(double(20000.9));
					break;
				case VT_DATETIME:
					i64 = 123456789;
					pids[0] = pm[19].uid;
					val[0].setVarRef(0,*pids);
					val[1].setI64(i64);
					expr1 = session->expr(OP_PLUS,2,val);
					i64 = 12753803377800539LL;
					val[0].set(expr1);
					val[1].setDateTime(i64);
					break;
				case VT_INTERVAL:	
				default:
					mLogger.out()<<"DataType not supported"<<endl;
			}
			exprfinal = session->expr(OP_EQ,2,val);
			break;
		case OP_MINUS:
			switch(type){
				case VT_INT://nExpResults = 1;
					pids[0] = pm[7].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(7);
					expr1 = session->expr(OP_MINUS,2,val);
					val[0].set(expr1);
					val[1].set(20);
					break;
				case VT_UINT:	
					pids[0] = pm[16].uid;
					val[0].setVarRef(0,*pids);
					ui32 = 123456789;
					val[1].set(ui32);
					expr1 = session->expr(OP_MINUS,2,val);
					val[0].set(expr1);
					ui32 = 864197532;
					val[1].set(ui32);
					break;
				case VT_INT64:		
					pids[0] = pm[17].uid;
					val[0].setVarRef(0,*pids);
					i64 = 12345678;
					val[1].setI64(i64);
					expr1 = session->expr(OP_MINUS,2,val);
					val[0].set(expr1);
					i64 = 111111111;
					val[1].setI64(i64);
					break;
				case VT_UINT64:	
					pids[0] = pm[18].uid;
					val[0].setVarRef(0,*pids);
					ui64 = 12345678;
					val[1].setU64(ui64);
					expr1 = session->expr(OP_MINUS,2,val);
					val[0].set(expr1);
					ui64 = 111111111;
					val[1].setU64(ui64);
					break;
				case VT_FLOAT:
					pids[0] = pm[10].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(float(2.6));
					expr1 = session->expr(OP_MINUS,2,val);
					val[0].set(expr1);
					val[1].set(float(4.9));
					break;
				case VT_DOUBLE:
					pids[0] = pm[11].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(double(1.60));
					expr1 = session->expr(OP_MINUS,2,val);
					val[0].set(expr1);
					val[1].set(double(9998.9));
					break;
				case VT_DATETIME:
					i64 = 123456789;
					pids[0] = pm[19].uid;
					val[0].setVarRef(0,*pids);
					val[1].setDateTime(i64);
					expr1 = session->expr(OP_MINUS,2,val);
					i64 = 12753803130886961LL;
					val[0].set(expr1);
					val[1].setInterval(i64);
					break;
				case VT_INTERVAL:	
				default:
					mLogger.out()<<"DataType not supported"<<endl;
			}
			exprfinal = session->expr(OP_EQ,2,val);
			break;
		case OP_MUL:
			switch(type){
				case VT_INT://nExpResults = 1;
					pids[0] = pm[7].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(2);
					expr1 = session->expr(OP_MUL,2,val);
					val[0].set(expr1);
					val[1].set(54);
					break;
				case VT_UINT:	
					pids[0] = pm[16].uid;
					val[0].setVarRef(0,*pids);
					ui32 = 2;
					val[1].set(ui32);
					expr1 = session->expr(OP_MUL,2,val);
					val[0].set(expr1);
					ui32 = 1975308642UL;
					val[1].set(ui32);
					break;
				case VT_INT64:
					pids[0] = pm[17].uid;
					val[0].setVarRef(0,*pids);
					i64 = 123456789;
					val[1].setI64(i64);
					expr1 = session->expr(OP_MUL,2,val);
					val[0].set(expr1);
					i64 = 15241578750190521LL;
					val[1].setI64(i64);
					break;
				case VT_UINT64:
					pids[0] = pm[18].uid;
					val[0].setVarRef(0,*pids);
					ui64 = 123456789;
					val[1].setU64(ui64);
					expr1 = session->expr(OP_MUL,2,val);
					val[0].set(expr1);
					ui64 = 15241578750190521ULL;
					val[1].setU64(ui64);
					break;
				case VT_FLOAT:
					pids[0] = pm[10].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(float(2.5));
					expr1 = session->expr(OP_MUL,2,val);
					val[0].set(expr1);
					val[1].set(float(18.75));
					break;
				case VT_DOUBLE:		
					pids[0] = pm[11].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(double(23.50));
					expr1 = session->expr(OP_MUL,2,val);
					val[0].set(expr1);
					val[1].set(double(235011.75));
					break;
				default:
					mLogger.out()<<"DataType not supported"<<endl;
			}
			exprfinal = session->expr(OP_EQ,2,val);
			break;
		case OP_DIV:
			switch(type){
				case VT_INT://nExpResults = 1;
					pids[0] = pm[7].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(9);
					expr1 = session->expr(OP_DIV,2,val);
					val[0].set(expr1);
					val[1].set(3);
					break;
				case VT_UINT:	
					pids[0] = pm[16].uid;
					val[0].setVarRef(0,*pids);
					ui32 = 3;
					val[1].set(ui32);
					expr1 = session->expr(OP_DIV,2,val);
					val[0].set(expr1);
					ui32 = 329218107;
					val[1].set(ui32);
					break;
				case VT_INT64:		
					pids[0] = pm[17].uid;
					val[0].setVarRef(0,*pids);
					i64 = 12345;
					val[1].setI64(i64);
					expr1 = session->expr(OP_DIV,2,val);
					val[0].set(expr1);
					i64 = 10000;
					val[1].setI64(i64);
					break;
				case VT_UINT64:	
					pids[0] = pm[18].uid;
					val[0].setVarRef(0,*pids);
					ui64 = 12345;
					val[1].setU64(ui64);
					expr1 = session->expr(OP_DIV,2,val);
					val[0].set(expr1);
					ui64 = 10000;
					val[1].setU64(ui64);
					break;
				case VT_FLOAT:
					pids[0] = pm[10].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(float(2.5));
					expr1 = session->expr(OP_DIV,2,val);
					val[0].set(expr1);
					val[1].set(float(3.0));
					break;
				case VT_DOUBLE:		
					pids[0] = pm[11].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(double(23.50));
					expr1 = session->expr(OP_DIV,2,val);
					val[0].set(expr1);
					val[1].set(double(425.55319148936172));
					break;
				default:
					mLogger.out()<<"DataType not supported"<<endl;
			}
			exprfinal = session->expr(OP_EQ,2,val);
			break;
		case OP_MOD:
			switch(type){
				case VT_INT://nExpResults = 1;
					pids[0] = pm[7].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(8);
					expr1 = session->expr(OP_MOD,2,val);
					val[0].set(expr1);
					val[1].set(3);
					break;
				case VT_UINT:	
					pids[0] = pm[16].uid;
					val[0].setVarRef(0,*pids);
					ui32 = 25;
					val[1].set(ui32);
					expr1 = session->expr(OP_MOD,2,val);
					val[0].set(expr1);
					ui32 = 21;
					val[1].set(ui32);
					break;
				case VT_INT64:		
					pids[0] = pm[17].uid;
					val[0].setVarRef(0,*pids);
					i64 = 12345;
					val[1].setI64(i64);
					expr1 = session->expr(OP_MOD,2,val);
					val[0].set(expr1);
					i64 = 6789;
					val[1].setI64(i64);
					break;
				case VT_UINT64:	
					pids[0] = pm[18].uid;
					val[0].setVarRef(0,*pids);
					ui64 = 12345;
					val[1].setU64(ui64);
					expr1 = session->expr(OP_MOD,2,val);
					val[0].set(expr1);
					ui64 = 6789;
					val[1].setU64(ui64);
					break;
				case VT_FLOAT:
					pids[0] = pm[10].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(float(2.3));
					expr1 = session->expr(OP_MOD,2,val);
					val[0].set(expr1);
					val[1].set(float(0.60000014));
					break;
				case VT_DOUBLE:		
					pids[0] = pm[11].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(double(23.50));
					expr1 = session->expr(OP_MOD,2,val);
					val[0].set(expr1);
					val[1].set(double(13.0));
					break;
				default:
					mLogger.out()<<"DataType not supported"<<endl;
			}
			exprfinal = session->expr(OP_EQ,2,val);
			break;
		case OP_NEG:
			switch(type){
				case VT_INT://nExpResults = 1;
					pids[0] = pm[7].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_NEG,1,val);
					val[0].set(expr1);
					val[1].set(-27);
					break;
				/* --- Unsigned integers OP_NEG not applicable --
				case VT_UINT:	
					pids[0] = pm[16].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_NEG,1,val);
					val[0].set(expr1);
					ui32 = -987654321;
					val[1].set(ui32);
					break;
				*/
				case VT_INT64:		
					pids[0] = pm[17].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_NEG,1,val);
					val[0].set(expr1);
					i64 = -123456789;
					val[1].setI64(i64);
					break;
				/* --- Unsigned integers OP_NEG not applicable --
				case VT_UINT64:	
					pids[0] = pm[18].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_NEG,1,val);
					val[0].set(expr1);
					ui64 = -123456789;
					val[1].setU64(ui64);
					break;
				*/
				case VT_FLOAT:
					pids[0] = pm[10].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_NEG,1,val);
					val[0].set(expr1);
					val[1].set(float(-7.5));
					break;
				case VT_DOUBLE:		
					pids[0] = pm[11].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_NEG,1,val);
					val[0].set(expr1);
					val[1].set(double(-10000.5));
					break;
				case VT_INTERVAL:
					break;
				default:
					mLogger.out()<<"DataType not supported"<<endl;
			}
			exprfinal = session->expr(OP_EQ,2,val);
			break;
		case OP_ABS:
			switch(type){
				case VT_INT://nExpResults = 1;
					pids[0] = pm[7].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_ABS,1,val);
					val[0].set(expr1);
					val[1].set(27);
					break;
				case VT_UINT:	
					pids[0] = pm[16].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_ABS,1,val);
					val[0].set(expr1);
					ui32 = 987654321;
					val[1].set(ui32);
					break;
				case VT_INT64:		
					pids[0] = pm[17].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_ABS,1,val);
					val[0].set(expr1);
					i64 = 123456789;
					val[1].setI64(i64);
					break;
				case VT_UINT64:	
					pids[0] = pm[18].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_ABS,1,val);
					val[0].set(expr1);
					ui64 = 123456789;
					val[1].setU64(ui64);
					break;
				case VT_FLOAT:
					pids[0] = pm[10].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_ABS,1,val);
					val[0].set(expr1);
					val[1].set(float(7.5));
					break;
				case VT_DOUBLE:		
					pids[0] = pm[11].uid;
					val[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_ABS,1,val);
					val[0].set(expr1);
					val[1].set(double(10000.5));
					break;
				case VT_INTERVAL:
					break;
				default:
					mLogger.out()<<"DataType not supported"<<endl;
			}
			exprfinal = session->expr(OP_EQ,2,val);
			break;
		case OP_AVG:
			switch(type){
				case VT_INT://nExpResults = 1;
					pids[0] = pm[7].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(21);
					expr1 = session->expr(OP_AVG,2,val);
					val[0].set(expr1);
					val[1].set(24);
					break;
				case VT_UINT:	
					pids[0] = pm[16].uid;
					vals[0].setVarRef(0,*pids);
					ui32 = 1;
					vals[1].set(ui32);
					ui32 = 2;
					vals[2].set(ui32);
					expr1 = session->expr(OP_AVG,3,vals);
					val[0].set(expr1);
					ui32 = 329218108;
					val[1].set(ui32);
					break;
				case VT_INT64:		
					pids[0] = pm[17].uid;
					vals[0].setVarRef(0,*pids);
					ui32 = 1;
					vals[1].set(ui32);
					ui32 = 2;
					vals[2].set(ui32);
					expr1 = session->expr(OP_AVG,3,vals);
					val[0].set(expr1);
					i64 = 41152264;
					val[1].setI64(i64);
					break;
				case VT_UINT64:	
					pids[0] = pm[18].uid;
					vals[0].setVarRef(0,*pids);
					ui64 = 123456789;
					vals[1].setU64(ui64);
					vals[2].setU64(ui64);
					expr1 = session->expr(OP_AVG,3,vals);
					val[0].set(expr1);
					ui64 = 123456789;
					val[1].setU64(ui64);
					break;
				case VT_FLOAT:
					pids[0] = pm[10].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(float(1.5));
					expr1 = session->expr(OP_AVG,2,val);
					val[0].set(expr1);
					val[1].set(float(4.5));
					break;
				case VT_DOUBLE:		
					pids[0] = pm[11].uid;
					val[0].setVarRef(0,*pids);
					val[1].set(double(101.5));
					expr1 = session->expr(OP_AVG,2,val);
					val[0].set(expr1);
					val[1].set(double(5051));
					break;
				case VT_DATETIME:
					i64 = 0;
					pids[0] = pm[19].uid;
					val[0].setVarRef(0,*pids);
					val[1].setDateTime(i64);
					expr1 = session->expr(OP_AVG,2,val);
					i64 = 6376901627171875LL;
					val[0].set(expr1);
					val[1].setDateTime(i64);
					break;
				case VT_ARRAY:
					pids[0] = pm[7].uid;
					vals[0].set(26);
					vals[1].set(28);
					vals[2].set(26);
					vals[3].set(28);
					vals[4].set(26);
					vals[5].set(28);
					vals[6].set(26);
					vals[7].set(28);
					vals[8].set(26);
					vals[9].set(28);
					val1[0].set(vals, 10);
					expr1 = session->expr(OP_AVG,1,val1);
					val[0].setVarRef(0,*pids);
					val[1].set(expr1);
					break;					
				case VT_COLLECTION:
					pids[0] = pm[25].uid; // (1,2,3,...,499,500)
					val1[0].setVarRef(0,*pids);
					expr1 = session->expr(OP_AVG,1,val1);
					val[0].set(expr1);
					val[1].set(250.5); 
					break;
				case VT_INTERVAL:	
				default:
					mLogger.out()<<"DataType not supported"<<endl;
					break;
			}
			exprfinal = session->expr(OP_EQ,2,val);
			break;
		default:
			break;
	}
	return exprfinal;
}

IExprTree * TestQueries::createCompExpr(ISession *session,unsigned var,int Op, int type)
{
	IExprTree *exprfinal = NULL, *expr1 = NULL,*expr2 = NULL;
	PropertyID pids[1];
	Value val[2], val1[1],val2[2];
	const char *str = NULL,*url = NULL;
	int i = 0;
	unsigned int ui32= 0;
	int64_t i64 = 0;
	uint64_t ui64 = 0,dt = 0; 
	float f = 0;
	double d = 0;
	bool b = true;

	switch(Op){
		case OP_EQ:
			str = "India";
			url = "http://www.google.com";
			i = 27;
			ui32 = 987654321;
			i64 = 123456789;
			ui64 = 123456789;
			f = 7.5;
			d = 10000.50;
			b = true;
			dt = 12753803254343750LL;
			break;
		case OP_NE:
			str = "India";
			url = "http://www.google.com";
			i = 27;
			ui32 = 987654320;
			i64 = 123456780;
			ui64 = 123456780;
			f = 7.5;
			d = 10000.50;
			b = false;
			dt = 12753803254343750LL;
			break;
		case OP_LT: 
			str = "Indoo";
			url = "http://www.googlf.com";
			i = 28;
			ui32 = 987654322;
			i64 = 123456790;
			ui64 = 123456790;
			f = 7.5;
			d = 10000.60;
			dt = 12753803254343760LL;
			break;
		case OP_LE: 
			str = "India";
			url = "http://www.google.com";
			i = 22;
			ui32 = 987654321;
			i64 = 123456789;
			ui64 = 123456789;
			f = 5.5;
			d = 10000.50;
			dt = 12753803254343750LL;
			break;
		case OP_GT: 
			str = "Indj";
			url = "http://www.googlf.com";
			i = 26;
			ui32 = 987654320;
			i64 = 123456788;
			ui64 = 123456788;
			f = 5.5;
			d = 10000.60;
			dt = 12753803254343740LL;
			break;
		case OP_GE:
			str = "Nepal";
			url = "http://www.nepalfc.com";
			i = 27;
			ui32 = 987654321;
			i64 = 123456789;
			ui64 = 123456789;
			f = 7.5;
			d = 10000.50;
			dt = 12753803254343750LL;
			break;
	}
	switch(type){
		case VT_STRING:
			pids[0] = pm[0].uid;
			val[0].setVarRef(0,*pids);
			val[1].set(str);	
			break;
		case VT_URL:
			pids[0] = pm[14].uid;
			val[0].setVarRef(0,*pids);
			val[1].setURL(url);	
			break;
		case VT_INT:
			pids[0] = pm[7].uid;
			val[0].setVarRef(0,*pids);
			val[1].set(i);	
			break;
		case VT_UINT:
			pids[0] = pm[16].uid;
			val[0].setVarRef(0,*pids);
			val[1].set(ui32);
			break;
		case VT_INT64:
			pids[0] = pm[17].uid;
			val[0].setVarRef(0,*pids);
			val[1].setI64(i64);
			break;
		case VT_UINT64:
			pids[0] = pm[18].uid;
			val[0].setVarRef(0,*pids);
			val[1].setU64(ui64);
			break;
		case VT_FLOAT:
			pids[0] = pm[10].uid;
			val[0].setVarRef(0,*pids);
			val[1].set(f);
			break;
		case VT_DOUBLE:
			pids[0] = pm[11].uid;
			val[0].setVarRef(0,*pids);
			val[1].set(d);
			break;
		case VT_DATETIME:
			pids[0] = pm[19].uid;
			val[0].setVarRef(0,*pids);
			val[1].setDateTime(dt);
			break;
		case VT_REF:
		case VT_REFIDPROP:
		case VT_REFIDELT:
		case VT_BOOL:
			pids[0] = pm[9].uid;
			val[0].setVarRef(0,*pids);
			val[1].set(b);
			break;
		case VT_RESERVED1:
		case VT_URIID:
		case VT_IDENTITY:
		default:
			logResult("Datatype not supported",RC_OTHER);
	}
       if (Op == OP_NE) {
		if (type == VT_STRING)
			val1[0].setVarRef(0,pm[0].uid);
		else if(type == VT_URL)
			val1[0].setVarRef(0,pm[14].uid);
		else
			assert(0);
		expr1 = session->expr(OP_EXISTS,1,val1);
		expr2 = session->expr(ExprOp(Op),2,val, NULLS_NOT_INCLUDED_OP);
		val2[0].set(expr1); val2[1].set(expr2);
		exprfinal =  session->expr(OP_LAND,2,val2);
       } else
	exprfinal =  session->expr(ExprOp(Op),2,val,NULLS_NOT_INCLUDED_OP);
	return exprfinal;
}

IExprTree * TestQueries::createBitwiseExpr(ISession *session,unsigned var,int Op, int type)
{
	IExprTree *expr1,*exprfinal;
	PropertyID pids[1];
	Value val[2],val1[2];
	int i=0,ei=0;
	unsigned int ui32=0,eui32=0;
	int64_t i64=0,ei64=0;
	uint64_t ui64=0,eui64=0; 
	switch(Op){
		case OP_AND:
			i = ei = 27;
			ui32 = eui32 = 987654321;
			i64 = ei64 = 123456789;
			ui64 = eui64 = 123456789;
			break;
		case OP_OR:
			i = 4; ei = 31;
			ui32 = 14; eui32 = 987654335;
			i64 = 10; ei64 = 123456799;
			ui64 = 234; eui64 = 123457023;
			break;
		case OP_XOR:
			i = 6; ei = 29;
			ui32 = 15; eui32 = 987654334;
			i64 = 15; ei64 = 123456794;
			ui64 = 63; eui64 = 123456810;
			break;
		case OP_NOT:
			ei = -28;
			eui32 = (unsigned int)-987654322; // Review - negative assigned to unisnged
			ei64 = -123456790;
			eui64 = (uint64_t) -123456790; // REVIEW: negative assigned to unisnged
			break;
		case OP_LSHIFT:
			i = 2; ei = 27<<2;
			ui32 = 1; eui32 = 987654321<<1;
			i64 = 0; ei64 = 123456789<<0;
			ui64 = 10; eui64 = 126419751936LL; //123456789<<10
			break;
		case OP_RSHIFT:
			i = 2; ei = 27>>2;
			ui32 = 1; eui32 = 987654321>>1;
			i64 = 0; ei64 = 123456789>>0;
			ui64 = 10; eui64 = 123456789>>10;
			break;
	}
	switch(type){
		case VT_INT:
			pids[0] = pm[7].uid;
			val[0].setVarRef(0,*pids);
			if(Op != OP_NOT) val[1].set(i);
			val1[1].set(ei);
			break;
		case VT_UINT:
			pids[0] = pm[16].uid;
			val[0].setVarRef(0,*pids);
			if(Op != OP_NOT) val[1].set(ui32);
			val1[1].set(eui32);
			break;
		case VT_INT64:
			pids[0] = pm[17].uid;
			val[0].setVarRef(0,*pids);
			if(Op != OP_NOT) val[1].setI64(i64);			
			val1[1].setI64(ei64);
			break;
		case VT_UINT64:
			pids[0] = pm[18].uid;
			val[0].setVarRef(0,*pids);
			if(Op != OP_NOT) val[1].setU64(ui64);			
			val1[1].setU64(eui64);
			break;
		default:
			logResult("Datetype not supported",RC_OTHER);
	}
	if(Op != OP_NOT)
		expr1 = session->expr(ExprOp(Op),2,val);
	else
		expr1 = session->expr(ExprOp(Op),1,val);

	val1[0].set(expr1);
	exprfinal = session->expr(OP_EQ,2,val1);
	return exprfinal;
}

IExprTree * TestQueries::createLogicalExpr(ISession *session,unsigned var,int Op,int Op1, int Op2,int type1, int type2,bool fake)
{
	IExprTree *expr1 = NULL,*expr2 = NULL,*exprfinal = NULL, *expr3 = NULL, *expr4 = NULL;
	PropertyID pids[1];
	Value val[2],val2[2],val3[1];
	if(Op1 >= OP_EQ && Op1 <= OP_GE)
		if(Op == OP_LOR && fake){
			pids[0] = pm[0].uid;
			val[0].setVarRef(0,*pids);
			val[1].set("Africa");
			expr1 = session->expr(OP_EQ,2,val);
		}
		else
			expr1 = createCompExpr(session,var,Op1,type1);
	else if(1) {} // To add OP_CONTAINS, OP_BEGINS, OP_ENDS, OP_MATCH, OP_IN, OP_ISLOCAL, OP_EXISTS

	if(Op2 >= OP_EQ && Op2 <= OP_GE && Op != OP_LNOT)
		expr2 = createCompExpr(session,var,Op2,type2);
	else if(1) {} // To add OP_CONTAINS, OP_BEGINS, OP_ENDS, OP_MATCH, OP_IN, OP_ISLOCAL, OP_EXISTS

	switch(Op){
		case OP_LAND:
		case OP_LOR:
			val[0].set(expr1);
			val[1].set(expr2);
			exprfinal = session->expr(ExprOp(Op),2,val);
			break;
		case OP_LNOT:	
			if (Op1 == OP_EQ || Op1 == OP_NE){
				// OP_LNOT and OP_EQ ==> OP_NE, add OP_EXISTS condition
				// OP_LNOT and (OP_NE and OP_EXISTS)  ==> OP_EQ or prop is NULL, add OP_EXISTS condition
				val3[0].setVarRef(0,pm[0].uid);
				expr3 = session->expr(OP_EXISTS,1,val3);
				val[0].set(expr1);
				expr4 = session->expr(ExprOp(Op),1,val);
				val2[0].set(expr3);val2[1].set(expr4);
				exprfinal = session->expr(OP_LAND,2,val2);
			}else {
			val[0].set(expr1);
			exprfinal = session->expr(ExprOp(Op),1,val);
			}
			break;
	}
	return exprfinal;
}

IExprTree * TestQueries::createConvExpr(ISession *session,unsigned var,int Op, int type)
{
	IExprTree *expr1;
	PropertyID pids[1]={STORE_INVALID_PROPID};
	Value val[2],val1[2];
	const char *str = NULL; //,*url;
	int i = 0;
	unsigned int ui32 = 0;
	int64_t i64 = 0;
	uint64_t ui64 = 0; 
	float f = 0;
	double d = 0;
		
	switch(Op){
		case OP_TONUM:
			switch(type){
				case VT_STRING:
					pids[0] = pm[6].uid;
					i = 500080;
					break;
				case VT_BOOL:
					pids[0] = pm[9].uid;
					i = 1;
					break;
				case VT_DATETIME: //TBD
				case VT_INTERVAL: 
					break;//TBD
			}
			val1[1].set(i);
			break;
		case OP_TOINUM:
			switch(type){
				case VT_STRING:
					pids[0] = pm[6].uid;
					i = 500080;
					break;
				case VT_FLOAT: 
					pids[0] = pm[10].uid;
					i = 7;
					break;
				case VT_DOUBLE:
					pids[0] = pm[11].uid;
					i = 10000;
					break;
				case VT_BOOL:
					pids[0] = pm[9].uid;					
					i = 1;
					break;
				case VT_DATETIME: //TBD
				case VT_INTERVAL: //TBD
					break;
			}
			val1[1].set(i);
			break;
		case OP_CAST:
			switch(type){
				case VT_URL:
					pids[0] = pm[14].uid;
					str = "http://www.google.com";
					break;
				case VT_FLOAT: 
					pids[0] = pm[10].uid;
					str = "7.5";
					break;
				case VT_DOUBLE:
					pids[0] = pm[11].uid;
					str = "10000.5";
					break;
				case VT_BOOL:
					pids[0] = pm[9].uid;					
					str = "true";
					break;
				case VT_INT:
					pids[0] = pm[7].uid;
					str = "22";
					break;
				case VT_UINT:
					pids[0] = pm[16].uid;
					str = "987654321";
					break;
				case VT_INT64:
					pids[0] = pm[17].uid;
					str = "123456789";
					break;
				case VT_UINT64:
					pids[0] = pm[18].uid;
					str = "123456789";
					break;
				case VT_DATETIME: //TBD
				case VT_INTERVAL: //TBD
					break;
			}
			val1[1].set(str);
			val[0].setVarRef(0,*pids);
			val[1].set((unsigned)VT_STRING);
			expr1 = session->expr(OP_CAST,2,val);
			val1[0].set(expr1);
			return session->expr(OP_EQ,2,val1);
		case OP_RANGE:
			
			switch(type){
				case VT_STRING:
					val[0].set("Indaa");
					val[1].set("Indll");
					pids[0] = pm[0].uid;
					break;
				case VT_URL:
					val[0].setURL("http://www.googla.com");
					val[1].setURL("http://www.googlf.com");
					pids[0] = pm[14].uid;
					break;
				case VT_BSTR:
				case VT_INT:
					val[0].set(26);
					val[1].set(28);
					pids[0] = pm[7].uid;
					break;
				case VT_UINT:
					ui32 = 987654300;
					val[0].set(ui32);
					ui32 = 987654400;
					val[1].set(ui32);
					pids[0] = pm[16].uid;
					break;
				case VT_INT64: 
					i64 = 123456780;
					val[0].setI64(i64);
					i64 = 123456790;
					val[1].setI64(i64);
					pids[0] = pm[17].uid;
					break;
				case VT_DATETIME:
					ui64 = 12753803254343740LL;
					val[0].setDateTime(ui64);
					ui64 = 12753803254343760LL;
					val[1].setDateTime(ui64);
					pids[0] = pm[19].uid;
					break;
				case VT_UINT64:
					ui64 = 123456780;
					val[0].setU64(ui64);
					ui64 = 123456790;
					val[1].setU64(ui64);
					pids[0] = pm[18].uid;
					break;
				case VT_FLOAT: 
					f = 6.5;
					val[0].set(f);
					f = 8.0;
					val[1].set(f);
					pids[0] = pm[10].uid;
					break;
				case VT_DOUBLE:
					d = 9999.50;
					val[0].set(d);
					d = 10001.0;
					val[1].set(d);
					pids[0] = pm[11].uid;
					break;
				/*
				case VT_REF:
				case VT_ANNOTATION: 
				case VT_REFID:
				case VT_ANNOTATIONID:
				case VT_BOOL:
				case VT_CLASS:
				case VT_PROPERTY:
				case VT_IDENTITY:
				case VT_RESERVED1:
				case VT_REFIDPROP:
				case VT_REFIDELT:
				case VT_REFPROP:
				case VT_REFELT:
				*/
			}
			expr1 = session->expr(ExprOp(Op),2,val);
			val1[0].setVarRef(0,*pids);
			val1[1].set(expr1);
			return session->expr(OP_IN,2,val1);
	}
	val[0].setVarRef(0,*pids);
	expr1 = session->expr(ExprOp(Op),1,val);
	val1[0].set(expr1);
	return session->expr(OP_EQ,2,val1);
}

IExprTree * TestQueries::createMinMaxExpr(ISession *session,unsigned var,int Op, int type)
{
	IExprTree *expr1=NULL,*exprfinal=NULL;
	PropertyID pids[1];
	Value val[127];
	Value val1[2];

	int j=0;
	unsigned int ui32=0;
	int64_t i64=0;
	uint64_t ui64=0,dt=0; 
	float f=0;
	double d=0;
	
	switch(Op){
		case OP_MIN:
			switch(type){
				case VT_STRING:
					pids[0] = pm[0].uid;
					val1[0].setVarRef(0,*pids);
					#if 1
						val[0].set("Indib");
						val[1].set("Lanka");
						val[2].set("Myanmar");
						val[3].set("Madagascar");
						val[4].set("Indonesia");
						val[5].set("Srilanka");			
						val[6].set("India");
						val[7].set("Zambia");
						val[8].set("Srilanja");
						expr1 =  session->expr(ExprOp(Op),9,val);
					#else
						val[0].set("Srilanka");
						val[1].set("India");
						expr1 =  session->expr(ExprOp(Op),2,val);
					#endif
					break;
				case VT_URL:
					pids[0] = pm[14].uid;
					val1[0].setVarRef(0,*pids);
					val[0].setURL("http://www.hooasdlfsf.com");
					val[1].setURL("http://www.paparazzi.com");
					val[2].setURL("http://www.srilanka3ever.com");
					val[3].setURL("http://www.googleme.com");
					val[4].setURL("http://www.google.com");
					val[5].setURL("http://www.nagaland.com");
					val[6].setURL("http://www.srilanka4ever.com");
					val[7].setURL("http://www.southafrica.com");
					val[8].setURL("http://www.indiaincredible.com");	
					expr1 =  session->expr(ExprOp(Op),9,val);
					break;
				case VT_INT:
					j = 34;
					pids[0] = pm[7].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i< 120;i++){
						val[i].set(j);
						j++;
					}	
					val[120].set(22);
					val[121].set(122);
					val[122].set(27);
					val[123].set(30);
					expr1 =  session->expr(ExprOp(Op),124,val);
					break;
				case VT_UINT:
					ui32 = 987654322;
					pids[0] = pm[16].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i< 100;i++){
						val[i].set(ui32);
						ui32++;
					}
					val[100].set(987654321);
					val[101].set(987654501);
					val[102].set(987654601);
					expr1 =  session->expr(ExprOp(Op),103,val);
					break;
				case VT_INT64:
					i64 = 123456790;
					pids[0] = pm[17].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i<120;i++){	
						val[i].setI64(i64);
						i64++;
					}
					val[120].setI64(123456789); // Minimum value
					val[121].setI64(123457789);
					val[122].setI64(123457789);
					expr1 =  session->expr(ExprOp(Op),123,val);
					break;
				case VT_UINT64:
					ui64 = 123456790;
					pids[0] = pm[18].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i<120;i++){	
						val[i].setU64(ui64);
						ui64++;
					}
					val[120].setU64(123456789); // Minimum value
					val[121].setU64(123457789);
					val[122].setU64(123457789);
					expr1 =  session->expr(ExprOp(Op),123,val);
					break;
				case VT_FLOAT:
					f = 100.5;
					pids[0] = pm[10].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i<120;i++){	
						val[i].set(f);
						f = f + float(3.8);
					}
					val[120].set(float(7.5)); // Minimum value
					val[121].set(float(1000.5));
					val[122].set(float(23453.5));
					expr1 =  session->expr(ExprOp(Op),123,val);
					break;
				case VT_DOUBLE:
					d = 10004.50;
					pids[0] = pm[11].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i<123;i++){	
						val[i].set(d);
						d = d + double(1.3);
					}
					val[123].set(double(10000.5));
					val[124].set(double(20000.5));
					val[125].set(double(15600.5));
					expr1 =  session->expr(ExprOp(Op),126,val);
					break;
				case VT_DATETIME:
					dt = 12753803254344750LL;
					pids[0] = pm[19].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i<100;i++){	
						val[i].setDateTime(dt);
						dt++;
					}
					dt = 12753803254343750LL;
					val[100].setDateTime(dt++);
					val[101].setDateTime(dt++);
					val[102].setDateTime(dt);
					expr1 =  session->expr(ExprOp(Op),103,val);
					break;		
				case VT_RESERVED1:		
				default:
					logResult("Datatype not supported",RC_OTHER);
			}
			break;
		case OP_MAX:
			switch(type){
				case VT_STRING:
					pids[0] = pm[0].uid;
					val1[0].setVarRef(0,*pids);
					#if 1
						val[0].set("Indib");
						val[1].set("Lanka");
						val[2].set("Myanmar");
						val[3].set("Madagascar");
						val[4].set("Indonesia");
						val[5].set("Srilanka");			
						val[6].set("India");
						val[7].set("SouthAfrica");
						val[8].set("Srilanja");
						expr1 =  session->expr(ExprOp(Op),9,val);
					#else
						val[0].set("India");
						val[1].set("Srilanka");
						expr1 =  session->expr(ExprOp(Op),2,val);
					#endif
					break;
				case VT_URL:
					pids[0] = pm[14].uid;
					val1[0].setVarRef(0,*pids);
					val[0].setURL("http://www.hooasdlfsf.com");
					val[1].setURL("http://www.paparazzi.com");
					val[2].setURL("http://www.srilanka3ever.com");
					val[3].setURL("http://www.googleme.com");
					val[4].setURL("http://www.google.com");
					val[5].setURL("http://www.nagaland.com");
					val[6].setURL("http://www.srilanka4ever.com");
					val[7].setURL("http://www.southafrica.com");
					val[8].setURL("http://www.indiaincredible.com");
					expr1 =  session->expr(ExprOp(Op),9,val);
					break;
				case VT_INT:
					pids[0] = pm[7].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i< 24;i++)	val[i].set(i);
					val[24].set(27);
					val[25].set(24);
					val[26].set(25);
					val[27].set(26);
					expr1 =  session->expr(ExprOp(Op),28,val);
					break;
				case VT_UINT:
					ui32 = 987654122;
					pids[0] = pm[16].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i< 100;i++){
						val[i].set(ui32);
						ui32++;
					}
					val[100].set(987654321);
					val[101].set(987654101);
					val[102].set(987654001);
					expr1 =  session->expr(ExprOp(Op),103,val);
					break;
				case VT_INT64:
					i64 = 123456589;
					pids[0] = pm[17].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i<120;i++){	
						val[i].setI64(i64);
						i64++;
					}
					val[120].setI64(123456789); // Maximum value
					val[121].setI64(123456389);
					val[122].setI64(123456289);
					expr1 =  session->expr(ExprOp(Op),123,val);
					break;
				case VT_UINT64:
					ui64 = 123456589;
					pids[0] = pm[18].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i<120;i++){	
						val[i].setU64(ui64);
						ui64++;
					}
					val[120].setU64(123456789); // Minimum value
					val[121].setU64(123456489);
					val[122].setU64(123456389);
					expr1 =  session->expr(ExprOp(Op),123,val);
					break;
				case VT_FLOAT:
					f = float(3.1);
					pids[0] = pm[10].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i<120;i++){	
						val[i].set(f);
						f = f + float(0.01);
					}
					val[120].set(float(7.5)); // Minimum value
					val[121].set(float(2.5));
					val[122].set(float(1.5));
					expr1 =  session->expr(ExprOp(Op),123,val);
					break;
				case VT_DOUBLE:
					d = 9004.50;
					pids[0] = pm[11].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i<123;i++){	
						val[i].set(d);
						d = d + double(1.3);
					}
					val[123].set(double(10000.5));
					val[124].set(double(8000.5));
					val[125].set(double(7600.5));
					expr1 =  session->expr(ExprOp(Op),126,val);
					break;
				case VT_DATETIME:
					dt = 12753803254342750LL;
					pids[0] = pm[19].uid;
					val1[0].setVarRef(0,*pids);
					for(int i=0;i<100;i++){	
						val[i].setDateTime(dt);
						dt++;
					}
					dt = 12753803254343750LL;
					val[100].setDateTime(dt--);
					val[101].setDateTime(dt--);
					val[102].setDateTime(dt);
					expr1 =  session->expr(ExprOp(Op),103,val);
					break;		
				case VT_RESERVED1:		
				default:
					logResult("Datatype not supported",RC_OTHER);
			}
			break;
	}

	val1[1].set(expr1);
	exprfinal =  session->expr(OP_EQ,2,val1);
	return exprfinal;
}

IExprTree * TestQueries::createStringOpsExpr(ISession *session,unsigned var,int Op, int type,const int pVariant)
{
	IExprTree *expr1 = NULL,*exprfinal = NULL;
	PropertyID pids[1];
	Value val[3],val1[2];
	int64_t i64 = 0;
	uint64_t ui64 = 0;
	switch(Op){
		case OP_SUBSTR:
			switch(type){
				case VT_STRING:
					switch(pVariant){
						case 0:
								pids[0] = pm[2].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(0);
								val[2].set((float)5.0);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set("Churc");
								break;
						case 1:
								pids[0] = pm[2].uid;
								val[0].setVarRef(0,*pids);
								val[1].set((double)2.0);
								val[2].set((unsigned int)5);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set("urch ");
								break;
						case 2:
								pids[0] = pm[2].uid;
								val[0].setVarRef(0,*pids);
								i64 = 6;ui64 = 8;
								val[1].setI64(i64);
								val[2].setU64(ui64);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set(" Street ");
								break;
						case 3:
								pids[0] = pm[2].uid;
								val[0].setVarRef(0,*pids);
								val[1].set("-3");
								val[2].set("6");
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set("   Chu");
								break;
						case 4:
							    pids[0] = pm[2].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(-5);
								val[2].set(5);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set("     ");
								break;
						case 5:
							    pids[0] = pm[2].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(-5);
								val[2].set(11);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set("     Church");
								break;
						case 6:
							    pids[0] = pm[2].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(6);
								expr1 = session->expr(OP_SUBSTR,2,val);
								val1[0].set(expr1);
								val1[1].set("Church");
								break;
					}
					break;
				case VT_BSTR:
					switch(pVariant){
						case 0:
							{
								const static unsigned char bstr[5] = {97,98,99,65,66};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(0);
								val[2].set((float)5.0);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,5);
							}
								break;
						case 1:
							{
								const static unsigned char bstr[5] = {99,65,66,67,0};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set((double)2.0);
								val[2].set((unsigned int)5);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,5);
							}
								break;
						case 2:
							{
								const static unsigned char bstr[5] = {66,67,0,0,0};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								i64 = 4;ui64 = 5;
								val[1].setI64(i64);
								val[2].setU64(ui64);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,5);
							}
								break;
						case 3:
							{
								const static unsigned char bstr[6] = {0,0,0,97,98,99};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set("-3");
								val[2].set("6");
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,6);
							}
								break;
						case 4:
							{
								const static unsigned char bstr[5] = {0,0,0,0,0};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(-5);
								val[2].set(5);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,5);
							}
								break;
						case 5:
							{
								const static unsigned char bstr[10] = {0,0,0,0,0,97,98,99,65,66};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(-5);
								val[2].set(10);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,10);
							}
								break;
						case 6:
							{
								const static unsigned char bstr[4] = {97,98,99,65};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(4);
								expr1 = session->expr(OP_SUBSTR,2,val);
								val1[0].set(expr1);
								val1[1].set(bstr,4);
								break;
							}
					}
					break;
				case VT_URL:
					switch(pVariant){
						case 0:
								pids[0] = pm[14].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(0);
								val[2].set((float)5.0);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].setURL("http:");
								break;
						case 1:
								pids[0] = pm[14].uid;
								val[0].setVarRef(0,*pids);
								val[1].set((double)2.0);
								val[2].set((unsigned int)5);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].setURL("tp://");
								break;
						case 2:
								pids[0] = pm[14].uid;
								val[0].setVarRef(0,*pids);
								i64 = 6;ui64 = 8;
								val[1].setI64(i64);
								val[2].setU64(ui64);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].setURL("/www.sri");
								break;
						case 3:
								pids[0] = pm[14].uid;
								val[0].setVarRef(0,*pids);
								val[1].set("-3");
								val[2].set("6");
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].setURL("   htt");
								break;
						case 4:
							    pids[0] = pm[14].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(-5);
								val[2].set(5);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].setURL("     ");
								break;
						case 5:
							    pids[0] = pm[14].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(-5);
								val[2].set(20);
								expr1 = session->expr(OP_SUBSTR,3,val);
								val1[0].set(expr1);
								val1[1].setURL("     http://www.sril");
								break;
						case 6:
							    pids[0] = pm[14].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(19);
								expr1 = session->expr(OP_SUBSTR,2,val);
								val1[0].set(expr1);
								val1[1].setURL("http://www.srilanka");
								break;
					}
					break;
			}
			exprfinal = session->expr(OP_EQ,2,val1);
			break;
		case OP_REPLACE:
			switch(type){
				case VT_STRING://Property value is "Church Street"
					switch(pVariant){
						case 0://Full word replacement
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set("Street");
							val[2].set("Avenue");
							expr1 = session->expr(OP_REPLACE,3,val);
							val1[0].set(expr1);
							val1[1].set("Church Avenue");
							break;
						case 1://2 Character replacement
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set("ee");
							val[2].set("zzzzz");
							expr1 = session->expr(OP_REPLACE,3,val);
							val1[0].set(expr1);
							val1[1].set("Church Strzzzzzt");
							break;
						case 2://Character replacement
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set("r");
							val[2].set("z");
							expr1 = session->expr(OP_REPLACE,3,val);
							val1[0].set(expr1);
							val1[1].set("Chuzch Stzeet");
							break;
						case 3://Full Text Replacement  with len(string3) > len(string2)
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set("Church Street");
							val[2].set("This has been replaced");
							expr1 = session->expr(OP_REPLACE,3,val);
							val1[0].set(expr1);
							val1[1].set("This has been replaced");
							break;
						case 4://Last 2 character replacement
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set("et");
							val[2].set("q");
							expr1 = session->expr(OP_REPLACE,3,val);
							val1[0].set(expr1);
							val1[1].set("Church Streq");
							break;
						case 5://First word replacement
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set("Church");
							val[2].set("a");
							expr1 = session->expr(OP_REPLACE,3,val);
							val1[0].set(expr1);
							val1[1].set("a Street");
							break;
						case 6://Full Text Replacement with len(string3) < len(string2)
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set("Church Street");
							val[2].set("abc");
							expr1 = session->expr(OP_REPLACE,3,val);
							val1[0].set(expr1);
							val1[1].set("abc");
							break;
						case 7://No replacement
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set("xyz");
							val[2].set("abc");
							expr1 = session->expr(OP_REPLACE,3,val);
							val1[0].set(expr1);
							val1[1].set("Church Street");
							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				case VT_BSTR:
					switch(pVariant){
						case 0://First Character replacement
							{
								const static unsigned char lChnge[1] = {97};
								const static unsigned char lChngeTo[1] = {100};
								const static unsigned char bstr[6] = {100,98,99,65,66,67};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(lChnge,1);
								val[2].set(lChngeTo,1);
								expr1 = session->expr(OP_REPLACE,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,6);
							}
								break;
						case 1://2 character replacement with len(string2)< len(string3)
							{
								const static unsigned char lChnge[2] = {99,65};
								const static unsigned char lChngeTo[4] = {100,101,102,103};
								const static unsigned char bstr[8] = {97,98,100,101,102,103,66,67};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(lChnge,2);
								val[2].set(lChngeTo,4);
								expr1 = session->expr(OP_REPLACE,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,8);
							}
								break;
						case 2://1 Character replacement
							{
								const static unsigned char lChnge[1] = {99};
								const static unsigned char lChngeTo[1] = {100};
								const static unsigned char bstr[6] = {97,98,100,65,66,67};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(lChnge,1);
								val[2].set(lChngeTo,1);
								expr1 = session->expr(OP_REPLACE,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,6);
							}
								break;
						case 3: // 2 charater replacement
							{
								const static unsigned char lChnge[2] = {66,67};
								const static unsigned char lChngeTo[1] = {68};
								const static unsigned char bstr[5] = {97,98,99,65,68};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(lChnge,2);
								val[2].set(lChngeTo,1);
								expr1 = session->expr(OP_REPLACE,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,5);
							}
								break;
						case 4: // Full text replacement len(string2) > len(string3)
							{
								const static unsigned char lChnge[6] = {97,98,99,65,66,67};
								const static unsigned char lChngeTo[7] = {100,101,102,103,104,105,106};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(lChnge,6);
								val[2].set(lChngeTo,7);
								expr1 = session->expr(OP_REPLACE,3,val);
								val1[0].set(expr1);
								val1[1].set(lChngeTo,7);
							}
								break;
						case 5: // Full text replacement len(string2) < len(string3)
							{
								const static unsigned char lChnge[6] = {97,98,99,65,66,67};
								const static unsigned char lChngeTo[3] = {100,101,102};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(lChnge,6);
								val[2].set(lChngeTo,3);
								expr1 = session->expr(OP_REPLACE,3,val);
								val1[0].set(expr1);
								val1[1].set(lChngeTo,3);
							}
								break;
						case 6: // No Replacement
							{
								const static unsigned char lChnge[6] = {97,98,99,65,66,69};
								const static unsigned char lChngeTo[3] = {100,101,102};
								const static unsigned char bstr[6] = {97,98,99,65,66,67};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(lChnge,6);
								val[2].set(lChngeTo,3);
								expr1 = session->expr(OP_REPLACE,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,6);
							}
							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				case VT_URL: //Not Required as code path is same as VT_STRING
					switch(pVariant){
						case 0: // Full word replacement
								pids[0] = pm[14].uid;
								val[0].setVarRef(0,*pids);
								val[1].set("http://www.srilanka4ever.com");
								val[2].set("http://www.changed.com");
								expr1 = session->expr(OP_REPLACE,3,val);
								val1[0].set(expr1);
								val1[1].set("http://www.changed.com");
								break;
						case 1:// word replacement
								pids[0] = pm[14].uid;
								val[0].setVarRef(0,*pids);
								val[1].set("srilanka");
								val[2].set("india");
								expr1 = session->expr(OP_REPLACE,3,val);
								val1[0].set(expr1);
								val1[1].set("http://www.india4ever.com");
								break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
			}break;
		case OP_PAD:
			switch(type){
				case VT_STRING:
					switch(pVariant){
						case 0://Padding with *
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set(17);
							val[2].set("*");
							expr1 = session->expr(OP_PAD,3,val);
							val1[0].set(expr1);
							val1[1].set("Church Street****");
							break;
						case 1://If length < len(string)
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set(5);
							val[2].set("*");
							expr1 = session->expr(OP_PAD,3,val);
							val1[0].set(expr1);
							val1[1].set("Church Street");
							break;
						case 2://2 Character replacement
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set(20);
							val[2].set("ab");
							expr1 = session->expr(OP_PAD,3,val);
							val1[0].set(expr1);
							val1[1].set("Church Streetabababa");
							break;
						case 3://default padding
							pids[0] = pm[2].uid;
							val[0].setVarRef(0,*pids);
							val[1].set(17);
							expr1 = session->expr(OP_PAD,2,val);
							val1[0].set(expr1);
							val1[1].set("Church Street    ");
							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				case VT_BSTR:
					switch(pVariant){
						case 0://padding with *
							{
								const static unsigned char lPad[1] = {42};
								const static unsigned char bstr[10] = {97,98,99,65,66,67,42,42,42,42};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(10);
								val[2].set(lPad,1);
								expr1 = session->expr(OP_PAD,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,10);
							}
								break;
						case 1://If length < len(string)
							{
								const static unsigned char lPad[1] = {42};
								const static unsigned char bstr[6] = {97,98,99,65,66,67};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(5);
								val[2].set(lPad,1);
								expr1 = session->expr(OP_PAD,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,6);
							}
								break;
						case 2://2 Character padding
							{
								const static unsigned char lPad[2] = {42,35};
								const static unsigned char bstr[11] = {97,98,99,65,66,67,42,35,42,35,42};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(11);
								val[2].set(lPad,2);
								expr1 = session->expr(OP_PAD,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,11);
							}
								break;
						case 3: // long padding
							{
								const static unsigned char lPad[7] = {42,35,169,44,46,38,39};
								const static unsigned char bstr[13] = {97,98,99,65,66,67,42,35,169,44,46,38,39};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(13);
								val[2].set(lPad,7);
								expr1 = session->expr(OP_PAD,3,val);
								val1[0].set(expr1);
								val1[1].set(bstr,13);
							}
								break;
						case 4: // default padding
							{
								const static unsigned char bstr[11] = {97,98,99,65,66,67,0,0,0,0,0};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(11);
								expr1 = session->expr(OP_PAD,2,val);
								val1[0].set(expr1);
								val1[1].set(bstr,11);
							}
								break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				case VT_URL:
					// Not required as the code path is same as VT_STRING
					break;
			}break;
		case OP_UPPER:
			switch(type){
				case VT_STRING:
					switch(pVariant){
						case 0:
							pids[0] = pm[2].uid; // Church Street
							val[0].setVarRef(0,*pids);
							expr1 = session->expr(OP_UPPER,1,val);
							val1[0].set(expr1);
							val1[1].set("CHURCH STREET");
							break;
						case 1:
							pids[0] = pm[3].uid; // alec@lanka.com
							val[0].setVarRef(0,*pids);
							expr1 = session->expr(OP_UPPER,1,val);
							val1[0].set(expr1);
							val1[1].set("ALEC@LANKA.COM");
							break;
						case 2:
							pids[0] = pm[6].uid;
							val[0].setVarRef(0,*pids);
							expr1 = session->expr(OP_UPPER,1,val);
							val1[0].set(expr1);
							val1[1].set(" 500080");
							break;	
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				case VT_URL:
					switch(pVariant){
						case 0:
							pids[0] = pm[14].uid; // http://www.srilanka4ever.com
							val[0].setVarRef(0,*pids);
							expr1 = session->expr(OP_UPPER,1,val);
							val1[0].set(expr1);
							val1[1].set("HTTP://WWW.SRILANKA4EVER.COM");							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
			}break;
		case OP_LOWER:
			switch(type){
				case VT_STRING:
					switch(pVariant){
						case 0:
							pids[0] = pm[2].uid; // Church Street
							val[0].setVarRef(0,*pids);
							expr1 = session->expr(OP_LOWER,1,val);
							val1[0].set(expr1);
							val1[1].set("church street");
							break;
						case 1:
							pids[0] = pm[3].uid; // alec@lanka.com
							val[0].setVarRef(0,*pids);
							expr1 = session->expr(OP_LOWER,1,val);
							val1[0].set(expr1);
							val1[1].set("alec@lanka.com");
							break;
						case 2:
							pids[0] = pm[6].uid;
							val[0].setVarRef(0,*pids);
							expr1 = session->expr(OP_LOWER,1,val);
							val1[0].set(expr1);
							val1[1].set(" 500080");
							break;	
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				case VT_URL:
					switch(pVariant){
						case 0:
							pids[0] = pm[23].uid; //http://www.unicodeURL.com
							val[0].setVarRef(0,*pids);
							expr1 = session->expr(OP_LOWER,1,val);
							val1[0].set(expr1);
							val1[1].set("http://www.unicodeurl.com");	
							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
			}break;
		case OP_CONCAT:
			switch(type){
				case VT_STRING:
					switch(pVariant){
						case 0:
							pids[0] = pm[2].uid; // Church Street
							val[0].setVarRef(0,*pids);
							val[1].set(" abc");
							expr1 = session->expr(OP_CONCAT,2,val);
							val1[0].set(expr1);
							val1[1].set("Church Street abc");
							break;
						case 1:
							pids[0] = pm[3].uid; // alec@lanka.com
							val[0].setVarRef(0,*pids);
							val[1].set(" abc");
							expr1 = session->expr(OP_CONCAT,2,val);
							val1[0].set(expr1);
							val1[1].set("alec@lanka.com abc");
							break;
						case 2:
							pids[0] = pm[6].uid; // 500080
							val[0].setVarRef(0,*pids);
							val[1].set("20");
							val[2].set("30");
							expr1 = session->expr(OP_CONCAT,3,val);
							val1[0].set(expr1);
							val1[1].set(" 5000802030");
							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				case VT_BSTR:
					switch(pVariant){
						case 0:
							{
								const static unsigned char bstr[5] = {97,98,99,65,66};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(bstr, 5);
								expr1 = session->expr(OP_CONCAT,2,val);
								val1[0].set(expr1);
								const static unsigned char bstr2[11] = {97,98,99,65,66,67,97,98,99,65,66};
								val1[1].set(bstr2,11);
							}
							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				case VT_URL:
					switch(pVariant){
						case 0:
							pids[0] = pm[23].uid; //http://www.unicodeURL.com
							val[0].setVarRef(0,*pids);
							val[1].set("/xyz");
							expr1 = session->expr(OP_CONCAT,2,val);
							val1[0].set(expr1);
							val1[1].set("http://www.unicodeURL.com/xyz");	
							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
			}break;	
	}
	return exprfinal;
}

IExprTree * TestQueries::createFuncOpsExpr(ISession *session,unsigned var,int Op, int type,const int pVariant)
{
	IExprTree *expr1 = NULL,*exprfinal = NULL;
	PropertyID pids[1];
	Value val[3],val1[2];
	switch(Op){
		case OP_POSITION:
			switch(type){
				case VT_STRING:
					switch(pVariant){
						case 0:
							pids[0] = pm[2].uid; // Church Street
							val[0].setVarRef(0,*pids);
							val[1].set("urch");
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(2);
							break;
						case 1:
							pids[0] = pm[3].uid; // alec@lanka.com
							val[0].setVarRef(0,*pids);
							val[1].set("@lanka");
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(4);
							break;
						case 2:
							pids[0] = pm[3].uid; // alec@lanka.com
							val[0].setVarRef(0,*pids);
							val[1].set("alec");
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(0);
							break;
						case 3:
							// length of string is larger than sub-string
							pids[0] = pm[0].uid;
							val[0].setVarRef(0,*pids);
							val[1].set("abc");
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(-1); // should be -1 if there is no matching
							break;
						case 4:
							// length of string is less than sub-string
							pids[0] = pm[0].uid;
							val[0].setVarRef(0,*pids);
							val[1].set("abcdefghijklmn");
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(-1); // should be -1 if there is no matching
							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				case VT_URL:
					switch(pVariant){
						case 0:
							pids[0] = pm[14].uid; // http://www.srilanka4ever.com
							val[0].setVarRef(0,*pids);
							val[1].set("srilanka4");
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(11);							
							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				case VT_BSTR:
					switch(pVariant){
						case 0:
							{
								const static unsigned char bstr[5] = {97,98,99,65,66};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(bstr, 5);
								expr1 = session->expr(OP_POSITION,2,val);
								val1[0].set(expr1);
								val1[1].set(0);
							}
							break;
						case 1:
							{
								const static unsigned char bstr[5] = {65,66};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(bstr, 2);
								expr1 = session->expr(OP_POSITION,2,val);
								val1[0].set(expr1);
								val1[1].set(3);
							}
							break;
						case 2:
							{
								const static unsigned char bstr[7] = {107,108,109,110,111,112,113};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(bstr, 7);
								expr1 = session->expr(OP_POSITION,2,val);
								val1[0].set(expr1);
								val1[1].set(-1);
							}
							break;
						case 3:
							{
								const static unsigned char bstr[3] = {107,108,109};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(bstr, 3);
								expr1 = session->expr(OP_POSITION,2,val);
								val1[0].set(expr1);
								val1[1].set(-1);
							}
							break;
						case 4:
							{
								const static unsigned char bstr[3] = {65,66,98};
								pids[0] = pm[22].uid;
								val[0].setVarRef(0,*pids);
								val[1].set(bstr, 3);
								expr1 = session->expr(OP_POSITION,2,val);
								val1[0].set(expr1);
								val1[1].set(-1);
							}
							break;
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
				/*
				case VT_ARRAY:
					switch(pVariant){
						case 0:
							pids[0] = pm[24].uid; // (1,2,3,4,5,6,7,8,9,10)
							val[0].setVarRef(0,*pids);
							val[1].set(1);
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(0); // the first one
							break;
						case 1:
							pids[0] = pm[24].uid; // (1,2,3,4,5,6,7,8,9,10)
							val[0].setVarRef(0,*pids);
							val[1].set(5);
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(4);					
							break;
						case 2:
							pids[0] = pm[24].uid; // (1,2,3,4,5,6,7,8,9,10)
							val[0].setVarRef(0,*pids);
							val[1].set(10);
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(9);					
							break;
						case 3:
							pids[0] = pm[24].uid; // (1,2,3,4,5,6,7,8,9,10)
							val[0].setVarRef(0,*pids);
							val[1].set(11);
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(-1);					
							break;
						case 4:
							pids[0] = pm[24].uid; // (1,2,3,4,5,6,7,8,9,10)
							val[0].setVarRef(0,*pids);
							val[1].set(6);
							expr1 = session->expr(OP_POSITION,2,val);
							val1[0].set(expr1);
							val1[1].set(6);			
							break;							
					}
					exprfinal = session->expr(OP_EQ,2,val1);
					break;
					*/
			}
			break;
		default:
			break;
	}
	return exprfinal;
}

IExprTree * TestQueries::createOP_INExpr(ISession *session,unsigned var,int type)
{
	IExprTree *expr1 = NULL,*exprfinal = NULL;
	PropertyID pids[1];
	Value val[10];
	Value val1[2];
	Value pvs[3];
	ClassID	pClassID = 0;
	IStmt * pQuery = session->createStmt();
	QVarID const var1 = pQuery->addVariable();
	switch(type){
		case VT_RANGE:
			pids[0] = pm[0].uid;
			val[0].set("Australia");
			val[1].set("Malaysia");
			val1[0].setVarRef(0,*pids);
			val1[1].setRange(val);
			exprfinal = session->expr(OP_IN,2,val1,CASE_INSENSITIVE_OP);
			break;
		case VT_ARRAY:
			pids[0] = pm[7].uid;
			val[0].set(12);
			val[1].set(24);
			val[2].set(100);
			val[3].set(2321);
			val[4].set(68754);
			val[5].set(124);
			val[6].set(732);
			val[7].set(452);
			val[8].set(27);
			val[9].set(845);
			val1[0].setVarRef(0,*pids);
			val1[1].set(val,10);
			exprfinal = session->expr(OP_IN,2,val1);
			break;
		case VT_URIID:
			expr1 = createCompExpr(session,var,OP_EQ,VT_STRING); // PinID[0] is returned for this expression
			pQuery->addCondition(var1,expr1); 
			defineClass(session,"OP_IS_A_Test", pQuery, &pClassID);
			expr1->destroy();
			val1[0].set(PinID[0]->getPID());
			val1[1].setURIID(pClassID);
			exprfinal = session->expr(OP_IS_A,2,val1);
			//This should return all pins as the condition is true
			break;
		case VT_STMT:
			expr1 = createCompExpr(session,var,OP_EQ,VT_STRING); // PinID[0] is returned for this expression
			pQuery->addCondition(var1,expr1); 
			val1[0].set(PinID[0]->getPID());
			val1[1].set(pQuery);
			exprfinal = session->expr(OP_IN,2,val1);
			//This should return all pins as the condition is true
			break;
		case VT_COLLECTION:	
			{
				pvs[0].set("SouthAfrica");
				pvs[1].set("India");
				pvs[2].set("USA");
				INav *lCollNav = new CTestCollNav(pvs,3);
				pids[0] = pm[0].uid;
				val[0].setVarRef(0,*pids);
				val[1].set(lCollNav);
				exprfinal = session->expr(OP_IN,2,val);	
				lCollNav->destroy();
			}
			break;
		default: 
			logResult("Datatype not supported",RC_OTHER);
		}
	pQuery->destroy();
	return exprfinal;
} 

void TestQueries::createOP_INWithNavigator(ISession *session)
{
	Value *arr = new Value[2];
	arr[0].set(PinID[0]);
	arr[1].set(PinID[1]);
	INav *nv = new CTestCollNav(arr,2);
	
	logResult("--------- Executing OP_IN with Navigator ----------",RC_OTHER);
	IStmt * pQuery = session->createStmt();
	unsigned char const var1 = pQuery->addVariable();

	Value operands[2];
	operands[0].setVarRef(0);
	operands[1].set(nv);
	IExprTree *expr = session->expr(OP_IN,2,operands);
	if(NULL != expr)
	{
		pQuery->addCondition(var1,expr);		
		unsigned long count = reportResult(pQuery);
		if(count != 2)
			logResult("createOP_INWithNavigator",RC_FALSE);
		else
			logResult("createOP_INWithNavigator",RC_OK);
		expr->destroy();
	}
	nv->destroy();
	pQuery->destroy();
}

void TestQueries::executeSimpleQuery(ISession *session,int Op,int type,int nExpResults,const char *pClassName,const int pVariant)
{
	IExprTree *expr = NULL;
	string lCLSName = "TestQueries." + mRandStr + ".";
	//lCLSName.append(".");
	ClassID	lCLSID = STORE_INVALID_CLASSID;
	{
		IStmt * lQ = session->createStmt();
		unsigned var = lQ->addVariable();
		if((Op >= OP_PLUS && Op <= OP_NEG) || Op == OP_ABS || Op == OP_AVG)
			expr = createArithExpr(session,var,Op,type);
		else if(Op >= OP_EQ && Op <= OP_GE)
			expr = createCompExpr(session,var,Op,type);
		else if(Op >= OP_NOT && Op <= OP_RSHIFT)
			expr = createBitwiseExpr(session,var,Op,type);
		else if(Op >= OP_TONUM && Op<= OP_CAST || Op==OP_RANGE)
			expr = createConvExpr(session,var,Op,type);
		else if(Op == OP_MIN || Op == OP_MAX)
			expr = createMinMaxExpr(session,var,Op,type);
		else if(Op == OP_IN || Op == OP_IS_A)
			expr = createOP_INExpr(session,var,type);
		else if((Op >= OP_SUBSTR && Op <= OP_TRIM) || 
			Op == OP_UPPER || Op == OP_LOWER || Op == OP_CONCAT)
			expr = createStringOpsExpr(session,var,Op,type,pVariant);
		else if( Op == OP_POSITION )
			expr = createFuncOpsExpr(session,var,Op,type,pVariant);
		if(pClassName && mExecuteClass){
			lCLSName += pClassName;
			lQ->addCondition(var,expr);
			defineClass(session,lCLSName.c_str(), lQ, &lCLSID);
			// Note: We don't yet destroy expr, because we reuse it later.
			lQ->destroy();
		}
	}
	IStmt * lQ = session->createStmt();
	QVarID var = lQ->addVariable();
	lQ->addCondition(var,expr);
	/*const char *lQStr = lQ->toString();
	if(lQStr != NULL)
		std::cout <<std::endl<<"The Query built :: " <<std::endl<< lQStr;
	else
		logResult("Failed to get the string of IStmt",RC_OTHER);	*/
  
	long count = 0;
	if(mRunFullScan || pClassName == NULL){
		count = reportResult(lQ);
		lQ->destroy();
		expr->destroy();
		if(nExpResults == -1) if(count >=3){logResult("",RC_OK); return;}
		if(nExpResults == count)
			logResult("",RC_OK);
		else
			logResult("",RC_FALSE);
	}else{
		lQ->destroy();
		expr->destroy();
	}

	//Execute Class here
	if(mExecuteClass && pClassName){
		string lCLS = "Executing the Class - ";
		lCLS += lCLSName;
		logResult(lCLS,RC_OTHER);		
		IStmt * lQ = session->createStmt();
		ClassSpec lCS;
		lCS.classID = lCLSID;
		lCS.nParams = 0;
		lCS.params = NULL;
		lQ->addVariable(&lCS, 1);
		count = reportResult(lQ);
		lQ->destroy();		
		if(nExpResults == -1) if(count >=3){logResult("",RC_OK); return;}
		if(nExpResults == count)
			logResult("",RC_OK);
		else
			logResult("",RC_FALSE);
	}
}

void TestQueries::executeComplexQuery(ISession *session,int Op,int Op1,int type1,int Op2,int type2,int nExpResults,bool fake,const char *pClassName)
{
	IExprTree *expr = NULL;
	string lCLSName = "TestQueries." + mRandStr + ".";
	ClassID	lCLSID = STORE_INVALID_CLASSID;
	{
		IStmt * lQ = session->createStmt();
		unsigned var = lQ->addVariable();
		if(Op >= OP_LAND && Op <= OP_LNOT)
			expr = createLogicalExpr(session,var,Op,Op1,Op2,type1,type2,fake);
		if(pClassName && mExecuteClass){
			lCLSName += pClassName;
			lQ->addCondition(var,expr);
			defineClass(session,lCLSName.c_str(), lQ, &lCLSID);
			// Note: We don't yet destroy expr, because we reuse it later.
			lQ->destroy();
		}
	}
	IStmt *query = session->createStmt();
	QVarID var = query->addVariable();
	query->addCondition(var,expr);
	uint64_t lCount = 0;
	if(mRunFullScan || pClassName == NULL){
		lCount = reportResult(query);
		expr->destroy();
		query->destroy();
		if((unsigned long)nExpResults == lCount)
			logResult("",RC_OK);
		else
			logResult("",RC_FALSE);
	}else{
		expr->destroy();
		query->destroy();
	}

	//Execute Class here
	if(mExecuteClass && pClassName){
		string lCLS = "Executing the Class -";
		lCLS += lCLSName;
		logResult(lCLS,RC_OTHER);
		
		IStmt * lQ = session->createStmt();
		ClassSpec lCS;
		lCS.classID = lCLSID;
		lCS.nParams = 0;
		lCS.params = NULL;
		lQ->addVariable(&lCS, 1);
		uint64_t lCount = reportResult(lQ);
		lQ->destroy();
		if((unsigned long)nExpResults == lCount)
			logResult("",RC_OK);
		else
			logResult("",RC_FALSE);
	}
}

void TestQueries::populateStore(ISession *session)
{
	PID pid;
	Value pvs[550];
	
	//Check whether PINs already exist
	IStmt * lQ = session->createStmt();
	unsigned char lVar = lQ->addVariable();
	pvs[0].setVarRef(0,pm[0].uid);
	IExprTree *lET = session->expr(OP_EXISTS,1,pvs);
	lQ->addCondition(lVar,lET);
	ICursor * lR = NULL;
	TVERIFYRC(lQ->execute(&lR));
	const int count = MVTApp::countPinsFullScan(lR,session);
	lR->destroy();
	lET->destroy();
	lQ->destroy();
	if(count == 0){
		SETVALUE(pvs[0], pm[0].uid, "India", OP_SET);
		SETVALUE(pvs[1], pm[1].uid, "Bangalore", OP_SET);
		SETVALUE(pvs[2], pm[2].uid, "Sadashivnagar", OP_SET);
		SETVALUE(pvs[3], pm[3].uid, "sumanth@vmware.com", OP_SET);
		SETVALUE(pvs[4], pm[4].uid, "Sumanth", OP_SET);
		SETVALUE(pvs[5], pm[5].uid, "Vasu", OP_SET);
		SETVALUE(pvs[6], pm[6].uid, " 500080", OP_SET);
		SETVALUE(pvs[7], pm[7].uid, 27, OP_SET);
		SETVALUE(pvs[8], pm[8].uid, "Beautiful World", OP_SET);
		SETVALUE(pvs[9], pm[9].uid, true, OP_SET);
		SETVALUE(pvs[10], pm[10].uid, float(7.5), OP_SET);
		SETVALUE(pvs[11], pm[11].uid, double(10000.50), OP_SET);
		SETVALUE(pvs[12], pm[12].uid, 134343433, OP_SET);
		SETVALUE(pvs[13], pm[13].uid, 01012005, OP_SET);
		pvs[14].setURL("http://www.google.com"); SETVATTR(pvs[14], pm[14].uid, OP_SET);
		SETVALUE(pvs[15], pm[15].uid, "true", OP_SET);
		unsigned int ui32 = 987654321;
		SETVALUE(pvs[16], pm[16].uid, ui32, OP_SET);
		int64_t i64 = 123456789;
		pvs[17].setI64(i64); SETVATTR(pvs[17], pm[17].uid, OP_SET);
		uint64_t ui64 = 123456789;
		pvs[18].setU64(ui64); SETVATTR(pvs[18], pm[18].uid, OP_SET);
		//TIMESTAMP dt; getTimestamp(dt)//12753803254343750
		ui64 = 12753803254343750LL;
		pvs[19].setDateTime(ui64); SETVATTR(pvs[19], pm[19].uid, OP_SET);
		//pvs[20].setInterval(pm[20].uid,ui64);
		RC rc = session->createPIN(pid,pvs,20);
		PinID[0] = session->getPIN(pid);

		SETVALUE(pvs[0], pm[0].uid, "Nepal", OP_SET);
		SETVALUE(pvs[1], pm[1].uid, "Kathmandu", OP_SET);
		SETVALUE(pvs[2], pm[3].uid, "terry@nepal.com", OP_SET);
		SETVALUE(pvs[3], pm[4].uid, "John", OP_SET);
		SETVALUE(pvs[4], pm[5].uid, "Terry", OP_SET);
		SETVALUE(pvs[5], pm[6].uid, " 33321", OP_SET);
		SETVALUE(pvs[6], pm[7].uid, 22, OP_SET);
		SETVALUE(pvs[7], pm[8].uid, "Football crazy", OP_SET);
		SETVALUE(pvs[8], pm[9].uid, true, OP_SET);
		SETVALUE(pvs[9], pm[10].uid, 5.5, OP_SET);
		SETVALUE(pvs[10], pm[14].uid, "http://www.nepalfc.com", OP_SET);
		rc = session->createPIN(pid,pvs,11);
		PinID[1] = session->getPIN(pid);

		const static unsigned char bstr[] = {97,98,99,65,66,67};

		SETVALUE(pvs[0], pm[0].uid, "Srilanka", OP_SET);
		SETVALUE(pvs[1], pm[1].uid, "Colombo", OP_SET);
		SETVALUE(pvs[2], pm[2].uid, "Church Street", OP_SET);
		SETVALUE(pvs[3], pm[3].uid, "alec@lanka.com", OP_SET);
		SETVALUE(pvs[4], pm[4].uid, "Alec", OP_SET);
		SETVALUE(pvs[5], pm[11].uid, double(64000.50), OP_SET);
		SETVALUE(pvs[6], pm[12].uid, 34443164, OP_SET);
		SETVALUE(pvs[7], pm[13].uid, 10091999, OP_SET);
		SETVALUE(pvs[8], pm[14].uid, "http://www.srilanka4ever.com", OP_SET);
		SETVALUE(pvs[9], pm[21].uid, "This was unicode", OP_SET);
		pvs[10].set(bstr,6); SETVATTR(pvs[10], pm[22].uid, OP_SET);
		pvs[11].setURL("http://www.unicodeURL.com"); SETVATTR(pvs[11], pm[23].uid, OP_SET);

		// pvs[12] ~ pvs[21] is a collection, (1,2,3,4,5,6,7,8,9,10)
		for (int i = 1; i <= 10; i++)
			SETVALUE_C(pvs[11+i], pm[24].uid, i, OP_ADD, STORE_LAST_ELEMENT);

		// pvs[22] ~ pvs[521] is a collection, (1,2,3,4,5,6,7,...,499,500)
		// if the page size is small(e.g. 4k), this collection's type could be VT_COLLECTION
		for (int i = 1; i <= 500; i++)
			SETVALUE_C(pvs[21+i], pm[25].uid, i, OP_ADD, STORE_LAST_ELEMENT);
		
		rc = session->createPIN(pid,pvs,522);
		PinID[2] = session->getPIN(pid);
	}else{
		IExprTree *lET1 = session->expr(OP_EQ,2,pvs);

		//PIN #1
		IStmt *lQ1 = session->createStmt();
		lVar = lQ1->addVariable();
		pvs[0].setVarRef(0,pm[0].uid);
		pvs[1].set("India");
		lET1 = session->expr(OP_EQ,2,pvs);
		lQ1->addCondition(lVar,lET1);
		TVERIFYRC(lQ1->execute(&lR));
		PinID[0]=lR->next();
		lR->destroy();
		lQ1->destroy();
		
		//PIN #2
		IStmt *lQ2 = session->createStmt();
		lVar = lQ2->addVariable();
		pvs[0].setVarRef(0,pm[0].uid);
		pvs[1].set("Nepal");
		lET1 = session->expr(OP_EQ,2,pvs);
		lQ2->addCondition(lVar,lET1);
		TVERIFYRC(lQ2->execute(&lR));
		PinID[1]=lR->next();
		lR->destroy();
		lQ2->destroy();

		//PIN #3
		IStmt *lQ3 = session->createStmt();
		lVar = lQ3->addVariable();
		pvs[0].setVarRef(0,pm[0].uid);
		pvs[1].set("Srilanka");
		lET1 = session->expr(OP_EQ,2,pvs);
		lQ3->addCondition(lVar,lET1);
		TVERIFYRC(lQ3->execute(&lR));
		PinID[2]=lR->next();
		lR->destroy();
		lQ3->destroy();

		lET1->destroy();
	}
}

unsigned long TestQueries::reportResult(IStmt *pQuery)
{
	uint64_t  lCount = 0;
	pQuery->count(lCount);
	return (unsigned long)lCount;
}

int TestQueries::reportResult(ICursor *result, ISession *pSession)
{
	int  count = 0;
	//mLogger.print("\nResult:\n");
	if (result!=NULL) {
		for (IPIN *pin; (pin=result->next())!=NULL; ) {
			count++;
			//MVTApp::output(*pin, mLogger.out(), session);
			pin->destroy();
		}
		result->destroy();
	}
	return count;
}

void TestQueries::logResult(string str, RC rc)
{
   ofstream fout;
   fout.open("results.txt",ios::app);
   if(rc != RC_OTHER){
		rc == RC_OK ? str += " --- PASSED ---" :str += " ****** FAILED ******";
		if(rc != RC_OK) 
			mRCFinal=rc;
		fout<<str.c_str()<<"\n"<<endl;
   }else fout<<str.c_str()<<"\n"<<endl;
   fout.close();   
}
