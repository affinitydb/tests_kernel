/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <map>
using namespace std;

// REVIEW: This test relies a lot displaying Values to the standard output for
// manual confirmation (when VERBOSE) is enabled.  This should revisited so that 
// expected results are explicitly confirmed directly in the code.

//#define VERBOSE

// Publish this test.
class TestCollections : public ITest
{
	public:
		RC mRCFinal;
		TEST_DECLARE(TestCollections);
		virtual char const * getName() const { return "testcollections"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "thorough testing of collections"; }
		virtual bool isPerformingFullScanQueries() const { return true; } // Note: countPinsFullScan...
		virtual bool includeInPerfTest() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
			struct Elmnts {
			unsigned int curr;
			const char* str; 
		};
		Elmnts els;
	protected:
		void populateStore(ISession *session,URIMap *pm,int npm, PID *pid);
		void simpleCollection(ISession *session,URIMap *pm,int npm, PID *pid);
		void uncommitedpinCollection(ISession *session,URIMap *pm,int npm);
		void arrayCollection (ISession *session);
		void inputarrayCollection (ISession *session,URIMap *pm,int npm, PID *pid);
		void editCollection(ISession *session,URIMap *pm,int npm, PID *pid);
		void cloneCollection(ISession *session,URIMap *pm,int npm, PID *pid);
		void refreshCollection(ISession *session);
		void cnavigCollection(ISession *session);
		void streamCollection(ISession *session);
		void inputcnavigCollection(ISession *session);
		void moveOPCollection(ISession *session);
		void miscCollection(ISession *session);
		void testSyncFTIndex(ISession *session,URIMap *pm,int npm);
		void testFullScanCollection(ISession *session, URIMap *pm,int npm,PID *pid);
	protected:
		// TODO: refine/complete these and provide as service in app.h
		void logResult(string str,RC rc);
		void verifyMove(Value const &val,const map<int,Elmnts> &coll);
		int countPinsFullScan(ICursor *result,ISession *session);
};
TEST_IMPLEMENT(TestCollections, TestLogger::kDStdOut);

// Implement this test.
int TestCollections::execute()
{
	mRCFinal = RC_FALSE;
	
	if (MVTApp::startStore())
	{
		mRCFinal = RC_OK;
		ISession * const session = MVTApp::startSession();
		URIMap pm[12];
		PID pid[4];

		populateStore(session,pm,sizeof(pm)/sizeof(pm[0]),pid);
		simpleCollection(session,pm,sizeof(pm)/sizeof(pm[0]),pid);
		uncommitedpinCollection(session,pm,sizeof(pm)/sizeof(pm[0]));
		arrayCollection(session);
		editCollection(session,pm,sizeof(pm)/sizeof(pm[0]),pid);
		cloneCollection(session,pm,sizeof(pm)/sizeof(pm[0]),pid);
		refreshCollection(session);
		cnavigCollection(session);
		streamCollection(session);
		inputarrayCollection(session,pm,sizeof(pm)/sizeof(pm[0]),pid);
		inputcnavigCollection(session);
		moveOPCollection(session);
		miscCollection(session);
		testFullScanCollection(session,pm,sizeof(pm)/sizeof(pm[0]),pid);
		#if 0
			// Note to Sumanth: This test does not always work, probably when rerun multiple times against the same store (didn't check in detail)...
			testSyncFTIndex(session,pm,sizeof(pm)/sizeof(pm[0]));
		#endif
		if(RC_OK!=session->deletePINs(pid,4,MODE_PURGE))
			mLogger.out()<<"Failed to delete the PINs."<<std::endl;
		session->terminate();
		MVTApp::stopStore();
	}

	return mRCFinal;
}

