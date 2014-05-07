/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

class TestDNFQuery : public ITest
{
		ISession *mSession;
		Tstring mClassStr;
	public:
		TEST_DECLARE(TestDNFQuery);
		virtual char const * getName() const { return "testdnfquery"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test disjunctive queires (DNF form in property lists)"; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void doTest();
		void testDNFSimple();
		bool testQuery(ClassID pClassID, int pExpCount);
};

TEST_IMPLEMENT(TestDNFQuery, TestLogger::kDStdOut);

void TestDNFQuery::doTest()
{
		testDNFSimple();
}

void TestDNFQuery::testDNFSimple()
{
	PropertyID lPropIDs[5];
	MVTApp::mapURIs(mSession, "TestDNFQuery.testDNFSimple.prop", 5, lPropIDs);
	
	// (!a and b and !c) or (!a and !b and c)
	ClassID lCLSID = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprNode *lET;
		{
			Value lV[2];
			// (!a and b and !c)
			lV[0].setVarRef(0,lPropIDs[0]);
			IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV, NOT_BOOLEAN_OP);

			lV[0].setVarRef(0,lPropIDs[1]);
			IExprNode *lET2 = mSession->expr(OP_EXISTS, 1, lV);

			lV[0].setVarRef(0,lPropIDs[2]);
			IExprNode *lET3 = mSession->expr(OP_EXISTS, 1, lV, NOT_BOOLEAN_OP);

			lV[0].set(lET1);
			lV[1].set(lET2);
			IExprNode *lET4 = mSession->expr(OP_LAND, 2, lV);

			lV[0].set(lET4);
			lV[1].set(lET3);
			IExprNode *lET5 = mSession->expr(OP_LAND, 2, lV);

			// !a and !b and c)

			lV[0].setVarRef(0,lPropIDs[0]);
			IExprNode *lET6 = mSession->expr(OP_EXISTS, 1, lV, NOT_BOOLEAN_OP);

			lV[0].setVarRef(0,lPropIDs[1]);
			IExprNode *lET7 = mSession->expr(OP_EXISTS, 1, lV, NOT_BOOLEAN_OP);

			lV[0].setVarRef(0,lPropIDs[2]);
			IExprNode *lET8 = mSession->expr(OP_EXISTS, 1, lV);

			lV[0].set(lET6);
			lV[1].set(lET7);
			IExprNode *lET9 = mSession->expr(OP_LAND, 2, lV);

			lV[0].set(lET9);
			lV[1].set(lET8);
			IExprNode *lET10 = mSession->expr(OP_LAND, 2, lV);

			lV[0].set(lET5);
			lV[1].set(lET10);
			lET = mSession->expr(OP_LOR, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();	
		char lB[124]; sprintf(lB, "TestDNFQuery.testDNFSimple.%s.Class.%d", mClassStr.c_str(), 0);
		TVERIFYRC(defineClass(mSession,lB, lQ, &lCLSID));
	}
	// (!a or b) and (d or e)
	ClassID lCLSID2 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprNode *lET;
		{
			Value lV[2];
			// (!a or b)
			lV[0].setVarRef(0,lPropIDs[0]);
			IExprNode *lET1 = mSession->expr(OP_EXISTS, 1, lV, NOT_BOOLEAN_OP);

			lV[0].setVarRef(0,lPropIDs[1]);
			IExprNode *lET2 = mSession->expr(OP_EXISTS, 1, lV);

			lV[0].set(lET1);
			lV[1].set(lET2);
			IExprNode *lET3 = mSession->expr(OP_LOR, 2, lV);

			// (d or e)

			lV[0].setVarRef(0,lPropIDs[3]);
			IExprNode *lET4 = mSession->expr(OP_EXISTS, 1, lV);

			lV[0].setVarRef(0,lPropIDs[4]);
			IExprNode *lET5 = mSession->expr(OP_EXISTS, 1, lV);

			lV[0].set(lET4);
			lV[1].set(lET5);
			IExprNode *lET6 = mSession->expr(OP_LOR, 2, lV);

			lV[0].set(lET3);
			lV[1].set(lET6);
			lET = mSession->expr(OP_LAND, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();	
		char lB[124]; sprintf(lB, "TestDNFQuery.testDNFSimple.%s.Class.%d", mClassStr.c_str(), 1);
		TVERIFYRC(defineClass(mSession,lB, lQ, &lCLSID2));
	}
	// Create 1 PIN with (!a and b and !c)
	{
		PID lPID;
		Value lV[1];
		SETVALUE(lV[0], lPropIDs[1], 1, OP_SET);
		CREATEPIN(mSession, &lPID, lV, 1);

		TVERIFY(testQuery(lCLSID, 1));
	}

	// Create 1 PIN with (!a and !b and c)
	{
		PID lPID;
		Value lV[2];
		SETVALUE(lV[0], lPropIDs[2], 1, OP_SET);
		CREATEPIN(mSession, &lPID, lV, 1);

		TVERIFY(testQuery(lCLSID, 2));
	}
}

bool TestDNFQuery::testQuery(ClassID pClassID, int pExpCount)
{
	if(STORE_INVALID_CLASSID != pClassID)
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		SourceSpec lCS; lCS.objectID = pClassID; lCS.nParams = 0; lCS.params = NULL;
		lQ->addVariable(&lCS, 1);
		uint64_t lCount = 0;
		TVERIFYRC(lQ->count(lCount));
		TVERIFY(lCount == uint64_t(pExpCount));
		return lCount == uint64_t(pExpCount);
	}
	return false;
}
int TestDNFQuery::execute()
{	
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();	
		MVTRand::getString(mClassStr, 5, 10, false, false);
		doTest();
		mSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
 	return 0;
}
