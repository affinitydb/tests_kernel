/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <map>
#define OutputString(a)	fprintf(stdout,a)

using namespace std;

// Publish this test.
class TestClone : public ITest
{
	public:	
		TEST_DECLARE(TestClone);
		virtual char const * getName() const { return "testclone"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of clone methods"; }
		virtual bool isPerformingFullScanQueries() const { return true; } // Note: countPinsFullScan...
		virtual bool includeInPerfTest() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }
		
	protected:
		void pinClone(ISession *session);
		void streamClone(ISession *session);
		void CNavigClone(ISession *session);
		void IQueryClone(ISession *session);
		void ACLPinClone(ISession *session);
		void PiExprTreeClone(ISession *session);
	protected:
		void comparePIN(ISession *session,IPIN *pin,const map<Afy::PropertyID,Afy::Value> &pvl,unsigned nLoops);
		bool comparePIN2(IPIN* pin, IPIN* pin2,ISession *session ) ;
		void reportResult(ICursor *result,ISession *session);
};
TEST_IMPLEMENT(TestClone, TestLogger::kDStdOut);

// Implement this test.
int TestClone::execute()
{
	if (!MVTApp::startStore()) return RC_FALSE;

	ISession * const session = MVTApp::startSession();
	
	pinClone(session);
	CNavigClone(session);
	IQueryClone(session);
	ACLPinClone(session);
	PiExprTreeClone(session);

	session->terminate();
	MVTApp::stopStore();

	return RC_OK  ;
}

