/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

#define ADD_TWO_VALUES 1

#ifdef MODE_NON_DISTINCT
#undef MODE_NON_DISTINCT
#define MODE_NON_DISTINCT 0
#endif

// Publish this test.
class TestLeftJoin : public ITest
{
	static const int sNumPINs = 100;
	static const int sNumProps = 10;
	PropertyID mPropIDs[sNumProps];
	PropertyID mLHSPropID, mRHSPropID, mRefPropID, mPINPropID;	
	std::vector<PID> mLHSPINs;
	std::map<Tstring, PID> mRHSPINs;
	int mNumRHSPINs, mNumFamily2PINs;
	Tstring mClassStr;
	Tstring mLHSStr;
	static const char *sRHSStr[5];
	ISession * mSession;
	public:
		TEST_DECLARE(TestLeftJoin);
		virtual char const * getName() const { return "testleftjoin"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "..."; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Not yet implemented in the kernel..."; return false; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void doTest();
		void createMeta();
		void createData();
		ClassID getClassID(int pClassIndex);
		void executeQuery(ClassSpec *pLHSCS, ClassSpec *pRHSCS, IExprTree *pET = NULL, unsigned pMode = 0);
	protected:
		void testLeftJoinEQ();
		void testLeftJoinIN();
		void makeRHSPropColl();
		void makeLHSPropColl();		
};
TEST_IMPLEMENT(TestLeftJoin, TestLogger::kDStdOut);

const char *TestLeftJoin::sRHSStr[] = 
{
	"airtel",
	"reliance",
	"rogers",	
	"virgin",
	"vodafone",	
};

void TestLeftJoin::testLeftJoinEQ()
{
	Value lLHSValue[1]; 
	lLHSValue[0].set(mLHSStr.c_str());
	
	ClassSpec lLHSCS[1];
	lLHSCS[0].classID = getClassID(0);
	lLHSCS[0].nParams = 1;
	lLHSCS[0].params = lLHSValue;		

	Value lRHSValue[1]; 
	lRHSValue[0].set(sRHSStr[0]);
	
	ClassSpec lRHSCS[1];
	lRHSCS[0].classID = getClassID(1);
	lRHSCS[0].nParams = 1;
	lRHSCS[0].params = lRHSValue;
	
	IStmt * lQ = mSession->createStmt();		
	unsigned char lVar1 = lQ->addVariable(lLHSCS, 1);
	unsigned char lVar2 = lQ->addVariable(lRHSCS, 1);
	
	Value lV[2];
	lV[1].setVarRef(lVar1,mRefPropID);
	lV[0].setVarRef(lVar2,mPINPropID);
	CmvautoPtr<IExprTree> lJoinET(mSession->expr(OP_EQ,2,lV));

	lQ->join(lVar1, lVar2, lJoinET, QRY_LEFT_OUTER_JOIN);
	uint64_t lCount = 0;
	TVERIFYRC(lQ->count(lCount, 0, 0, ~0, MODE_VERBOSE));
	TVERIFY(lCount == (uint64_t)mLHSPINs.size());
	lQ->destroy();
}

void TestLeftJoin::testLeftJoinIN()
{
	Value lVal[2]; lVal[0].set(mLHSStr.c_str()); lVal[1].set(mLHSStr.c_str());
	Value lLHSVal[1]; lLHSVal[0].setRange(lVal);
	ClassSpec lLHSCS[1];
	lLHSCS[0].classID = getClassID(2);
	lLHSCS[0].nParams = 1;
	lLHSCS[0].params = lLHSVal;

	char lStart[2]; lStart[0] = (char)0; lStart[1] = '\0';
	char lEnd[2]; lEnd[0] = (char)255; lEnd[1] = '\0';
	Value lVal2[2]; lVal2[0].set(lStart); lVal2[1].set(lEnd);
	Value lRHSVal[1]; lRHSVal[0].setRange(lVal2);
	ClassSpec lRHSCS[1];
	lRHSCS[0].classID = getClassID(3);
	lRHSCS[0].nParams = 1;
	lRHSCS[0].params = lRHSVal;

	IStmt * lQ = mSession->createStmt();		
	unsigned char lVar1 = lQ->addVariable(lLHSCS, 1);
	unsigned char lVar2 = lQ->addVariable(lRHSCS, 1);
	Value lV[2];
	lV[1].setVarRef(lVar1,mRefPropID);
	lV[0].setVarRef(lVar2,mPINPropID);
	CmvautoPtr<IExprTree> lJoinET(mSession->expr(OP_EQ,2,lV));
	lQ->join(lVar1, lVar2, lJoinET, QRY_LEFT_OUTER_JOIN);
	uint64_t lCount = 0;
	TVERIFYRC(lQ->count(lCount, 0, 0, ~0, MODE_VERBOSE));
	TVERIFY((int)lCount == mNumRHSPINs);
	lQ->destroy();
}

void TestLeftJoin::makeRHSPropColl()
{
	int i = 0;
	for(i = 0; i < 5; i++)
	{
		std::map<Tstring, PID>::const_iterator lIter = mRHSPINs.find(sRHSStr[i]);
		for(; lIter !=  mRHSPINs.end(); lIter++)
		{
			PID lPID = lIter->second;
			IPIN *lPIN = mSession->getPIN(lPID);
			if(lPIN)
			{
				Value lV[2]; 				
				SETVALUE_C(lV[0], mRHSPropID, sRHSStr[3], OP_ADD, STORE_LAST_ELEMENT);
				SETVALUE_C(lV[1], mRHSPropID, sRHSStr[4], OP_ADD, STORE_LAST_ELEMENT);
				TVERIFYRC(lPIN->modify(lV, 2));
				lPIN->destroy();
			}
		}
	}
}
void TestLeftJoin::makeLHSPropColl()
{
	// Make Family1Prop a collection
	Tstring lLHSStr1; MVTRand::getString(lLHSStr1, 10, 10, false, true);
	Tstring lLHSStr2; MVTRand::getString(lLHSStr2, 10, 10, false, true);
	size_t k = 0;
	for(k = 0; k < mLHSPINs.size(); k++)
	{
		PID lPID = mLHSPINs[k];
		IPIN *lPIN = mSession->getPIN(lPID);
		if(lPIN)
		{
			Value lV[2]; 
			SETVALUE_C(lV[0], mLHSPropID, lLHSStr1.c_str(), OP_ADD, STORE_LAST_ELEMENT);
			SETVALUE_C(lV[1], mLHSPropID, lLHSStr2.c_str(), OP_ADD, STORE_LAST_ELEMENT);
			TVERIFYRC(lPIN->modify(lV, 2));
			lPIN->destroy();
		}
	}
}
void TestLeftJoin::doTest()
{
	mLHSPropID = mPropIDs[0];
	mRHSPropID = mPropIDs[1];
	mRefPropID = mPropIDs[5];
	mPINPropID = PROP_SPEC_PINID;
	MVTRand::getString(mClassStr, 10, 10, false, true);
	createMeta();
	createData();

	mLogger.out() << "PINs in LHS Family = " << (long)mLHSPINs.size() << " ";
	mLogger.out() << "PINs in RHS Family = " << mNumRHSPINs << std::endl;		

	// Join Family1 and Family2(OP_EQ Families)
	testLeftJoinEQ();
	
	// Join Family3 and Family4(OP_IN Families)
	testLeftJoinIN();

	// Make Family2Prop a collection
	makeRHSPropColl();	

	// Join Family1 and Family2 with Family2.prop being collection(OP_EQ Families)
	testLeftJoinEQ();
	
	// Join Family3 and Family4 with Family4.prop being collection(OP_IN Families)
	testLeftJoinIN();

	// Make Family1Prop a collection
	makeLHSPropColl();	

	// Join Family1 and Family2 with Family1.prop and Family2.prop being collection(OP_EQ Families)
	testLeftJoinEQ();
	
	// Join Family3 and Family4 with Family3.prop and Family4.prop being collection(OP_IN Families)
	testLeftJoinIN();
}

void TestLeftJoin::createMeta()
{
	// Family #0
	ClassID lLHSFamilyID = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mLHSPropID);
			lV[1].setParam(0);
			lET = mSession->expr(OP_EQ, 2, lV, CASE_INSENSITIVE_OP);
		}
		lQ->addCondition(lVar,lET);			
		char lB[64];			
		sprintf(lB, "TestLeftJoin.%s.Family%d", mClassStr.c_str(), 0);
		if(RC_OK!=defineClass(mSession,lB, lQ, &lLHSFamilyID)){
			mLogger.out() << "ERROR(TestLeftJoin): Failed to create Family " << lB << std::endl;					
		}
	}

	// Family #1
	ClassID lRHSFamilyID = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mRHSPropID);
			lV[1].setParam(0);
			lET = mSession->expr(OP_EQ, 2, lV, CASE_INSENSITIVE_OP);
		}
		lQ->addCondition(lVar,lET);			
		char lB[64];			
		sprintf(lB, "TestLeftJoin.%s.Family%d", mClassStr.c_str(), 1);
		if(RC_OK!=defineClass(mSession,lB, lQ, &lRHSFamilyID)){
			mLogger.out() << "ERROR(TestLeftJoin): Failed to create Family " << lB << std::endl;					
		}
	}
	
	// Family #2
	ClassID lLHSFamilyID2 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mLHSPropID);
			lV[1].setParam(0);
			lET = mSession->expr(OP_IN, 2, lV, CASE_INSENSITIVE_OP);
		}
		lQ->addCondition(lVar,lET);			
		char lB[64];			
		sprintf(lB, "TestLeftJoin.%s.Family%d", mClassStr.c_str(), 2);
		if(RC_OK!=defineClass(mSession,lB, lQ, &lLHSFamilyID2)){
			mLogger.out() << "ERROR(TestLeftJoin): Failed to create Family " << lB << std::endl;					
		}
	}

	// Family #3
	ClassID lRHSFamilyID2 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mRHSPropID);
			lV[1].setParam(0);
			lET = mSession->expr(OP_IN, 2, lV, CASE_INSENSITIVE_OP);
		}
		lQ->addCondition(lVar,lET);			
		char lB[64];			
		sprintf(lB, "TestLeftJoin.%s.Family%d", mClassStr.c_str(), 3);
		if(RC_OK!=defineClass(mSession,lB, lQ, &lRHSFamilyID2)){
			mLogger.out() << "ERROR(TestLeftJoin): Failed to create Family " << lB << std::endl;					
		}
	}

	// Family #4
	ClassID lLHSFamilyID3 = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mRefPropID);
			lV[1].setParam(0);
			lET = mSession->expr(OP_EQ, 2, lV, CASE_INSENSITIVE_OP);
		}
		lQ->addCondition(lVar,lET);			
		char lB[64];			
		sprintf(lB, "TestLeftJoin.%s.Family%d", mClassStr.c_str(), 4);
		if(RC_OK!=defineClass(mSession,lB, lQ, &lLHSFamilyID3)){
			mLogger.out() << "ERROR(TestLeftJoin): Failed to create Family " << lB << std::endl;					
		}
	}
}

