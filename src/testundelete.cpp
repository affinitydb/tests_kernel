/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h"
#include "mvauto.h"

class TestUndelete : public ITest
{
		static const int sNumProps = 4;
		static const int sNumPINs = 3000;	
		PropertyID mPropIDs[sNumProps];
		DataEventID mCLSID, mFamilyID, mFamilyID1, mFamilyID2,mFamilyID3;
		std::vector<PID> mPIDs;
		Tstring mQueryStr;
		std::vector<Tstring> mPIDStr;		
	public:
		TEST_DECLARE(TestUndelete);
		virtual char const * getName() const { return "testundelete"; }
		virtual char const * getHelp() const { return ""; }
		
		virtual char const * getDescription() const { return "tests the undelete/soft delete feature"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual void destroy() { delete this; }		
		virtual int execute();
	private:
		void doTest() ;
		void createMeta();
		void createData();
		IStmt * getQuery(int pQueryType, PropertyID pPropID = STORE_INVALID_URIID, Value *pPropValue = NULL, STMT_OP=STMT_QUERY);
		void quickTest();
		void undeletePINs(int pUndeleteType, PID *pPIDs, int pNumPIDs);
		void deletePINs(int pDelType, PID *pPIDs, int pNumPIDs);
		int getPINIndex(PID lPID);
		const char *getPINStr(int pPINIndex);
		void testClass();
		void testFamily();
		void testFullScan();
		void testFT();
		void test(IStmt *pQuery);
		ISession * mSession;
};
TEST_IMPLEMENT(TestUndelete, TestLogger::kDStdOut);

void TestUndelete::doTest()
{
	// Do a quick test a single PIN
	quickTest();	
	
	// Test delete/undelete of PINs vs Classes
	mLogger.out() << "Case #1: With class..."  << std::endl;
	testClass();

	// Test delete/undelete of PINs vs Families
	mLogger.out() << "Case #2: With family..."  << std::endl;
	testFamily();

	// Test delete/undelete of PINs vs Full Scan
	if(!MVTApp::isRunningSmokeTest())
	{
		mLogger.out() << "Case #3: With Full Scan..."  << std::endl;
		testFullScan();
	}

	// FT would not supported on soft-deletes ever :)
	// Test delete/undelete of PINs vs FT
	//mLogger.out() << "Case #4: With FT..." << std::endl;
	//testFT();

	mPIDs.clear();
	mPIDStr.clear();
}

void TestUndelete::testClass()
{	
	IStmt *lQ = getQuery(2);
	long const lStartTime = getTimeInMs();
	test(lQ);
	long const lEndTime = getTimeInMs();
	mLogger.out() << "Time taken for testClass() :: " << (lEndTime - lStartTime) << " ms" << std::endl;
	lQ->destroy();
}

void TestUndelete::testFamily()
{
	Value lV[1]; lV[0].set(mQueryStr.c_str());
	IStmt *lQ = getQuery(6, 0, lV);
	long const lStartTime = getTimeInMs();
	test(lQ);
	long const lEndTime = getTimeInMs();
	mLogger.out() << "Time taken for testFamily() :: " << (lEndTime - lStartTime) << " ms" << std::endl;	
	lQ->destroy();
}

void TestUndelete::testFullScan()
{
	Value lV[1]; lV[0].set(mQueryStr.c_str());
	IStmt *lQ = getQuery(1, mPropIDs[2], lV);
	long const lStartTime = getTimeInMs();
	test(lQ);
	long const lEndTime = getTimeInMs();
	mLogger.out() << "Time taken for testFullScan() :: " << (lEndTime - lStartTime) << " ms" << std::endl;	
	lQ->destroy();
}

void TestUndelete::testFT()
{
	Value lV[1]; lV[0].set(mQueryStr.c_str());
	IStmt *lQ = getQuery(0, mPropIDs[2], lV);
	long const lStartTime = getTimeInMs();
	test(lQ);
	long const lEndTime = getTimeInMs();
	mLogger.out() << "Time taken for testFT() :: " << (lEndTime - lStartTime) << " ms" << std::endl;	
	lQ->destroy();
}

void TestUndelete::test(IStmt *pQuery)
{
	uint64_t lActualCount = 0, lDeleteCount = 0, lAftDeleteCount = 0, lAftUndeleteCount = 0;
	std::vector<PID> lDeletedPINs;

	pQuery->count(lActualCount);	
	ICursor *lR = NULL;
	TVERIFYRC(pQuery->execute(&lR));	
	if(lR)	
	{
		PID lPID = {STORE_INVALID_PID, STORE_OWNER};	
		while(RC_OK == lR->next(lPID))
		{
			if(MVTRand::getBool())
			{ 
				TVERIFYRC(mSession->deletePINs(&lPID, 1));
				lDeletedPINs.push_back(lPID);
			}			
		}		
		lR->destroy(); lR = NULL;
	}	
	uint64_t lNumDeletePINs = lDeletedPINs.size();
	long const lCountStartTime = getTimeInMs();
	pQuery->count(lDeleteCount, NULL, 0, ~0, MODE_DELETED);
	long const lCountEndTime = getTimeInMs();
	mLogger.out() << "Num of Deleted PINs :: " << lDeleteCount << ", Time taken to IStmt::count() on deleted PINs :: " << (lCountEndTime - lCountStartTime) << " ms" << std::endl;	
	TVERIFY(lNumDeletePINs == lDeleteCount && "More/Less PINs returned after delete");

	pQuery->count(lAftDeleteCount);
	TVERIFY((lActualCount-lNumDeletePINs) == lAftDeleteCount && "Deleted PINs not returned in query");

	long const lResultStartTime = getTimeInMs();
	TVERIFYRC(pQuery->execute(&lR, 0, 0, ~0, 0, MODE_DELETED));
	unsigned long lIResultCount = 0;
	if(lR)
	{
		PID lPID = {STORE_INVALID_PID, STORE_OWNER};
		while(RC_OK == lR->next(lPID)) lIResultCount++;			
		lR->destroy();
	}
	else
		TVERIFY(false && "Failed to return ICursor");

	long const lResultEndTime = getTimeInMs();
	mLogger.out() << "Num of Deleted PINs :: " << lIResultCount << ", Time taken to Iterate thro ICursor::next(&PID) on deleted PINs :: " << (lResultEndTime - lResultStartTime) << " ms" << std::endl;	
	TVERIFY(lIResultCount == lNumDeletePINs && "ICursor->next() did not return any/all pins");
	
	undeletePINs(2, &lDeletedPINs[0], int(lNumDeletePINs));

	pQuery->count(lDeleteCount, NULL, 0, ~0, MODE_DELETED);
	TVERIFY(lDeleteCount == 0 && "Not all PINs were undeleted");

	pQuery->count(lAftUndeleteCount);
	TVERIFY(lActualCount == lAftUndeleteCount && "Not all PINs were undeleted");

	lDeletedPINs.clear();
	undeletePINs(2, &mPIDs[0], sNumPINs);
}

void TestUndelete::quickTest()
{
	// Delete a PIN
	int lPINIndex;
	uint64_t lBefDelCount = 0, lAftDelCount = 0;
	uint64_t lAftUndeleteCount = 0;
	PID lPID;
	Value lV[1]; 
	IStmt *lQ;
	IPIN * lPIN = NULL;

	lPINIndex = MVTRand::getRange(0 , (int)(mPIDs.size()-1));
	lPID = mPIDs[lPINIndex];

	// lPID exists
	lPIN = mSession->getPIN(lPID, 0);
	if(!lPIN) TVERIFY(!"Cannot retrieve PIN that is not deleted");
	else 
	{
		TVERIFY( (lPIN->getFlags()&PIN_DELETED)==0 ) ;
		lPIN->destroy();
	}

	lPIN = mSession->getPIN(lPID, MODE_DELETED);
	if(!lPIN) TVERIFY(!"Cannot retrieve PIN");
	else 
	{
		// Retrieving same PIN with MODE_DELETED says that it is deleted 
		TVERIFY( (lPIN->getFlags()&PIN_DELETED)==0 ) ;
		lPIN->destroy();
	}

	lV[0].set(lPINIndex); 
	lQ = getQuery(4, mPropIDs[1], lV); 
	
	lQ->count(lBefDelCount); deletePINs(2, &lPID, 1);
	lQ->count(lAftDelCount, NULL, 0, ~0, MODE_DELETED);
	TVERIFY(lBefDelCount == lAftDelCount && "Deleted PIN not returned in query");
	lQ->destroy();

	// By default you can't get a deleted PIN
	lPIN = mSession->getPIN(lPID);	
	TVERIFY(lPIN==NULL);

	// MODE_DELETED says to get the PIN even if it is deleted
	lPIN = mSession->getPIN(lPID, MODE_DELETED);	
	if(!lPIN) TVERIFY(false && "Deleted pin cannot be retrieved with MODE_DELETED");
	else
	{
		TVERIFY( (lPIN->getFlags()&PIN_DELETED)!=0 ) ;
		lPIN->destroy();
	}

	undeletePINs(0, &lPID, 1);

	// Check with PIN was undeleted
	// Get PIN usign ISessin::getPIN()
	lPIN = mSession->getPIN(lPID);
	if(!lPIN) TVERIFY(false && "PIN is still marked as deleted after an undelete");
	else lPIN->destroy();
	
	// Passing MODE_DELETED causes no trouble to retrieve undeleted pins
	lPIN = mSession->getPIN(lPID, MODE_DELETED);
	if(!lPIN) TVERIFY(false && "undeleted PIN cannot be retrieved");
	else 
	{
		TVERIFY( (lPIN->getFlags()&PIN_DELETED)==0 ) ;
		lPIN->destroy();
	}

	/*
	// FT Query
	lV[0].set(getPINStr(lPINIndex));
	lAftUndeleteCount = 0;
	IStmt *lQ1 = getQuery(0, mPropIDs[0], lV);	lQ1->count(lAftUndeleteCount);
	TVERIFY(lAftDelCount == lAftUndeleteCount && "Undeleted PIN not returned in FT query");
	lQ1->destroy();
	*/

	if(!MVTApp::isRunningSmokeTest())
	{
		// Full Scan Query on VT_STR
		lV[0].set(getPINStr(lPINIndex));
		lAftUndeleteCount = 0;
		IStmt *lQ2 = getQuery(1, mPropIDs[0], lV);	lQ2->count(lAftUndeleteCount);
		TVERIFY(lAftDelCount == lAftUndeleteCount && "Undeleted PIN not returned in Full Scan query");
		lQ2->destroy();
	}

	// Family Query
	lV[0].set(getPINStr(lPINIndex));
	lAftUndeleteCount = 0;
	IStmt *lQ3 = getQuery(3, mPropIDs[0], lV);	lQ3->count(lAftUndeleteCount);
	TVERIFY(lAftDelCount == lAftUndeleteCount && "Undeleted PIN not returned in Family query");
	lQ3->destroy();

	// Family with PIN Index
	lV[0].set(lPINIndex);
	lAftUndeleteCount = 0;
	IStmt *lQ4 = getQuery(4, mPropIDs[1], lV);	lQ4->count(lAftUndeleteCount);
	TVERIFY(lAftDelCount == lAftUndeleteCount && "Undeleted PIN not returned in Class query");
	lQ4->destroy();
	
	if(!MVTApp::isRunningSmokeTest())
	{
		// Full Scan Query with VT_INT
		lV[0].set(lPINIndex);
		lAftUndeleteCount = 0;
		IStmt *lQ5 = getQuery(5, mPropIDs[1], lV);	lQ5->count(lAftUndeleteCount);
		TVERIFY(lAftDelCount == lAftUndeleteCount && "Undeleted PIN not returned in Full Scan query");
		lQ5->destroy();
	}

	undeletePINs(2, &mPIDs[0], sNumPINs);

	//Create pin, soft delete and then purge.
	{
		Value val[1];Tstring str;PID delPid;IPIN *pin;
		MVTRand::getString(str,10,0);
		val[0].set(str.c_str());val[0].setPropID(mPropIDs[3]);
		TVERIFYRC(mSession->createPIN(val,1,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
		delPid = pin->getPID();
		//softdelete the pin
		TVERIFYRC(mSession->deletePINs(&pin,1));
		pin =  mSession->getPIN(delPid, MODE_DELETED);
		//Purge the deleted pin
		TVERIFYRC(mSession->deletePINs(&pin,1,MODE_PURGE));
		//verify if the pin is really purged
		pin =  mSession->getPIN(delPid, MODE_DELETED);
		if (NULL != pin){
			mLogger.out()<<"Got a purged pin!!!"<<std::endl;
			TVERIFY(false);
		}
	}
}

void TestUndelete::undeletePINs(int pDelType, PID *pPIDs, int pNumPIDs)
{
	int i = 0;
	switch(pDelType)
	{
		case 0: //IPIN::undelete() not ALLOWED on ISession::getPIN()
			/*{
				SourceSpec lCS; lCS.objectID = mCLSID; lCS.nParams = 0; lCS.params = NULL;
				IStmt *lQ = mSession->createStmt();
				unsigned char lVar = lQ->addVariable(&lCS, 1);
				lQ->setPIDs(lVar, pPIDs, pNumPIDs);
				ICursor *lR = lQ->execute(0,0,~0,0,MODE_DELETED);
				if(lR)
				{
					for(IPIN *lPIN = lR->next(); lPIN!=NULL; lPIN=lR->next())
					{ lPIN->undelete(); lPIN->destroy();}
					lR->destroy();
				}
				lQ->destroy();
			}
			break;
			*/
		case 1: // IStmt::undeletePINs();
			for(i = 0; i < pNumPIDs; i++)
			{
				Value lV[1]; lV[0].set(getPINIndex(pPIDs[i]));
				IStmt *lQ = getQuery(4, mPropIDs[1], lV, STMT_UNDELETE);
				TVERIFYRC(lQ->execute());
				lQ->destroy();
			}
			break;
		case 2: // ISession::undeletePINs();
			TVERIFYRC(mSession->undeletePINs(pPIDs, pNumPIDs));
			break;
	}
}

void TestUndelete::deletePINs(int pUndeleteType, PID *pPIDs, int pNumPIDs)
{
	int i = 0;
	switch(pUndeleteType)
	{
		case 0: //IPIN::deletePIN();
			for(i = 0; i < pNumPIDs; i++)
			{
				IPIN *lPIN = mSession->getPIN(pPIDs[i]); assert(lPIN!=NULL);
				TVERIFYRC(lPIN->deletePIN());
				lPIN=NULL; // pin has been deleted
			}
			break;
		case 1: // IStmt::deletePINs();
			for(i = 0; i < pNumPIDs; i++)
			{
				Value lV[1]; lV[0].set(getPINIndex(pPIDs[i]));
				IStmt *lQ = getQuery(4, mPropIDs[1], lV, STMT_DELETE);
				TVERIFYRC(lQ->execute());
				lQ->destroy();
			}
			break;
		case 2: // ISession::deletePINs();

			// Get a snapshot to see what happens to snapshot
			IPIN *lPIN = mSession->getPIN(pPIDs[i]); assert(lPIN!=NULL);

			TVERIFYRC(mSession->deletePINs(pPIDs, pNumPIDs,0));

			TVERIFY((lPIN->getFlags()&PIN_DELETED)==0);		// pin doesn't know that it is deleted
			TVERIFY(RC_DELETED == lPIN->refresh()) ;
			TVERIFY((lPIN->getFlags()&PIN_DELETED)!=0);
			lPIN->destroy() ;

			break;
	}
}

/*
 * Create a Class and Family to query for deleted PINs
 *
 */
void TestUndelete::createMeta()
{
	// Create a Family
	Tstring lRandStr; MVTRand::getString(lRandStr, 10, 0, false, false);
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned const char lVar = lQ->addVariable();
		Value lV[2];
		lV[0].setVarRef(0,mPropIDs[0]);
		lV[1].setParam(0);
		CmvautoPtr<IExprNode> lET(mSession->expr(OP_BEGINS, 2, lV));
		TVERIFYRC(lQ->addCondition(lVar,lET));

		char lB[64]; sprintf(lB, "TestUndelete.%s.%d", lRandStr.c_str(), 0);
		TVERIFYRC(defineClass(mSession,lB, lQ, &mFamilyID));
	}

	// Create a Family on PIN Index aka prop1
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned const char lVar = lQ->addVariable();
		Value lV[2];
		lV[0].setVarRef(0,mPropIDs[1]);
		lV[1].setParam(0);
		CmvautoPtr<IExprNode> lET(mSession->expr(OP_EQ, 2, lV));
		TVERIFYRC(lQ->addCondition(lVar,lET));

		char lB[64]; sprintf(lB, "TestUndelete.%s.%d", lRandStr.c_str(), 1);
		TVERIFYRC(defineClass(mSession,lB, lQ, &mFamilyID1));
	}

	// Create a Family on prop2
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned const char lVar = lQ->addVariable();
		Value lV[2];
		lV[0].setVarRef(0,mPropIDs[2]);
		lV[1].setParam(0);
		CmvautoPtr<IExprNode> lET(mSession->expr(OP_EQ, 2, lV));
		TVERIFYRC(lQ->addCondition(lVar,lET));

		char lB[64]; sprintf(lB, "TestUndelete.%s.%d", lRandStr.c_str(), 2);
		TVERIFYRC(defineClass(mSession,lB, lQ, &mFamilyID2));
	}

