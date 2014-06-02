/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

class TestPINIn : public ITest{
	public:
		static const int mNumProps = 6;
		PropertyID mPropIds[mNumProps];
		static const int mNumPINs = 200;
		PID mPIDs[mNumPINs];
	public:		
		TEST_DECLARE(TestPINIn);
		virtual char const * getName() const { return "testpinin"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "tests IStmt::setPIDs() and @pin in [...,...,...]"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }		
	protected:
		void createBasePINs(ISession *pSession);
		void createClass(ISession *pSession, DataEventID &pCLSID, const int pCase = 0);
		
};
TEST_IMPLEMENT(TestPINIn, TestLogger::kDStdOut);

int	TestPINIn::execute(){
	if (!MVTApp::startStore()){
		return RC_FALSE;
	}

	ISession * const lSession =	MVTApp::startSession();		
	createBasePINs(lSession);		


	//Case #1: With a Condition
	{
		IStmt *lQ = lSession->createStmt();
		unsigned char lVar = lQ->addVariable();

		// Inject some portion of the pins generated
		const int lNumPINs = MVTRand::getRange(1,mNumPINs);
		TVERIFYRC(lQ->setPIDs(lVar,mPIDs,lNumPINs));

		TExprTreePtr lE;
		{
			// prop0 contains "a"  OR  prop1 > "a"  AND has prop5

			Value lVal[2];
			lVal[0].setVarRef(0,mPropIds[0]);
			lVal[1].set("a");
			TExprTreePtr lE1 = EXPRTREEGEN(lSession)(OP_CONTAINS, 2, lVal, 0);
			
			lVal[0].setVarRef(0,mPropIds[1]);
			lVal[1].set("a");
			TExprTreePtr lE2 = EXPRTREEGEN(lSession)(OP_GT, 2, lVal, 0);

			lVal[0].set(lE1);
			lVal[1].set(lE2);
			TExprTreePtr lE3 = EXPRTREEGEN(lSession)(OP_LOR, 2, lVal, 0);

			lVal[0].setVarRef(0,mPropIds[5]);
			TExprTreePtr lE4 = EXPRTREEGEN(lSession)(OP_EXISTS, 1, lVal, 0);

			lVal[0].set(lE3);
			lVal[1].set(lE4);
			lE = EXPRTREEGEN(lSession)(OP_LAND, 2, lVal, 0);				
		}
		TVERIFYRC(lQ->addCondition(lVar,lE));		

		uint64_t lCount = 0;
		TVERIFYRC(lQ->count(lCount));
		TVERIFY(lCount <= (unsigned long)lNumPINs);

		lE->destroy();
		lQ->destroy();
	}


	//Case #2: With a Class
	{
		DataEventID lCLSID; createClass(lSession,lCLSID);
		SourceSpec lCS;
		lCS.objectID = lCLSID;
		lCS.nParams = 0;
		lCS.params = NULL;
		IStmt *lQ = lSession->createStmt();	
		unsigned char lVar = lQ->addVariable(&lCS, 1);
		const int lNumPINs = MVTRand::getRange(1,mNumPINs);
		TVERIFYRC(lQ->setPIDs(lVar,mPIDs,lNumPINs));

		uint64_t lCount = 0;
		TVERIFYRC(lQ->count(lCount));
		TVERIFY(lCount <= (unsigned long)lNumPINs);

		lQ->destroy();			
	}


	//Case #3: With 2 Classes
	{
		DataEventID lCLSID; createClass(lSession,lCLSID);
		SourceSpec lCS[2];
		lCS[0].objectID = lCLSID;
		lCS[0].nParams = 0;
		lCS[0].params = NULL;

		createClass(lSession,lCLSID,1);
		lCS[1].objectID = lCLSID;
		lCS[1].nParams = 0;
		lCS[1].params = NULL;

		IStmt *lQ = lSession->createStmt();	
		unsigned char lVar = lQ->addVariable(lCS, 2);
		const int lNumPINs = MVTRand::getRange(1,mNumPINs);
		TVERIFYRC(lQ->setPIDs(lVar,mPIDs,lNumPINs));

		uint64_t lCount = 0;
		TVERIFYRC(lQ->count(lCount));
		TVERIFY(lCount <= (unsigned long)lNumPINs);

		lQ->destroy();			
	}

	lSession->terminate();
	MVTApp::stopStore();
	return RC_OK;
}
void TestPINIn::createBasePINs(ISession *pSession){
	
	URIMap lData;
	int i;
	Tstring lStr; MVTRand::getString(lStr,10,10,false,false);
	for (i = 0; i < mNumProps; i++)
	{
		char lB2[64];
		sprintf(lB2, "TestPINIn.%s.prop%d", lStr.c_str(), i);
		lData.URI = lB2; lData.uid = STORE_INVALID_URIID; pSession->mapURIs(1, &lData);
		mPropIds[i] = lData.uid;
	}

	for (i = 0; i < mNumPINs; i++)
	{
		Value lVal[6];
		Tstring str,str2,wstr;
		MVTRand::getString(str, 50, 0, false);
		SETVALUE(lVal[0], mPropIds[0], str.c_str(), OP_SET);
		MVTRand::getString(str2, 50, 0, false);
		SETVALUE(lVal[1],  mPropIds[1], str2.c_str(), OP_SET);
		wstr=MVTRand::getString2(50, 0, false);
		SETVALUE(lVal[2],  mPropIds[2], wstr.c_str(), OP_SET);
		double lDouble = (double) 1000.0 * rand()/RAND_MAX;
		SETVALUE(lVal[3],  mPropIds[3], lDouble > 0? lDouble: (double)i, OP_SET);
		int lNum = (int) 1000.0 * rand()/RAND_MAX;
		SETVALUE(lVal[4],  mPropIds[4], lNum > 0? lNum: i, OP_SET);

		// Approx half the pins will have property5, which either joins or
		// kicks them out of the query
		if(MVTRand::getBool()){
			lVal[5].set("https://wiki.vmware.com");lVal[5].setPropID(mPropIds[5]);
			CREATEPIN(pSession, &mPIDs[i], lVal, 6);
		}else{
			CREATEPIN(pSession, &mPIDs[i], lVal, 5);		
		}		
	}
	
}

