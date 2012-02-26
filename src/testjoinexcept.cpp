/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

class TestJoinExcept:  public ITest
{
		static const int sNumPINs = 1000;
		static const int sNumExceptPINs = 20;		
		static const int sNumProps = 5;
		PropertyID mPropIds[sNumProps];
	public:
		TEST_DECLARE(TestJoinExcept);
		virtual char const * getName() const { return "testjoinexcept"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return " Time consuming array scan + class scan"; }
		virtual void destroy() { delete this; }
		virtual int execute();		
	private:
		ISession * mSession ;	
		PID mBigAlbumPID, mSmallAlbumPID;
		ClassID mClassID, mFamilyID, mFamily2ID;
		PropertyID mPostsPropID;
		std::vector<PID> mPIDs, mExceptPIDs;

		void doTest();
		void createMeta();
		void createData();

		void runJoinQuery(ClassID pLeftCLSID, ClassID pRightCLSID, QUERY_SETOP pJoinOp, unsigned long pExpectedCount);
		void runJoinQuery(PID pPID, PropertyID pPropID, ClassID pRightCLSID, QUERY_SETOP pJoinOp, unsigned long pExpectedCount);
};

TEST_IMPLEMENT(TestJoinExcept, TestLogger::kDStdOut);

void TestJoinExcept::createMeta()
{
	Tstring lStr; MVTRand::getString(lStr, 5, 10, false, true);				
	mClassID = STORE_INVALID_CLASSID;
	{	
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned char lVar = lQ->addVariable();
		{
			Value lV[1];
			lV[0].setVarRef(0,mPropIds[1]);
			CmvautoPtr<IExprTree> lET(mSession->expr(OP_EXISTS, 1, lV));
			TVERIFYRC(lQ->addCondition(lVar,lET));			
		}
		char lB[64]; sprintf(lB, "TestJoinExcept.%s.Class%d", lStr.c_str(), 0);
		TVERIFYRC(defineClass(mSession,lB, lQ, &mClassID));
	}
	mFamilyID = STORE_INVALID_CLASSID;
	{	
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned char lVar = lQ->addVariable();
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIds[1]);
			lV[1].setParam(0);
			CmvautoPtr<IExprTree> lET(mSession->expr(OP_IN, 2, lV, CASE_INSENSITIVE_OP));
			TVERIFYRC(lQ->addCondition(lVar,lET));			
		}
		char lB[64]; sprintf(lB, "TestJoinExcept.%s.Family%d", lStr.c_str(), 1);
		TVERIFYRC(defineClass(mSession,lB, lQ, &mFamilyID));
	}
	mFamily2ID = STORE_INVALID_CLASSID;
	{	
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned char lVar = lQ->addVariable();
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIds[4]);
			lV[1].setParam(0);
			CmvautoPtr<IExprTree> lET(mSession->expr(OP_IN, 2, lV, CASE_INSENSITIVE_OP));
			TVERIFYRC(lQ->addCondition(lVar,lET));			
		}
		char lB[64]; sprintf(lB, "TestJoinExcept.%s.Family%d", lStr.c_str(), 2);
		TVERIFYRC(defineClass(mSession,lB, lQ, &mFamily2ID));
	}
}
void TestJoinExcept::createData()
{
	// Create the Children PINs	
	mLogger.out() << "Creating " << sNumPINs * 2 << " PINs ...";
	int i;
	for(i = 0; i < sNumPINs; i++)
	{		
		if(i%100 == 0) mLogger.out() << ".";
		PID lPID;
		Value lV[2]; Tstring lStr; MVTRand::getString(lStr, 5, 10, false, true);
		SETVALUE(lV[0], mPropIds[1], lStr.c_str(), OP_SET);
		CREATEPIN(mSession, lPID, lV, 1);
		mPIDs.push_back(lPID);
	}
	for(i = 0; i < sNumExceptPINs; i++)
	{
		Value lV[2]; Tstring lStr; MVTRand::getString(lStr, 5, 10, false, true);
		PID lExceptPID;
		SETVALUE(lV[0], mPropIds[3], lStr.c_str(), OP_SET);
		CREATEPIN(mSession, lExceptPID, lV, 1);
		mExceptPIDs.push_back(lExceptPID);
	}
	mLogger.out() << " DONE" << std::endl;
	
	// Create the Album PINs
	{
		Value lV[1];
		
		SETVALUE(lV[0], mPropIds[0], "Big Album PIN", OP_SET);
		CREATEPIN(mSession, mBigAlbumPID, lV, 1);
		mLogger.out() << "Big Album PIN : " << mBigAlbumPID.pid << std::endl;
		
		SETVALUE(lV[0], mPropIds[0], "Small Album PIN", OP_SET);
		CREATEPIN(mSession, mSmallAlbumPID, lV, 1);
		mLogger.out() << "Small Album PIN : " << mSmallAlbumPID.pid << std::endl;
	}
	
	// Add the children as posts to Big Album PIN
	IPIN *lPIN = mSession->getPIN(mBigAlbumPID);
	for(i = 0; i < (int)mPIDs.size(); i++)
	{
		Value lV[2];
		SETVALUE_C(lV[0], mPostsPropID, mPIDs[i], OP_ADD, STORE_LAST_ELEMENT);
		TVERIFYRC(lPIN->modify(lV, 1));
	}
	lPIN->destroy();

	// Add the children as posts to Small Album PIN
	lPIN = mSession->getPIN(mSmallAlbumPID);
	for(i = 0; i < sNumExceptPINs; i++)
	{
		Value lV[2];
		SETVALUE_C(lV[0], mPostsPropID, mPIDs[i], OP_ADD, STORE_LAST_ELEMENT);
		SETVALUE_C(lV[1], mPostsPropID, mExceptPIDs[i], OP_ADD, STORE_LAST_ELEMENT);
		TVERIFYRC(lPIN->modify(lV, 2));
	}
	lPIN->destroy();
}

