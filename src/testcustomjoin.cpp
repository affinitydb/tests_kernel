/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"
#include "classhelper.h"
#include "collectionhelp.h"
using namespace std;
#define MAXNUMPINS 10  

class testcustomjoin : public ITest
{
public:
	TEST_DECLARE(testcustomjoin);
	virtual char const * getName() const { return "testcustomjoin"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "test join : Repro for Bug# 28777"; }
	virtual int execute();
	virtual void destroy() { delete this; }
	void defineClasses(void);
	void createPins(int numPins);
	void customQuery(void);
public:
	ISession * mSession ;
	IPIN * mIPIN;
	static const int sNumProps = 5;
	static const int sNumClasses = 5;
	PropertyID mProp[sNumProps];
	ClassID mClass[sNumClasses];
	PID pinid[1];
};
TEST_IMPLEMENT(testcustomjoin, TestLogger::kDStdOut);

int testcustomjoin::execute()
{
	if (!MVTApp::startStore()) {mLogger.print("Failed to start store\n"); return RC_NOACCESS;}
	
	mSession = MVTApp::startSession();
	MVTApp::mapURIs(mSession, "testcustomjoin.", sNumProps, &mProp[0]); 

	defineClasses(); 
	createPins(MAXNUMPINS);
	customQuery();
	mSession->terminate();
	MVTApp::stopStore();
	return RC_OK;
}

void testcustomjoin::defineClasses()
{
	int i;
	for(i=0;i<sNumClasses;i++)
	{
		string strRand ; MVTRand::getString( strRand, 15+i, 0 ) ;
		CmvautoPtr<IStmt> lFamilyQ(mSession->createStmt());
		unsigned char lVar = lFamilyQ->addVariable();
		{
		Value lV[2];
		lV[0].setVarRef(0,mProp[i]);
		IExprTree *lET1 = mSession->expr(OP_EXISTS, 1, lV);
		TVERIFYRC(lFamilyQ->addCondition(lVar,lET1));			
		}
		strRand = "testcustomjoinFamily." + strRand;
		mClass[i] = STORE_INVALID_CLASSID;
		TVERIFYRC(defineClass(mSession, strRand.c_str(), lFamilyQ, &mClass[i] ));
	}
}

void testcustomjoin::createPins(int numPins) 
{
	unsigned int cntElements = numPins;
	vector<PID> referencedPins(cntElements);
	vector<PID> classPins(cntElements);
	unsigned int i ;IPIN *pin;
	for ( i = 0 ; i < cntElements ; i++ )
	{
		Value v[2] ; 
		v[0].set( i ) ; v[0].property = mProp[1] ;
		v[1].set( i ) ; v[1].property = mProp[2] ;
		TVERIFYRC(mSession->createPIN(&v[0],2,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
		referencedPins[i] = pin->getPID();
		if(pin!=NULL) pin->destroy();
		
		Value v1[2] ; 
		v1[0].set(i) ; v1[0].property = mProp[3] ;
		v1[1].set(referencedPins[i]) ; v1[1].property = mProp[4] ;
		TVERIFYRC(mSession->createPIN(v1,2,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
		classPins[i] = pin->getPID();
		if(pin!=NULL) pin->destroy();
	}

	// Create pin that points to all of the referencedPins
	Value *vals = (Value *)mSession->malloc(cntElements*sizeof(Value));
	TVERIFY(vals!=NULL);
	// Create the references 
	for ( i = 0 ; i < cntElements ; i++ )
	{
		vals[i].set(referencedPins[i]) ; vals[i].op = OP_ADD ; vals[i].property = mProp[0] ; 
		vals[i].meta = META_PROP_SSTORAGE ;
		//TVERIFYRC(mSession->modifyPIN( pinid[0], &ref, 1 )) ;
	}
	TVERIFYRC(mSession->createPIN(vals, cntElements, &pin, MODE_PERSISTENT|MODE_COPY_VALUES)) ;
	pinid[0] = pin->getPID();
	if(vals) mSession->free(vals);
	// Sanity check
	MvStoreEx::CollectionIterator collection(pin,mProp[0]);
	TVERIFY( collection.getSize() == cntElements ) ;
	if(pin!=NULL) pin->destroy();
	pin = NULL ;
}

void testcustomjoin::customQuery()
{
	uint64_t lCount;
	PropertyID propspec = PROP_SPEC_PINID;
	//Case 0 : LHS and RHS has the same set of pins
	{
		SourceSpec lclassSpec1;
		lclassSpec1.objectID = mClass[2];	lclassSpec1.nParams =0 ;
		IStmt *lQuery = mSession->createStmt();
		if(lQuery)
		{
			unsigned char lhs = lQuery->addVariable(pinid[0],mProp[0]);
			unsigned char rhs = lQuery->addVariable(&lclassSpec1,1); 
			Value lV1[2];
			lV1[0].setVarRef(lhs,mProp[1]);
			lV1[1].setVarRef(rhs,mProp[2]);
			IExprTree *lET2 = mSession->expr(OP_EQ, 2, lV1);
			lQuery->join(lhs,rhs,lET2);
			TVERIFYRC(lQuery->count(lCount));
			TVERIFY(lCount==MAXNUMPINS);
		}
		lQuery->destroy();
	}
	//Case 1 : LHS and RHS has different set of pins ---Fails Bug# 28777
	{
		SourceSpec lclassSpec1;
		lclassSpec1.objectID = mClass[3];	lclassSpec1.nParams =0 ;
		IStmt *lQuery = mSession->createStmt();
		if(lQuery)
		{
			unsigned char lhs = lQuery->addVariable(pinid[0],mProp[0]);
			unsigned char rhs = lQuery->addVariable(&lclassSpec1,1); 
			Value lV1[2];
			lV1[0].setVarRef(lhs,mProp[1]);
			lV1[1].setVarRef(rhs,mProp[3]);
			IExprTree *lET2 = mSession->expr(OP_EQ, 2, lV1);
			lQuery->join(lhs,rhs,lET2);
			TVERIFYRC(lQuery->count(lCount));
			TVERIFY(lCount==MAXNUMPINS);
		}
		lQuery->destroy();
	}
	//Case 2 : LHS and RHS has different set of pins and expression has vt_refid from RHS ---Fails Bug# 28777
	{
		SourceSpec lclassSpec1;
		lclassSpec1.objectID = mClass[3];	lclassSpec1.nParams =0 ;
		IStmt *lQuery = mSession->createStmt();
		if(lQuery)
		{
			unsigned char lhs = lQuery->addVariable(pinid[0],mProp[0]);
			unsigned char rhs = lQuery->addVariable(&lclassSpec1,1); 
			Value lV1[2];
			lV1[0].setVarRef(lhs,propspec);
			lV1[1].setVarRef(rhs,mProp[4]);
			IExprTree *lET2 = mSession->expr(OP_EQ, 2, lV1);
			lQuery->join(lhs,rhs,lET2);
			TVERIFYRC(lQuery->count(lCount));
			TVERIFY(lCount==MAXNUMPINS);
		}
		lQuery->destroy();
	}
}