	// Create a Class
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned const char lVar = lQ->addVariable();
		Value lV[1];
		lV[0].setVarRef(0,mPropIDs[2]);
		CmvautoPtr<IExprNode> lET(mSession->expr(OP_EXISTS, 1, lV));
		TVERIFYRC(lQ->addCondition(lVar,lET));

		char lB[64]; sprintf(lB, "TestUndelete.%s.%d", lRandStr.c_str(), 3);
		TVERIFYRC(defineClass(mSession,lB, lQ, &mCLSID));
	}
	// Create a family for specific case purge after undelete
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned const char lVar = lQ->addVariable();
		Value lV[2];
		lV[0].setVarRef(0,mPropIDs[3]);
		lV[1].setParam(0);
		CmvautoPtr<IExprNode> lET(mSession->expr(OP_EQ, 2, lV));
		TVERIFYRC(lQ->addCondition(lVar,lET));

		char lB[64]; sprintf(lB, "TestUndelete.%s.%d", lRandStr.c_str(), 4);
		TVERIFYRC(defineClass(mSession,lB, lQ, &mFamilyID3));
	}
}

void TestUndelete::createData()
{
	//Create PINs
	Tstring lPropStr1, lPropStr2;	
	MVTRand::getString(lPropStr2, 10, 10, false, true);
	mQueryStr = lPropStr2;
	mLogger.out() << "Creating " << sNumPINs << " PINs ..." ;
	unsigned sMode = mSession->getInterfaceMode();
	mSession->setInterfaceMode(ITF_REPLICATION|sMode);

	ushort const lStoreID = 0x21F8;
	int lStartIndex = MVTRand::getRange(20, 100);	
	int i, k = 0;		
	for(i = 0; i < sNumPINs; i++)
	{
		PID lPID;
		bool lRepl = MVTRand::getBool() && i > (int)sNumPINs/4;

		if(lRepl)
		{
			while(1)
			{
				LOCALPID(lPID) = (uint64_t(lStoreID) << 48) + (lStartIndex << 16) + k;
				IPIN *lPIN = mSession->getPIN(lPID);
				if(lPIN) { lPIN->destroy(); lStartIndex++; continue;}
				else break;
			}
		}
		int j = 0;
		MVTRand::getString(lPropStr1, 10, 10, false, true);
		Value lV[3];		
		SETVALUE(lV[j], mPropIDs[0], lPropStr1.c_str(), OP_SET); j++;
		SETVALUE(lV[j], mPropIDs[1], i, OP_SET); j++;
		if(MVTRand::getBool())
			{ SETVALUE(lV[j], mPropIDs[2], lPropStr2.c_str(), OP_SET); j++; }

		if(lRepl)
		{
			IPIN *lPIN=NULL;
			RC rc = mSession->createPIN(lV, j, &lPIN, MODE_COPY_VALUES|MODE_PERSISTENT);
			if(rc == RC_OK) lPID = lPIN->getPID();
			else lPID.pid = STORE_INVALID_PID;
			if(lPIN) lPIN->destroy();			
			if(k != 0 && k % 10 == 0) { lStartIndex++; k = 0; }
			k++;
		}	
		else
		{	
			IPIN *pin;
			TVERIFYRC(mSession->createPIN(lV, j, &pin, MODE_PERSISTENT|MODE_COPY_VALUES));
			lPID = pin->getPID();
		}
		if(lPID.pid != STORE_INVALID_PID) { mPIDs.push_back(lPID); mPIDStr.push_back(lPropStr1);}
		if((i % 100) == 0) mLogger.out() << ".";
	}
	mSession->setInterfaceMode(sMode);
	mLogger.out() << std::endl;
}

