/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

class TestDropBaseClass:  public ITest
{
		static const int sNumProps = 5;
		PropertyID mPropIds[sNumProps];
		Tstring mClassStr; 
	public:
		TEST_DECLARE(TestDropBaseClass);
		virtual char const * getName() const { return "testdropbaseclass"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return " test to check working of derived class if base class is dropped"; }
		virtual void destroy() { delete this; }
		virtual int execute();		
	private:
		ISession * mSession ;	
		ClassID mBaseClassID, mBase2ClassID, mDerivedClassID, mDerived2ClassID, mDerivedFamilyID;
		Tstring mBaseClassName, mBase2ClassName, mDerivedClassName, mDerived2ClassName, mDerivedFamilyName;
		
		void doTest();
		void createMeta();
		void createPINs(int pNumPINs = 100);
		unsigned long queryCount(ClassID pClassID);
};

TEST_IMPLEMENT(TestDropBaseClass, TestLogger::kDStdOut);

void TestDropBaseClass::createMeta()
{
	char lB[64]; 
	sprintf(lB, "TestDropBaseClass.%s.BaseClass%d", mClassStr.c_str(), 0); mBaseClassName = lB;
	sprintf(lB, "TestDropBaseClass.%s.BaseClass%d", mClassStr.c_str(), 1); mBase2ClassName = lB;
	sprintf(lB, "TestDropBaseClass.%s.Class%d", mClassStr.c_str(), 2); mDerivedClassName = lB;
	sprintf(lB, "TestDropBaseClass.%s.Class%d", mClassStr.c_str(), 3); mDerived2ClassName = lB;
	sprintf(lB, "TestDropBaseClass.%s.Family%d", mClassStr.c_str(), 4); mDerivedFamilyName = lB;
	
	mBaseClassID = STORE_INVALID_CLASSID;
	{	
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned char lVar = lQ->addVariable();
		{
			Value lV[1];
			lV[0].setVarRef(0,mPropIds[0]);
			CmvautoPtr<IExprTree> lET(mSession->expr(OP_EXISTS, 1, lV));
			TVERIFYRC(lQ->addCondition(lVar,lET));			
		}
		
		TVERIFYRC(defineClass(mSession,mBaseClassName.c_str(), lQ, &mBaseClassID));
	}
	mBase2ClassID = STORE_INVALID_CLASSID;
	{	
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned char lVar = lQ->addVariable();
		{
			Value lV[1];
			lV[0].setVarRef(0,mPropIds[1]);
			CmvautoPtr<IExprTree> lET(mSession->expr(OP_EXISTS, 1, lV));
			TVERIFYRC(lQ->addCondition(lVar,lET));			
		}
		
		TVERIFYRC(defineClass(mSession,mBase2ClassName.c_str(), lQ, &mBase2ClassID));
	}
	mDerivedClassID = STORE_INVALID_CLASSID;
	{	
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		SourceSpec lCS = {mBaseClassID, 0, NULL};
		unsigned char lVar = lQ->addVariable(&lCS, 1);
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIds[2]);
			lV[1].set(0);
			CmvautoPtr<IExprTree> lET(mSession->expr(OP_GE, 2, lV));
			TVERIFYRC(lQ->addCondition(lVar,lET));			
		}
		TVERIFYRC(defineClass(mSession,mDerivedClassName.c_str(), lQ, &mDerivedClassID));
	}
	mDerived2ClassID = STORE_INVALID_CLASSID;
	{	
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		SourceSpec lCS[2];
		lCS[0].objectID = mBaseClassID; lCS[0].nParams = 0; lCS[0].params = NULL;
		lCS[1].objectID = mBase2ClassID; lCS[1].nParams = 0; lCS[1].params = NULL;
		unsigned char lVar = lQ->addVariable(lCS, 2);
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIds[2]);
			IExprTree  *lET1 = mSession->expr(OP_EXISTS, 1, lV);

			lV[0].setVarRef(0,mPropIds[3]);
			IExprTree  *lET2 = mSession->expr(OP_EXISTS, 1, lV);

			lV[0].set(lET1);
			lV[1].set(lET2);
			CmvautoPtr<IExprTree> lET(mSession->expr(OP_LAND, 2, lV));
			TVERIFYRC(lQ->addCondition(lVar,lET));			
		}
		TVERIFYRC(defineClass(mSession,mDerived2ClassName.c_str(), lQ, &mDerived2ClassID));
	}
	mDerivedFamilyID = STORE_INVALID_CLASSID;
	{	
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		SourceSpec lCS = {mBaseClassID, 0, NULL};
		unsigned char lVar = lQ->addVariable(&lCS, 1);
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIds[3]);
			lV[1].setParam(0);
			CmvautoPtr<IExprTree> lET(mSession->expr(OP_IN, 2, lV));
			TVERIFYRC(lQ->addCondition(lVar,lET));			
		}
		TVERIFYRC(defineClass(mSession,mDerivedFamilyName.c_str(), lQ, &mDerivedFamilyID));
	}
}
void TestDropBaseClass::createPINs(int pNumPINs)
{
	// Create the Children PINs	
	mLogger.out() << "Creating " << pNumPINs << " PINs ...";
	int i;
	for(i = 0; i < pNumPINs; i++)
	{		
		if(i%100 == 0) mLogger.out() << ".";
		Value lV[5]; Tstring lStr; MVTRand::getString(lStr, 5, 10, false, true);
		SETVALUE(lV[0], mPropIds[0], lStr.c_str(), OP_SET);
		SETVALUE(lV[1], mPropIds[1], lStr.c_str(), OP_SET);
		SETVALUE(lV[2], mPropIds[2], i, OP_SET);
		SETVALUE(lV[3], mPropIds[3], lStr.c_str(), OP_SET);
		SETVALUE(lV[4], mPropIds[4], lStr.c_str(), OP_SET);
		PID lPID; CREATEPIN(mSession, lPID, lV, 5);		
	}
	mLogger.out() << " DONE" << std::endl;	
}

unsigned long TestDropBaseClass::queryCount(ClassID pClassID)
{
	CmvautoPtr<IStmt> lQ(mSession->createStmt());			
	SourceSpec lCS = {pClassID, 0 , NULL};
	lQ->addVariable(&lCS, 1);
	uint64_t lCount = 0;
	TVERIFYRC(lQ->count(lCount, 0, 0, ~0));
	return (unsigned long)lCount;
}

void TestDropBaseClass::doTest()
{
	createMeta();

	createPINs(100);

	TVERIFY(100 == queryCount(mBaseClassID));

	TVERIFY(100 == queryCount(mBase2ClassID));

	TVERIFY(100 == queryCount(mDerivedClassID));

	TVERIFY(100 == queryCount(mDerived2ClassID));

	TVERIFY(100 == queryCount(mDerivedFamilyID));
	
	TVERIFYRC(dropClass(mSession,mBaseClassName.c_str()));
	TVERIFYRC(dropClass(mSession,mBase2ClassName.c_str()));

	createPINs(100);
	
	TVERIFY(200 == queryCount(mDerivedClassID));

	TVERIFY(200 == queryCount(mDerived2ClassID));

	TVERIFY(200 == queryCount(mDerivedFamilyID));
	
}
int TestDropBaseClass::execute() 
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();
		MVTApp::mapURIs(mSession, "TestDropBaseClass.prop.", sNumProps, mPropIds);
		MVTRand::getString(mClassStr, 5, 10, false, true);				
	
		doTest();
		mSession->terminate();
		MVTApp::stopStore();
	}
	return lSuccess?0:1;
}
