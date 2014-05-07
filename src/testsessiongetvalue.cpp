/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

// Publish this test.
class TestSessionGetValue : public ITest
{
	public:
		RC mRCFinal;
		TEST_DECLARE(TestSessionGetValue);
		virtual char const * getName() const { return "testsessiongetvalue"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of getValue(s) methods of ISession"; }
		virtual bool includeInPerfTest() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void populateStore(ISession *session,URIMap *pm,int npm, PID *pid);
		void testGetValue(ISession *session,URIMap *pm,int npm, PID *pid);
		//void testGetValues(ISession *session,URIMap *pm,int npm, PID *pid);		
		//void testGetMissingValues(bool,bool);
	protected:
		// TODO: refine/complete these and provide as service in app.h
		void logResult(string str,RC rc);		

		ISession * mSession;
};
TEST_IMPLEMENT(TestSessionGetValue, TestLogger::kDStdOut);

// Implement this test.
int TestSessionGetValue::execute()
{
	mRCFinal = RC_OK;
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();
		URIMap pm[21];
		PID pid[8];

		populateStore(mSession,pm,sizeof(pm)/sizeof(pm[0]),pid);
		testGetValue(mSession,pm,sizeof(pm)/sizeof(pm[0]),pid);
#if 0
		testGetValues(mSession,pm,sizeof(pm)/sizeof(pm[0]),pid);
		
		testGetMissingValues(false/*session memory*/,false/*ssv data*/);
		testGetMissingValues(true,false); 
		testGetMissingValues(false,true);
		testGetMissingValues(true,true); 
#endif

		mSession->terminate();
		MVTApp::stopStore();
	}

	else { TVERIFY(!"Unable to start store"); }
	return mRCFinal;
}