void TestLeftJoin::createData()
{
	MVTRand::getString(mLHSStr, 10, 10, false, true);		
	mNumRHSPINs = 0;
	mNumFamily2PINs = 0;
	int i = 0;
	
	std::vector<PID> lPINs;
	// Create PINs for Family2
	for(i = 0; i < (int)sNumPINs/2; i++)
	{
		Value lV[5];	
		Tstring lRHSStr;
		SETVALUE(lV[0], mPropIDs[2], MVTRand::getRange(10, 100), OP_SET);
		SETVALUE(lV[1], mPropIDs[3], MVTRand::getBool(), OP_SET);
		Tstring lStr; MVTRand::getString(lStr, 10, 10, false, true);
		SETVALUE(lV[2], mPropIDs[4], lStr.c_str(), OP_SET);
		bool lJoinFamily = MVTRand::getRange(0, 100) > 50?true:false;
		if(lJoinFamily)
		{
			int lRand = MVTRand::getRange(0, 2);
			lRHSStr = sRHSStr[lRand];
			SETVALUE(lV[3], mRHSPropID, lRHSStr.c_str(), OP_SET);
		}
		PID lPID = {STORE_INVALID_PID, STORE_OWNER};
		CREATEPIN(mSession, lPID, lV, lJoinFamily?4:3);
		IPIN *lPIN = mSession->getPIN(lPID); TVERIFY(lPIN != NULL);
		if(lPIN) 
		{
			if(lJoinFamily) 
			{ 
				mRHSPINs.insert(std::map<Tstring, PID>::value_type(lRHSStr, lPID));
				mNumFamily2PINs++;
			}
			lPINs.push_back(lPID);
			lPIN->destroy();
			mNumRHSPINs++;
		}		
	}

	// Create PINs for Family1
	for(i = 0; i < (int)sNumPINs/2; i++)
	{
		Value lV[5];			
		SETVALUE(lV[0], mLHSPropID, mLHSStr.c_str(), OP_SET);
		SETVALUE(lV[1], mPropIDs[2], MVTRand::getRange(10, 100), OP_SET);
		SETVALUE(lV[2], mPropIDs[3], MVTRand::getBool(), OP_SET);
		Tstring lStr; MVTRand::getString(lStr, 10, 10, false, true);
		SETVALUE(lV[3], mPropIDs[4], lStr.c_str(), OP_SET);
		SETVALUE(lV[4], mPropIDs[5], lPINs[i], OP_SET); // add VT_REFID property to pins created before

		PID lPID = {STORE_INVALID_PID, STORE_OWNER};
		CREATEPIN(mSession, lPID, lV, 5);
		IPIN *lPIN = mSession->getPIN(lPID); TVERIFY(lPIN != NULL);
		mLHSPINs.push_back(lPID);
		if(lPIN) lPIN->destroy();		
	}
	lPINs.clear();
}