void TestClone::pinClone(ISession *session)
{
	IPIN *pin=NULL,*pin1=NULL,*pin2=NULL;
									  
	static const unsigned nLoops =250;//increased to higher values to demonstrate larger collections
	PropertyID lPropIDs[nLoops];
	MVTApp::mapURIs(session,"TestClone.pinClone",nLoops,lPropIDs);

	Value pv;
	Value pvs[1000];
	const Value *pval;
	RC rc;
	PID pid;
	char *p,buff[3000];

	map<Afy::PropertyID,Afy::Value> pvl;
	strcpy(buff,"");
	//case 1: Uncommited pin clone.
	pin = session->createPIN();
	//add number of prop values
	std::string buf = "http://www.yehhaicricket.com/india/Rahuld/rahul.html";
	for (unsigned i=0;i<nLoops;i++) {
		buf  += rand()%26+((rand()&1)!=0?'a':'A');
		char *bufnew = new char[buf.length() + 1]; // REVIEW: Should this use ISession::alloc to properly match
												  // the way it will be freed?
		strcpy(bufnew,buf.c_str());
		pv.setURL(bufnew); pv.setPropID(lPropIDs[i]);
		//insert prop value pair into the map
		pvl.insert(map<Afy::PropertyID,Afy::Value>::value_type(lPropIDs[i],pv));
		TVERIFYRC(pin->modify(&pv,1));
	}
	TVERIFYRC(session->commitPINs(&pin,1));
	pin1 = pin->clone(0,0,MODE_NEW_COMMIT);
	TVERIFY(pin1->isPersistent());
	TVERIFY(pin1->getPID().pid != STORE_INVALID_PID ) ;
	comparePIN(session,pin1,pvl,nLoops);
	comparePIN2(pin,pin1,session);

	//clone without the MODE_NEW_COMMIT
	pin2 = pin->clone() ;
	TVERIFY(!pin2->isPersistent() ) ;
	TVERIFY(pin2->getPID().pid == STORE_INVALID_PID ) ;
	comparePIN(session,pin2,pvl,nLoops);
	TVERIFYRC(session->commitPINs(&pin2,1)) ;
	comparePIN2(pin,pin2,session);

	pin->destroy();
	pin1->destroy();
	pin2->destroy();
	pvl.clear();

	//case 2: clone collection
	URIMap pm[3];
	memset(pm,0,3*sizeof(URIMap));

	pm[0].URI = "TestClone.pinClone.City";
	pm[1].URI = "TestClone.pinClone.State";
	pm[2].URI = "TestClone.pinClone.Zip";

	session->mapURIs(3,pm);

	SETVALUE(pvs[0], pm[0].uid, "Bangalore", OP_SET);
	SETVALUE(pvs[1], pm[1].uid, "Karnataka", OP_SET);
	SETVALUE(pvs[2], pm[2].uid, 560234, OP_SET);

	session->createPINAndCommit(pid,pvs,2);
	pin = session->getPIN(pid);

	//add nLoops more elements to City property 
	for (unsigned i = 0; i<nLoops; i++) {
		p=buff+strlen(buff);
		*p++=rand()%26+((rand()&1)!=0?'a':'A');
		*p='\0';
		SETVALUE_C(pvs[0], pm[0].uid, buff, OP_ADD, STORE_LAST_ELEMENT);
		rc =pin->modify(pvs,1);
	}
	pin1 = pin->clone(0,0,MODE_NEW_COMMIT);
	
	size_t x = MVTApp::getCollectionLength(*pin1->getValue(pm[0].uid));
	TVERIFY( x == (nLoops+1) && pin1->isPersistent()) ;

	comparePIN2(pin, pin1,session ) ;

	pin->destroy();
	pin1->destroy();

	// case 3: Clone Big collection (only fails with large nLoops)
	session->createPINAndCommit(pid,NULL,0);
	pin = session->getPIN(pid);

	for (unsigned i = 0; i<nLoops; i++) {
		p=buff+strlen(buff);
		*p++=rand()%26+((rand()&1)!=0?'a':'A');
		*p='\0';
		SETVALUE_C(pvs[0], pm[0].uid, buff, OP_ADD, STORE_LAST_ELEMENT);
		pvs[0].meta |= META_PROP_SSTORAGE ;
		rc =pin->modify(pvs,1);
	}

	pin1 = pin->clone(0,0,MODE_NEW_COMMIT);

	TVERIFY( nLoops == MVTApp::getCollectionLength(*pin1->getValue(pm[0].uid)));
	TVERIFY( pin1->isPersistent()) ;

	comparePIN2(pin, pin1,session ) ;

	pin1->destroy() ;
	pin->destroy() ;	

	//case 4: for overwrite values(simple)
	pin =  session->createPIN();
	SETVALUE(pvs[0], lPropIDs[1], "Jaanu meri jaan", OP_SET);
	SETVALUE(pvs[1], lPropIDs[2], "Mein tere qurbaan", OP_SET);
	pvl.insert(map<Afy::PropertyID,Afy::Value>::value_type(lPropIDs[2],pvs[1]));
	pin->modify(pvs,2);
	
	SETVALUE(pvs[0], lPropIDs[1], "jeete hain Shaan se", OP_SET);
	pvl.insert(map<Afy::PropertyID,Afy::Value>::value_type(lPropIDs[1],pvs[0]));
	pin1= pin->clone(pvs,1,MODE_NEW_COMMIT);
	comparePIN(session,pin1,pvl,2);
	// The clone is committed but the original is not
	TVERIFY(pin1->isPersistent() ) ;	
	pin->refresh() ;
	TVERIFY(!pin->isPersistent() ) ;
//	MVTApp::output(*pin1,mLogger.out(),session);	
	TVERIFYRC(session->commitPINs(&pin,1,0)) ;

	// pins are different because of argument to clone method:
	TVERIFY(0==strcmp("Jaanu meri jaan", pin->getValue(lPropIDs[1])->str)) ;
	TVERIFY(0==strcmp("jeete hain Shaan se", pin1->getValue(lPropIDs[1])->str)) ;

	pin->destroy();
	pin1->destroy();

	//case 4: for overwrite values(OP_DELETE, OP_EDIT, at specific collection element).
	SETVALUE(pvs[0], pm[0].uid, "Rahul the WALL Dravid", OP_SET);
	SETVALUE(pvs[1], pm[1].uid, "Karnataka da huli", OP_SET);
	SETVALUE(pvs[2], pm[2].uid, 7900, OP_SET);

	session->createPINAndCommit(pid,pvs,3);
	pin = session->getPIN(pid);
	//OP_DELETE
	pvs[0].setDelete(pm[1].uid);
	pin1 = pin->clone(pvs,1,MODE_NEW_COMMIT);
	MVTApp::output(*pin1,mLogger.out(),session);
	x=pin1->getNumberOfProperties();
	TVERIFY(x==2) ; TVERIFY( pin1->isPersistent()) ;
	pin->destroy();
	pin1->destroy();

	//Collection Manips
	SETVALUE(pvs[0], pm[0].uid, "Rahul the WALL Dravid", OP_SET);
	SETVALUE_C(pvs[1], pm[0].uid, "Karnataka da huli", OP_ADD_BEFORE, STORE_LAST_ELEMENT);
	SETVALUE_C(pvs[2], pm[0].uid, "Indiranagar", OP_ADD_BEFORE, STORE_LAST_ELEMENT);
	SETVALUE_C(pvs[3], pm[0].uid, "St Josephs Collge of commerce", OP_ADD, STORE_LAST_ELEMENT);
	rc = session->createPINAndCommit(pid,pvs,4);
	pin = session->getPIN(pid);
	MVTApp::output(*pin,mLogger.out(),session);

	//1)modify a specifc colection el -- cnavig
	pval = pin->getValue(pm[0].uid);
	if (pval->type==VT_COLLECTION) {
		SETVALUE_C(pvs[0], pm[0].uid, "India Vs Pakistan - 2005", OP_SET, pval->nav->navigate(GO_FIRST)->eid);
	} else if (pval->type==VT_ARRAY) {
		SETVALUE_C(pvs[0], pm[0].uid, "India Vs Pakistan - 2005", OP_SET, pval->varray[0].eid);
	} else
		TVERIFYRC(RC_TYPE);
		
	pin1 = pin->clone(pvs,1,MODE_NEW_COMMIT);
	pin1->refresh();
	MVTApp::output(*pin1,mLogger.out(),session);
	//MVTApp::output((*pin1->getValue(pm[0].uid), mLogger.out()); mLogger.out() << std::endl;
	//TODO : after clarification impl VT_ARRAY
	pin1->destroy();
	pin->destroy();
	
	//case 1 : Big PINS
#if 0
	session->createPIN();
	for (unsigned i=0; i<100; i++){ //>100 no yet impl. ln:403,modifypin.cpp
		pvs[i].set(i,"this is some string");
	}
	rc = pin->modify(pvs,100);

	pin1 = pin->clone(0,0,MODE_NEW_COMMIT);
	TVERIFY(pin1->getNumberOfProperties() ==i ) ;
	TVERIFY(pin1->isPersistent()) ;
#endif
	
	/*todo
	1. .Add more meat to Big PINS.*/
}

