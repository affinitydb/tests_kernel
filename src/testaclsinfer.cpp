/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

class TestACLsInfer : public ITest
{
		ISession * mSession ;
		static const int sNumProps = 5;
		static const int sNumPINs = 100;	
		static const int sNumIdentities = 10;
		PropertyID mPropIDs[sNumProps];
		PID mFolderPID;
		ClassID mCLSID, mCLSID2;
		Tstring mClassStr;		
		int mExpectedCount;
	public:
		TEST_DECLARE(TestACLsInfer);
		virtual char const * getName() const { return "testaclsinfer"; }
		virtual char const * getHelp() const { return ""; }		
		virtual char const * getDescription() const { return "tests the ACL inference optimization (lazy evaluation)"; }
		virtual void destroy() { delete this; }		
		virtual int execute();
	private:
		void doTest();
		void createIdentities();
		void createPINs();
		void createMeta();
		IdentityID getIdentityID(int pIndex);
		void testACLs(ISession *pSession);
		void testJoinACLs(ISession *pSession);
		void testIdentities();
		void shareFolderPIN(IdentityID pIID);
};
TEST_IMPLEMENT(TestACLsInfer, TestLogger::kDStdOut);

void TestACLsInfer::doTest()
{
	mSession = MVTApp::startSession();
	MVTApp::mapURIs(mSession, "TestACLsInfer.prop.", sNumProps, mPropIDs);
		
	createIdentities();
	createMeta();
	createPINs();
	// Test for STORE_OWNER First
	testACLs(mSession);
	testJoinACLs(mSession);
	mSession->terminate();
	
	// Test for other identities
	testIdentities();
}

void TestACLsInfer::testIdentities()
{
	int i = 0;
	
	for(i = 0; i < sNumIdentities; i++)
	{
		char lIdentity[255]; sprintf(lIdentity, "testaclsinfer.identity%d", i);
		ISession *lSession = MVTApp::startSession(0, lIdentity); TVERIFY(lSession != NULL);
		IdentityID lIID = lSession->getCurrentIdentityID(); TVERIFY(lIID != STORE_INVALID_IDENTITY);	
		testACLs(lSession);
		testJoinACLs(lSession);
		lSession->terminate();
	}
}