void TestPINIn::createClass(ISession *pSession, DataEventID &pCLSID, const int pCase){
	IStmt * lQ	= pSession->createStmt();
	unsigned char const lVar = lQ->addVariable();
	IExprNode *lE;
	switch(pCase){
		case 0:
			{
				// ( prop2<"z"  OR 100<prop4<800 ) AND has prop5

				Value lVal[2];
				lVal[0].setVarRef(0,mPropIds[2]);
				lVal[1].set("z");
				TExprTreePtr lE1 = EXPRTREEGEN(pSession)(OP_LT, 2, lVal, 0);
				
				lVal[0].setVarRef(0,mPropIds[4]);
				static Value lV[2];
				lV[0].set(100);			
				lV[1].set(800);	
				lVal[1].setRange(lV);
				TExprTreePtr lE2 = EXPRTREEGEN(pSession)(OP_IN, 2, lVal, 0);

				lVal[0].set(lE1);
				lVal[1].set(lE2);
				TExprTreePtr lE3 = EXPRTREEGEN(pSession)(OP_LOR, 2, lVal, 0);

				lVal[0].setVarRef(0,mPropIds[5]);
				TExprTreePtr lE4 = EXPRTREEGEN(pSession)(OP_EXISTS, 1, lVal, 0);

				lVal[0].set(lE3);
				lVal[1].set(lE4);
				lE = EXPRTREEGEN(pSession)(OP_LAND, 2, lVal, 0);				
			}break;
		case 1:
			{
				// See bug 11627, this query is so big that it is put on an SSV page
				// ( prop2 contains "a" AND prop3 mod 2 = 0 ) AND NOT exists prop5
				Value lVal[2];
				lVal[0].setVarRef(0,mPropIds[2]);
				lVal[1].set("a");
				TExprTreePtr lE1 = EXPRTREEGEN(pSession)(OP_CONTAINS, 2, lVal, 0);
				
				lVal[0].setVarRef(0,mPropIds[3]);
				lVal[1].set((double)2);
				TExprTreePtr lE2 = EXPRTREEGEN(pSession)(OP_MOD, 2, lVal, 0);

				lVal[0].set(lE2);
				lVal[1].set(0);
				TExprTreePtr lE3 = EXPRTREEGEN(pSession)(OP_EQ, 2, lVal, 0);

				lVal[0].set(lE1);
				lVal[1].set(lE3);
				TExprTreePtr lE4 = EXPRTREEGEN(pSession)(OP_LAND, 2, lVal, 0);

				lVal[0].setVarRef(0,mPropIds[5]);
				TExprTreePtr lE5 = EXPRTREEGEN(pSession)(OP_EXISTS, 1, lVal, 0);

				lVal[0].set(lE5);
				TExprTreePtr lE6 = EXPRTREEGEN(pSession)(OP_LNOT, 1, lVal, 0);

				lVal[0].set(lE4);
				lVal[1].set(lE6);
				lE = EXPRTREEGEN(pSession)(OP_LAND, 2, lVal, 0);				
			}break;
		default:
			mLogger.out() << "ERROR (createClass): Invalid case " << std::endl;
			lQ->destroy();
			return;
	}
	lQ->addCondition(lVar,lE);

	pCLSID = MVTUtil::createUniqueClass(pSession, "TestPINIn", lQ);

	lE->destroy();
	lQ->destroy();
}