void TestSessionGetValue::populateStore(ISession *session,URIMap *pm,int npm, PID *pid)
{
	MVTApp::mapURIs(session,"TestSessionGetValue",npm,pm);
	Value pvs[21];
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
	pvs[14].set("http://www.google.com"); SETVATTR(pvs[14], pm[14].uid, OP_SET);
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
	pvs[20].setURIID(pm[14].uid); SETVATTR(pvs[20], PROP_SPEC_VALUE, OP_SET);
	IPIN *pin;
	TVERIFYRC(session->createPIN(pvs,21,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	pid[0] = pin->getPID();

	// PIN 2
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
	SETVALUE(pvs[11], PROP_SPEC_UPDATED, 0, OP_SET);
	TVERIFYRC(session->createPIN(pvs,12,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	pid[1] = pin->getPID();

	// PIN 3
	SETVALUE(pvs[0], pm[0].uid, "Srilanka", OP_SET);
	SETVALUE(pvs[1], pm[1].uid, "Colombo", OP_SET);
	SETVALUE(pvs[2], pm[2].uid, "Church Street", OP_SET);
	SETVALUE(pvs[3], pm[3].uid, "alec@lanka.com", OP_SET);
	SETVALUE(pvs[4], pm[4].uid, "Alec", OP_SET);
	SETVALUE(pvs[5], pm[11].uid, double(64000.50), OP_SET);
	SETVALUE(pvs[6], pm[12].uid, 34443164, OP_SET);
	SETVALUE(pvs[7], pm[13].uid, 10091999, OP_SET);	
	pvs[8].set("http://www.srilanka4ever.com"); SETVATTR_C(pvs[8], pm[14].uid, OP_ADD, STORE_COLLECTION_ID);
	TVERIFYRC(session->createPIN(pvs,9,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	pid[2] = pin->getPID();
	
	// Twice modifying the PIN to add 2 coll elements... Some issue if done at once.
	// Shown to Sumanth
	IPIN *pin1 = session->getPIN(pid[2]);
	pvs[0].set("http://www.srilankanever.com"); SETVATTR_C(pvs[0], pm[14].uid, OP_ADD, STORE_LAST_ELEMENT);
	RC rc = pin1->modify(pvs,1);

	pvs[0].set("http://www.srilankaalways.com"); SETVATTR_C(pvs[0], pm[14].uid, OP_ADD, STORE_LAST_ELEMENT);
	rc = pin1->modify(pvs,1);

	RefP rv;
	rv.eid = STORE_COLLECTION_ID;
	rv.pin = session->getPIN(pid[0],true);
	rv.pid = pm[1].uid;
	rv.vid = 0;
	
	SETVALUE(pvs[0], PROP_SPEC_VALUE, rv, OP_SET);
	TVERIFYRC(session->createPIN(pvs,1,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	pid[3] = pin->getPID();
	
	// PIN 5
	RefVID ref;
	ref.eid = STORE_COLLECTION_ID;
	ref.id = pid[2];
	ref.pid = pm[0].uid;
	ref.vid = 0;

	SETVALUE(pvs[0], PROP_SPEC_VALUE, ref, OP_SET);
	TVERIFYRC(session->createPIN(pvs,1,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	pid[4] = pin->getPID();

	// PIN 6
	RefP rv1; rv1.eid = STORE_COLLECTION_ID; rv1.vid = 0;
	IPIN *pinx = session->getPIN(pid[2]);
	Value pvx = *pinx->getValue(pm[14].uid);
	if (pvx.type==VT_COLLECTION) {
		if (pvx.isNav()) {
			pvx.nav->navigate(GO_FIRST)->eid;
			rv1.eid = pvx.nav->navigate(GO_NEXT)->eid;
		} else {
			rv1.eid = pvx.varray[1].eid;
		}
	} else TVERIFYRC(RC_TYPE);
	rv1.pin = pinx;
	rv1.pid = pm[14].uid;	
	SETVALUE(pvs[0], PROP_SPEC_VALUE, rv1, OP_SET);
	TVERIFYRC(session->createPIN(pvs,1,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	pid[5] = pin->getPID();

	// PIN 7
	RefVID ref1;
	if (pvx.type==VT_COLLECTION)
		ref1.eid = pvx.isNav() ? pvx.nav->navigate(GO_NEXT)->eid:pvx.varray[2].eid;
	
	ref1.id = pid[2];
	ref1.pid = pm[14].uid;
	SETVALUE(pvs[0], PROP_SPEC_VALUE, ref1, OP_SET);
	TVERIFYRC(session->createPIN(pvs,1,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	pid[6] = pin->getPID();

	// PIN 8
	// Creating a PIN with collection for PROP_SPEC_VALUE property
	SETVALUE(pvs[0], pm[4].uid, "Collection of PROP_SPEC_VALUE", OP_SET);
	SETVATTR(pvs[1], PROP_SPEC_VALUE, OP_SET);
	TVERIFYRC(session->createPIN(pvs,2,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	pid[7] = pin->getPID();

	IPIN *pin8 = session->getPIN(pid[7]);
	SETVALUE_C(pvs[0], PROP_SPEC_VALUE, rv, OP_ADD, STORE_LAST_ELEMENT);
	rc = pin8->modify(pvs,1);

	SETVALUE_C(pvs[0], PROP_SPEC_VALUE, ref, OP_ADD, STORE_LAST_ELEMENT);
	rc = pin8->modify(pvs,1);

	SETVALUE_C(pvs[0], PROP_SPEC_VALUE, rv1, OP_ADD, STORE_LAST_ELEMENT);
	rc = pin8->modify(pvs,1);
	
	SETVALUE_C(pvs[0], PROP_SPEC_VALUE, ref1, OP_ADD, STORE_LAST_ELEMENT);
	rc = pin8->modify(pvs,1);
	
	RefP rv2;
	rv2.eid = STORE_COLLECTION_ID;
	rv2.pin = session->getPIN(pid[0],true);
	rv2.pid = PROP_SPEC_VALUE;
	rv2.vid = 0;

	SETVALUE_C(pvs[0], PROP_SPEC_VALUE, rv2, OP_ADD, STORE_LAST_ELEMENT);
	rc = pin8->modify(pvs,1);
	
	pin1->destroy();
	pin8->destroy();	
}

void TestSessionGetValue::testGetValue(ISession *session,URIMap *pm,int npm, PID *pid){
	RC rc = RC_FALSE;
	Value val[1];
	
	// Get VT_STRING Property value

	// REVIEW: Isn't this string being Leaked?
	TVERIFYRC(session->getValue(val[0],pid[0],pm[1].uid));	
	TVERIFY(strcmp(val[0].str,"Bangalore") == 0);

	// Caller must free!
	session->free(const_cast<char*>(val[0].str));

	// Get VT_STRING property value
	TVERIFYRC(session->getValue(val[0],pid[0],pm[14].uid));
	TVERIFY(strcmp(val[0].str,"http://www.google.com") == 0);
	session->free(const_cast<char*>(val[0].str));

	// Get VT_INT property value
	TVERIFYRC(session->getValue(val[0],pid[0],pm[7].uid));
	TVERIFY(val[0].i == 27);

	// Get VT_UINT property value
	TVERIFYRC(session->getValue(val[0],pid[0],pm[16].uid));
	TVERIFY(val[0].ui == 987654321);

	// Get VT_INT64 property value
	TVERIFYRC(session->getValue(val[0],pid[0],pm[17].uid));
	TVERIFY(val[0].i64 == 123456789);

	// Get VT_UINT64 property value
	TVERIFYRC(session->getValue(val[0],pid[0],pm[18].uid));
	TVERIFY(val[0].ui64 == 123456789);

	// Get VT_FLOAT property value
	TVERIFYRC(session->getValue(val[0],pid[0],pm[10].uid));
	TVERIFY(val[0].f == 7.5);

	// Get VT_DOUBLE property value
	TVERIFYRC(session->getValue(val[0],pid[0],pm[11].uid));
	TVERIFY(val[0].d == 10000.5);

	// Get VT_BOOL property value
	rc = RC_FALSE;
	TVERIFYRC(session->getValue(val[0],pid[0],pm[9].uid));
	TVERIFY(val[0].b);

	// Get PROP_SPEC_UPDATED value
	rc = RC_FALSE;
	TVERIFYRC(session->getValue(val[0],pid[1],PROP_SPEC_UPDATED));
	IPIN *lPIN1 = session->getPIN(pid[1]);
	if(val[0].ui64 == lPIN1->getValue(PROP_SPEC_UPDATED)->ui64)
		rc = RC_OK;
	logResult("getValue(PROP_SPEC_UDPDATED) ",rc);
	lPIN1->destroy();

#if 0 //obsolete
	// For the overloaded getValue() method
	// PROP_SPEC_VALUE with VT_PROPERTY
	rc = RC_FALSE;
	TVERIFYRC(session->getValue(val[0],pid[0]));
	if(strcmp(val[0].str,"http://www.google.com") == 0)
		rc = RC_OK;
	logResult("getValue(PROP_SPEC_VALUE with VT_PROPERTY) ",rc);

	// PROP_SPEC_VALUE with VT_REFPROP
	rc = RC_FALSE;
	TVERIFYRC(session->getValue(val[0],pid[3]));
	TVERIFY(strcmp(val[0].str,"Bangalore") == 0);

	// PROP_SPEC_VALUE with VT_REFIDPROP
	TVERIFYRC(session->getValue(val[0],pid[4]));
	TVERIFY(strcmp(val[0].str,"Srilanka") == 0);
	
	// PROP_SPEC_VALUE with VT_REFELT	

	TVERIFYRC(session->getValue(val[0],pid[5]));
	TVERIFY(strcmp(val[0].str,"http://www.srilankanever.com") == 0);

	// PROP_SPEC_VALUE with VT_REFIDELT
	TVERIFYRC(session->getValue(val[0],pid[6]));
	TVERIFY(strcmp(val[0].str,"http://www.srilankaalways.com") == 0);
#endif

	//
	// Error scenarios (bad input from user)
	//
	mLogger.out() << endl << endl << "Starting error scenarios" << endl ;

	// Case 1: Pointing to a pid that doesn't exist in the store should fail clealy
	// ERROR:Read error 9 for page 0000FFFF

	PID invalidpid ; INITLOCALPID(invalidpid); 
	ulong invalidPage = 0xFFFFF ;  // expected past end of page
	LOCALPID(invalidpid) = (uint64_t(session->getLocalStoreID()) << STOREID_SHIFT) + (invalidPage << PAGE_SHIFT) + 0x10;

	rc = session->getValue(val[0],invalidpid,pm[1].uid);
	TVERIFY( rc == RC_NOTFOUND ) ; 

	// (Sanity check)
	IPIN * p1 = session->getPIN(invalidpid);
	TVERIFY(p1==NULL);
	
	// Case 2: point to an index that doesn't exist on the pid page
	// ERROR:Invalid idx 100 for page 0000000F (8 entries)
	IPIN *pin;
	PID newPid; Value valExpected; valExpected.set(1); valExpected.property=pm[1].uid;
	TVERIFYRC(session->createPIN(&valExpected, 1 ,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	newPid = pin->getPID();

	invalidpid = newPid ;
	invalidpid.pid += 100 ; // slot on the page past the real number of pins
	TVERIFY(RC_NOTFOUND == session->getValue(val[0],invalidpid,pm[1].uid)); // Seems like a good error msg

	IPIN * p2 = session->getPIN(invalidpid);
	TVERIFY(p2==NULL);


	// Case 3: 
	// ERROR:Cannot read page 00000004: incorrect PGID
	invalidPage = 0x04 ; // Normally an index page
	LOCALPID(invalidpid) = (uint64_t(session->getLocalStoreID()) << STOREID_SHIFT) + (invalidPage << 16) + 0x10;
	TVERIFY(RC_NOTFOUND==session->getValue(val[0],invalidpid,pm[1].uid));

	IPIN * p3 = session->getPIN(invalidpid);
	TVERIFY(p3==NULL);


	// Case 4: pid that has been deleted
	// No kernel errors are added
	invalidpid = newPid ;
	TVERIFYRC(session->deletePINs(&newPid,1,MODE_PURGE));
	TVERIFY(RC_NOTFOUND==session->getValue(val[0],invalidpid,pm[1].uid));

	// Try to fool store by creating a new pid that might replace the purged pid
	PID replacementPID ; Value valReplacement;valReplacement.set(1);valReplacement.property=pm[1].uid;
	TVERIFYRC(session->createPIN(&valReplacement,1,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	replacementPID = pin->getPID();
	
	// Store is not fooled
	TVERIFY(replacementPID!=newPid);
	TVERIFY(RC_NOTFOUND==session->getValue(val[0],invalidpid,pm[1].uid));
}

#if 0
void TestSessionGetValue::testGetValues(ISession *session,URIMap *pm,int npm, PID *pid){
	RC rc = RC_FALSE;
	Value vals[21];
	memset(vals, 0, sizeof(vals));
	
	// Getting PIN 1 properties except PROP_SPEC_VALUE 		
	vals[0].property = pm[0].uid; vals[0].eid=STORE_COLLECTION_ID;
	vals[1].property = pm[1].uid; vals[1].eid=STORE_COLLECTION_ID;
	vals[2].property = pm[2].uid; vals[2].eid=STORE_COLLECTION_ID;
	vals[3].property = pm[3].uid; vals[3].eid=STORE_COLLECTION_ID;
	vals[4].property = pm[4].uid; vals[4].eid=STORE_COLLECTION_ID;
	vals[5].property = pm[5].uid; vals[5].eid=STORE_COLLECTION_ID;
	vals[6].property = pm[6].uid; vals[6].eid=STORE_COLLECTION_ID;
	vals[7].property = pm[7].uid; vals[7].eid=STORE_COLLECTION_ID;
	vals[8].property = pm[8].uid; vals[8].eid=STORE_COLLECTION_ID;
	vals[9].property = pm[9].uid; vals[9].eid=STORE_COLLECTION_ID;
	vals[10].property = pm[10].uid; vals[10].eid=STORE_COLLECTION_ID;
	vals[11].property = pm[11].uid; vals[11].eid=STORE_COLLECTION_ID;
	vals[12].property = pm[12].uid; vals[12].eid=STORE_COLLECTION_ID;
	vals[13].property = pm[13].uid; vals[13].eid=STORE_COLLECTION_ID;
	vals[14].property = pm[14].uid; vals[14].eid=STORE_COLLECTION_ID;
	vals[15].property = pm[15].uid; vals[15].eid=STORE_COLLECTION_ID;
	vals[16].property = pm[16].uid; vals[16].eid=STORE_COLLECTION_ID;
	vals[17].property = pm[17].uid; vals[17].eid=STORE_COLLECTION_ID;
	vals[18].property = pm[18].uid; vals[18].eid=STORE_COLLECTION_ID;
	vals[19].property = pm[19].uid; vals[19].eid=STORE_COLLECTION_ID;	

	int nvals=20;
	session->getValues(vals,nvals,pid[0]);
	IPIN *pin = session->getPIN(pid[0]);
	rc = RC_OK;
	for (int i = 0; i<nvals;i++){
		switch(vals[i].type){
			mLogger.out()<<pin->getValue(pm[i].uid)->str<<std::endl;
		case VT_STRING:
			mLogger.out()<<pin->getValue(pm[i].uid)->str;
			if(strcmp(pin->getValue(pm[i].uid)->str,vals[i].str) != 0)
				rc = RC_FALSE;
			session->free(const_cast<char*>(vals[i].str));
			break;
		case VT_INT:
			if(pin->getValue(pm[i].uid)->i != vals[i].i)
				rc = RC_FALSE;
			break;
		case VT_UINT:
			if(pin->getValue(pm[i].uid)->ui != vals[i].ui)
				rc = RC_FALSE;
			break;
		case VT_INT64:
			if(pin->getValue(pm[i].uid)->i64 != vals[i].i64)
				rc = RC_FALSE;
			break;
		case VT_UINT64: case VT_DATETIME:
			if(pin->getValue(pm[i].uid)->ui64 != vals[i].ui64)
				rc = RC_FALSE;
			break;
		case VT_FLOAT:
			if(pin->getValue(pm[i].uid)->f != vals[i].f)
				rc = RC_FALSE;
			break;
		case VT_DOUBLE:
			if(pin->getValue(pm[i].uid)->d != vals[i].d)
				rc = RC_FALSE;
			break;
		case VT_BOOL:
			if(pin->getValue(pm[i].uid)->b != vals[i].b)
				rc = RC_FALSE;
			break;
		default:
			rc = RC_FALSE;
			break;
		}
		if (rc != RC_OK) break;
	}
	logResult("getValues(All Data Types) ",rc);
	pin->destroy(); pin = NULL;

	// To get collection elements
	vals[0].property = vals[1].property = vals[2].property = pm[14].uid;
	vals[0].eid = 0x10000000;
	vals[1].eid = 0x10000001;
	vals[2].eid = 0x10000002;
	nvals=3;
	session->getValues(vals,nvals,pid[2]);
	//mLogger.out()<<vals[0].val.str;
	rc = RC_FALSE;
	if(strcmp(vals[0].str,"http://www.srilanka4ever.com") == 0)
		if(strcmp(vals[1].str,"http://www.srilankanever.com") == 0)
			if(strcmp(vals[2].str,"http://www.srilankaalways.com") == 0)
				rc = RC_OK;
	logResult("getValues(Collection Elements) ",rc);

	// For Special properties - PROP_SPEC_UPDATED
	vals[0].property = PROP_SPEC_UPDATED; vals[0].eid=STORE_COLLECTION_ID;
	vals[1].property = pm[0].uid; vals[1].eid=STORE_COLLECTION_ID;
	session->getValues(vals,2,pid[1]);
	rc = RC_FALSE;
	pin = session->getPIN(pid[1]);
	if(vals[0].ui64 == pin->getValue(PROP_SPEC_UPDATED)->ui64)
		if(strcmp(vals[1].str,pin->getValue(pm[0].uid)->str) == 0)
			rc = RC_OK;
	logResult("getValues(PROP_SPEC_UPDATED) ",rc);
	pin->destroy();
}
#endif

void TestSessionGetValue::logResult(string str, RC rc)
{
	ofstream fout;
	rc == RC_OK ? str += " passed ******":str += " failed ******",mRCFinal=rc;
	fout.open("results.txt",ios::app);
	fout<<str.c_str()<<"\n"<<endl;
	fout.close();
}

void freeVals( ISession * session, const Value * inVals, int cnt )
{
	for ( int i = 0 ; i < cnt ; i++ )
	{
		if ( inVals[i].type == VT_STRING )
		{
			session->free(const_cast<char*>(inVals[i].str)) ;
		}
	}
}

#if 0
void TestSessionGetValue::testGetMissingValues(bool useSessionMem, bool bSSV)
{
	mLogger.out() << endl <<  "testGetMissingValues: SessionMem? " << useSessionMem 
								<< " SSV data? " << bSSV
								<< endl ;

	RC rc ;

	// 11780 scenario
	PropertyID prop0 = MVTUtil::getProp(mSession,"TestSessionGetValue.1");
	PropertyID prop1 = MVTUtil::getProp(mSession,"TestSessionGetValue.2");


	// META_PROP_SSTORAGE is important because of the store has 
	// to do more work to retrieve the real content
	Value v[2] ;
	v[0].set("Prop0Data"); v[0].property = prop0; 
	v[1].set("Prop1Data"); v[1].property = prop1; 

	if (bSSV)
	{
		v[0].meta=v[1].meta=META_PROP_SSTORAGE; 
	}

	// Behavior is different especially in IPC when the vals is in session memory or not
	Value * readVals = NULL;
	if ( useSessionMem )
	{
		readVals = (Value*)mSession->malloc(sizeof(Value)*2);
	}
	else
	{
		readVals = (Value*)malloc(sizeof(Value)*2);
	}

	//
	// Case 1 - Success case
	//
	mLogger.out() << "Case1" << endl;
	PID p1; IPIN *pin;
	TVERIFYRC(mSession->createPIN(v,2,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	p1 = pin->getPID();

	readVals[0].setError(prop0);
	readVals[1].setError(prop1);
	TVERIFYRC(mSession->getValues(readVals,2,p1));
	TVERIFY(readVals[0].type==VT_STRING);TVERIFY(0==strcmp(readVals[0].str,"Prop0Data"));
	TVERIFY(0==strcmp(readVals[1].str,"Prop1Data"));

	freeVals(mSession,readVals,2);

	//
	// Case 2 - Pin only has one of the two properties
	//
	mLogger.out() << "Case2" << endl;
	PID p2 ;
	TVERIFYRC(mSession->createPIN(v,1 /*ONLY 1 property passed*/,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	p2 = pin->getPID();

	readVals[0].setError(prop0);
	readVals[1].setError(prop1);
	rc=mSession->getValues(readVals,2,p2);
	
	TVERIFY(rc == RC_FALSE);
	TVERIFY(readVals[0].type == VT_STRING);

	if (readVals[0].type == VT_STRING)
		TVERIFY(0==strcmp(readVals[0].str,"Prop0Data"));
	else if ( readVals[0].type == VT_STREAM ) 
	{
		// Uninitialized VT_STREAM? Printing out value because it was often bad pointer
		mLogger.out() << "Got VT_STREAM 0x" << std::hex << (unsigned int)(size_t)(readVals[0].stream.is) << endl ;
	}

	TVERIFY(readVals[1].type == VT_ERROR);

	freeVals(mSession,readVals,2);

	//
	// Case 3 - reversed order (missing prop then present prop
	//  Expect first is set to VT_ERROR and second initialized properly
	// 

	mLogger.out() << "Case3" << endl;
	readVals[0].setError(prop1);
	readVals[1].setError(prop0);
	rc=mSession->getValues(readVals,2,p2);
	
	TVERIFY(rc == RC_FALSE);
	TVERIFY(readVals[1].type == VT_STRING);

	TVERIFY(readVals[0].type == VT_ERROR);

	if (readVals[1].type == VT_STRING)
		TVERIFY(0==strcmp(readVals[1].str,"Prop0Data"));
	else if ( readVals[1].type == VT_STREAM )
		// Uninitialized VT_STREAM?
		mLogger.out() << "Got VT_STREAM 0x" << std::hex << (unsigned int)(size_t)(readVals[1].stream.is) << endl ;

	freeVals(mSession,readVals,2);

	// TODO: more cases, e.g. other data types?

	if ( useSessionMem )
	{
		mSession->free(readVals);
	}
	else
	{
		free(readVals);
	}
}
#endif
