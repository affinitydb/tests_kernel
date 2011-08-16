/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

// Publish this test.
class TestLength : public ITest
{
	public:
		RC mRCFinal;
		std::vector<PID> mTestPINs;
		enum OPS {PI_STRING,PI_URL,PI_UURL,PI_BSTR,PI_STREAM,PI_ALL};
		struct ExpRes
		{
			int expVTSTR;
			int expVTURL;
			int expVTUURL;
			int expVTBSTR;
			int expVTSTREAM;
		};
		TEST_DECLARE(TestLength);
		virtual char const * getName() const { return "testlength"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of OP_LENGTH"; }
		virtual bool isPerformingFullScanQueries() const { return true; } // Note: countPinsFullScan...
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testoplength(ISession *session,URIMap *pm, int npm,int op,ExpRes &expres);
		
		
};
TEST_IMPLEMENT(TestLength, TestLogger::kDStdOut);

// Implement this test.
int TestLength::execute()
{
	mRCFinal = RC_FALSE;
	if (MVTApp::startStore())
	{
		mRCFinal = RC_OK;
		ISession * const session = MVTApp::startSession();
		URIMap pm[21];
		ExpRes expres;
		expres.expVTURL=0;
		expres.expVTSTR=0;
		expres.expVTSTREAM=0;
		
		/*
		//PropMaps
		memset(pm,0,sizeof(pm));				
		pm[0].URI="country";
		pm[1].URI="city";
		pm[2].URI="street";
		pm[3].URI="email";
		pm[4].URI="firstname";
		pm[5].URI="lastname";
		pm[6].URI="comments"; //VT_STREAM
		pm[7].URI="age"; // int32_t / VT_INT
		pm[8].URI="somestring"; 
		pm[9].URI="single"; //boolean
		pm[10].URI="experience"; //VT_FLOAT
		pm[11].URI="salary"; //VT_DOUBLE
		pm[12].URI="ssn"; //int64_t/VT_INT64
		pm[13].URI="dob";  //datetime
		pm[14].URI="website"; //URL
		pm[15].URI="true_false"; //string
		pm[16].URI="VT_UINT"; // uint32_t / DWORD 
		pm[17].URI="VT_INT64";  // int64 
		pm[18].URI="VT_UINT64";  // uint64 
		pm[19].URI="VT_DATETIME"; // VT_DATETIME
		pm[20].URI="VT_INTERVAL"; // VT_INTERVAL
		session->mapURIs(sizeof(pm)/sizeof(pm[0]),pm);
		*/
		MVTApp::mapURIs(session,"TestLength.prop",21,pm);
		for (int i=0;i<10;i++){
			int op = rand()%PI_ALL;
			testoplength(session,pm,sizeof(pm)/sizeof(pm[0]),op,expres);
		}
		
		MVTApp::unregisterTestPINs(mTestPINs,session);
 		session->terminate();
		MVTApp::stopStore();
	}

 	return mRCFinal;
}
void TestLength::testoplength(ISession *session,URIMap *pm, int npm,int op,ExpRes &expres)
{
	Value pvs[3];
	PID pid;
	int res;
	Tstring outstr;
	Value args[1],argsfinal[2];
	IExprTree *exprfinal,*expr;
	ICursor *result;
	PropertyID pids[1];
	Value const *pvl;

	IStmt *query = session->createStmt();
	unsigned var = query->addVariable();

	SETVALUE(pvs[0], pm[0].uid, "Andromeda", OP_SET);
	SETVALUE(pvs[1], pm[1].uid, "city ZX V7", OP_SET);
	SETVALUE(pvs[2], pm[2].uid, "Galactico Street", OP_SET);

	session->createPIN(pid,pvs,3);
	IPIN *pin = session->getPIN(pid);
	MVTApp::registerTestPINs(mTestPINs,&pid);

	switch(op){
	case PI_STRING:
		SETVALUE(pvs[0], pm[0].uid, "Starshiptroopers", OP_SET);
		pin->modify(pvs,1);
		expres.expVTSTR++;
		pin->destroy();
		pids[0] = pm[0].uid;
		args[0].setVarRef(0,*pids);
		expr = session->expr(OP_LENGTH,1,args);
		argsfinal[0].set(expr);
		argsfinal[1].set(16);
		exprfinal = session->expr(OP_EQ,2,argsfinal);

		query->addCondition(var,exprfinal);
		TVERIFYRC(query->execute(&result));
		res = MVTApp::countPinsFullScan(result,session);
		result->destroy();
		exprfinal->destroy();
		query->destroy();
		res != expres.expVTSTR?mRCFinal=RC_FALSE:mRCFinal=RC_OK;
		break;
	case PI_URL:
		pvs[0].setURL("http://www.cricinfo.com");
		pvs[0].setPropID(pm[14].uid);
		pin->modify(pvs,1);
		expres.expVTURL++;
		pvl = pin->getValue(pm[14].uid);
		cout<<pvl->type<<endl;
		pin->destroy();
		pids[0] = pm[14].uid;
		args[0].setVarRef(0,*pids);
		expr = session->expr(OP_LENGTH,1,args);
		argsfinal[0].set(expr);
		argsfinal[1].set(23);
		exprfinal = session->expr(OP_EQ,2,argsfinal);

		query->addCondition(var,exprfinal);
		TVERIFYRC(query->execute(&result));
		res = MVTApp::countPinsFullScan(result,session);
		result->destroy();
		exprfinal->destroy();
		query->destroy();
	 	res != expres.expVTURL?mRCFinal=RC_FALSE:mRCFinal=RC_OK;
	break;
	case PI_STREAM:
		if(pin != NULL) pin->destroy();
		if(query != NULL) query->destroy();
		break;
		//need to investigate this 
		/*MVTRand::getString(outstr,20000,100000);
		pvs[0].set(pm[6].uid,outstr.c_str());
		rc = pin->modify(pvs,1);
		pvl = pin->getValue(pm[6].uid);
		cout<<pvl->val.type<<endl;
		pvl->val.length;
		expres.expVTSTREAM++;
		
		pids[0] = pm[6].uid;
		args[0].setVarRef(0,*pids);
		expr = session->expr(OP_LENGTH,1,args);
		argsfinal[0].set(expr);
		argsfinal[1].set(50000);
		exprfinal = session->expr(OP_EQ,2,argsfinal);

		query->addCondition(exprfinal);
		result = query->execute();
		res = MVTApp::countPinsFullScan(result,session);
		query->destroy();

		res != expres.expVTSTREAM?mRCFinal=RC_FALSE:mRCFinal=RC_OK;*/
	case PI_UURL:
		if(pin != NULL) pin->destroy();
		if(query != NULL) query->destroy();
		break;
	case PI_BSTR:
		if(pin != NULL) pin->destroy();
		if(query != NULL) query->destroy();
		break;
	default:
		if(pin != NULL) pin->destroy();
		if(query != NULL) query->destroy();
		break;
	}
}