void TestCollections::populateStore(ISession *session,URIMap *pm,int npm, PID *pid)
{
	// TODO: randomize this + provide as a service in app.h...
	/*
	memset(pm,0,npm*sizeof(URIMap));
	pm[0].URI="TestCollections.address";
	pm[1].URI="TestCollections.city";
	pm[2].URI="TestCollections.street";
	pm[3].URI="TestCollections.email";
	pm[4].URI="TestCollections.firstname";
	pm[5].URI="TestCollections.lastname";
	pm[6].URI="TestCollections.pincode";
	pm[7].URI="TestCollections.age";
	pm[8].URI="TestCollections.somestring";
	pm[9].URI="TestCollections.num1";
	pm[10].URI="TestCollections.num2";
	pm[11].URI="TestCollections.num3";
	session->mapURIs(npm,pm);
	*/
	MVTApp::mapURIs(session,"TestCollections.props",12,pm);

	Value pvs[11];
	IPIN *pin;
	SETVALUE(pvs[0], pm[1].uid, "Bangalore", OP_SET);
	SETVALUE(pvs[1], pm[4].uid, "Mark", OP_SET);
	SETVALUE(pvs[2], pm[5].uid, "Venguerov", OP_SET);
	SETVALUE(pvs[3], pm[6].uid, "500006", OP_SET);
	SETVALUE(pvs[4], pm[7].uid, 61, OP_SET);
	SETVALUE(pvs[5], pm[8].uid, "!@#$% World", OP_SET);
	SETVALUE(pvs[6], pm[9].uid, 0, OP_SET);
	pvs[7].set("http://www.dhkkan.org"); SETVATTR(pvs[7], pm[3].uid, OP_SET);
	SETVALUE(pvs[8], pm[10].uid, 250, OP_SET);
	SETVALUE(pvs[9], pm[11].uid, 12, OP_SET);
	session->createPIN(pvs,10,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
	pid[0] = pin->getPID();
	
	SETVALUE(pvs[0], pm[1].uid, "Mumbai", OP_SET);
	SETVALUE(pvs[1], pm[4].uid, "Yasir", OP_SET);
	SETVALUE(pvs[2], pm[5].uid, "Mohammad", OP_SET);
	SETVALUE(pvs[3], pm[6].uid, "800005", OP_SET);
	SETVALUE(pvs[4], pm[7].uid, 59, OP_SET);
	SETVALUE(pvs[5], pm[8].uid, "this is a string", OP_SET);
	SETVALUE(pvs[6], pm[9].uid, 75, OP_SET);
	SETVALUE(pvs[7], pm[3].uid, "Yasir@vmware.com", OP_SET);
	session->createPIN(pvs,8,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
	pid[1] = pin->getPID();

	SETVALUE(pvs[0], pm[1].uid, "Delhi", OP_SET);
	SETVALUE(pvs[1], pm[4].uid, "Harsh", OP_SET);
	SETVALUE(pvs[2], pm[5].uid, "Raju", OP_SET);
	SETVALUE(pvs[3], pm[6].uid, "100011", OP_SET);
	SETVALUE(pvs[4], pm[7].uid, 65, OP_SET);
	SETVALUE(pvs[5], pm[8].uid, "This is a test string", OP_SET);
	session->createPIN(pvs,6,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
	pid[2] = pin->getPID();

	//collection pins
	pvs[0].set("element 1");pvs[0].setPropID(pm[0].uid);pvs[0].op=OP_ADD;pvs[0].eid=STORE_COLLECTION_ID;
	pvs[1].set("element 2");pvs[1].setPropID(pm[0].uid);pvs[1].op=OP_ADD;pvs[1].eid=STORE_COLLECTION_ID;
	pvs[2].set("element 3");pvs[2].setPropID(pm[0].uid);pvs[2].op=OP_ADD;pvs[2].eid=STORE_COLLECTION_ID;
	pvs[3].set("element 4");pvs[3].setPropID(pm[0].uid);pvs[3].op=OP_ADD;pvs[2].eid=STORE_COLLECTION_ID;
	pvs[4].set("element 5");pvs[4].setPropID(pm[1].uid);pvs[4].op=OP_ADD_BEFORE;pvs[4].eid=STORE_FIRST_ELEMENT;
	pvs[5].set("element 6");pvs[5].setPropID(pm[1].uid);pvs[5].op=OP_ADD_BEFORE;pvs[5].eid=STORE_FIRST_ELEMENT;
	pvs[6].set("element 7");pvs[6].setPropID(pm[1].uid);pvs[6].op=OP_ADD_BEFORE;pvs[6].eid=STORE_FIRST_ELEMENT;

	TVERIFYRC(session->createPIN(pvs,7,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	pid[3] = pin->getPID();
}

void TestCollections::simpleCollection(ISession *session,URIMap *pm,int npm, PID *pid)
{
	mLogger.out() << "simpleCollection" << std::endl;

	IPIN *pin;
	Value pv;
	RC rc;
	
	//case OP_ADD
	//case 1: modify a pin by adding a collection element at the end.
	pin = session->getPIN(pid[0]);
	pv.set("http://choo.org");
	SETVATTR_C(pv, pm[3].uid, OP_ADD, STORE_LAST_ELEMENT);

	rc = pin->modify(&pv,1);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(pm[3].uid), mLogger.out()); mLogger.out() << std::endl;
#endif
	logResult ("****Case1 ",rc);
	pin->destroy();

	//case 2: modify a pin by adding a collection element after the first element.
	//add another element for granularity sake

	pin = session->getPIN(pid[1]);
	SETVALUE_C(pv, pm[1].uid, "Dummy elemnt", OP_ADD, STORE_LAST_ELEMENT);
	rc = pin->modify(&pv,1);

	SETVALUE_C(pv, pm[1].uid, "This is a test for adding a simple collection - VT_STRING", OP_ADD, STORE_FIRST_ELEMENT);
	rc = pin->modify(&pv,1);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(pm[1].uid), mLogger.out()); mLogger.out() << std::endl;
#endif
	logResult ("****Case2 ",rc);

	//OP_ADD_BEFORE
	//case 3:  modify a pin by adding a collection element before the last element.
	pin->refresh(); //after interface mode change
	SETVALUE_C(pv, pm[1].uid, "XXXXXXXXXXXXXXXXXXXX", OP_ADD_BEFORE, STORE_LAST_ELEMENT);

	rc = pin->modify(&pv,1);
	logResult ("****Case3 ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(pm[1].uid), mLogger.out()); mLogger.out() << std::endl;
#endif
	pin->destroy();

	//case 4: modify a pin by adding a collection element after the last element.
	//pin = session->getPIN(pid[1]);
	pin = session->getPIN(pid[1]);
	SETVALUE_C(pv, pm[1].uid, "Dummy elemnt1", OP_ADD, STORE_LAST_ELEMENT);
	rc = pin->modify(&pv,1);
	logResult ("****Case4 ",rc);

	SETVALUE_C(pv, pm[1].uid, "XXXXXXXXXXXXXXXXXXXX", OP_ADD_BEFORE, STORE_FIRST_ELEMENT);
	rc = pin->modify(&pv,1);
	logResult ("****Case5 ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(pm[1].uid), mLogger.out()); mLogger.out() << std::endl;
#endif
	pin->destroy();

	//TODO implement the OP_MOVE or OP_MOVE_BEFORE when implemented.
	//TODO add same cases for all datatypes and random data.
}

void TestCollections::uncommitedpinCollection(ISession *session,URIMap *pm,int npm)
{
	mLogger.out() << "uncommitedpinCollection" << std::endl;

	IPIN * pin;
	RC rc;

	Value pv;
	PropertyID propID = pm[0].uid;

	SETVALUE(pv, propID, "uncommited pin", OP_ADD);
	// = pin->modify(&pv,1);
	TVERIFYRC(session->createPIN(&pv,1,&pin,MODE_COPY_VALUES|MODE_PERSISTENT));
	//logResult ("****Case6(1) ",rc);

	//modify the same uncommited pin by adding more elements
	SETVALUE_C(pv, propID, "uncommited pin1", OP_ADD, STORE_COLLECTION_ID);
	rc = pin->modify(&pv,1);
	logResult ("****Case6(2) ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif

	SETVALUE_C(pv, propID, "uncommited pin2", OP_ADD_BEFORE, STORE_FIRST_ELEMENT);
	rc = pin->modify(&pv,1);
	logResult ("****Case6(3) ",rc);

	SETVALUE_C(pv, propID, "uncommited pin3", OP_ADD_BEFORE, STORE_LAST_ELEMENT);
	rc = pin->modify(&pv,1);
	logResult ("****Case6(4) ",rc);

	SETVALUE_C(pv, propID, "uncommited pin4", OP_ADD, STORE_FIRST_ELEMENT);
	rc = pin->modify(&pv,1);
	logResult ("****Case6(5) ",rc);

	SETVALUE_C(pv, propID, "uncommited pin5", OP_ADD, STORE_LAST_ELEMENT);
	rc = pin->modify(&pv,1);
	logResult ("****Case6 ",rc);
	
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif	

	SETVALUE_C(pv, propID, "Commited pin5", OP_ADD, STORE_LAST_ELEMENT);
	rc = pin->modify(&pv,1);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	logResult ("****Case6(7) ",rc);
	pin->destroy();

	//check uncommited pin with property map.
	Value pvs[3];
	SETVALUE(pvs[0], pm[1].uid, "Bangalore", OP_SET);
	SETVALUE(pvs[1], pm[4].uid, "Mark", OP_SET);
	SETVALUE(pvs[2], pm[5].uid, "Venguerov", OP_SET);
	TVERIFYRC( session->createPIN(pvs,3,&pin,MODE_COPY_VALUES|MODE_PERSISTENT));

	SETVALUE_C(pv, pm[5].uid, "uncommited value", OP_ADD, STORE_LAST_ELEMENT);
	rc = pin->modify(&pv,1);
	logResult ("****Case8 ",rc);

#ifdef VERBOSE
	MVTApp::output(*pin->getValue(pm[5].uid), mLogger.out()); mLogger.out() << std::endl;
#endif
	pin->destroy();
}

void TestCollections::arrayCollection(ISession *session)
{
	mLogger.out() << "arrayCollection" << std::endl;

	IPIN * pin;
	RC rc;

	Value pvc[4];
	PropertyID propID =MVTApp::getProp(session,"TestCollections::arrayCollection");

	SETVALUE_C(pvc[0], propID, "uncommitedprop1", OP_ADD, STORE_LAST_ELEMENT);

	//add all after last element
	SETVALUE_C(pvc[1], propID, "uncommitedprop2", OP_ADD, STORE_LAST_ELEMENT);
	SETVALUE_C(pvc[2], propID, "uncommitedprop3", OP_ADD, STORE_LAST_ELEMENT);
	SETVALUE_C(pvc[3], propID, "uncommitedprop4", OP_ADD, STORE_LAST_ELEMENT);
	rc = session->createPIN(pvc, 4,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
	logResult ("****Case9 Pin Commit ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	pin->destroy();

	//case2: Add/Add before elements interchangably in the array.
	SETVALUE_C(pvc[0], propID, "el1", OP_ADD, STORE_LAST_ELEMENT);
	SETVALUE_C(pvc[1], propID, "el2", OP_ADD, STORE_FIRST_ELEMENT);
	SETVALUE_C(pvc[2], propID, "elbeforefirst", OP_ADD_BEFORE, STORE_FIRST_ELEMENT);
	SETVALUE_C(pvc[3], propID, "elbeforelast", OP_ADD_BEFORE, STORE_LAST_ELEMENT);
	rc = session->createPIN(pvc, 4,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
	logResult ("****Case10 Pin Commit ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	pin->destroy();
}

void TestCollections::editCollection(ISession *session,URIMap *pm,int npm, PID *pid)
{
	mLogger.out() << "editCollection" << std::endl;

	IPIN *pin;
	Value pvs[5];
	Value const *pv;
	Value pvl[10];
	RC rc;
	PropertyID propID =pm[3].uid;

	pin = session->getPIN(pid[0]);

	SETVALUE_C(pvs[0], pm[1].uid, "AddElem 1", OP_ADD, STORE_COLLECTION_ID);
	rc = pin->modify(pvs,1);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(pm[1].uid), mLogger.out()); mLogger.out() << std::endl;
#endif

	//edit property el 2.
	pv = pin->getValue(pm[1].uid);
	SETVALUE_C(pvs[0], pm[1].uid, "After edit", OP_SET, pv->varray[1].eid);
	rc = pin->modify(pvs,1);
	logResult("****Case 11 ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(pm[1].uid), mLogger.out()); mLogger.out() << std::endl;
#endif
	pin->destroy();

	//test the above update with uncommited pins too.
	string str = "uncommitedprop";
	for (int i =0; i < 10; i++) {
		SETVALUE_C(pvl[i], propID, "uncommitedprop", OP_ADD, STORE_LAST_ELEMENT);
	} 
	
	session->createPIN(pvl, 10,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	//modify the collection at a random index and commit.
	pv = pin->getValue(propID);
	SETVALUE_C(pvs[0], pm[1].uid, "Modified", OP_SET, pv->varray[rand()%10].eid);
	rc = pin->modify(pvs,1);
	logResult("****Case 12 ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif

	//Delete a known el
	pv = pin->getValue(propID);
	pvs[0].setDelete(propID,pv->varray[1].eid);
	rc = pin->modify(pvs,1);
	logResult("****Case 13 ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif

	//Delete at a random element
	pv = pin->getValue(propID);
	pvs[0].setDelete(propID,pv->varray[rand()%10].eid);

	rc = pin->modify(pvs,1);
	logResult("****Case 14 ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	pin->destroy();
}

void TestCollections::cloneCollection(ISession *session,URIMap *pm,int npm, PID *pid)
{
	mLogger.out() << "cloneCollection" << std::endl;

	IPIN *pin, *pin1;
	RC rc;
	Value pvs[10];
	PropertyID propID = pm[4].uid;

	pin = session->getPIN(pid[1]);
	//add elements to the collection, clone it.
	//after this add/up/del more elements to this clone.

	SETVALUE_C(pvs[0], pm[1].uid, "fields of gold", OP_ADD, STORE_LAST_ELEMENT);
	SETVALUE_C(pvs[1], pm[1].uid, "Sting", OP_ADD, STORE_LAST_ELEMENT);

	rc = pin->modify(pvs,2);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(pm[1].uid), mLogger.out()); mLogger.out() << std::endl;
#endif
	memset(pvs,0,2*sizeof(Value));

	pin1 = pin->clone();

	SETVALUE_C(pvs[0], pm[1].uid, "on the clone", OP_ADD, STORE_LAST_ELEMENT);
	rc = pin1->modify(pvs,1);
	
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(pm[1].uid), mLogger.out()); mLogger.out() << std::endl;
	MVTApp::output(*pin1->getValue(pm[1].uid), mLogger.out()); mLogger.out() << std::endl;
#endif
	logResult("****Case 15(clone2) ",rc);
	pin->destroy();

	//check cloning with uncmmited pins
	memset(pvs,0,10*sizeof(Value));
	for (int i =0; i < 10; i++) {
		SETVALUE_C(pvs[i], propID, "uncommitedprop", OP_ADD_BEFORE, STORE_LAST_ELEMENT);
	} 
	session->createPIN(pvs, 10, &pin,MODE_PERSISTENT|MODE_COPY_VALUES);

	pin->clone()->destroy();
	memset(pvs,0,10*sizeof(Value));
	for (int i =0; i < 10; i++) {
		SETVALUE_C(pvs[i], propID, "cloneprop", OP_ADD_BEFORE, STORE_LAST_ELEMENT);
		rc = pin->modify(pvs,1);
	} 
	logResult("****Case 15(clone2) ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	pin->destroy();
	pin1->destroy();
}

void TestCollections::refreshCollection(ISession *session)
{
	mLogger.out() << "refreshCollection" << std::endl;

	IPIN *pin; //, *pin1;
	//ISession *session1 = MVTApp::startSession();
	PropertyID lPropIDs[1];
	MVTApp::mapURIs(session,"TestCollections.refreshCollection",1,lPropIDs);
	PropertyID propID = lPropIDs[0];
	RC rc;
	Value pvs[5];

	//session1->setInterfaceMode(session1->getInterfaceMode() | ITF_COLLECTIONS_AS_ARRAYS);
	//refresh with multiple session and uncommited pin collection

	for (int i =0; i < 5; i++) {
		SETVALUE_C(pvs[i], propID, "refresh", OP_ADD_BEFORE, STORE_LAST_ELEMENT);
	} 
	rc = session->createPIN(pvs, 5, &pin,MODE_PERSISTENT|MODE_COPY_VALUES);
	PID const lPID = pin->getPID();
	pin->destroy();
	pin = session->getPIN(lPID);
	//do a refresh to make sure you have latest version
	pin->refresh();
	//now add collection elements
	memset(pvs,0,1*sizeof(Value));
	for (int i =0; i < 5; i++) {
		SETVALUE_C(pvs[0], propID, "added collection with pin1", OP_ADD, STORE_LAST_ELEMENT);
		rc = pin->modify(pvs,1);
	} 
	logResult("****Case 16(refresh) ",rc);

	//MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
	//refreshing the original pin
	//session->setInterfaceMode(session->getInterfaceMode() | ITF_COLLECTIONS_AS_ARRAYS);
	//pin->refresh();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	//it should have never come here
	//todo: update and delete.
	pin->destroy();
}

void TestCollections::cnavigCollection(ISession *session)
{
	mLogger.out() << "cnavigCollection" << std::endl;

	//create a url collection. 
	IPIN *pin;
	PropertyID lPropIDs[1];
	MVTApp::mapURIs(session,"TestCollections.cnavigCollection",1,lPropIDs);
	PropertyID propID = lPropIDs[0];
	//PropertyID propID = 2170;
	RC rc;
	Value pvs[5];
	Value const *pv;

	string finStr;
	for (int i=0; i<5;i++){
		finStr = "" ;
		char *buff = new char[3];
		finStr += "http://su.org";
		sprintf(buff,"%d",i);
		finStr += buff;
		delete[] buff;

		pvs[i].set(finStr.c_str());
		SETVATTR_C(pvs[i], propID, OP_ADD, STORE_LAST_ELEMENT);
	} 
	rc = session->createPIN(pvs, 5,&pin, MODE_PERSISTENT|MODE_COPY_VALUES);
	pin->refresh();

#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	//use go_last, go_first, go_next, go_findbyid for edit operations.
	pv = pin->getValue(propID);
	if (pv->type == VT_COLLECTION) {
		if (pv->isNav())
		{
			pvs[0].set("http://max.org");
			SETVATTR_C(pvs[0], propID, OP_ADD, pv->nav->navigate(GO_LAST)->eid);

			pvs[1].set("http://mark.org");
			SETVATTR_C(pvs[1], propID, OP_ADD, pv->nav->navigate(GO_PREVIOUS)->eid);

			pvs[2].set("http://yasir.org");
			SETVATTR_C(pvs[2], propID, OP_ADD, pv->nav->navigate(GO_NEXT)->eid);

			pvs[3].set("http://shivam.org");
			SETVATTR_C(pvs[3], propID, OP_ADD, pv->nav->navigate(GO_FIRST)->eid);
		} else {
			pvs[0].set("http://max.org");
			SETVATTR_C(pvs[0], propID, OP_ADD, pv->varray[pv->length-1].eid);

			pvs[1].set("http://mark.org");
			SETVATTR_C(pvs[1], propID, OP_ADD, pv->varray[pv->length-2].eid);

			pvs[2].set("http://yasir.org");
			SETVATTR_C(pvs[2], propID, OP_ADD, pv->varray[pv->length-1].eid);

			pvs[3].set("http://shivam.org");
			SETVATTR_C(pvs[3], propID, OP_ADD, pv->varray[0].eid);
		}
	} else TVERIFYRC(RC_TYPE);

	rc = pin->modify(pvs,4);
	pin->refresh();//forcing to be CNAVIG why????
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	//also order also seems messed up.
	//todo: after clarify, do for update and del also.
	pin->destroy();
}

void TestCollections::inputarrayCollection (ISession *session,URIMap *pm,int npm, PID *pid)
{
	mLogger.out() << "inputarrayCollection" << std::endl;

	Value *val = new Value[4];
	IPIN *pin;
	Value pvs[4];
	PropertyID propID =pm[4].uid;

	val[0].set("a");
	val[1].set("ac");
	val[2].set("acc");
	
	pvs[0].set(val,3);
	SETVATTR_C(pvs[0], propID, OP_ADD, STORE_LAST_ELEMENT);

	RC rc =  session->createPIN(pvs,1,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);

	delete[] val;
	memset(pvs,0,1*sizeof(Value));
		
	//modify the same collection.
	val = new Value[2];
	val[0].set("accd");

	pvs[0].set(val,1);
	SETVATTR_C(pvs[0], propID, OP_ADD_BEFORE, STORE_LAST_ELEMENT);

	rc = pin->modify(pvs,1); 
	pin->refresh();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	delete[] val;
	//modify the above collection to add an large array of elements
	val = new Value[5];
	/*string finStr;
	for (int j=0; j<5;j++){
		finStr = "" ;
		char *buff = new char[3];
		finStr += "adding element ";
		sprintf(buff,"%d",j);
		finStr += buff;
		val[j].set(finStr.c_str());
		
		
	}*/
	val[0].set("adding element 0");
	val[1].set("adding element 1");
	val[2].set("adding element 2");
	val[3].set("adding element 3");
	val[4].set("adding element 4");

	pvs[0].set(val,5);
	SETVATTR_C(pvs[0], propID, OP_ADD, STORE_LAST_ELEMENT);
	rc = pin->modify(pvs,1);

	pin->refresh();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	
	delete[] val;
	
	val = new Value[2];
	val[0].set("Changed element 1");
	val[1].set("Changed element 2");

	pvs[0].set(val,1);
	SETVATTR_C(pvs[0], propID, OP_SET, val[0].varray[1].eid);
	rc = pin->modify(pvs,1);

	pin->refresh();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	pin->destroy();
}

void TestCollections::inputcnavigCollection(ISession *session)
{
	mLogger.out() << "inputcnavigCollection" << std::endl;

	INav *colnav;
	IPIN *pin;
	PropertyID lPropIDs[1];
	MVTApp::mapURIs(session,"TestCollections.inputcnavigCollection",1,lPropIDs);
	PropertyID propID = lPropIDs[0];
	//PropertyID propID = 2171;
	RC rc;
	Value pvs[2];
	Value const *pv;

	SETVALUE_C(pvs[0], propID, "add0", OP_ADD, STORE_LAST_ELEMENT);
	SETVALUE_C(pvs[1], propID, "add1", OP_ADD, STORE_LAST_ELEMENT);

	rc =  session->createPIN(pvs,2,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
	pin->refresh();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	
	pv = pin->getValue(propID);
	if (pv->type==VT_COLLECTION) {
		if (pv->isNav()) {
			SETVALUE_C(pvs[0], propID, pv->nav, OP_ADD, STORE_LAST_ELEMENT);
		} else {
			pvs[0].set((Value*)pv->varray,pv->length); SETVATTR_C(pvs[0], propID, OP_ADD, STORE_LAST_ELEMENT);
		}
	} else TVERIFYRC(RC_TYPE);

	rc = session->createPIN(pvs,1,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
	pin->refresh();
	
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif

	pv = pin->getValue(propID);
	//modify using CNavig coll of a given element
	if (pv->type==VT_COLLECTION) {
		if (pv->isNav()) {
			colnav = pv->nav;
			colnav->navigate(GO_FINDBYID,2);
			SETVALUE_C(pvs[0], propID, colnav, OP_ADD, STORE_LAST_ELEMENT);
			rc = pin->modify(pvs,1);
		} else {
			// ??? 
		}
	} else
		TVERIFYRC(RC_TYPE);
	
	pin->refresh();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
#endif
	pin->destroy();
}
void TestCollections::streamCollection(ISession *session)
{		
	//Notes: my page size is 32k.
}
void TestCollections::testSyncFTIndex(ISession *session,URIMap *pm,int npm)
{
	mLogger.out() << "testSyncFTIndex" << std::endl;

	IPIN * pin;
	RC rc;
	Value pvs[3],pv;
	SETVALUE(pvs[0], pm[1].uid, "Honalulu", OP_SET);
	SETVALUE(pvs[1], pm[4].uid, "Axle", OP_SET);
	SETVALUE(pvs[2], pm[5].uid, "Slash", OP_SET);
	TVERIFYRC(session->createPIN(pvs,3,&pin,MODE_COPY_VALUES|MODE_PERSISTENT));

	//Element = 1
	SETVALUE_C(pv, pm[5].uid, "Rose", OP_ADD, STORE_LAST_ELEMENT);
	rc = pin->modify(&pv,1,0);

	// Query for the above PIN. No Results should be returned as its not yet committed.
	IStmt *query = session->createStmt();
	unsigned var = query->addVariable();   
	rc = query->setConditionFT(var,"Rose",0);
	ICursor *result = NULL;
	TVERIFYRC(query->execute(&result));
	int count = 0;
	IPIN *respin;
	for (; (respin=result->next())!=NULL; ) {
		count++;
		respin->destroy();
	}
	if(count != 0) rc = RC_OK; else rc = RC_FALSE;
	logResult ("FT Search Result before COMMIT ",rc);
	result->destroy();
	query->destroy();

	// commit the pin and now do a FT Search without the flag
	//rc = session->commitPINs(&pin, 1,0);
	//logResult ("Committed the PIN ",rc);
	IStmt *query1 = session->createStmt();
	var = query1->addVariable();
	rc = query1->setConditionFT(var,"Rose",0);
	ICursor *result1 = NULL;
	TVERIFYRC(query1->execute(&result));
	count = 0;
	for (;(respin=result1->next())!=NULL; ) {
		count++;
		respin->destroy();
	}
	result1->destroy();
	query1->destroy();
	// Check whether the pin is indexed even though the flag was not specified -
	if(count != 0) rc = RC_FALSE; else rc = RC_OK;
	logResult ("FT Search Result after COMMIT ",rc);

	// Add an element to the collection and do an FT Search on the element value added
	SETVALUE_C(pv, pm[5].uid, "Rain", OP_ADD, STORE_LAST_ELEMENT);
	rc = pin->modify(&pv,1);
	IStmt *query2 = session->createStmt();
	var = query2->addVariable();
	rc = query2->setConditionFT(var,"Rain",0);
	ICursor *result2 = NULL;
	TVERIFYRC(query2->execute(&result));
	count = 0;
	for (;(respin=result2->next())!=NULL; ) {
		count++;
		respin->destroy();
	}
	result2->destroy();
	query2->destroy();
	if(count != 0) rc = RC_OK; else rc = RC_FALSE;
	logResult ("FT Search Result after FT Index ",rc);

	// Delete an element in the collection and do a FT search without the Flag.   
	Value const *pv1 = pin->getValue(pm[5].uid);
	mLogger.out()<< pv1->varray[2].str;
	pv.setDelete(pm[5].uid,pv1->varray[2].eid);
	rc = pin->modify(&pv,1,0);
	IStmt *query3 = session->createStmt();
	var = query3->addVariable();
	rc = query3->setConditionFT(var,"Rain",0);
	ICursor *result3 = NULL;
	TVERIFYRC(query3->execute(&result3));
	count = 0;
	for (;(respin=result3->next())!=NULL; ) {
		count++;
		respin->destroy();
	}
	result3->destroy();
	query3->destroy();
	if(count != 0) rc = RC_OK; else rc = RC_FALSE;
	logResult ("FT Search Result after Deleting an element ",rc);
	
	// Delete a normal property and do a FT Search   
	pvs[0].setDelete(pm[1].uid);
	rc = pin->modify(pvs,1);
	IStmt *query4 = session->createStmt();
	var = query4->addVariable();
	rc = query4->setConditionFT(var,"Honalulu",0);
	ICursor *result4 = NULL;
	TVERIFYRC(query4->execute(&result));
	count = 0;
	for (;(respin=result4->next())!=NULL; ) {
		count++;
		respin->destroy();
	}
	result4->destroy();
	query4->destroy();
	if(count != 0) rc = RC_FALSE; else rc = RC_OK;
	logResult ("FT Search Result after Deleting a Normal Property ",rc);

	// Check whether the previously deleted collection element is still indexed
	IStmt *query5 = session->createStmt();
	var = query5->addVariable();
	rc = query5->setConditionFT(var,"Rain",0);
	ICursor *result5 = NULL;
	TVERIFYRC(query5->execute(&result5));
	count = 0;
	for (;(respin=result5->next())!=NULL; ) {
		count++;
		respin->destroy();
	}
	result5->destroy();
	query5->destroy();
	if(count != 0) rc = RC_FALSE; else rc = RC_OK;
	logResult ("FT Search Result on previously deleted collection element ",rc);
	pin->destroy();
}

void TestCollections::moveOPCollection(ISession *session)
{
	mLogger.out() << "moveOPCollection" << std::endl;

	// Need to 
	IPIN *pin;
	Value val[100];
	PropertyID lPropIDs[1];
	MVTApp::mapURIs(session,"TestCollections.inputcnavigCollection",1,lPropIDs);
	PropertyID lPropID = lPropIDs[0];
	//PropertyID lPropID = 2172;

	//case 1: simple move
	unsigned i;
	for (i=0;i<10;i++){
		val[i].set("aaaa");val[0].op = OP_ADD;val[i].setPropID(lPropID);val[i].eid = STORE_LAST_ELEMENT;
	}
	session->createPIN(val,10,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
	const Value *lVal = pin->getValue(lPropID);

	val[0].set("bbbb");
	val[0].op = OP_ADD;
	val[0].setPropID(lPropID);
	val[0].eid  = STORE_COLLECTION_ID;

	val[1].set((unsigned)lVal->varray[0].eid);
	val[1].op = OP_MOVE_BEFORE;
	val[1].setPropID(lPropID);
	val[1].eid = STORE_LAST_ELEMENT;

	RC rc = session->modifyPIN(pin->getPID(),val,2);
	pin->refresh();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(lPropID),mLogger.out()<<endl,session);
#endif
	pin->destroy();
	Tstring str;
	//add validation
	//case 2:OP_MOVE (simple) on a commited pin
	unsigned int z;
	for (z=0;z<5;z++){
		MVTRand::getString(str,10,0,true);
		char *bufnew = new char[str.length() + 1];
		strcpy(bufnew,str.c_str());
		val[z].set(bufnew);val[z].setPropID(lPropID);val[z].op = OP_ADD;
	}
	session->createPIN(val,z,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(lPropID),mLogger.out()<<endl,session);
#endif
	pin->destroy();

	val[0].set("Element 1");val[0].setPropID(lPropID);
	val[0].eid = STORE_LAST_ELEMENT;val[0].op = OP_ADD;

	val[1].set("Element 2");val[1].setPropID(lPropID);
	val[1].eid = STORE_LAST_ELEMENT;val[1].op = OP_ADD;

	val[2].set("Element 3");val[2].setPropID(lPropID);
	val[2].eid = STORE_LAST_ELEMENT;val[2].op = OP_ADD;

	session->createPIN(val,3,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
#ifdef VERBOSE
	MVTApp::output(*pin,mLogger.out()<<endl,session);
#endif
	//above output shows the return order as 2 3 1 instead of 1 2 3

	PropertyID lPropId = lPropID;
	const Value *lVal1 = pin->getValue(lPropId);

	val[0].set((unsigned)lVal1->varray[1].eid);
	val[0].op = OP_MOVE;
	val[0].setPropID(lPropId);
	val[0].eid = STORE_FIRST_ELEMENT;

	rc = pin->modify(val,1);
	pin->refresh();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(lPropId),mLogger.out()<<endl,session);
#endif
	
	const Value *lVal2 = pin->getValue(lPropId);
	val[0].set((unsigned)lVal2->varray[0].eid);
	val[0].op = OP_MOVE;
	val[0].setPropID(lPropId);
	val[0].eid = STORE_LAST_ELEMENT;

	rc = pin->modify(val,1);
	pin->refresh();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(lPropId),mLogger.out()<<endl,session);
#endif
	pin->destroy();

	//case 3: move at random element in a large collection
	for (i =0;i<100;i++){
		MVTRand::getString(str,15,0,true);
		char *bufnew = new char[str.length() + 1];
		strcpy(bufnew,str.c_str());
		val[i].set(bufnew);val[i].setPropID(lPropId);val[i].op = OP_ADD;
	}
	session->createPIN(val,i,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(lPropId),mLogger.out()<<endl,session);
#endif
	const Value *lVal3 = pin->getValue(lPropId);
	for (i=0;i<20;i++){
		unsigned int ran = rand()%99;
		unsigned int ran1 = rand()%99;
		val[i].set((unsigned)lVal3->varray[ran].eid); 
		val[i].setPropID(lPropId);
		val[i].op = OP_MOVE;
		val[i].eid = lVal3->varray[ran1].eid;
		//put it in a map and confirm later
		//coll.insert(map<unsigned int,Elmnts>::value_type(ran,els)); 
	}
	rc = pin->modify(val,i);
	pin->refresh();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(lPropId),mLogger.out()<<endl,session);
#endif
	pin->destroy();
	//case 4: VT_COLLECTION as set
}

void TestCollections::miscCollection(ISession *session)
{
#if 1
	mLogger.out() << "miscCollection" << std::endl;

	IPIN *pin;
	Value val[10];
	PID pid;
	PropertyID lPropIDs[3];
	MVTApp::mapURIs(session,"TestCollections.inputcnavigCollection",3,lPropIDs);
	PropertyID lPropID = lPropIDs[0];
	PropertyID lPropID1 = lPropIDs[1];
	PropertyID lPropID2 = lPropIDs[2];

	//PropertyID lPropID = 2173;
	//case: qsort interchaging the values, el2 becomes the first el.
	val[0].set("abc");val[0].setPropID(lPropID);val[0].op = OP_ADD;val[0].eid = STORE_FIRST_ELEMENT;
	val[1].set("def");val[1].setPropID(lPropID);val[1].op = OP_ADD;val[1].eid = STORE_LAST_ELEMENT;
	val[2].set("efg");val[2].setPropID(lPropID);val[2].op = OP_ADD;val[2].eid = STORE_LAST_ELEMENT;

	session->createPIN(val,3,&pin,MODE_PERSISTENT|MODE_COPY_VALUES);
	pid = pin->getPID();
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(lPropID),mLogger.out()<<endl,session);
#endif
	val[0].set("ami");val[0].setPropID(lPropID1);
	RC rc = session->modifyPIN(pid,val,3);
	//pin->modify(val,1);
	pin->refresh();
	pin->destroy();
	pin = session->getPIN(pid);
#ifdef VERBOSE
	MVTApp::output(*pin->getValue(lPropID1),mLogger.out()<<endl,session);
#endif
	//case: Add a totally new property as a collection to an existing pin.
	val[0].set("noon");val[0].setPropID(lPropID2);val[0].op = OP_ADD;val[0].eid = STORE_LAST_ELEMENT;
	val[1].set("nen");val[1].setPropID(lPropID2);val[1].op = OP_ADD;val[1].eid = STORE_LAST_ELEMENT;
	val[2].set("npp");val[2].setPropID(lPropID2);val[2].op = OP_ADD;val[2].eid = STORE_LAST_ELEMENT;
	rc = pin->modify(val,3);
	logResult ("****Case1 ",rc);
#ifdef VERBOSE
	MVTApp::output(*pin,mLogger.out(),session);
#endif
	pin->destroy();
#endif
}

void TestCollections::testFullScanCollection(ISession *session,URIMap *pm,int npm,PID *pid)
{
	mLogger.out() << "testFullScanCollection" << std::endl;

	IPIN *pin = session->getPIN(pid[3]);
#ifdef VERBOSE
	MVTApp::output(*pin,mLogger.out(),session);
#endif
	//case 1: simple OP_EQ eval
	IStmt *query = session->createStmt();
	unsigned char var = query->addVariable();
	PropertyID pids[1],pids1[1];
	Value args[2],args1[2],argsfinal[2];

	pids[0]=pm[0].uid;
	args[0].setVarRef(0,*pids);
	args[1].set("eLement 3");
	IExprNode *expr = session->expr(OP_EQ,2,args,CASE_INSENSITIVE_OP);
	query->addCondition(var,expr);

	ICursor *result = NULL;
	TVERIFYRC(query->execute(&result));
	int cnt = countPinsFullScan(result,session);
	if (cnt != 1)
		logResult ("****Case1: full scan collection ",RC_FALSE);
	pin->destroy();
	result->destroy();
	expr->destroy();
	query->destroy();

	//case 2: OP_EQ with a logical expression matching in multiple collections
	//a. OP_LAND
	query = session->createStmt();
	var = query->addVariable();

	pids[0]=pm[0].uid;
	args[0].setVarRef(0,*pids);
	args[1].set("element 3");
	expr = session->expr(OP_EQ,2,args);

	pids1[0]=pm[1].uid;
	args1[0].setVarRef(0,*pids1);
	args1[1].set("ELEMENT 6");
	IExprNode *expr1 = session->expr(OP_EQ,2,args1,CASE_INSENSITIVE_OP);
	
	argsfinal[0].set(expr);
	argsfinal[1].set(expr1);
	IExprNode *exprfinal = session->expr(OP_LAND,2,argsfinal);

	query->addCondition(var,exprfinal);
	TVERIFYRC(query->execute(&result));

	cnt = countPinsFullScan(result,session);
	if (cnt != 1)
		logResult ("****Case2(a): full scan collection ",RC_FALSE);

	exprfinal->destroy();
	result->destroy();
	query->destroy();
}

void TestCollections::logResult(string str, RC rc)
{
	ofstream fout;
	rc == RC_OK ? str += "passed ******":str += "failed ******",mRCFinal=rc;
	fout.open("results.txt",ios::app);
	fout<<str.c_str()<<"\n"<<endl;
	fout.close();
}

int TestCollections::countPinsFullScan(ICursor *result,ISession *session)
{
	int count=0;
	for (IPIN *pin; (pin=result->next())!=NULL; ){
#ifdef VERBOSE
		MVTApp::output(*pin,mLogger.out(),session);
#endif
		count++;
		pin->destroy();
	}
	return count;
}

void TestCollections::verifyMove(Value const &val,const map<int,Elmnts> &coll)
{
}