void TestClone::CNavigClone(ISession *session)
{
	IPIN *pin;
	PropertyID lPropIDs[1];
	MVTApp::mapURIs(session,"TestClone.CNavigClone",1,lPropIDs);
	PropertyID propID = lPropIDs[0];
	RC rc;
	Value pvs[2];
	Value const *pv;
	PID pid;

	SETVALUE_C(pvs[0], propID, "add0", OP_ADD, STORE_LAST_ELEMENT);
	SETVALUE_C(pvs[1], propID, "add1", OP_ADD, STORE_LAST_ELEMENT);
	rc =  session->createPINAndCommit(pid,pvs,2);
	pin = session->getPIN(pid);
	pin->refresh();
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
	
	pv = pin->getValue(propID);
	if (pv->type==VT_COLLECTION) {
		SETVALUE_C(pvs[0], propID, pv->nav, OP_ADD, STORE_LAST_ELEMENT);
	} else if (pv->type==VT_ARRAY) {
		pvs[0].set((Value*)pv->varray,pv->length); pvs[0].setPropID(propID); pvs[0].op=OP_ADD; pvs[0].eid=STORE_LAST_ELEMENT;
	} else TVERIFYRC(RC_TYPE);
	session->createPINAndCommit(pid,pvs,1);	
	pin= session->getPIN(pid);
	pin->refresh();
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;

	//modify using CNavig coll of a given element
	if (pv->type==VT_COLLECTION) {
		SETVALUE_C(pvs[0], propID, pv->nav->clone(), OP_ADD, STORE_LAST_ELEMENT);
		rc = pin->modify(pvs,1);
	} else if (pv->type==VT_ARRAY) {
		rc = pin->modify(pvs,2);
	} else
		TVERIFYRC(RC_TYPE);

	pin->refresh();
	MVTApp::output(*pin->getValue(propID), mLogger.out()); mLogger.out() << std::endl;
	pin->destroy();
	/*colnav->destroy();
	colnav1->destroy();*/ //throws an assert
	//Invalid Address specified to RtlValidateHeap( 00320000, 0094BA48 )
}

