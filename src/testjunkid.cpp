/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

class TestJunkID : public ITest
{
		static const int sNumProps = 5;
		static const int sNumPINs = 100;
		PropertyID	mPropIDs[sNumProps];
		ISession * mSession;
		std::vector<PID> mPIDs;
	public:
		TEST_DECLARE(TestJunkID);
		virtual char const * getName() const { return "testjunkid"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Kernel errors on junk PIDs, ElementIDs etc (bug 29621)"; }
		virtual int execute();
		virtual void destroy() { delete this; }	
	protected:
		void doTest();
		void createPINs();
		void doGetPIN();
		void doAddElement();
		
};
TEST_IMPLEMENT(TestJunkID, TestLogger::kDStdOut);

void TestJunkID::createPINs()
{
	int i = 0;
	for(i = 0; i < sNumPINs; i++)
	{
		Value lV[5];
		SETVALUE(lV[0], mPropIDs[0], i, OP_SET);
		SETVALUE_C(lV[1], mPropIDs[1], MVTRand::getRange(1, 100), OP_ADD, STORE_LAST_ELEMENT);
		SETVALUE_C(lV[2], mPropIDs[1], MVTRand::getRange(1, 100), OP_ADD, STORE_LAST_ELEMENT);
		PID lPID; CREATEPIN(mSession, &lPID, lV, 3);
		mPIDs.push_back(lPID);
	}
}
void TestJunkID::doGetPIN()
{
	PID lLastPID = mPIDs[sNumPINs-1];
	uint32_t lPageID = uint32_t((lLastPID.pid << PAGE_SHIFT) >> 32);	
	mLogger.out() << std::hex << "Last PID is " << lLastPID.pid << " on Page " << lPageID << std::endl;
	for(int i = sNumPINs; i < (sNumPINs + 100); i++)
	{
		PID lPID; INITLOCALPID(lPID);
		lPID.pid = ( uint64_t(mSession->getLocalStoreID()) << STOREID_SHIFT ) + ( lPageID << PAGE_SHIFT ) + i ; 
		if(!MVTApp::isRunningSmokeTest()) {lPID.pid+=1; IPIN *lPIN = mSession->getPIN(lPID);  TVERIFY(lPIN == NULL); }
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned char lVar = lQ->addVariable();
		{
			Value lV[2];
			lV[0].setVarRef(0);
			lV[1].set(lPID);
			CmvautoPtr<IExprTree> lET(mSession->expr(OP_EQ, 2, lV));
			TVERIFYRC(lQ->addCondition(lVar,lET));
		}
		ICursor* lC = NULL;
		TVERIFYRC(lQ->execute(&lC));
		CmvautoPtr<ICursor> lR(lC);
		if(lR.IsValid())
		{
			for(IPIN *lPIN = lR->next(); lPIN != NULL; lPIN = lR->next())
				lPIN->destroy();
		}
	}
}

void TestJunkID::doAddElement()
{
	size_t i = 0;
	for(i = 0; i < mPIDs.size(); i++)
	{
		IPIN *lPIN = mSession->getPIN(mPIDs[i]);
		Value lV[2];
		SETVALUE_C(lV[0], mPropIDs[1], "junk", OP_ADD_BEFORE, -100);
		TVERIFY(RC_NOTFOUND == lPIN->modify(lV, 1));

		SETVALUE_C(lV[0], mPropIDs[1], "junk", OP_ADD_BEFORE, 666);
		TVERIFY(RC_NOTFOUND == lPIN->modify(lV, 1));

		lPIN->destroy();
	}
}

void TestJunkID::doTest()
{
	MVTUtil::mapURIs(mSession, "TestJunkID.prop.", sNumProps, mPropIDs);
	createPINs();
	doGetPIN();	
	doAddElement();
}

int TestJunkID::execute() 
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();
		doTest();
		mSession->terminate();
		MVTApp::stopStore();
	}
	return lSuccess?0:1;
}
