/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"
#include "classhelper.h"
#define MAXNUMPINS 90 //Even numbers only
#define NUMQUERYPIN 20 //Even numbers only && <MAXNUMPINS

class testclassfilter : public ITest
{
public:
	TEST_DECLARE(testclassfilter);
	virtual char const * getName() const { return "testclassfilter"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "Test for class query with a filter condition"; }
	virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
	virtual int execute();
	virtual void destroy() { delete this; }
	void defineClass(int numClasses);
	void createPins(int numPins);
	void verifyJoinQuery(bool lhs);
public:
	ISession * mSession ;
	IPIN * mIPIN;
	static const int sNumProps = 4;
	static const int sNumClasses = 2;
	PropertyID mProp[sNumProps];
	ClassID mClass[sNumClasses];
};
TEST_IMPLEMENT(testclassfilter, TestLogger::kDStdOut);

int testclassfilter::execute()
{
	if (!MVTApp::startStore()) {mLogger.print("Failed to start store\n"); return RC_NOACCESS;}
	
	mSession = MVTApp::startSession();
	MVTApp::mapURIs(mSession, "testclassfilter.", sNumProps, &mProp[0]); 

	defineClass(sNumClasses); 
	createPins(MAXNUMPINS);
	verifyJoinQuery(true); 
	verifyJoinQuery(false); //swap lhs and rhs

	mSession->terminate();
	MVTApp::stopStore();
	return RC_OK;
}

void testclassfilter::defineClass(int numClasses)
{
	int i;
	for(i=0;i<numClasses;i++)
	{
		string strRand ; MVTRand::getString( strRand, 20, 0 ) ;
		CmvautoPtr<IStmt> lClassQ(mSession->createStmt());
		unsigned char lVar = lClassQ->addVariable();
		{
			Value lV[2];
			lV[0].setVarRef(0,mProp[i]);
			IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV);
			TVERIFYRC(lClassQ->addCondition(lVar,lET1));			
		}
		strRand = "testclassfilterClass." + strRand;
		mClass[i] = STORE_INVALID_CLASSID;
		TVERIFYRC(ITest::defineClass(mSession, strRand.c_str(), lClassQ, &mClass[i] ));
	}
}

void testclassfilter::createPins(int numPins) 
{
	Value tValue[2];
	int i;
	mLogger.out()<<std::endl<<"Creating "<<numPins<<" pins.";
	for(i=0;i<numPins/2;i++)
	{	
		tValue[0].set(1);
		tValue[0].property=mProp[0];
		TVERIFYRC(mSession->createPIN(tValue,1,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));
	}
	for(i=0;i<numPins/2;i++)
	{	
		if(i<NUMQUERYPIN)
		{
			tValue[0].set(1);
			tValue[0].property=mProp[1];
			if(i<NUMQUERYPIN/2)
				tValue[1].set(2);
			else
				tValue[1].set(99);
			tValue[1].property=mProp[2];
			TVERIFYRC(mSession->createPIN(tValue,2,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));
		}
		else
		{
			tValue[0].set(1);
			tValue[0].property=mProp[1];
			TVERIFYRC(mSession->createPIN(tValue,1,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));
		} 
	}
}

