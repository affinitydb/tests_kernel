/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

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
	unsigned int i ;
	for ( i = 0 ; i < cntElements ; i++ )
	{
		Value v[2] ; 
		v[0].set( i ) ; v[0].property = mProp[1] ;
		v[1].set( i ) ; v[1].property = mProp[2] ;
		TVERIFYRC(mSession->createPIN(referencedPins[i],&v[0],2));
		
		Value v1[2] ; 
		v1[0].set(i) ; v1[0].property = mProp[3] ;
		v1[1].set(referencedPins[i]) ; v1[1].property = mProp[4] ;
		TVERIFYRC(mSession->createPIN(classPins[i],v1,2));
	}

	// Create pin that points to all of the referencedPins
	TVERIFYRC(mSession->createPIN(pinid[0],NULL,0));

	// Create the references 
	for ( i = 0 ; i < cntElements ; i++ )
	{
		Value ref ;	
		ref.set(referencedPins[i]) ; ref.op = OP_ADD ; ref.property = mProp[0] ; 
		ref.meta = META_PROP_SSTORAGE ;
		TVERIFYRC(mSession->modifyPIN( pinid[0], &ref, 1 )) ;
	}
	
	// Sanity check
	IPIN * pin = mSession->getPIN(pinid[0]) ;
	MvStoreEx::CollectionIterator collection(pin,mProp[0]);
	TVERIFY( collection.getSize() == cntElements ) ;
	pin = NULL ;
	
}

void testcustomjoin::customQuery()
{
	uint64_t lCount;
	PropertyID propspec = PROP_SPEC_PINID;
	//Case 0 : LHS and RHS has the same set of pins
	{
		ClassSpec lclassSpec1;
		lclassSpec1.classID = mClass[2];	lclassSpec1.nParams =0 ;
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
		ClassSpec lclassSpec1;
		lclassSpec1.classID = mClass[3];	lclassSpec1.nParams =0 ;
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
		ClassSpec lclassSpec1;
		lclassSpec1.classID = mClass[3];	lclassSpec1.nParams =0 ;
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
