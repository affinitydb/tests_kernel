/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

class TestOrToIN : public ITest
{
		static const int sNumProps = 5;
		static const int sNumPINs = 1000;
		
		PropertyID mPropIDs[sNumProps];
		ISession *mSession;		
		Tstring mClassStr;
		int mClassIndex, mFamilyIndex;
		int mClass1PINs, mClass2PINs, mClass3PINs, mClass4PINs;
		int mNumProp1PINs, mNumProp1GT50PINs;
	public:
		TEST_DECLARE(TestOrToIN);
		virtual char const * getName() const { return "testortoin"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "simple test for conversion of 'or' to OP_IN in conditions"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void doTest();
		void createClasses();
		void createFamilies();
		void createData();
		ClassID getClassID(int pIndex, bool pIsFamily = false);
		
		void testClasses();
		void testClass(int pIndex, unsigned long pExpCount);
		void testFamilies();
		void testFamily(int pIndex, unsigned long pExpCount, unsigned int pNumParams = 0, const Value *pParams = NULL);
};
TEST_IMPLEMENT(TestOrToIN, TestLogger::kDStdOut);

ClassID TestOrToIN::getClassID(int pIndex, bool pIsFamily)
{
	Tstring lClass = pIsFamily?"Family":"Class";
	char lB[124]; sprintf(lB, "TestOrToIN.%s.%s.%d", mClassStr.c_str(), lClass.c_str(), pIndex);
	ClassID lCLSID = STORE_INVALID_CLASSID;
	TVERIFYRC(mSession->getClassID(lB, lCLSID));
	return lCLSID;	
}

void TestOrToIN::createClasses()
{
	mLogger.out() << "\t Creating Classes ..." << std::endl;
	// Create a Class /pin[(prop0 = 20 or prop0 = 30)]
	ClassID lClassID1 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(20);
			IExprTree *lET1 = mSession->expr(OP_EQ, 2, lV);

			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(30);
			IExprTree *lET2 = mSession->expr(OP_EQ, 2, lV);

			lV[0].set(lET1);
			lV[1].set(lET2);
			lET = mSession->expr(OP_LOR, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();
		char lB[124]; sprintf(lB, "TestOrToIN.%s.Class.%d", mClassStr.c_str(), mClassIndex++);
		TVERIFYRC(defineClass(mSession,lB, lQ, &lClassID1));
		lQ->destroy();
	}

	// Create a Class /pin[(prop0 = 20 or prop0 = 30) and prop1 > 50]
	ClassID lClassID2 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(20);
			IExprTree *lET1 = mSession->expr(OP_EQ, 2, lV);

			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(30);
			IExprTree *lET2 = mSession->expr(OP_EQ, 2, lV);

			lV[0].set(lET1);
			lV[1].set(lET2);
			IExprTree *lET3 = mSession->expr(OP_LOR, 2, lV);

			lV[0].setVarRef(0,mPropIDs[1]);
			lV[1].set(50);
			IExprTree *lET4 = mSession->expr(OP_GT, 2, lV);
			
			lV[0].set(lET3);
			lV[1].set(lET4);
			lET = mSession->expr(OP_LAND, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();
		char lB[124]; sprintf(lB, "TestOrToIN.%s.Class.%d", mClassStr.c_str(), mClassIndex++);
		TVERIFYRC(defineClass(mSession,lB, lQ, &lClassID2));
		lQ->destroy();
	}

	// Create a Class /pin[(prop0 = 20 or prop0 = 30) and (prop1 = 40 or prop1 = 50)]
	ClassID lClassID3 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(20);
			IExprTree *lET1 = mSession->expr(OP_EQ, 2, lV);

			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(30);
			IExprTree *lET2 = mSession->expr(OP_EQ, 2, lV);

			lV[0].set(lET1);
			lV[1].set(lET2);
			IExprTree *lET3 = mSession->expr(OP_LOR, 2, lV);

			lV[0].setVarRef(0,mPropIDs[1]);
			lV[1].set(40);
			IExprTree *lET4 = mSession->expr(OP_EQ, 2, lV);

			lV[0].setVarRef(0,mPropIDs[1]);
			lV[1].set(50);
			IExprTree *lET5 = mSession->expr(OP_EQ, 2, lV);

			lV[0].set(lET4);
			lV[1].set(lET5);
			IExprTree *lET6 = mSession->expr(OP_LOR, 2, lV);
			
			lV[0].set(lET3);
			lV[1].set(lET6);
			lET = mSession->expr(OP_LAND, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();
		char lB[124]; sprintf(lB, "TestOrToIN.%s.Class.%d", mClassStr.c_str(), mClassIndex++);
		TVERIFYRC(defineClass(mSession,lB, lQ, &lClassID3));
		lQ->destroy();
	}

	// Create a Class /pin[(prop2 = 'abc' or prop2 = 'xyz')]
	ClassID lClassID4 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[2]);
			lV[1].set("abc");
			IExprTree *lET1 = mSession->expr(OP_EQ, 2, lV);

