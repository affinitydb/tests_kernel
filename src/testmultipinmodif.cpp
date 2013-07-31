/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

#include <map>
#define NOFPINS 5000
#define NMODPINS 500
using namespace std;

// Publish this test.
class TestMultiPinModif : public ITest
{
	//TODO::
	//1. Create multiple threads.
	//2. Params
	//3. copyPINs()
	//4. recovery
	//5. IStream and other datatypes
	public:
		PropertyID pm[6];
		Tstring lRandStr;
		ClassID cls,clsd;
		map<int,Tstring> qmap;//to run multiple times w.o failure.
		IStmt *query;
		TEST_DECLARE(TestMultiPinModif);
		virtual char const * getName() const { return "testmultipinmodif"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of multiple pin modifications"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "takes very long time"; return false; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual bool isLongRunningTest() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void testmultimodif(ISession *session);
		void testmultidelete(ISession *session);
		void testmulticopy(ISession *session);
		void createPINS(ISession *session,PropertyID *pm,int npm,const int qCase);
		IExprTree * createExprTree(ISession *session, IStmt &query,const int qCase);
		bool verifyModif(ISession *session, int qCase,Tstring serstr);
};
TEST_IMPLEMENT(TestMultiPinModif, TestLogger::kDStdOut);

// Implement this test.
#define lAllClassNotifs (CLASS_NOTIFY_JOIN | CLASS_NOTIFY_LEAVE | CLASS_NOTIFY_CHANGE | CLASS_NOTIFY_DELETE | CLASS_NOTIFY_NEW)
int TestMultiPinModif::execute()
{
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		MVTRand::getString(lRandStr,10,10,false,false);
		MVTApp::mapURIs(session,"TestMultiPinModif.prop",6,pm);

		testmultimodif(session);
		testmultidelete(session);
#if 0 
		if(!MVTApp::isRunningSmokeTest()) testmulticopy(session);
#endif
		qmap.empty();

		session->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
 	return RC_OK;
}

void TestMultiPinModif::testmultimodif(ISession *session)
{
	Value val[3];
	Tstring str,str1,str2;
	uint64_t cnt;
	query = session->createStmt(STMT_UPDATE);
	createPINS(session,pm,sizeof(pm)/sizeof(pm[0]),0);
	IExprTree *expr = createExprTree(session,*query,0);

	//case 0 FS1: simple EQ query and update multiple property of a pin.
	
	MVTRand::getString(str,30,0,false);
	val[0].set(str.c_str());val[0].setPropID(pm[0]);
	val[0].meta  = META_PROP_FTINDEX;
	val[1].set(123456);val[1].setPropID(pm[3]);
	query->addCondition(0,expr);
	query->setValues(val,2);
	TVERIFYRC(query->count(cnt));
	TVERIFYRC(query->execute());
	
	TVERIFY(verifyModif(session,0,str));

	expr->destroy();
	query->destroy();
	//case 1: FullText query. and add a collection property
	query = session->createStmt(STMT_UPDATE);
	createPINS(session,pm,sizeof(pm)/sizeof(pm[0]),1);
	expr = createExprTree(session,*query,1);
	
	MVTRand::getString(str,30,0,false);
	MVTRand::getString(str1,30,0,false);
	MVTRand::getString(str2,30,0,false);

	val[0].set(str.c_str());val[0].setPropID(pm[3]);val[0].eid=STORE_LAST_ELEMENT;val[0].op=OP_ADD;
	val[1].set(str1.c_str());val[1].setPropID(pm[3]);val[1].eid=STORE_LAST_ELEMENT;val[1].op=OP_ADD;
	val[2].set(str2.c_str());val[2].setPropID(pm[3]);val[2].eid=STORE_LAST_ELEMENT;val[2].op=OP_ADD;
	val[0].meta = val[1].meta = val[2].meta = META_PROP_FTINDEX;
	query->setValues(val,3);
	
	TVERIFYRC(query->count(cnt));
	TVERIFYRC(query->execute());
	
	TVERIFY(verifyModif(session,1,str1));
	query->destroy();

	//case 2: Class query and modify pins so that they leave the class.
	query = session->createStmt();
	createPINS(session,pm,sizeof(pm)/sizeof(pm[0]),3);
	expr = createExprTree(session,*query,3);
	MVTRand::getString(str,45,0,true);
	MVTRand::getString(str1,60,0,false);
	MVTRand::getString(str2,15,0,true);
	
	val[0].set(str.c_str());val[0].setPropID(pm[0]);
	val[1].set(str1.c_str());val[1].setPropID(pm[1]);
	val[2].set(str2.c_str());val[2].setPropID(pm[2]);
	val[0].meta = val[1].meta = val[2].meta = META_PROP_FTINDEX;
	
	IStmt *clsq = session->createStmt(STMT_UPDATE);
	SourceSpec csp;
	csp.objectID=cls;
	csp.nParams=0;
	csp.params=NULL;
	clsq->addVariable(&csp,1);
	clsq->setValues(val,3);
	TVERIFYRC(clsq->count(cnt));
	TVERIFYRC(clsq->execute());

	TVERIFY(verifyModif(session,3,str1));
	clsq->destroy();
	query->destroy();

	//case 3: combo of FT and class query.
	query = session->createStmt();
	createPINS(session,pm,sizeof(pm)/sizeof(pm[0]),3);
	expr = createExprTree(session,*query,3);
	IStmt *comboq = session->createStmt(STMT_UPDATE);
	csp.objectID=cls;
	csp.nParams=0;
	csp.params=NULL;
	unsigned char var2 = comboq->addVariable(&csp,1);
	comboq->setConditionFT(var2,"multimodftstr");
	val[0].setDelete(pm[0]);
	comboq->setValues(val,1);
	TVERIFYRC(comboq->count(cnt));
	TVERIFYRC(comboq->execute());

	comboq->destroy();
	query->destroy();

	//case 4: effect of transactions on multimodif.
#if 1
	query = session->createStmt(STMT_UPDATE);
	createPINS(session,pm,sizeof(pm)/sizeof(pm[0]),4);
	expr = createExprTree(session,*query,4);
	MVTRand::getString(str,20,0,false);
	val[0].set(str.c_str());val[0].setPropID(pm[0]);
	TVERIFYRC(query->addCondition(0,expr));
	TVERIFYRC(query->setValues(val,1));
	TVERIFYRC(query->count(cnt));

	//start a transaction and rollback
	TVERIFYRC(session->startTransaction());
	TVERIFYRC(query->execute());
	TVERIFYRC(session->rollback());

	TVERIFY(verifyModif(session,4,str));
	expr->destroy();
	query->destroy();
#endif
    
}
void TestMultiPinModif::createPINS(ISession *session,PropertyID *pm,int npm,const int qCase)
{
	Value val[4];
	Tstring str,str1,str2;

	//Case0: simple OP_EQ and full scan modify.
	switch(qCase){
		case (0):
		case (1):
		case (4):
		case (5):
		case (7):
		case (8):
			MVTRand::getString(str,20,0,false);
			MVTRand::getString(str1,30,0,true);
			MVTRand::getString(str2,20,0,false);
			qmap.insert(map<int,Tstring>::value_type(qCase,str));
			std::cout<<str<<" while insert"<<std::endl;
			for(int i=0;i<NOFPINS;i++){
				val[0].set(str.c_str());val[0].setPropID(pm[0]);
				val[1].set(str1.c_str());val[1].setPropID(pm[1]);
				val[2].set(str2.c_str());val[2].setPropID(pm[2]);
				val[0].meta = val[1].meta = val[2].meta = META_PROP_FTINDEX;
				TVERIFYRC(session->createPIN(val,3,NULL,MODE_PERSISTENT|MODE_COPY_VALUES));
			}
			break;
		case (2):
		case (6):
			MVTRand::getString(str,20,0,false);
			MVTRand::getString(str1,30,0,true);
			MVTRand::getString(str2,20,0,false);
			qmap.insert(map<int,Tstring>::value_type(qCase,str2));
			for(int i=0;i<NOFPINS;i++){
				val[0].set(str.c_str());val[0].setPropID(pm[0]);
				val[1].set(str1.c_str());val[1].setPropID(pm[1]);
				val[2].set(str2.c_str());val[2].setPropID(pm[2]);
				val[3].set((unsigned)123456);val[3].setPropID(pm[4]);
				val[0].meta = val[1].meta = val[2].meta =META_PROP_FTINDEX;
				TVERIFYRC(session->createPIN(val,4,NULL,MODE_PERSISTENT|MODE_COPY_VALUES));
			}
			break;
		case (3):
			MVTRand::getString(str,20,0,false);
			MVTRand::getString(str1,30,0,true);
			MVTRand::getString(str2,20,0,false);
			qmap.insert(map<int,Tstring>::value_type(qCase,str1));
			for(int i=0;i<NOFPINS;i++){
				val[0].set(str.c_str());val[0].setPropID(pm[0]);
				val[1].set(str1.c_str());val[1].setPropID(pm[1]);
				val[2].set("multimodftstr");val[2].setPropID(pm[2]);
				val[3].setURL("http://www.vmware.com");val[3].setPropID(pm[5]);
				val[0].meta = val[1].meta = val[2].meta = val[3].meta = META_PROP_FTINDEX;
				TVERIFYRC(session->createPIN(val,4,NULL,MODE_PERSISTENT|MODE_COPY_VALUES));
			}
			break;
		default:
			break;
		}
}

IExprTree * TestMultiPinModif::createExprTree(ISession *session, IStmt &query,const int qCase)
{
	cls = STORE_INVALID_CLASSID;
	clsd = STORE_INVALID_CLASSID;
	PropertyID pids[1],pids1[1];
	Value args[2],args1[2],argsfinal[2];
	IExprTree *expr = NULL, *exprexst = NULL, *expreq = NULL;
	unsigned char var;
	char lB[100];
	RC rc;
	map<int,Tstring>::const_iterator iter;

	switch(qCase){
		case (0):
		case (5):
		case (8):
			var = query.addVariable();
			pids[0]=pm[0];
			args[0].setVarRef(0,*pids);
			iter = qmap.find(qCase);
			args[1].set(iter->second.c_str());
			std::cout<<iter->second.c_str()<<" while query"<<std::endl;
			expr = session->expr(OP_EQ,2,args);
			break;
		case (1):
			var = query.addVariable();
			iter = qmap.find(qCase);
			query.setConditionFT(var,iter->second.c_str());
			break;
		case (2):
			var = query.addVariable();
			pids[0]=pm[4];
			args[0].setVarRef(0,*pids);
			exprexst = session->expr(OP_EXISTS,1,args);

			pids1[0]=pm[1];
			args1[0].setVarRef(0,*pids1);
			iter = qmap.find(qCase);
			args1[1].set(iter->second.c_str());
			expreq = session->expr(OP_EQ,2,args1);

			argsfinal[0].set(exprexst);
			argsfinal[1].set(expreq);
			expr = session->expr(OP_LAND,2,argsfinal);
			query.addCondition(var,expr);	
			sprintf(lB,"TestMultiPINModif.%s.MULTIMODIF", lRandStr.c_str());
			rc = defineClass(session,lB,&query,&cls);
			TVERIFY((RC_ALREADYEXISTS == rc) || (RC_OK == rc)); 
			TVERIFYRC(rc = session->enableClassNotifications(cls,lAllClassNotifs));
			break;
		case (3):
			var = query.addVariable();
			pids[0]=pm[5];
			args[0].setVarRef(0,*pids);
			expr = session->expr(OP_EXISTS,1,args);
			query.addCondition(var,expr);
			sprintf(lB,"TestMultiPINModif.%s.MULTIMODIF1", lRandStr.c_str());
			rc = defineClass(session,lB,&query,&cls);
			TVERIFY((RC_ALREADYEXISTS == rc) || (RC_OK == rc)); 
			TVERIFYRC(rc = session->enableClassNotifications(cls,lAllClassNotifs));
			break;
		case (4):
		case (7):
			var = query.addVariable();
			pids[0]=pm[0];
			args[0].setVarRef(0,*pids);
			iter = qmap.find(qCase);
			args[1].set(iter->second.c_str());
			expr = session->expr(OP_EQ,2,args);
			break;
		case (6):
			var = query.addVariable();
			pids[0]=pm[2];
			args[0].setVarRef(0,*pids);
			exprexst = session->expr(OP_EXISTS,1,args);

			query.addCondition(var,exprexst);
			sprintf(lB,"TestMultiPINModif.%s.MULTIDELETE", lRandStr.c_str());
			rc = defineClass(session,lB,&query,&clsd);
			TVERIFY((RC_ALREADYEXISTS == rc) || (RC_OK == rc));
			TVERIFYRC(rc = session->enableClassNotifications(clsd,lAllClassNotifs));
			break;
		default:
			break;
	}
	return expr;
}
bool TestMultiPinModif::verifyModif(ISession *session, int qCase,Tstring serstr)
{
	bool result=true;
	IStmt *modquery = session->createStmt();
	unsigned char var;
	uint64_t cnt;
	
	map<int,Tstring>::const_iterator iter;

	switch(qCase){
		case (0):
		case (1):
		case (2):
			var = modquery->addVariable();
			modquery->setConditionFT(var,serstr.c_str(),0,pm,6);
			TVERIFYRC(modquery->count(cnt));
			if (NOFPINS != cnt)
			{
				TVERIFY(false) ; 
				mLogger.out() << "Expected " << NOFPINS << " got " << cnt << endl ;
				result = false;
			}
			break;
		case (3):
			//get a random pin and check for number of props in it
			return true;
		case (4):
			var = modquery->addVariable();
			modquery->setConditionFT(var,serstr.c_str(),0,pm,6);
			TVERIFYRC(modquery->count(cnt));
			if (0 != cnt)
			{
				TVERIFY(false) ; mLogger.out() << "Expected no matches, got " << cnt << endl ;
				result = false;
			}
			break;
		case (5):
		case (6):
		case (7):
			var = modquery->addVariable();
			iter = qmap.find(qCase);
			modquery->setConditionFT(var,iter->second.c_str(),0,pm,6);
			TVERIFYRC(modquery->count(cnt));
			if(7 == qCase) //transaction rolled back, so all the pins are expected.
			{	if( NOFPINS != cnt)
				{
					TVERIFY(false) ; mLogger.out() << "Expected no matches, got " << cnt << endl ;
					result = false;
				}
				else
					return true;
			}
			if (0 != cnt)
			{
				TVERIFY(false) ; mLogger.out() << "Expected no matches, got " << cnt << endl ;
				result = false;
			}
			break;
		case (8):
			var = modquery->addVariable();
			iter = qmap.find(qCase);
			modquery->setConditionFT(var,iter->second.c_str(),0,pm,6);
			TVERIFYRC(modquery->count(cnt));
			if ((NOFPINS * 2) != cnt)
			{
				TVERIFY(false) ; mLogger.out() << "Expected " << (NOFPINS * 2) << " matches, got " << cnt << endl ;
				result = false;
			}
			break;
		default:
			return false;
	}
	modquery->destroy();
	return result;
}

void TestMultiPinModif::testmultidelete(ISession *session)
{
	uint64_t cnt;
	query = session->createStmt(STMT_DELETE);
	//case 5: delete of "normal" pins.(not belonging to any class)
	createPINS(session,pm,sizeof(pm)/sizeof(pm[0]),5);
	IExprTree *expr = createExprTree(session,*query,5);
	query->addCondition(0,expr);
	query->count(cnt);
	TVERIFYRC(query->execute());
	TVERIFY(verifyModif(session,5,""));
	expr->destroy();
	query->destroy();

	//case 6: delete pins which belong to a class.
	query = session->createStmt();
	createPINS(session,pm,sizeof(pm)/sizeof(pm[0]),6);
	expr = createExprTree(session,*query,6);

	IStmt *clsquery = session->createStmt(STMT_DELETE);
	SourceSpec csp;
	csp.objectID=clsd;
	csp.nParams=0;
	csp.params=NULL;

	clsquery->addVariable(&csp,1);
	TVERIFYRC(clsquery->count(cnt));

	TVERIFYRC(clsquery->execute());
	TVERIFY(verifyModif(session,6,""));
	query->destroy();
	clsquery->destroy();
//#if 0 //(enabled now, works fine.)
	//case 7: effect of a transaction on a multiple delete.
	query = session->createStmt(STMT_DELETE);
	createPINS(session,pm,sizeof(pm)/sizeof(pm[0]),7);
	expr = createExprTree(session,*query,7);
	session->startTransaction();
	TVERIFYRC(query->execute());
	TVERIFYRC(session->rollback());
	TVERIFY(verifyModif(session,7,""));
	query->destroy();
//#endif
}

void TestMultiPinModif::testmulticopy(ISession *session)
{
	//case 8: copy "normal pins"
	std::cout << "while copyPINs(...)... " << endl; 
	
        uint64_t cnt = 0;
	query = session->createStmt(STMT_INSERT);
	
	createPINS(session,pm,sizeof(pm)/sizeof(pm[0]),8);
	IExprTree *expr = createExprTree(session,*query,8);
	TVERIFYRC(query->addCondition(0,expr));
	TVERIFYRC(query->count(cnt));
	/* restricting the copy to a desired no, as the copyPINs() create pins in a loop 
	as long as the query condition is satisfied*/
	TVERIFYRC(query->execute(NULL,0,0,NOFPINS));
	TVERIFY(verifyModif(session,8,""));
	expr->destroy();
	query->destroy();
}