IStmt * TestUndelete::getQuery(int pQueryType, PropertyID pPropID, Value *pPropValue, STMT_OP sop)
{
	IStmt *lQ = mSession->createStmt(sop);
	unsigned char lVar;
	switch(pQueryType)
	{
		case 0: // FT Query on VT_STRING
			assert(pPropID!=STORE_INVALID_URIID || pPropValue != NULL);
			lVar = lQ->addVariable();
			lQ->setConditionFT(lVar, pPropValue->str, 0, &pPropID, 1);
			break;
		case 1: // Full Scan
			assert(pPropID!=STORE_INVALID_URIID || pPropValue != NULL);
			lVar = lQ->addVariable();
			{
				Value lV[2];
				lV[0].setVarRef(0,pPropID);
				lV[1].set(pPropValue->str);
				IExprNode *lET = mSession->expr(OP_EQ, 2, lV);
				lQ->addCondition(lVar,lET);
				lET->destroy();
			}
			break;
		case 2: // Class Query
			{
				SourceSpec lCS;
				lCS.objectID = mCLSID;
				lCS.nParams = 0;
				lCS.params = NULL;
				lVar = lQ->addVariable(&lCS, 1);
			}
			break;
		case 3: // Family Query
			{
				assert(pPropValue != NULL);
				SourceSpec lCS;
				lCS.objectID = mFamilyID;
				lCS.nParams = 1;
				lCS.params = pPropValue;
				lVar = lQ->addVariable(&lCS, 1);
			}
			break;
		case 4: // Family with PIN Index
			assert(pPropValue != NULL || pPropValue->i >= 0);
			{
				SourceSpec lCS;
				lCS.objectID = mFamilyID1;
				lCS.nParams = 1;
				lCS.params = pPropValue;
				lVar = lQ->addVariable(&lCS, 1);
			}
			break;
		case 5: // Full Scan on PIN Index
			assert(pPropID!=STORE_INVALID_URIID || pPropValue != NULL || pPropValue->i >= 0);
			lVar = lQ->addVariable();
			{
				Value lV[2];
				lV[0].setVarRef(0,pPropID);
				lV[1].set(pPropValue->i);
				IExprNode *lET = mSession->expr(OP_EQ, 2, lV);
				lQ->addCondition(lVar,lET);
				lET->destroy();
			}
			break;
		case 6: // Family with prop2
			assert(pPropValue != NULL || pPropValue->str != NULL);
			{
				SourceSpec lCS;
				lCS.objectID = mFamilyID2;
				lCS.nParams = 1;
				lCS.params = pPropValue;
				lVar = lQ->addVariable(&lCS, 1);
			}
			break;
	}
	return lQ;
}