void TestJoinExcept::runJoinQuery(ClassID pLeftCLSID, ClassID pRightCLSID, QUERY_SETOP pJoinOp, unsigned long pExpectedCount)
{
	// storejoin /pin[pin is class()] as A, [pin is family()] as B were A except|intersects B
	//mLogger.out() << ">>> " << std::endl;
	assert(pJoinOp != QRY_SEMIJOIN && pJoinOp != QRY_JOIN);	
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());			
		ClassSpec lCS[2]; 
		lCS[0].classID = pLeftCLSID; lCS[0].nParams = 0; lCS[0].params = NULL;
		lCS[1].classID = pRightCLSID; lCS[1].nParams = 0; lCS[1].params = NULL;
		unsigned char lLeftVar = lQ->addVariable(&lCS[0], 1);		
		unsigned char lRightVar = lQ->addVariable(&lCS[1], 1);
		if (pJoinOp<QRY_UNION) lQ->setOp(lLeftVar, lRightVar, QRY_INTERSECT); else lQ->setOp(lLeftVar, lRightVar, pJoinOp);
		uint64_t lCount = 0;
		TIMESTAMP lStart; getTimestamp(lStart);
		if(!isVerbose())
			TVERIFYRC(lQ->count(lCount));
		else
			TVERIFYRC(lQ->count(lCount, 0, 0, ~0, MODE_VERBOSE));
		TIMESTAMP lEnd; getTimestamp(lEnd);
		//mLogger.out() << "runJoinQuery: Count = " << lCount << std::endl;		
		
		if(lCount != pExpectedCount)
			mLogger.out() << "Expected count " << pExpectedCount << "; IStmt::count() returned " << lCount << std::endl;
		TVERIFY(lCount == pExpectedCount);	

		lCount = 0;
		ICursor* lC = NULL;
		TVERIFYRC(lQ->execute(&lC));
		CmvautoPtr<ICursor> lR(lC);
		if(lR.IsValid())
		{
			PID lPID;
			while(lR->next(lPID) == RC_OK)
			{
				if(isVerbose()) mLogger.out() << std::hex << lPID.pid << std::dec << std::endl;
				lCount++;
			}
		}
		if(lCount != pExpectedCount)
			mLogger.out() << "Expected count " << pExpectedCount << "; ICursor::next(&PID) count " << lCount << std::endl;
		TVERIFY(lCount == pExpectedCount);

		lCount = 0;
		TVERIFYRC(lQ->execute(&lC));
		CmvautoPtr<ICursor> lRes(lC);
		if(lRes.IsValid())
		{
			for(IPIN *lPIN = lRes->next(); lPIN != NULL; lPIN = lRes->next())
			{
				if(isVerbose()) mLogger.out() << std::hex << lPIN->getPID().pid << std::dec << std::endl;
				lCount++;
			}
		}
		if(lCount != pExpectedCount)
			mLogger.out() << "Expected count " << pExpectedCount << "; ICursor::next() count " << lCount << std::endl;
		TVERIFY(lCount == pExpectedCount);
	}
}