void testclassfilter::verifyJoinQuery(bool lhs)
{
	SourceSpec lCS1;
	lCS1.objectID = mClass[0];	lCS1.nParams = 0;
	SourceSpec lCS2;
	lCS2.objectID = mClass[1];	lCS2.nParams = 0;
	uint64_t lCount;
	static const uint64_t opEqCount = (MAXNUMPINS/2) + (NUMQUERYPIN/2);
	static const uint64_t opExCount = (MAXNUMPINS/2) + (NUMQUERYPIN);
	static const uint64_t opNeCount = (MAXNUMPINS/2);

	Value lV[2];
	//Case : 1EXC - pin is class1() union (pin is class2() and mProp[2] exists) using lQ->addcondition()
	{
	IStmt * const lQ =mSession->createStmt();
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	unsigned char lVar2 = lQ->addVariable(&lCS2,1);
	lV[0].setVarRef(0,mProp[2]);
	IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV);
	TVERIFYRC(lQ->addCondition(lVar2,lET1));
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opExCount);	//Failing
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 1EXV - pin is class1() union (pin is class2() and mProp[2] exists) using lQ->addVariable()
	{
	IStmt * const lQ =mSession->createStmt();
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	lV[0].setVarRef(0,mProp[2]);
	IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV);
	unsigned char lVar2 = lQ->addVariable(&lCS2,1,lET1);
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opExCount); //Passing
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 2EXC - (pin is class2() and mProp[2] exists) union pin is class1() using lQ->addcondition()
	{
	IStmt * const lQ =mSession->createStmt();
	unsigned char lVar2 = lQ->addVariable(&lCS2,1);
	lV[0].setVarRef(0,mProp[2]);
	IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV);
	TVERIFYRC(lQ->addCondition(lVar2,lET1));
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opExCount);	//Passing
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 2EXV - (pin is class2() and mProp[2] exists) union pin is class1() using lQ->addVariable()
	{
	IStmt * const lQ =mSession->createStmt();
	lV[0].setVarRef(0,mProp[2]);
	IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV);
	unsigned char lVar2 = lQ->addVariable(&lCS2,1,lET1);
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opExCount); //Passing
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 1EQC - pin is class1() union (pin is class2() and mProp[2] = 99 ) using lQ->addCondition()
	{
	IStmt * const lQ =mSession->createStmt();
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	unsigned char lVar2 = lQ->addVariable(&lCS2,1);
	lV[0].setVarRef(0,mProp[2]);
	lV[1].set(99);
	IExprNode *lET1 = mSession->expr(OP_EQ, 2, lV);
	TVERIFYRC(lQ->addCondition(lVar2,lET1));
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opEqCount);	//Fails
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 1EQV - pin is class1() union (pin is class2() and mProp[2] = 99 ) using lQ->addVariable()
	{
	IStmt * const lQ =mSession->createStmt();
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	lV[0].setVarRef(0,mProp[2]);
	lV[1].set(99);
	IExprNode *lET1 = mSession->expr(OP_EQ, 2, lV);
	unsigned char lVar2 = lQ->addVariable(&lCS2,1,lET1);
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opEqCount); //Passing
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 2EQC - (pin is class2() and mProp[2] = 99 ) union pin is class1() using lQ->addCondition()
	{
	IStmt * const lQ =mSession->createStmt();
	unsigned char lVar2 = lQ->addVariable(&lCS2,1);
	lV[0].setVarRef(lVar2,mProp[2]);
	lV[1].set(99);
	IExprNode *lET1 = mSession->expr(OP_EQ, 2, lV);
	TVERIFYRC(lQ->addCondition(lVar2,lET1));
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opEqCount);	//Passing
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 2EQV - (pin is class2() and mProp[2] = 99 ) union pin is class1() using lQ->addVariable()
	{
	IStmt * const lQ =mSession->createStmt();
	lV[0].setVarRef(0,mProp[2]);
	lV[1].set(99);
	IExprNode *lET1 = mSession->expr(OP_EQ, 2, lV);
	unsigned char lVar2 = lQ->addVariable(&lCS2,1,lET1);
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opEqCount); //Passing
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 1NEC - pin is class1() union (pin is class2() and mProp[3] exists) using lQ->addcondition() - "None of the pins has mProp [3]"
	{
	IStmt * const lQ =mSession->createStmt();
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	unsigned char lVar2 = lQ->addVariable(&lCS2,1);
	lV[0].setVarRef(0,mProp[3]);
	IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV);
	TVERIFYRC(lQ->addCondition(lVar2,lET1));
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opNeCount);	//Failing
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 1NEV - pin is class1() union (pin is class2() and mProp[3] exists) using lQ->addVariable() - "None of the pins has mProp [3]"
	{
	IStmt * const lQ =mSession->createStmt();
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	lV[0].setVarRef(0,mProp[3]);
	IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV);
	unsigned char lVar2 = lQ->addVariable(&lCS2,1,lET1);
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opNeCount); //Passing
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 2NEC - (pin is class2() and mProp[3] exists) union pin is class1() using lQ->addCondition() - "None of the pins has mProp [3]"
	{
	IStmt * const lQ =mSession->createStmt();
	unsigned char lVar2 = lQ->addVariable(&lCS2,1);
	lV[0].setVarRef(lVar2,mProp[3]);
	IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV);
	TVERIFYRC(lQ->addCondition(lVar2,lET1));
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opNeCount);	//Passing
	lET1->destroy();
	lQ->destroy();
	}
	//Case : 2NEV - (pin is class2() and mProp[3] exists) union pin is class1() using lQ->addVariable() - "None of the pins has mProp [3]"
	{
	IStmt * const lQ =mSession->createStmt();
	lV[0].setVarRef(0,mProp[3]);
	IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV);
	unsigned char lVar2 = lQ->addVariable(&lCS2,1,lET1);
	unsigned char lVar1 = lQ->addVariable(&lCS1,1);
	if (lhs) {lQ->setOp(lVar1,lVar2,QRY_UNION);}
	else {lQ->setOp(lVar2,lVar1,QRY_UNION);}
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount==opNeCount); //Passing
	lET1->destroy();
	lQ->destroy();
	}
}
