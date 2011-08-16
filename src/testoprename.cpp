/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <fstream>
#include "serialization.h"
#include "teststream.h"
using namespace std;

// Publish this test.
class TestRename : public ITest
{
	public:
		RC mRCFinal;
		TEST_DECLARE(TestRename);
		virtual char const * getName() const { return "testoprename"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test for OP_RENAME"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testrename(ISession *session);
		
		
};

#define lAllClassNotifs (CLASS_NOTIFY_JOIN | CLASS_NOTIFY_LEAVE | CLASS_NOTIFY_CHANGE | CLASS_NOTIFY_DELETE | CLASS_NOTIFY_NEW)
TEST_IMPLEMENT(TestRename, TestLogger::kDStdOut);

int TestRename::execute()
{
	mRCFinal = RC_OK;
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		testrename(session);
		session->terminate();
 		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }

 	return mRCFinal;
}
void TestRename::testrename(ISession *session)
{
	IPIN *pin;
	URIMap pm[6];
	PID pid;
	Value val[10];
	Tstring str;
	/*
	//declare a few properties
	pm[0].URI ="Name";pm[0].displayName = "Name";pm[0].uid=STORE_INVALID_PROPID;
	pm[1].URI ="Age";pm[1].displayName = "Age";pm[1].uid=STORE_INVALID_PROPID;
	pm[2].URI ="Club";pm[2].displayName = "Club";pm[2].uid=STORE_INVALID_PROPID;
	pm[3].URI ="Country";pm[3].displayName = "Country";pm[3].uid=STORE_INVALID_PROPID;
	pm[4].URI ="Position";pm[4].displayName = "Position";pm[4].uid=STORE_INVALID_PROPID;
	session->mapURIs(5,pm);
	*/
	MVTApp::mapURIs(session,"TestOpRename.testrename",6,pm);

	// case 1: simple rename from a lower prop to a higher prop.
	MVTRand::getString(str,50,0,true,true);
	val[0].set(str.c_str());val[0].setPropID(pm[0].uid);
	MVTRand::getString(str,100,0,true,true);
	val[1].set(str.c_str());val[1].setPropID(pm[1].uid);

	RC rc = session->createPIN(pid,val,2);
	pin = session->getPIN(pid);

	//rename the property 2 to property 5
	val[0].setRename(pm[1].uid,pm[5].uid);
	PropertyID propid1 = pm[5].uid;
	PropertyID propid2 = pm[1].uid;
	rc = pin->modify(val,1);

#if 1
	pin->refresh();
#endif

//Fails for this scenario too.
#if 0
	pid = pin->getPID();
	pin->destroy();
	pin = session->getPIN(pid);
#endif


	if (RC_OK != rc || NULL == pin->getValue(propid1) || strcmp(pin->getValue(propid1)->str,str.c_str())!=0 || NULL != pin->getValue(propid2))
		mRCFinal = RC_FALSE;

	pin->destroy();
	pin = NULL;

	//case 2: rename from a higher prop to a lower prop
	MVTRand::getString(str,50,0,true,true);
	val[0].set(str.c_str());val[0].setPropID(pm[1].uid);
	MVTRand::getString(str,100,0,true,true);
	val[1].set(str.c_str());val[1].setPropID(pm[3].uid);

	rc = session->createPIN(pid,val,2);
	pin = session->getPIN(pid);

	//rename the property 3 to property 1
	val[0].setRename(pm[3].uid,pm[0].uid);
	rc = pin->modify(val,1);

	if (RC_OK != rc || strcmp(pin->getValue(pm[0].uid)->str,str.c_str())!=0 || NULL != pin->getValue(pm[3].uid))
		mRCFinal = RC_FALSE;

	pin->destroy();
	pin = NULL;

	//case 3: uncommited pin and rename of property
	MVTRand::getString(str,100,0,true,true);
	val[0].set(str.c_str());val[0].setPropID(pm[2].uid);
	val[1].set(12345);val[1].setPropID(pm[0].uid);
	pin = session->createUncommittedPIN(val,2,MODE_COPY_VALUES);

	val[0].setRename(pm[2].uid,pm[1].uid);
	rc = pin->modify(val,1);
	rc = session->commitPINs(&pin,1);
	if (RC_OK != rc || strcmp(pin->getValue(pm[1].uid)->str,str.c_str())!=0 || NULL != pin->getValue(pm[2].uid))
		mRCFinal = RC_FALSE;

	pin->destroy();
	pin = NULL;

	//case 4: renaming a collection property in a PIN.
	MVTRand::getString(str,75,100,true,true);
	val[0].set(str.c_str());val[0].setPropID(pm[0].uid);
	val[0].op = OP_ADD; val[0].eid = STORE_LAST_ELEMENT;
	MVTRand::getString(str,75,100,true,true);
	val[1].set(str.c_str());val[1].setPropID(pm[0].uid);
	val[1].op = OP_ADD; val[1].eid = STORE_LAST_ELEMENT;
	rc = session->createPIN(pid,val,2);
	
	if(RC_OK == rc){
		pin = session->getPIN(pid);
		val[0].setRename(pm[0].uid,pm[1].uid);
		rc = pin->modify(val,1);
		if(RC_OK != rc || MVTApp::getCollectionLength(*pin->getValue(pm[1].uid)) !=2 || NULL != pin->getValue(pm[0].uid))
			mRCFinal = RC_FALSE;
		pin->destroy();
		pin = NULL;
	}
	//case 5: rename a pin with a stream property.
	val[0].set(MVTApp::wrapClientStream(session, new MyStream(120000)));val[0].setPropID(pm[0].uid);
	rc = session->createPIN(pid,val,1);

	if(RC_OK == rc){
		pin = session->getPIN(pid);
		val[0].setRename(pm[0].uid,pm[1].uid);
		rc = pin->modify(val,1);
		if(RC_OK != rc ||pin->getValue(pm[1].uid)->stream.is->length() != 120000 || NULL != pin->getValue(pm[0].uid))
			mRCFinal = RC_FALSE;
		pin->destroy();
		pin = NULL;
	}
	
	//case 6: testclassmembership.
	IStmt *query = session->createStmt();
	unsigned char var = query->addVariable();
	PropertyID pids[1];
	Value args[1];
	ClassID cls = STORE_INVALID_CLASSID;

	pids[0]=pm[1].uid;
	args[0].setVarRef(0,*pids);
	IExprTree *classopexistexpr = session->expr(OP_EXISTS,1,args);
	query->addCondition(var,classopexistexpr);
	char lB[100];
	Tstring lStr; MVTRand::getString(lStr,10,10,false,false);
	sprintf(lB,"test.oprename.class.%s",lStr.c_str());
	defineClass(session,lB,query, &cls);
	session->enableClassNotifications(cls,lAllClassNotifs);
	classopexistexpr->destroy();
	query->destroy();
	
	MVTRand::getString(str,100,0,true,true);
	val[0].set(str.c_str());val[0].setPropID(pm[0].uid);
	rc = session->createPIN(pid,val,1);

	if(RC_OK == rc){
		pin = session->getPIN(pid);
		val[0].setRename(pm[0].uid,pm[1].uid);
		rc = pin->modify(val,1);
		if (RC_OK != rc || !pin->testClassMembership(cls))
			mRCFinal = RC_FALSE;
		pin->destroy();
		pin = NULL;
	}
}
