/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <fstream>
#include "serialization.h"
#include "teststream.h"
using namespace std;

// I know its a bad habit, but ...
#define CHECK_EQU( VAR, VALUE ) if ( VAR != VALUE ) { fout<<"Failed: " #VAR " != " #VALUE ", returned " << ((int)VAR) << ":" __FILE__ "(" << __LINE__ <<")\n"; }

// Publish this test.
// Note (maxw, Dec2010):
//   This test produces a few IExpr leaks; in all likelihood many of them are related with bug #112,
//   but once bug #112 is fixed, we should re-check.
class TestPersistence : public ITest
{
	public:
		RC mRCFinal;
		PID pid[2];
		URIMap pm[28];
		ofstream fout,fsrl;
		TEST_DECLARE(TestPersistence);
		virtual char const * getName() const { return "testpersistence"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing persistence of store datatypes"; }
		virtual bool includeInBashTest(char const *& pReason) const { pReason = "Need to restart the store for true pass..."; return false; }
		virtual bool excludeInIPCSmokeTest(char const *& pReason) const { pReason = "Need to restart the store for true pass..."; return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void createPins(ISession *session,URIMap *pm,int npm);
		void checkPeristence(ISession *session, PID *pid,URIMap *pm,int npm);
		void modifyPins(ISession *session,URIMap *pm,int npm);
		void refreshPins(ISession *session,URIMap *pm,int npm);
		void logResult(uint8_t type,string str);
		void serial(ISession *session,PID *pid);
	
		PropertyID propOnRef;
		PropertyID propOnRef1;
		PID pid1000,pid1001;
};
TEST_IMPLEMENT(TestPersistence, TestLogger::kDStdOut);

int TestPersistence::execute() 
{
	mRCFinal = RC_OK;
	fout.open("output.log",ios::app);
	fsrl.open("serial.log",ios::app);
	fsrl.close();
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		createPins(session,pm,sizeof(pm)/sizeof(pm[0]));
		session->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	MVStoreKernel::threadSleep(1000);
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		fout<<"Start persistence tests after create pins"<<endl;
		fout<<"======================================="<<endl;
		checkPeristence(session,pid,pm,sizeof(pm)/sizeof(pm[0]));
		fout<<"End persistence tests after create pins"<<endl;
		fout<<"======================================="<<endl;
		session->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	MVStoreKernel::threadSleep(1000);
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();

		fout<<"Start persistence tests after modify pins"<<endl;
		fout<<"======================================="<<endl;
		modifyPins(session,pm,sizeof(pm)/sizeof(pm[0]));
		fout<<"End persistence tests after modify pins"<<endl;
		fout<<"======================================="<<endl;
		
		fout<<"Start persistence tests after refresh pins"<<endl;
		fout<<"======================================="<<endl;
		refreshPins(session,pm,sizeof(pm)/sizeof(pm[0]));
		fout<<"End persistence tests after refresh pins"<<endl;
		fout<<"======================================="<<endl;

		session->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	MVStoreKernel::threadSleep(1000);
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		fout<<"Start persistence tests after serialize of pins"<<endl;
		fout<<"======================================="<<endl;
		serial(session,pid);
		fout<<"End persistence tests after serialize of pins"<<endl;
		fout<<"======================================="<<endl;

		session->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	
	return mRCFinal;
}

void TestPersistence::createPins(ISession *session,URIMap *pm,int npm)
{
	Value val[31];
	memset(pm,0,npm*sizeof(URIMap));

	pm[0].URI = "TestPersistence.VT_STRING";
	pm[1].URI = "TestPersistence.VT_INT";
	pm[2].URI = "TestPersistence.VT_USTR";
	pm[3].URI = "TestPersistence.VT_BSTR";
	pm[4].URI = "TestPersistence.VT_URL";
	pm[5].URI = "TestPersistence.VT_UURL";
	pm[6].URI = "TestPersistence.VT_UINT";
	pm[7].URI = "TestPersistence.VT_INT64";
	pm[8].URI = "TestPersistence.VT_UINT64";
	pm[9].URI = "TestPersistence.VT_FLOAT";
	pm[10].URI = "TestPersistence.VT_DOUBLE";
	pm[11].URI = "TestPersistence.VT_REF";
	pm[12].URI = "TestPersistence.VT_REFID";
	pm[13].URI = "TestPersistence.VT_REFIDPROP";
	pm[14].URI = "TestPersistence.VT_REFIDELT";
	pm[15].URI = "TestPersistence.VT_REFPROP";
	pm[16].URI = "TestPersistence.VT_REFELT";
	pm[17].URI = "TestPersistence.VT_ARRAY";
	pm[18].URI = "TestPersistence.VT_COLLECTION";
	pm[19].URI = "TestPersistence.VT_BOOL";
	pm[20].URI = "TestPersistence.VT_DATETIME";
	pm[21].URI = "TestPersistence.VT_INTERVAL";
	pm[22].URI = "TestPersistence.VT_STREAM";
	pm[23].URI = "TestPersistence.VT_STMT";
	pm[24].URI = "TestPersistence.VT_EXPRTREE";
	pm[25].URI = "TestPersistence.VT_URIID";
	pm[26].URI = "TestPersistence.VT_IDENTITY";
	
	RC rc = session->mapURIs(28,pm);

	session->createPIN(pid1000,NULL,0);
	session->createPIN(pid1001,NULL,0);
	propOnRef=pm[13].uid;
	propOnRef1=pm[12].uid;

	//pin for VT_REFXXX
	PID rpid;
	val[0].set("pin for reference datatypes");val[0].setPropID(propOnRef);
	val[1].set("collection prop0");val[1].setPropID(propOnRef1);val[1].op = OP_ADD;val[1].eid = STORE_LAST_ELEMENT;
	val[2].set("collection prop1");val[2].setPropID(propOnRef1);val[2].op = OP_ADD;val[2].eid = STORE_LAST_ELEMENT;
	val[3].set("collection prop2");val[3].setPropID(propOnRef1);val[3].op = OP_ADD;val[3].eid = STORE_LAST_ELEMENT;
	rc = session->createPIN(rpid,val,4);
	IPIN *rpin = session->getPIN(rpid);
	
	//pin for persistence test
	val[0].set("Revenge of the Sith");val[0].setPropID(pm[0].uid);
	val[1].set(-1234);val[1].setPropID(pm[1].uid);
	val[2].set("Anikin Skywalker");val[2].setPropID(pm[2].uid);
	//bstr
	char buf[16] = "Luke Skywalker";unsigned char *ubuf;
	ubuf = (unsigned char*)malloc(strlen(buf)); memcpy(ubuf,buf,strlen(buf));
	val[3].set(ubuf,(unsigned long)strlen(buf));val[3].setPropID(pm[3].uid);
	val[4].setURL("http://www.starwars.com");val[4].setPropID(pm[4].uid);
	val[5].setURL("http://www.lucasfilms.com");val[5].setPropID(pm[5].uid);
	val[6].set(unsigned(123612));val[6].setPropID(pm[6].uid);
	int64_t i64 = 123456789;
	val[7].setI64(i64);val[7].setPropID(pm[7].uid);
	uint64_t ui64 = 123456789;
	val[8].setU64(ui64);val[8].setPropID(pm[8].uid);
	val[9].set(float(7.5));val[9].setPropID(pm[9].uid);
	val[10].set(double(123.566));val[10].setPropID(pm[10].uid);
	val[11].set(rpin);val[11].setPropID(pm[11].uid);
	val[12].set(rpid);val[12].setPropID(pm[12].uid);
	
	//VT_REFIDPROP
	RefVID rvid;
	rvid.id = pid1000; rvid.pid = propOnRef;
	rvid.eid=STORE_COLLECTION_ID; rvid.vid=0;
	val[13].set(rvid); val[13].setPropID( pm[13].uid );	

	//VT_REFIDELT
	RefVID rvidelt;
	rvidelt.id=pid1001; rvidelt.pid=propOnRef1; rvidelt.eid=1; rvidelt.vid=0; 
	val[14].set(rvidelt); val[14].setPropID(pm[14].uid);
	
	//VT_REFPROP
	RefV rval;
	rval.pin = rpin; rval.pid=propOnRef; rval.eid=STORE_COLLECTION_ID; rval.vid=0;
	val[15].set(rval);val[15].setPropID(pm[15].uid); 
	//VT_REFELT
	RefV rvalelt;
	rvalelt.pin = rpin; rvalelt.pid=propOnRef1; rvalelt.eid=STORE_FIRST_ELEMENT; rvalelt.vid=0;
	val[16].set(rvalelt);val[16].setPropID(pm[16].uid);
	//VT_ARRAY
	Value *cval = new Value[4];
	cval[0].set("a");
	cval[1].set("ac");
	cval[2].set("acc");
	val[17].set(cval,3);val[17].setPropID(pm[17].uid);
	val[17].op = OP_ADD;val[17].eid = STORE_LAST_ELEMENT;
	//VT_COLLECTION(?)
	val[18].set("Implement CNAVIG here");val[18].setPropID(pm[18].uid);
	//VT_BOOL
	val[19].set(true);val[19].setPropID(pm[19].uid);
	//VT_INTERVAL
	i64 = 12753803254343750LL;
	val[20].setInterval(i64);val[20].setPropID(pm[20].uid);
	//VT_DATETIME
	ui64 = 12753803254343750LL;
	val[21].setDateTime(ui64);val[21].setPropID(pm[21].uid);
	//VT_STREAM
	unsigned long streamlen = 6194304;
	val[22].set(MVTApp::wrapClientStream(session, new MyStream(streamlen)));val[22].setPropID(pm[22].uid);

	//VT_STMT
#if 1 //BUG 724
	IStmt *query = session->createStmt();
	unsigned qvar = query->addVariable();
	query->setConditionFT(qvar,"luke");
	val[23].set(query);val[23].setPropID(pm[23].uid);
#endif

	//VT_EXPR 
#if 1 //BUG 724
	IStmt *expquery = session->createStmt();
	expquery->addVariable();
	PropertyID epids[1];
	Value eargs[2];

	epids[0] = pm[1].uid;
	eargs[0].setVarRef(0,*epids);
	eargs[1].set("lucas");
	IExprTree *expr1 = session->expr(OP_EQ,2,eargs,CASE_INSENSITIVE_OP);
	IExpr *compexpr = expr1->compile();
	expr1->destroy(); expr1 = NULL;
	val[24].set(compexpr);val[24].setPropID(pm[24].uid);
#endif
	
	//VT_URIID
	val[25].setURIID(pm[0].uid);val[25].setPropID(pm[25].uid);
	//VT_INDENTITY
	IdentityID id;
	id = session->storeIdentity("Princess Leah",NULL,0);
	val[26].setIdentity(id);val[26].setPropID(pm[26].uid);

	rc = session->createPIN(pid[0],val,27);
	delete[] cval;
	compexpr->destroy();
	query->destroy();
	expquery->destroy();
	rpin->destroy();
}

void TestPersistence::checkPeristence(ISession *session, PID *pid,URIMap *pm,int npm)
{
	IPIN *pin = session->getPIN(pid[0]);
	TVERIFY(pin!=NULL);
	if (pin==NULL) return;
	const Value* l_value;
	//VT_STRING
	if(pin->getValue(pm[0].uid)->type != VT_STRING && pin->getValue(pm[0].uid)->type != VT_STREAM)// || pin->getValue(pm[0].uid)->str != "Revenge of the Sith")
		logResult(pin->getValue(pm[0].uid)->type,"VT_STRING");
	//VT_INT
	if(pin->getValue(pm[1].uid)->type != VT_INT)
		logResult(pin->getValue(pm[1].uid)->type,"VT_INT");
	//VT_BSTR
	if(pin->getValue(pm[3].uid)->type != VT_BSTR)
		logResult(pin->getValue(pm[3].uid)->type,"VT_BSTR");
	//VT_URL
	if(pin->getValue(pm[4].uid)->type != VT_URL)
		logResult(pin->getValue(pm[4].uid)->type,"VT_URL");
	//VT_UINT
	if(pin->getValue(pm[6].uid)->type != VT_UINT)
		logResult(pin->getValue(pm[6].uid)->type,"VT_UINT");
	//VT_INT64
	if(pin->getValue(pm[7].uid)->type != VT_INT64)
		logResult(pin->getValue(pm[7].uid)->type,"VT_INT64");
	//VT_UINT64
	if(pin->getValue(pm[8].uid)->type != VT_UINT64)
		logResult(pin->getValue(pm[8].uid)->type,"VT_UINT64");
	//VT_FLOAT
	if(pin->getValue(pm[9].uid)->type != VT_FLOAT)
		logResult(pin->getValue(pm[9].uid)->type,"VT_FLOAT");
	//VT_DOUBLE
	if(pin->getValue(pm[10].uid)->type != VT_DOUBLE)
		logResult(pin->getValue(pm[10].uid)->type,"VT_DOUBLE");
	//VT_REF -- rerurning VT_REFID failing
	if(pin->getValue(pm[11].uid)->type != VT_REF)
		logResult(pin->getValue(pm[11].uid)->type,"VT_REF");
	//VT_REFID
	if(pin->getValue(pm[12].uid)->type != VT_REFID)
		logResult(pin->getValue(pm[12].uid)->type,"VT_REFID");

	//VT_REFIDPROP
	l_value = pin->getValue( pm[13].uid );
	// CHECK_EQU( l_value->type, VT_REFIDPROP ); // not really an error because of the way that we assign 
	CHECK_EQU( l_value->refId->id.ident, 0 );
	CHECK_EQU( l_value->refId->id.pid, pid1000.pid );
	CHECK_EQU( l_value->refId->pid, propOnRef );

	//VT_REFIDELT
	l_value = pin->getValue( pm[14].uid );
	CHECK_EQU( l_value->type, VT_REFIDELT );
	CHECK_EQU( l_value->refId->id.ident, 0 );
	CHECK_EQU( l_value->refId->id.pid, pid1001.pid );
	CHECK_EQU( l_value->refId->pid, propOnRef1 );
	CHECK_EQU( l_value->refId->eid, 1 );
	// CHECK_EQU( l_value->refId->vid, 0 ); // should this be valid?

	//VT_REFPROP
	if(pin->getValue(pm[15].uid)->type != VT_REFPROP)
		logResult(pin->getValue(pm[15].uid)->type,"VT_REFPROP");
	//VT_REFELT
	if(pin->getValue(pm[16].uid)->type != VT_REFELT)
		logResult(pin->getValue(pm[16].uid)->type,"VT_REFELT");
	//VT_ARRAY
	if(pin->getValue(pm[17].uid)->type != VT_ARRAY && pin->getValue(pm[17].uid)->type != VT_COLLECTION)
		logResult(pin->getValue(pm[17].uid)->type,"VT_ARRAY");
	//VT_COLLECTION
	if(pin->getValue(pm[18].uid)->type != VT_COLLECTION && pin->getValue(pm[18].uid)->type != VT_ARRAY)
		logResult(pin->getValue(pm[18].uid)->type,"VT_COLLECTION");
	//VT_BOOL
	if(pin->getValue(pm[19].uid)->type != VT_BOOL)
		logResult(pin->getValue(pm[19].uid)->type,"VT_BOOL");
	//VT_DATETIME
	if(pin->getValue(pm[20].uid)->type != VT_INTERVAL)
		logResult(pin->getValue(pm[20].uid)->type,"VT_INTERVAL");
	//VT_INTERVAL
	if(pin->getValue(pm[21].uid)->type != VT_DATETIME)
		logResult(pin->getValue(pm[21].uid)->type,"VT_DATETIME");
	//VT_STREAM
	if(pin->getValue(pm[22].uid)->type != VT_STREAM && pin->getValue(pm[22].uid)->type != VT_STRING)
		logResult(pin->getValue(pm[22].uid)->type,"VT_STREAM");
	//VT_STMT
	if(pin->getValue(pm[23].uid)->type != VT_STMT)
		logResult(pin->getValue(pm[23].uid)->type,"VT_STMT");
	//VT_EXPR
	if(pin->getValue(pm[24].uid)->type != VT_EXPR)
		logResult(pin->getValue(pm[24].uid)->type,"VT_EXPR");
	//VT_URIID
	if(pin->getValue(pm[25].uid)->type != VT_URIID)
		logResult(pin->getValue(pm[25].uid)->type,"VT_URIID");
	//VT_IDENTITY
	if(pin->getValue(pm[26].uid)->type != VT_IDENTITY)
		logResult(pin->getValue(pm[26].uid)->type,"VT_IDENTITY");
	MVTApp::output(*pin,mLogger.out(),session);
	pin->destroy();
}

void TestPersistence::modifyPins(ISession *session,URIMap *pm,int npm)
{
	IPIN *pin = session->getPIN(pid[0]);

	TVERIFY(pin!=NULL);
	if (pin==NULL) return;

	Value val[3];

	val[0].set("This property was modified");val[0].setPropID(pm[0].uid);
	val[1].set(float(6799.0001));val[1].setPropID(pm[9].uid);
	val[2].set(false);val[2].setPropID(pm[19].uid);

	TVERIFYRC(pin->modify(val,3));
	checkPeristence(session,pid,pm,sizeof(pm)/sizeof(pm[0]));
	pin->destroy();
}

void TestPersistence::refreshPins(ISession *session,URIMap *pm,int npm)
{
	IPIN *pin = session->getPIN(pid[0]);

	TVERIFY(pin!=NULL);
	if (pin==NULL) return;

	pin->refresh();
	checkPeristence(session,pid,pm,sizeof(pm)/sizeof(pm[0]));
	pin->destroy();
}

void TestPersistence::logResult(uint8_t type,string str)
{
	string retVal;
	switch(type)
	{
		case VT_STRING:
			retVal = "VT_STRING";break;
		case VT_BSTR:
			retVal = "VT_BSTR";break;
		case VT_INT:
			retVal = "VT_INT";break;
		case VT_URL:
			retVal = "VT_URL";break;
		case VT_INT64:
			retVal = "VT_INT64";break;
		case VT_UINT64:
			retVal = "VT_UINT64";break;
		case VT_FLOAT:
			retVal = "VT_FLOAT";break;
		case VT_DOUBLE:
			retVal = "VT_DOUBLE";break;
		case VT_REF:
			retVal = "VT_REF";break;
		case VT_REFID:
			retVal = "VT_REFID";break;
		case VT_REFIDPROP:
			retVal = "VT_REFIDPROP";break;
		case VT_REFIDELT:
			retVal = "VT_REFIDELT";break;
		case VT_REFPROP:
			retVal = "VT_REFPROP";break;
		case VT_REFELT:
			retVal = "VT_REFELT";break;
		case VT_ARRAY:
			retVal = "VT_ARRAY";break;
		case VT_COLLECTION:
			retVal = "VT_COLLECTION";break;
		case VT_BOOL:
			retVal = "VT_BOOL";break;
		case VT_INTERVAL:
			retVal = "VT_INTERVAL";break;
		case VT_DATETIME:
			retVal = "VT_DATETIME";break;
		case VT_STREAM:
			retVal = "VT_STREAM";break;
		case VT_STMT:
			retVal = "VT_STMT";break;
		case VT_EXPR:
			retVal = "VT_EXPR";break;
		case VT_URIID:
			retVal = "VT_URIID";break;
		case VT_IDENTITY:
			retVal = "VT_IDENTITY";break;
		default:
			retVal = "What the ??";break;
	}
	
	fout<<"Failed. Expected: "<<str<<" Found: "<<retVal<<"\n";
}

void TestPersistence::serial(ISession *session,PID *pid)
{
	IPIN *pin = session->getPIN(pid[0]);

	TVERIFY(pin!=NULL);
	if (pin==NULL) return;

	ifstream in;
	ofstream out;

	out.open("serial.log");
	MvStoreSerialization::ContextOutRaw lSerCtxout(out, *session);
	MvStoreSerialization::OutRaw::pin(lSerCtxout, *pin);
	out.close();

	unsigned int const sMode = session->getInterfaceMode();
	session->setInterfaceMode(sMode | ITF_REPLICATION);
	IPIN *newp = session->createUncommittedPIN();
	in.open("serial.log");
	MvStoreSerialization::ContextInRaw lSerCtxin(in, *session);
	MvStoreSerialization::InRaw::pin(lSerCtxin, *newp);
	session->setInterfaceMode(sMode);

	TVERIFYRC(session->commitPINs(&newp,1));
	pid[0] = newp->getPID();
	checkPeristence(session,pid,pm,sizeof(pm)/sizeof(pm[0]));
	in.close();

	pin->destroy();
	newp->destroy();
}