			lV[0].setVarRef(0,mPropIDs[2]);
			lV[1].set("xyz");
			IExprTree *lET2 = mSession->expr(OP_EQ, 2, lV);

			lV[0].set(lET1);
			lV[1].set(lET2);
			lET = mSession->expr(OP_LOR, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();
		char lB[124]; sprintf(lB, "TestOrToIN.%s.Class.%d", mClassStr.c_str(), mClassIndex++);
		TVERIFYRC(defineClass(mSession,lB, lQ, &lClassID4));
		lQ->destroy();
	}
}

void TestOrToIN::createFamilies()
{
	mLogger.out() << "\t Creating Families ..." << std::endl;

	// Create a Family /pin[(prop0 = 20 or prop0 = 30) and prop1 > $var]
	ClassID lFamilyID1;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(20);
			IExprTree *lET1 = mSession->expr(OP_EQ, 2, lV);

			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(30);
			IExprTree *lET2 = mSession->expr(OP_EQ, 2, lV);

			lV[0].set(lET1);
			lV[1].set(lET2);
			IExprTree *lET3 = mSession->expr(OP_LOR, 2, lV);

			lV[0].setVarRef(0,mPropIDs[1]);
			lV[1].setParam(0);
			IExprTree *lET4 = mSession->expr(OP_GT, 2, lV);
			
			lV[0].set(lET3);
			lV[1].set(lET4);
			lET = mSession->expr(OP_LAND, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();
		char lB[124]; sprintf(lB, "TestOrToIN.%s.Family.%d", mClassStr.c_str(), mFamilyIndex++);
		TVERIFYRC(defineClass(mSession,lB, lQ, &lFamilyID1));
		lQ->destroy();
	}

	// Create a Family /pin[(prop0 = 20 or prop0 = 30) and (prop1 = 40 or prop1 = 50) and prop2 in $var]
	ClassID lFamilyID2 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(20);
			IExprTree *lET1 = mSession->expr(OP_EQ, 2, lV);

			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(30);
			IExprTree *lET2 = mSession->expr(OP_EQ, 2, lV);

			lV[0].set(lET1);
			lV[1].set(lET2);
			IExprTree *lET3 = mSession->expr(OP_LOR, 2, lV);

			lV[0].setVarRef(0,mPropIDs[1]);
			lV[1].set(40);
			IExprTree *lET4 = mSession->expr(OP_EQ, 2, lV);

			lV[0].setVarRef(0,mPropIDs[1]);
			lV[1].set(50);
			IExprTree *lET5 = mSession->expr(OP_EQ, 2, lV);

			lV[0].set(lET4);
			lV[1].set(lET5);
			IExprTree *lET6 = mSession->expr(OP_LOR, 2, lV);

			lV[0].set(lET3);
			lV[1].set(lET6);
			IExprTree *lET7 = mSession->expr(OP_LAND, 2, lV);

			lV[0].setVarRef(0,mPropIDs[2]);
			lV[1].setParam(0);
			IExprTree *lET8 = mSession->expr(OP_IN, 2, lV);
			
			lV[0].set(lET7);
			lV[1].set(lET8);
			lET = mSession->expr(OP_LAND, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();
		char lB[124]; sprintf(lB, "TestOrToIN.%s.Family.%d", mClassStr.c_str(), mFamilyIndex++);
		TVERIFYRC(defineClass(mSession,lB, lQ, &lFamilyID2));
		lQ->destroy();
	}

	// Create a Family /pin[prop0 in $var and (prop1 = 40 or prop1 = 50)]
	ClassID lFamilyID3 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].setParam(0);
			IExprTree *lET1 = mSession->expr(OP_IN, 2, lV);
			
			lV[0].setVarRef(0,mPropIDs[1]);
			lV[1].set(40);
			IExprTree *lET2 = mSession->expr(OP_EQ, 2, lV);

			lV[0].setVarRef(0,mPropIDs[1]);
			lV[1].set(50);
			IExprTree *lET3 = mSession->expr(OP_EQ, 2, lV);

			lV[0].set(lET2);
			lV[1].set(lET3);
			IExprTree *lET4 = mSession->expr(OP_LOR, 2, lV);

			lV[0].set(lET1);
			lV[1].set(lET4);
			lET = mSession->expr(OP_LAND, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();
		char lB[124]; sprintf(lB, "TestOrToIN.%s.Family.%d", mClassStr.c_str(), mFamilyIndex++);
		TVERIFYRC(defineClass(mSession,lB, lQ, &lFamilyID3));
		lQ->destroy();
	}
}