void TestClone::IQueryClone(ISession *session)
{
	IStmt *query, *queryclone;
	Value pvs[3];
	PID pid;
	Value args[2];//,argsfinal[2];
	IExprTree *expr;//,*exprfinal
	ICursor *result;
	PropertyID pids[3];
	MVTApp::mapURIs(session,"TestClone.IQueryClone",3,pids);	
	RC rc;

	//create cpl pins to check simple query.
	pvs[0].set("Lovely Liv Tyler");pvs[0].setPropID(pids[0]);
	pvs[1].set("Steven Tyler aka Aerosmith ki beti");pvs[1].setPropID(pids[1]);
	rc = session->createPINAndCommit(pid,pvs,2);

	pvs[0].set("Amrageddon");pvs[0].setPropID(pids[0]);
	pvs[1].set("Lord of the Rings");pvs[1].setPropID(pids[1]);
	rc = session->createPINAndCommit(pid,pvs,2);

	query = session->createStmt();
	unsigned var = query->addVariable();	
	
	args[0].setVarRef(0,pids[0]);
	args[1].set("Amrageddon");
	expr = session->expr(OP_EQ,2,args/*,CASE_INSENSITIVE_OP*/);
	query->addCondition(var,expr);

	//Get the query string
	const char *lQStr = query->toString();
	std::cout<<"The query string "<<lQStr<<std::endl;
	uint64_t lQCnt; query->count(lQCnt);
	
	//clone the query and add more conditions to it.
	queryclone = query->clone(); 
	
	//Get the query clone string
	const char *lQCStr = queryclone->toString();
	std::cout<<"The queryclone string  "<<lQCStr<<std::endl;
	uint64_t lQCCnt; queryclone->count(lQCCnt);
	
	TVERIFY(strcmp(lQStr,lQCStr) == 0) ; TVERIFY(lQCnt == lQCCnt) ;

	TVERIFYRC(queryclone->execute(&result)); 
	IPIN * cloneFirstResult = result->next() ;
	TVERIFY(cloneFirstResult != NULL);
	cloneFirstResult->destroy() ; cloneFirstResult = NULL ;
	result->destroy();
	//reportResult(result,session);
	
	args[0].setVarRef(0,pids[1]);
	args[1].set("Lord of the Rings");
	expr = session->expr(OP_EQ,2,args,CASE_INSENSITIVE_OP);
	
	queryclone->addCondition(var,expr);
	TVERIFYRC(queryclone->execute(&result));

	cloneFirstResult = result->next() ;
	TVERIFY(cloneFirstResult != NULL);
	cloneFirstResult->destroy() ;
	expr->destroy();
	result->destroy();	
	queryclone->destroy();
	query->destroy();

	//case 2
	/*argsfinal[0].set(queryclone);
	argsfinal[0].set(expr);
	exprfinal = session->expr(OP_LOR,2,args);*/
}

void TestClone::streamClone(ISession *session)
{
	/*IPIN *pin,*pin1;
	session->createPIN();
	Value pvs[3];
	Tstring streamstr;

	const sesmode = session->getInterfaceMode();
	session->setInterfaceMode(sesmode | ITF_FORCE_STR_STREAM);

	IStream *istr;*/
}

void TestClone::ACLPinClone(ISession *session)
{
	//create ids, set rights, clone and recheck on the cloned pin.
	//create a cpl ids.
	IdentityID iid,iid1;
	Value pvs[2];
	PID pid;
	Value const *pv;
	PropertyID lPropIDs[1];
	MVTApp::mapURIs(session,"TestClone.ACLPinClone",1,lPropIDs);


	iid = session->storeIdentity("Julia Roberts",NULL,0);
	iid1 = session->storeIdentity("Meg Ryan",NULL,0);
	//Assign acl at creation for PIN1

	pvs[0].setIdentity(iid);
	SETVATTR_C(pvs[0], PROP_SPEC_ACL, OP_ADD, STORE_LAST_ELEMENT);
	pvs[0].meta = META_PROP_READ | META_PROP_WRITE;

	SETVALUE(pvs[1], lPropIDs[0], "This is a test for clone and ACLS", OP_ADD);
	TVERIFYRC(session->createPINAndCommit(pid,pvs,2));

	IPIN *pin = session->getPIN(pid);
	IPIN *pin1 = pin->clone(0,0,MODE_NEW_COMMIT);
	
	//check the rights on the pin
	pv = pin1->getValue(PROP_SPEC_ACL);

	//REVIEW: Are || meant instead of &&?
	if(pv->meta != (META_PROP_READ | META_PROP_WRITE) && pv->iid != iid && !pin1->isPersistent())
		TVERIFY2(0,"Invalid rights") ;

	//modify the pin, change rights, clone and check again using overwrite values
	pvs[0].setIdentity(iid1);
	SETVATTR_C(pvs[0], PROP_SPEC_ACL, OP_ADD, STORE_LAST_ELEMENT);
	pvs[0].meta = META_PROP_READ;
	
	IPIN *pin2 = pin1->clone(pvs,1,MODE_NEW_COMMIT);
	pv = pin2->getValue(PROP_SPEC_ACL);
	//VT_ARRAY -- check rights now!!!!!
	if (pv->type == VT_ARRAY) {
		// REVIEW: || instead of &&?
		if(pv->varray[0].meta != 3 && pv->varray[0].iid != iid)
			TVERIFY(0) ;
		if(pv->varray[1].meta != 1 && pv->varray[1].iid != iid1)
			TVERIFY(0) ;
	}
	pin->destroy();
	pin1->destroy();
	pin2->destroy();
}

