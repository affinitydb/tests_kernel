/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

// This test covers pins added and removed from a class
// which will indirectly excercise the store's page locking.

// Publish this test.
class TestGetPageLock : public ITest
{
	public:
		static const int sNumProps = 2;
		PropertyID mPropIDs[sNumProps];
		RC mRCFinal;
		ClassID pagecls;
		ClassID pagecls1;

		TEST_DECLARE(TestGetPageLock);
		virtual char const * getName() const { return "testgetpagelock"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test getpage() lock issue"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testcreatemeta(ISession *session);
		void testGetPage(ISession *session);
};
TEST_IMPLEMENT(TestGetPageLock, TestLogger::kDStdOut);

// Implement this test.
int TestGetPageLock::execute()
{
	mRCFinal = RC_FALSE;
	if (MVTApp::startStore())
	{
		mRCFinal = RC_OK;
		ISession * const session = MVTApp::startSession();
		MVTApp::mapURIs(session,"TestGetPageLock.prop",sNumProps,mPropIDs);
		pagecls = STORE_INVALID_CLASSID;
		pagecls1 = STORE_INVALID_CLASSID;
		testcreatemeta(session);
		testGetPage(session);
		session->terminate();
		MVTApp::stopStore();
	}
 	return mRCFinal;
}
void TestGetPageLock::testcreatemeta(ISession *session)
{
	Tstring lClassName; MVTRand::getString(lClassName,10,10,false,false); 
	char lB[100], lB1[100]; // Names of the two classes
	sprintf(lB,"testGetPageLockClass%s.%d",lClassName.c_str(),1);
	sprintf(lB1,"testGetPageLockClass%s.%d",lClassName.c_str(),2);
	if (RC_NOTFOUND == session->getClassID(lB,pagecls)){
		// /pin[pin is image()]
		IStmt *query = session->createStmt();
		unsigned char var = query->addVariable();
		Value args[2];
		PropertyID pids[1];
		pids[0] = mPropIDs[0];
		args[0].setVarRef(0,*pids);
		args[1].set("photo");
		IExprTree *expr = session->expr(OP_CONTAINS,2,args);

		query->addCondition(var,expr);

		TVERIFYRC(defineClass(session,lB, query, &pagecls));

		expr->destroy();
		query->destroy();
	}
	if (RC_NOTFOUND == session->getClassID(lB1,pagecls1)){
		//pin[pin is image() and tag = $tagName]"
		IStmt *query = session->createStmt();
		Value args[2];
		PropertyID pids[1];

		SourceSpec csp[1];
		csp[0].objectID = pagecls; // Based on class created above
		csp[0].nParams = 0;
		csp[0].params = NULL;
		unsigned char var = query->addVariable(csp,1);

		pids[0] = mPropIDs[1];
		args[0].setVarRef(0,*pids);
		args[1].setParam(0); // parameter to fill in at the time query is executed
		IExprTree *expr = session->expr(OP_EQ,2,args);
		query->addCondition(var,expr);
		
		TVERIFYRC(defineClass(session,lB1, query, &pagecls1));

		expr->destroy();
		query->destroy();
	}
}
void TestGetPageLock::testGetPage(ISession *session)
{
	//create a pin to match this family.
	const long cntPins = 5;

	Value val[6];
	for (int i=0; i < cntPins; i++){
		val[0].set("photo/xyz");val[0].setPropID(mPropIDs[0]);
		val[1].set("tag1");val[1].setPropID(mPropIDs[1]);val[1].op = OP_ADD;val[1].eid = STORE_LAST_ELEMENT;
		val[2].set("tag2");val[2].setPropID(mPropIDs[1]);val[2].op = OP_ADD;val[2].eid = STORE_LAST_ELEMENT;
		val[3].set("tag3");val[3].setPropID(mPropIDs[1]);val[3].op = OP_ADD;val[3].eid = STORE_LAST_ELEMENT;
		val[4].set("tag4");val[4].setPropID(mPropIDs[1]);val[4].op = OP_ADD;val[4].eid = STORE_LAST_ELEMENT;
		val[5].set("tag5");val[5].setPropID(mPropIDs[1]);val[5].op = OP_ADD;val[5].eid = STORE_LAST_ELEMENT;
		TVERIFYRC(session->createPIN(val,6,NULL,MODE_PERSISTENT|MODE_COPY_VALUES));;
	}
	//query the pin and remove a particular eid
	IStmt *query = session->createStmt();
	SourceSpec csp;
	Value args[1];
	args[0].set("tag5");
	csp.objectID = pagecls1;
	csp.nParams = 1;
	csp.params = args;
	unsigned char var = query->addVariable(&csp,1);
	ICursor *result = NULL;
	query->execute(&result);
	PID pid;

	uint64_t cntQResults = 0 ;
	for (IPIN *rpin=result->next(); rpin!=NULL; rpin=result->next() ){
		if (NULL != rpin){
			pid = rpin->getPID();

			// Removing this element will mean that the pin no longer belongs to the class
			val[0].setDelete(mPropIDs[1],STORE_LAST_ELEMENT);
			val[0].setPropID(mPropIDs[1]);
			TVERIFYRC(rpin->modify(val,1));
			rpin->destroy();
		}
		else
		{
			TVERIFY(false) ;
		}
		cntQResults++ ;
	}

	TVERIFY(cntQResults == uint64_t(cntPins));

	result->destroy();
	query->destroy();

	// Get the last pin in the query and add a different results
	IPIN *pin = session->getPIN(pid);
	val[0].set("tag7");val[0].setPropID(mPropIDs[1]);val[0].op = OP_ADD;val[0].eid = STORE_LAST_ELEMENT;
	TVERIFYRC(pin->modify(val,1));
	pin->destroy();

	//  Sanity check - class query should be empty
	query = session->createStmt();
	var = query->addVariable(&csp,1);
	TVERIFYRC(query->count(cntQResults));

	TVERIFY(cntQResults == 0);
}