void TestOrToIN::createData()
{
	mLogger.out() << "\t Creating " << sNumPINs << " PINs ..." << std::endl;
	int i;
	for(i = 0; i < sNumPINs; i++)
	{
		Value lV[5];
		int lProp0Val = MVTRand::getRange(1, 10) * 10;
		int lProp1Val = MVTRand::getRange(1, 10) * 10;
		bool lfProp2 = MVTRand::getBool();
		const char * lProp2Val = lfProp2?MVTRand::getBool()?"abc":"xyz":"junk";

		PID lPID = {STORE_INVALID_PID, STORE_OWNER};
		SETVALUE(lV[0], mPropIDs[0], lProp0Val, OP_SET);
		SETVALUE(lV[1], mPropIDs[1], lProp1Val, OP_SET);
		SETVALUE(lV[2], mPropIDs[2], lProp2Val, OP_SET);
		CREATEPIN(mSession, &lPID, lV, 3);
		if(lPID.pid != STORE_INVALID_PID)
		{
			if(lProp0Val == 20 || lProp0Val == 30)
			{ 
				mClass1PINs++; 
				if(lProp1Val > 50) mClass2PINs++;
				if(lProp1Val == 40 || lProp1Val == 50) mClass3PINs++;
			}
			if(lProp1Val == 40 || lProp1Val == 50) mNumProp1PINs++; 
			if(lProp1Val > 50) mNumProp1GT50PINs++;
			if(lfProp2) mClass4PINs++;
		}
	}
}

void TestOrToIN::testClasses()
{
	// Test Class #1
	{
		testClass(0, mClass1PINs);
	}

	// Test Class #2
	{
		testClass(1, mClass2PINs);
	}

	// Test Class #3
	{
		testClass(2, mClass3PINs);
	}
	// Test Class #4
	{
		testClass(3, mClass4PINs);
	}
}

void TestOrToIN::testFamilies()
{
	// Test Family #1
	{
		Value lParam[1]; lParam[0].set(50);
		testFamily(0, mClass2PINs, 1, lParam);
	}

	// Test Family #2
	{
		testFamily(1, mClass3PINs);
	}

	// TestFamily #3
	{
		testFamily(2, mNumProp1PINs);
	}
}

void TestOrToIN::testClass(int pIndex, unsigned long pExpCount)
{
	ClassID lCLSID = getClassID(pIndex);
	if(STORE_INVALID_CLASSID != lCLSID)
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		SourceSpec lCS; lCS.objectID = lCLSID; lCS.nParams = 0; lCS.params = NULL;
		lQ->addVariable(&lCS, 1);
		uint64_t lCount = 0;
		TVERIFYRC(lQ->count(lCount));
		mLogger.out() << "testClass: expected count=" << pExpCount << ", got count=" << lCount << std::endl;
		TVERIFY(lCount == pExpCount);
	}
}

void TestOrToIN::testFamily(int pIndex, unsigned long pExpCount, unsigned int pNumParams, const Value *pParams)
{
	ClassID lCLSID = getClassID(pIndex, true);
	if(STORE_INVALID_CLASSID != lCLSID)
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		SourceSpec lCS; lCS.objectID = lCLSID; lCS.nParams = pNumParams; lCS.params = pParams;
		lQ->addVariable(&lCS, 1);
		uint64_t lCount = 0;
		TVERIFYRC(lQ->count(lCount));
		mLogger.out() << "testFamily: expected count=" << pExpCount << ", got count=" << lCount << std::endl;
		TVERIFY(lCount == pExpCount);
	}
}

void TestOrToIN::doTest()
{
	MVTApp::mapURIs(mSession, "TestOrToIN.prop", sNumProps, mPropIDs);
	MVTRand::getString(mClassStr, 5, 10, false, false);	
	mClassIndex = 0; mFamilyIndex = 0;
	mClass1PINs = 0; mClass2PINs = 0; mClass3PINs = 0; mClass4PINs = 0;
	mNumProp1PINs = 0; mNumProp1GT50PINs = 0;
	createClasses();
	createFamilies();
	createData();
	
	testClasses();
	testFamilies();
}

int TestOrToIN::execute()
{	
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();	
		doTest();
		mSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
	return 0;
}