void TestClone::PiExprTreeClone(ISession *session){
	Value pvs[4];
	PID pid;
	Value args[2];
	PropertyID pids[4];// = {5500,5501,5502,5503};
	MVTApp::mapURIs(session,"TestClone.PiExprTreeClone",4,pids);
	RC rc;
	
	std::cout<<" Test cloning IExprTree/IExpr : "<<std::endl;
	//Create some pins
	pvs[0].set("Mumbai still inundated");pvs[0].setPropID(pids[0]);
	pvs[1].set("Death toll 924");pvs[1].setPropID(pids[1]);
	rc = session->createPINAndCommit(pid,pvs,2);

	pvs[0].set("Heavy rains in Mumbai");pvs[0].setPropID(pids[0]);
	pvs[1].set("Stay at home declared");pvs[1].setPropID(pids[1]);
	pvs[2].set("Stay at home declared (1)");pvs[2].setPropID(pids[2]);
	pvs[3].set("Stay at home declared (2)");pvs[3].setPropID(pids[3]);
	rc = session->createPINAndCommit(pid,pvs,4);
	
	{		
		PropertyID pid[1] = {pids[0]};
		args[0].setVarRef(0,*pid);
		args[1].set("Mumbai still inundated");
		IExprTree *expr = session->expr(OP_EQ,2,args);

		unsigned int lExpTFlgs = expr->getFlags();
		unsigned int lExpTNumOps = expr->getNumberOfOperands();
		ExprOp lExpTOp = expr->getOp();
		const Value lExpTVal = expr->getOperand(lExpTNumOps-1);

		IExprTree *exprclone = expr->clone();

		unsigned int lExpTCFlgs = exprclone->getFlags();
		unsigned int lExpTCNumOps = exprclone->getNumberOfOperands();
		ExprOp lExpTCOp = exprclone->getOp();
		const Value lExpTCVal = exprclone->getOperand(lExpTCNumOps-1);
		
		TVERIFY2( strcmp(lExpTVal.str,lExpTCVal.str) == 0 &&
			lExpTFlgs == lExpTCFlgs &&
			lExpTOp == lExpTCOp &&
			lExpTNumOps == lExpTCNumOps &&
			lExpTOp == lExpTCOp, 
			"ERROR:(PiExprTreeClone) The IExprTree clone specs are diff from original" ) ;

		//Get IExpr instance
		IExpr *lExp = expr->compile();
		const char *lEStr = lExp->toString();

		IExpr *lExpC = lExp->clone();
		const char *lECStr = lExp->toString();
		TVERIFY2( strcmp(lEStr,lECStr) == 0, "ERROR:(PiExprTreeClone) The IExpr clone string is diff from original" ) ;

		lExpC->destroy();
		lExp->destroy();
		expr->destroy();
		exprclone->destroy();
	}

	// Case 2 : IExprTree Complex expression	
	IExprTree *lET;
	{        
		Value val[2];
		// 1: pin has pids[0]
		PropertyID pid[1] = {pids[0]};
		val[0].setVarRef(0, *pid);
		TExprTreePtr lE1 = EXPRTREEGEN(session)(OP_EXISTS, 1, val, 0);

		// 2: pin has pids[0]
		pid[0] = pids[1];
		val[0].setVarRef(0, *pid);
		TExprTreePtr lE2 = EXPRTREEGEN(session)(OP_EXISTS, 1, val, 0);

		// 3: 1	|| 2
		val[0].set(lE1);
		val[1].set(lE2);
		TExprTreePtr lE3 = EXPRTREEGEN(session)(OP_LOR, 2, val, 0);

		// 4: pin has pids[2]
		pid[0] = pids[2];
		val[0].setVarRef(0, *pid);
		TExprTreePtr lE4 = EXPRTREEGEN(session)(OP_EXISTS, 1, val, 0);

		// 5: 3	|| 4
		val[0].set(lE3);
		val[1].set(lE4);
		TExprTreePtr lE5 = EXPRTREEGEN(session)(OP_LOR, 2, val, 0);	

		// 6: pin has pids[3]
		pid[0] = pids[3];
		val[0].setVarRef(0, *pid);
		TExprTreePtr lE6 = EXPRTREEGEN(session)(OP_EXISTS, 1, val, 0);

		// 7: 5	|| 6
		val[0].set(lE5);
		val[1].set(lE6);
		lET = EXPRTREEGEN(session)(OP_LOR, 2, val, 0);
	}
	IExprTree *lETC = lET->clone();
	int lETCnt = 0;
	{
		IStmt * lQ	= session->createStmt();
		unsigned char const var = lQ->addVariable();
		lQ->addCondition(var,lET);
		ICursor *lR = NULL;
		TVERIFYRC(lQ->execute(&lR));
		lETCnt = MVTApp::countPinsFullScan(lR,session);
		lR->destroy();
		lET->destroy();
		lQ->destroy();
	}
	int lETCCnt = 0;
	{
		IStmt * lQ	= session->createStmt();
		unsigned char const var = lQ->addVariable();
		lQ->addCondition(var,lETC);
		ICursor *lR = NULL;
		TVERIFYRC(lQ->execute(&lR));
		lETCCnt = MVTApp::countPinsFullScan(lR,session);
		lR->destroy();
		lETC->destroy();
		lQ->destroy();
	}
	TVERIFY( lETCnt == lETCCnt) ;

	//Case 3 : IExpr complex Expression
	//IExpr * lEC = lE->clone();
	//Value lEVal[2];
	//lE->execute(*lEVal);
	//Value lECVal[2];
	//lEC->execute(*lECVal);
	//std::cout<<" Executing IExpr "<<std::endl;

	// Add more cases here...
}