int TestUndelete::getPINIndex(PID lPID)
{
	assert(lPID.pid != STORE_INVALID_PID);
	IPIN *lPIN = mSession->getPIN(lPID); 
	if(!lPIN)
	{
		int i = 0;
		for( i = 0; i < (int)mPIDs.size(); i++) if(lPID.pid == mPIDs[i].pid && lPID.ident == mPIDs[i].ident) break;
		Value lV[1]; lV[0].set(i);
		IStmt *lQ = getQuery(4, mPropIDs[1], lV);
		ICursor *lR = NULL;
             	TVERIFYRC(lQ->execute(&lR, 0,0,~0,0,MODE_DELETED));
		if(lR) { lPIN = lR->next(); lR->destroy(); } else assert(lPIN!=NULL && "Failed to get PIN");
		lQ->destroy();
	}
	int lRetVal = 0;
	if(lPIN->defined(&mPropIDs[1], 1)) 
	{
		Value const *lV = lPIN->getValue(mPropIDs[1]); 
		lRetVal = lV->i;
		TVERIFY(lV!=NULL && "NULL Value returned"); 
	}
	lPIN->destroy();
	
	return lRetVal;
}

const char *TestUndelete::getPINStr(int pPINIndex)
{
	assert(pPINIndex >= 0 && pPINIndex < (int)mPIDStr.size());
	const char *lRetVal = mPIDStr[pPINIndex].c_str();	
	return lRetVal;
}

int TestUndelete::execute()
{
	bool lSuccess = true;	
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();
		MVTApp::mapURIs(mSession, "TestUndelete.prop.", sNumProps, mPropIDs);
		createMeta();
		createData();
		doTest() ;
		mSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	return lSuccess?0:1;
}