void TestACLsInfer::testJoinACLs(ISession *pSession)
{
	TVERIFY(pSession != NULL);
	CmvautoPtr<IStmt> lQ(pSession->createStmt());
	Value lParam[3];
	lParam[0].setError(mPropIDs[2]); lParam[1].setError(mPropIDs[2]); lParam[2].setRange(lParam);
	ClassSpec lCS[2] = {{mCLSID, 0, NULL}, {mCLSID2, 1, &lParam[2]}};
	unsigned char lVar1 = lQ->addVariable(&lCS[0], 1);
	unsigned char lVar2 = lQ->addVariable(&lCS[1], 1);
	IExprTree *lET;
	{
		Value lV[2];
		lV[0].setVarRef(lVar1,mPropIDs[1]);
		lV[1].setVarRef(lVar2,mPropIDs[2]);		
		lET = pSession->expr(OP_EQ, 2, lV);
	}
	lQ->join(lVar1, lVar2, lET);
	uint64_t lCount = 0;
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount == uint64_t(mExpectedCount));
	unsigned long lResultCount = 0;
	ICursor* lC = NULL;
	TVERIFYRC(lQ->execute(&lC));
	CmvautoPtr<ICursor> lR(lC);
	if(lR.IsValid())
	{
		for(IPIN *lPIN = lR->next(); lPIN != NULL; lPIN = lR->next(), lResultCount++)
		{
			PID lPID = lPIN->getPID();
			lPIN->destroy(); lPIN = NULL;
			lPIN = pSession->getPIN(lPID); TVERIFY(lPIN != NULL);
			lPIN->destroy();
		}
	}
	TVERIFY(lResultCount == (unsigned long)mExpectedCount);
}
void TestACLsInfer::testACLs(ISession *pSession)
{
	TVERIFY(pSession != NULL);
	CmvautoPtr<IStmt> lQ(pSession->createStmt());
	ClassSpec lCS; lCS.classID = mCLSID; lCS.nParams = 0; lCS.params = NULL;
	lQ->addVariable(&lCS, 1);
	uint64_t lCount = 0;
	TVERIFYRC(lQ->count(lCount));
	TVERIFY(lCount == uint64_t(mExpectedCount));
	unsigned long lResultCount = 0;
	ICursor* lC = NULL;
	TVERIFYRC(lQ->execute(&lC));
	CmvautoPtr<ICursor> lR(lC);
	if(lR.IsValid())
	{
		for(IPIN *lPIN = lR->next(); lPIN != NULL; lPIN = lR->next(), lResultCount++)
		{
			PID lPID = lPIN->getPID();
			lPIN->destroy(); lPIN = NULL;
			lPIN = pSession->getPIN(lPID); TVERIFY(lPIN != NULL);
			lPIN->destroy();
		}
	}
	TVERIFY(lResultCount == (unsigned long)mExpectedCount);
}
void TestACLsInfer::createMeta()
{
	mCLSID = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[1]);
			lV[1].setParam(0);
			lET = mSession->expr(OP_IN, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();	
		char lB[124]; sprintf(lB, "TestACLsInfer.%s.Family.%d", mClassStr.c_str(), 0);
		TVERIFYRC(defineClass(mSession,lB, lQ, &mCLSID));
	}
	mCLSID2 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[2]);
			lV[1].setParam(0);
			lET = mSession->expr(OP_IN, 2, lV);
		}
		TVERIFYRC(lQ->addCondition(lVar,lET)); lET->destroy();	
		char lB[124]; sprintf(lB, "TestACLsInfer.%s.Family.%d", mClassStr.c_str(), 1);
		TVERIFYRC(defineClass(mSession,lB, lQ, &mCLSID2));
	}
}
void TestACLsInfer::createPINs()
{
	INITLOCALPID(mFolderPID);
	{
		Value lV[2];
		SETVALUE(lV[0], mPropIDs[0], "Folder PIN", OP_SET);
		CREATEPIN(mSession, mFolderPID, lV, 1);
		IPIN *lPIN = mSession->getPIN(mFolderPID);
		int i = 0;
		for(i = 0; i < sNumIdentities; i++)
		{
			IdentityID lIID = getIdentityID(i);
			lV[0].setIdentity(lIID);
			SETVATTR_C(lV[0], PROP_SPEC_ACL, OP_ADD, STORE_LAST_ELEMENT);
			lV[0].meta = ACL_READ;
			if(MVTRand::getBool()) lV[0].meta = lV[0].meta | ACL_WRITE;
			TVERIFYRC(lPIN->modify(lV, 1));
		}
		if(lPIN) { if(isVerbose()) MVTApp::output(*lPIN, mLogger.out(), mSession); lPIN->destroy();}
	}
	mLogger.out() << "\tCreating " << sNumPINs << " PINs ...";
	RefVID *lRef = (RefVID *)mSession->alloc(1*sizeof(RefVID));
	int i = 0;
	for(i = 0; i < sNumPINs; i++)
	{
		if(i % 100 == 0) mLogger.out() << ".";
		PID lPID;
		Value lV[5];
		Tstring lStr; MVTRand::getString(lStr, 10, 10, true, false);
		SETVALUE(lV[0], mPropIDs[0], lStr.c_str(), OP_SET);	
		SETVALUE(lV[1], mPropIDs[1], i, OP_SET);		
		RefVID l = {mFolderPID, PROP_SPEC_ACL, STORE_COLLECTION_ID, STORE_CURRENT_VERSION};
		*lRef = l;
		SETVALUE(lV[2], PROP_SPEC_ACL, *lRef, OP_SET);
		SETVALUE(lV[3], mPropIDs[2], i, OP_SET);	
		CREATEPIN(mSession, lPID, lV, 4);
	}
	mExpectedCount = sNumPINs;
	mSession->free(lRef);
	mLogger.out() << " DONE " << std::endl;
}

IdentityID TestACLsInfer::getIdentityID(int pIndex)
{
	TVERIFY(pIndex < sNumIdentities);
	char lIdentity[255];
	sprintf(lIdentity, "testaclsinfer.identity%d", pIndex);
	IdentityID const lIID = mSession->getIdentityID(lIdentity); 
	TVERIFY(lIID != STORE_INVALID_IDENTITY);
	return lIID;
}

void TestACLsInfer::createIdentities()
{
	int i = 0;
	for (i = 0; i < sNumIdentities; i++)
	{
		char lIdentity[255];
		sprintf(lIdentity, "testaclsinfer.identity%d", i);
		IdentityID const lIID = mSession->storeIdentity(lIdentity, NULL);
		TVERIFY(lIID != STORE_INVALID_IDENTITY);
	}
}

int TestACLsInfer::execute()
{
	bool lSuccess = true;	
	if (MVTApp::startStore())
	{
		MVTRand::getString(mClassStr, 5, 10, false, true);
		doTest() ;		
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	return lSuccess?0:1;
}