void TestClone::comparePIN(ISession *session,IPIN *pin,const map<Afy::PropertyID,Afy::Value> &pvl,unsigned nLoops)
{
	const Value *pv;
	map<Afy::PropertyID,Afy::Value>::const_iterator iter;
	size_t size = pvl.size();
	unsigned npins = pin->getNumberOfProperties();
	if(size != pin->getNumberOfProperties()) {
		TVERIFY2(0,"Property mismatch") ;
		return;
	}
	
	for (unsigned i =0; i<npins; i++) {
		pv = pin->getValueByIndex(i);
		
		iter = pvl.find(pv->getPropID());
		switch (pv->type) {
		case VT_STRING: case VT_URL:
			if (0 != strcmp(iter->second.str,pv->str)) {
				cout<<"Failed at propertyID: "<<i;
				cout<<" (expected: "<<iter->second.str;
				cout<<" found: "<<pv->str<<" )"<<endl;
				TVERIFY(0) ;
				return;
			}
		break;
		case VT_INT:
			if (iter->second.i != pv->i) {
				cout<<"Failed at propertyID: "<<i;
				cout<<" (expected: "<<iter->second.i;
				cout<<" found: "<<pv->i<<" )"<<endl;
				TVERIFY(0) ;
				return;
			}
			//todo:: add all types
		default:
			break;
		}
	}
}

bool TestClone::comparePIN2(IPIN* pin1, IPIN* pin2,ISession *session )
{
	// Use convenient MVTApp::equal comparison function
	if ( !MVTApp::equal( *pin1, *pin2, *session ))
	{
		TVERIFY2(0,"Comparison failure of cloned pins");
		MVTApp::output(*pin1, mLogger.out(), session);
		MVTApp::output(*pin2, mLogger.out(), session);
		return false ;
	}

	return true ;
}

void TestClone::reportResult(ICursor *result,ISession *session)
{
	mLogger.out()<<"\nResult:\n";
	for (IPIN *pin; (pin=result->next())!=NULL; ) {
		mLogger.out()<<"\nPIN:\n";
		MVTApp::output(*pin, mLogger.out(), session);
	}
	mLogger.out()<<"\n";
	result->destroy();
}