void TestJoinExcept::runJoinQuery(PID pPID, PropertyID pPropID, ClassID pRightCLSID, QUERY_SETOP pJoinOp, unsigned long pExpectedCount)
{
	// storejoin /pin[@pid=$pinid]/posts as A, [pin is family()] as B were A except|joins|intersects B	
	//mLogger.out() << ">>> " << std::endl;
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());			
		ClassSpec lCS; lCS.classID = pRightCLSID; lCS.nParams = 0; lCS.params = NULL;
		unsigned char lLeftVar = lQ->addVariable(pPID, pPropID);		
		unsigned char lRightVar = lQ->addVariable(&lCS, 1);
		if (pJoinOp<QRY_UNION) lQ->setOp(lLeftVar, lRightVar, QRY_INTERSECT); else lQ->setOp(lLeftVar, lRightVar, pJoinOp);
		uint64_t lCount = 0;
		TIMESTAMP lStart; getTimestamp(lStart);
		if(!isVerbose())
			TVERIFYRC(lQ->count(lCount));
		else
			TVERIFYRC(lQ->count(lCount, 0, 0, ~0, MODE_VERBOSE));
		TIMESTAMP lEnd; getTimestamp(lEnd);
		//mLogger.out() << "runJoinQuery2: Count = " << lCount << std::endl;		
		if(lCount != pExpectedCount)
			mLogger.out() << "Expected count " << pExpectedCount << "; IStmt::count() returned " << lCount << std::endl;
		TVERIFY(lCount == pExpectedCount);	

		lCount = 0;
		ICursor* lC = NULL;
		TVERIFYRC(lQ->execute(&lC));
		CmvautoPtr<ICursor> lR(lC);
		if(lR.IsValid())
		{
			PID lPID;
			while(lR->next(lPID) == RC_OK)
			{
				if(isVerbose()) mLogger.out() << std::hex << lPID.pid << std::dec << std::endl;
				lCount++;
			}
		}
		if(lCount != pExpectedCount)
			mLogger.out() << "Expected count " << pExpectedCount << "; ICursor::next(&PID) count " << lCount << std::endl;
		TVERIFY(lCount == pExpectedCount);

		lCount = 0;
		TVERIFYRC(lQ->execute(&lC));
		CmvautoPtr<ICursor> lRes(lC);
		if(lRes.IsValid())
		{
			for(IPIN *lPIN = lRes->next(); lPIN != NULL; lPIN = lRes->next())
			{
				if(isVerbose()) mLogger.out() << std::hex << lPIN->getPID().pid << std::dec << std::endl;
				lCount++;
			}
		}
		if(lCount != pExpectedCount)
			mLogger.out() << "Expected count " << pExpectedCount << "; ICursor::next() count " << lCount << std::endl;
		TVERIFY(lCount == pExpectedCount);
	}
}

void TestJoinExcept::doTest()
{
	createData();

	createMeta();
	
	//mLogger.out() << "Case #1 : " << std::endl;
	runJoinQuery(mSmallAlbumPID, mPostsPropID, mFamilyID, QRY_SEMIJOIN, sNumExceptPINs);
	runJoinQuery(mSmallAlbumPID, mPostsPropID, mClassID, QRY_SEMIJOIN, sNumExceptPINs);
	runJoinQuery(mSmallAlbumPID, mPostsPropID, mFamilyID, QRY_EXCEPT, sNumExceptPINs);
	runJoinQuery(mSmallAlbumPID, mPostsPropID, mFamilyID, QRY_EXCEPT, sNumExceptPINs);

	//mLogger.out() << "Case #2 : " << std::endl;
	runJoinQuery(mBigAlbumPID, mPostsPropID, mFamilyID, QRY_SEMIJOIN, sNumPINs);
	runJoinQuery(mBigAlbumPID, mPostsPropID, mClassID, QRY_SEMIJOIN, sNumPINs);
	runJoinQuery(mBigAlbumPID, mPostsPropID, mFamilyID, QRY_EXCEPT, 0);
	runJoinQuery(mBigAlbumPID, mPostsPropID, mFamilyID, QRY_EXCEPT, 0);

	//mLogger.out() << "Case #3 : " << std::endl;
	runJoinQuery(mFamily2ID, mFamilyID, QRY_EXCEPT, 0);
	runJoinQuery(mFamilyID, mClassID, QRY_EXCEPT, 0);
}

int TestJoinExcept::execute() 
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();
		MVTApp::mapURIs(mSession, "TestJoinExcept.prop.", sNumProps, mPropIds);
		mPostsPropID = mPropIds[2];
		doTest();
		mSession->terminate();
		MVTApp::stopStore();
	}
	return lSuccess?0:1;
}