ClassID TestLeftJoin::getClassID(int pClassIndex)
{
	char lB[64];			
	sprintf(lB, "TestLeftJoin.%s.Family%d", mClassStr.c_str(), pClassIndex);
	ClassID lCLSID = STORE_INVALID_CLASSID;
	TVERIFYRC(mSession->getClassID(lB, lCLSID));
	return lCLSID;
}

int TestLeftJoin::execute()
{
	bool lSuccess = true; 
	if (MVTApp::startStore())
	{
		mSession =	MVTApp::startSession();
		MVTApp::mapURIs(mSession, "TestLeftJoin.prop.", sNumProps, mPropIDs);
		doTest();		
		mSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
	return lSuccess ? 0 : 1;
}

void TestLeftJoin::executeQuery(ClassSpec *pLHSCS, ClassSpec *pRHSCS, IExprTree *pET, unsigned pMode)
{
	IStmt * lQ = mSession->createStmt();		
	unsigned char lVar1 = lQ->addVariable(pLHSCS, 1);
	unsigned char lVar2 = lQ->addVariable(pRHSCS, 1);
	Value lV[2];
	lV[1].setVarRef(lVar1,mRefPropID);
	lV[0].setVarRef(lVar2,mPINPropID);
	CmvautoPtr<IExprTree> lJoinET(mSession->expr(OP_EQ,2,lV));

	lQ->join(lVar1, lVar2, lJoinET, QRY_LEFT_OUTER_JOIN);
	uint64_t lCount = 0;
	TVERIFYRC(lQ->count(lCount, 0, 0, ~0, MODE_VERBOSE));
	TVERIFY(lCount == (uint64_t)mLHSPINs.size());

	OrderSeg lOrder = {NULL,mRHSPropID,ORD_DESC,0,0};
	lQ->setOrder(&lOrder, 1); 
	AfyDB::ICursor *lR = NULL;
	lQ->execute(&lR, NULL, 0, ~0, 0, pMode|MODE_VERBOSE);
	int lResultCount = 0;
	if(lR)
	{		
		for(IPIN *lPIN = lR->next(); lPIN != NULL; lPIN = lR->next())
		{
			lResultCount++;
			PID lResultPID = lPIN->getPID();
			if(isVerbose()) mLogger.out() << std::hex << lResultPID.pid << std::endl;
			TVERIFY(lResultPID.pid != STORE_INVALID_PID && "Junk PIN returned in the result set");
			bool lFound = false;
			size_t j = 0;
			for(j = 0; j < mLHSPINs.size(); j++)
			{
				PID lPID = mLHSPINs[j];
				if(lPID.pid == lResultPID.pid)
				{
					lFound = true; break;
				}
			}
			if(!lFound) TVERIFY(false && "PIN belonging to Family1 wasn't returned in LEFT JOIN");					
		}
		lR->destroy();
	}
	TVERIFY(lResultCount == (int)mLHSPINs.size() && "Not all PINs were returned");
	lQ->destroy();
}
