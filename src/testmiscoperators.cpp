/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

// Publish this test.
class TestMiscOperators : public ITest
{
	public:
		RC mRCFinal;
		URIMap pm[6];
		TEST_DECLARE(TestMiscOperators);
		virtual char const * getName() const { return "testmiscoperators"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of OP_BEGINS / OP_ENDS / OP_CONTAINS "; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		//any other missing operators will be added here.
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testOPContains(ISession *session);
		void testOPBegins(ISession *session);
		void testOPEnds(ISession *session);
};

TEST_IMPLEMENT(TestMiscOperators, TestLogger::kDStdOut);

// Implement this test.
int TestMiscOperators::execute()
{
	mRCFinal = RC_FALSE;
	if (MVTApp::startStore())
	{
		mRCFinal=RC_OK;
		ISession * const session = MVTApp::startSession();
		/*
		memset(pm,0,5*sizeof(URIMap));
		pm[0].URI="Prop1";pm[0].uid=STORE_INVALID_URIID;
		pm[1].URI="Prop2";pm[1].uid=STORE_INVALID_URIID;
		pm[2].URI="Prop3";pm[2].uid=STORE_INVALID_URIID;
		pm[3].URI="Prop4";pm[3].uid=STORE_INVALID_URIID;
		pm[4].URI="Prop5";pm[4].uid=STORE_INVALID_URIID;
		session->mapURIs(5,pm);
		*/
		MVTApp::mapURIs(session,"TestMiscOperators.prop",6,pm);

		testOPContains(session);
		testOPBegins(session);
		testOPEnds(session);
		session->terminate();
		MVTApp::stopStore();
	}

 	return mRCFinal;
}
#define lAllClassNotifs (CLASS_NOTIFY_JOIN | CLASS_NOTIFY_LEAVE | CLASS_NOTIFY_CHANGE | CLASS_NOTIFY_DELETE | CLASS_NOTIFY_NEW)

void TestMiscOperators::testOPContains(ISession *session)
{
	Tstring str,str1,str2,str3;
	Value val[6];
	PropertyID pids[1];
	Value args[2];
	uint64_t cnt=0;

	IPIN *pin=session->createPIN();
	MVTRand::getString(str,15,0,false);
	val[0].set(str.c_str());val[0].setPropID(pm[0].uid);
	MVTRand::getString(str1,10,0,false);
	val[1].set(str1.c_str());val[1].setPropID(pm[1].uid);
	pin->modify(val,2);
	RC rc = session->commitPINs(&pin,1);
	pin->destroy();

	//case 1: OP_CONTAINS
	IStmt *query = session->createStmt();
	unsigned char var = query->addVariable();
	pids[0]=pm[1].uid;

	args[0].setVarRef(0,*pids);
	str2=str1.substr(5,3);
	args[1].set(str2.c_str());
	IExprTree *expr = session->expr(OP_CONTAINS,2,args,CASE_INSENSITIVE_OP);
	query->addCondition(var,expr);
	query->count(cnt);

	if(cnt !=1)
		mRCFinal = RC_FALSE;
	
	expr->destroy();
	query->destroy();

	//case 2: OP_CONTAINS with no case sensitive op
	pin=session->createPIN();
	MVTRand::getString(str,45,0,true,false);
	val[0].set(str.c_str());val[0].setPropID(pm[0].uid);
	MVTRand::getString(str1,30,0,false);
	val[1].set(str1.c_str());val[1].setPropID(pm[1].uid);
	pin->modify(val,2);
	rc = session->commitPINs(&pin,1);
	pin->destroy();
	
	query = session->createStmt();
	var = query->addVariable();
	pids[0]=pm[0].uid;

	args[0].setVarRef(0,*pids);
	str2="";
	str2=str.substr(15,15);
	args[1].set(str2.c_str());
	expr = session->expr(OP_CONTAINS,2,args);
	query->addCondition(var,expr);
	query->count(cnt);

	if(cnt !=1)
		mRCFinal = RC_FALSE;
	
	expr->destroy();
	query->destroy();

	//case 3: collections
	pin=session->createPIN();

	MVTRand::getString(str,15,0,true,false);
	MVTRand::getString(str1,40,0,true);
	MVTRand::getString(str2,29,0,false,false);
	MVTRand::getString(str3,19,0,true,false);

	val[0].set(str.c_str());val[0].setPropID(pm[0].uid); val[0].setOp(OP_ADD); val[0].eid=STORE_LAST_ELEMENT;
	val[1].set(str1.c_str());val[1].setPropID(pm[0].uid); val[1].setOp(OP_ADD); val[1].eid=STORE_LAST_ELEMENT;
	val[2].set(str2.c_str());val[2].setPropID(pm[0].uid); val[2].setOp(OP_ADD); val[2].eid=STORE_LAST_ELEMENT;
	val[3].set(str3.c_str());val[3].setPropID(pm[0].uid); val[3].setOp(OP_ADD); val[3].eid=STORE_LAST_ELEMENT;
	std::cout<<str<<std::endl;

	pin->modify(val,4);
	rc = session->commitPINs(&pin,1);
	pin->destroy();

	query = session->createStmt();
	var = query->addVariable();
	pids[0]=pm[0].uid;
	str2="";
	str2=str.substr(4,4);
	std::cout<<str2<<std::endl;

	args[1].set(str2.c_str());
	expr = session->expr(OP_CONTAINS,2,args);
	query->addCondition(var,expr);
	query->count(cnt);

	if(cnt !=1)
		mRCFinal = RC_FALSE;
	
	expr->destroy();
	query->destroy();

	//OP_CONTAINS with start char as case sensitive
	cnt =0;
	str="";
	MVTRand::getString(str,10,0);
	char ch = (char) 65 + rand() % 25;
	char lTmpStr[1] = {ch};
	str.insert(0,lTmpStr);
	val[0].set(str.c_str());val[0].setPropID(pm[5].uid);
	pin = session->createPIN(val,1,MODE_COPY_VALUES);
	session->commitPINs(&pin,1);

	query =  session->createStmt();
	var = query->addVariable();

	pids[0]=pm[5].uid;
	args[0].setVarRef(0,*pids);
	str1 = str.substr(0,4);
	std::transform(str1.begin()+1,str1.begin() + 2, str1.begin()+1, (int(*)(int)) tolower);
	args[1].set(str1.c_str());

	expr = session->expr(OP_CONTAINS,2,args,CASE_INSENSITIVE_OP);
	query->addCondition(var,expr);
	
	query->count(cnt);

	if(cnt != 1 )
		mRCFinal = RC_FALSE;

	query->destroy();
	expr->destroy();
}

void TestMiscOperators::testOPBegins(ISession *session)
{
	Tstring str,str1,str2;
	Value val[6];
	PropertyID pids[1];
	Value args[2];
	uint64_t cnt=0;

	IPIN *pin = session->createPIN();
	MVTRand::getString(str,30,0,false,false);
	val[0].set(str.c_str());val[0].setPropID(pm[1].uid);
	val[1].set("xyzxyz");val[1].setPropID(pm[2].uid);
	pin->modify(val,2);
	RC rc = session->commitPINs(&pin,1);
	pin->destroy();

    //case 1 : simple OP_BEGINS
	IStmt *query = session->createStmt();
	unsigned char var = query->addVariable();

	pids[0]=pm[1].uid;
	args[0].setVarRef(0,*pids);
	str2=str.substr(0,6);
	args[1].set(str2.c_str());
	IExprTree *expr = session->expr(OP_BEGINS,2,args);
	query->addCondition(var,expr);
	query->count(cnt);

	if(cnt !=1)
		mRCFinal = RC_FALSE;
	
	expr->destroy();
	query->destroy();

	//case 2: with collections.
	pin = session->createPIN();
	MVTRand::getString(str,25,0,true,false);
	val[0].set(str.c_str());val[0].setPropID(pm[0].uid);val[0].eid=STORE_FIRST_ELEMENT;val[0].op=OP_ADD;
	MVTRand::getString(str1,25,0,true,false);
	val[1].set(str1.c_str());val[1].setPropID(pm[0].uid);val[1].eid=STORE_FIRST_ELEMENT;val[1].op=OP_ADD;
	MVTRand::getString(str2,40,0,true,false);
	val[2].set(str2.c_str());val[2].setPropID(pm[0].uid);val[2].eid=STORE_FIRST_ELEMENT;val[2].op=OP_ADD;
	pin->modify(val,3);
	rc = session->commitPINs(&pin,1);

	query = session->createStmt();
	var = query->addVariable();
	pids[0]=pm[0].uid;
	args[0].setVarRef(0,*pids);
	str="";
	str=str2.substr(0,12);
	args[1].set(str.c_str());
	expr = session->expr(OP_BEGINS,2,args);
	query->addCondition(var,expr);
	query->count(cnt);

	if(cnt !=1)
		mRCFinal = RC_FALSE;

	expr->destroy();
	query->destroy();

	//case 3: OP_BEGINS in a class query.
	ClassID cls = STORE_INVALID_CLASSID;
	IStmt *classquery = session->createStmt();
	unsigned char var1  = classquery->addVariable();
	pids[0]=pm[2].uid;
	args[0].setVarRef(0,*pids);
	args[1].set("eupho");

	IExprTree *clsexpr = session->expr(OP_BEGINS,2,args);
	classquery->addCondition(var1,clsexpr);
	char lB[100];
	Tstring lStr; MVTRand::getString(lStr,10,10,false,false);
	sprintf(lB,"testmiscoperators.%s.class", lStr.c_str());
	defineClass(session,lB,classquery,&cls);
	session->enableClassNotifications(cls,lAllClassNotifs);

	classquery->destroy();
	clsexpr->destroy();

	//create pins for the above condition.
	int x;

	for (x=0;x<1000;x++){
		IPIN *clspin = session->createPIN();
		val[0].set("euphoriaic");val[0].setPropID(pm[2].uid);
		clspin->modify(val,1);
		session->commitPINs(&clspin,1);
		clspin->destroy();
	}
	SourceSpec csp;
	csp.objectID=cls;
	csp.nParams=0;
	csp.params=NULL;

	query = session->createStmt();
	var = query->addVariable(&csp,1);
	query->count(cnt);

	if(cnt <1000)
		mRCFinal = RC_FALSE;

	query->destroy();
}

void TestMiscOperators::testOPEnds(ISession *session)
{
	Tstring str,str1,str2;
	Value val[3];
	PropertyID pids[1];
	Value args[2];
	uint64_t cnt;

	//case 1: OP_ENDS
	IPIN *pin = session->createPIN();
	MVTRand::getString(str,25,0,true,false);
	val[0].set(str.c_str());val[0].setPropID(pm[1].uid);
	MVTRand::getString(str1,30,0,false,true);
	val[1].set(str1.c_str());val[1].setPropID(pm[2].uid);
	pin->modify(val,2);
	session->commitPINs(&pin,1);
	pin->destroy();

	IStmt *query = session->createStmt();
	unsigned char var = query->addVariable();
	
	pids[0]=pm[1].uid;
	args[0].setVarRef(0,*pids);
	str2 = str.substr((str.length()-10),str.length());
	args[1].set(str2.c_str());
	IExprTree *expr = session->expr(OP_ENDS,2,args);
	query->addCondition(var,expr);
	query->count(cnt);

	if (cnt != 1)
		mRCFinal = RC_FALSE;

	expr->destroy();
	query->destroy();

    //case 2: OP_ENDS with collections.
	pin = session->createPIN();
	MVTRand::getString(str,40,0,true,false);
	val[0].set(str.c_str());val[0].setPropID(pm[0].uid);val[0].eid=STORE_LAST_ELEMENT;val[0].op=OP_ADD_BEFORE;
	MVTRand::getString(str1,50,0,true,false);
	val[1].set(str1.c_str());val[1].setPropID(pm[0].uid);val[1].eid=STORE_LAST_ELEMENT;val[1].op=OP_ADD_BEFORE;
	MVTRand::getString(str2,200,0,true,false);
	val[2].set(str2.c_str());val[2].setPropID(pm[0].uid);val[2].eid=STORE_LAST_ELEMENT;val[2].op=OP_ADD_BEFORE;

	pin->modify(val,3);
	session->commitPINs(&pin,1);

	query = session->createStmt();
	var = query->addVariable();

	pids[0]=pm[0].uid;
	args[0].setVarRef(0,*pids);
	str = "";
	str = str2.substr((str2.length()-30),str2.length());
	args[1].set(str.c_str());
	expr = session->expr(OP_ENDS,2,args);
	query->addCondition(var,expr);
	query->count(cnt);

	if (cnt != 1)
		mRCFinal = RC_FALSE;

	expr->destroy();
	query->destroy();
}
