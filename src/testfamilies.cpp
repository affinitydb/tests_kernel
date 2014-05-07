/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "TestCollNav.h"
using namespace std;

class TestFamilies : public ITest{
	public:
		PropertyID mPropIds[5];
		static const int mNumPINs = 10000;
		static const int mNumClasses = 7;
		PID mPID[mNumPINs];
		ClassID mCLSIDs[mNumClasses];
		int mNumParams[mNumClasses]; // How many parameters the query requires
		int mExpResult[mNumClasses]; // Expected results for each query
		uint64_t mUI64a; 
		uint64_t mUI64b;
		long volatile mFinalResult; // thread will set non-zero if failure
		MVTestsPortability::Mutex mLock; // REVIEW: not really necessary in its current usage
		clock_t mTimeTaken;
		Afy::IAffinity *mStoreCtx;
	public:
		TEST_DECLARE(TestFamilies);
		virtual char const * getName() const { return "testfamilies"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "tests class families (class queries with params)"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }	
	public:
		bool executeFamilies(ISession *pSession,const int pCase);
	protected:
		bool createPINs(ISession *pSession);
		IExprNode * createExpr(ISession *pSession, IStmt &lQ, const int pCase = 0, unsigned char const lVar = 0);
		void getParams(Value *pParam, const int pCase);
		bool quickTest(ISession *pSession);

};
TEST_IMPLEMENT(TestFamilies, TestLogger::kDStdOut);

class CSession
{
	public:
		TestFamilies & mContext;
		const char *mIdentityname;
		ISession * mSession;
		CSession(TestFamilies & pContext, const char *pIdentityName)
			: mContext(pContext), mIdentityname(pIdentityName){}
		void onStart()
		{
			mSession = MVTApp::startSession(mContext.mStoreCtx);
		}		
};

THREAD_SIGNATURE threadFamily(void * pSI){
	// Thread will execute all the family queries in its own session
	CSession * const lSI = (CSession *)pSI;	
	bool lSuccess = true;
	lSI->onStart();
	int i = 0;
	for(i = 0; i < lSI->mContext.mNumClasses && lSuccess; i++)
		if(!lSI->mContext.executeFamilies(lSI->mSession, i)){
			lSuccess = false;
			lSI->mContext.mLock.lock();
			INTERLOCKEDI(&lSI->mContext.mFinalResult);
			lSI->mContext.mLock.unlock();
		}
	lSI->mSession->terminate();
	return 0;
}

int	TestFamilies::execute(){
	bool lSuccess =	true;
	mUI64a = mUI64b = 0;
	if (MVTApp::startStore()){
		ISession * const lSession =	MVTApp::startSession();	
		mStoreCtx = MVTApp::getStoreCtx();

		// Map properties based on random strings
		static int const sNumProps  = sizeof(mPropIds) / sizeof(mPropIds[0]);
		URIMap lData;
		Tstring lPropName;MVTRand::getString(lPropName,10,10,false,false);

		int i;
		for (i = 0; i < sNumProps; i++)
		{
			char lB2[64];
			sprintf(lB2, "TestFamilies.prop%s.%d", lPropName.c_str(), i);
			lData.URI = lB2; lData.uid = /*lBasePropID++;//*/STORE_INVALID_URIID; 
			lSession->mapURIs(1, &lData);
			mPropIds[i] = lData.uid;
		}

		// An initial scenario
		if(!quickTest(lSession)){
			mLogger.out() << "ERROR (execute): Quick test failed " << std::endl;
			lSuccess = false;
		}
		
		// Create PINs with random number of Properties
		if(!createPINs(lSession))
			return RC_FALSE;
		
		// Create Classes with specific Queries and get the Expected result count
		for(i = 0; i < mNumClasses; i++){			
			IStmt * lQ	= lSession->createStmt();
			unsigned char const lVar = lQ->addVariable();
			IExprNode *lE = createExpr(lSession, *lQ, i, lVar); // Create different query depending on "i"
			if(lE){
				
				TVERIFYRC(lQ->addCondition(lVar,lE));
				char lB[64];Tstring lClassName; MVTRand::getString(lClassName,10,10,false,false);
				sprintf(lB, "TestFamilies.Class%s.%d",lClassName.c_str(), i);
				mCLSIDs[i] = STORE_INVALID_CLASSID;
				TVERIFYRC(defineClass(lSession, lB, lQ, &mCLSIDs[i]));
				lE->destroy();				
			}else{
				mLogger.out() << "Failed to create IExprNode " << std::endl;
				lSuccess = false;
			}
			lQ->destroy();
		}
		// Get the Expected Result count using parameterized query
		for (i = 0; i < mNumClasses; i++){
			IStmt * lQ1	= lSession->createStmt();
			unsigned char const lVar = lQ1->addVariable();
			IExprNode *lE1 = createExpr(lSession, *lQ1, i, lVar);
			Value lParam[10];getParams(lParam, i);
			lQ1->addCondition(lVar,lE1);
			uint64_t lCount = 0;
			lQ1->count(lCount,lParam,mNumParams[i]);
			if(lCount == 0)
				mLogger.out() << " WARNING :(execute) Zero result returned for case " << i << ". BUG ?!? " << std::endl;
			mExpResult[i] = (int)lCount;	
			lE1->destroy();
			lQ1->destroy();
		}
		for(i = 0; i < mNumClasses; i++){
			mTimeTaken = 0;
			if(!executeFamilies(lSession,i)) lSuccess = false;
			mLogger.out() << "Time taken to execute family " << i << "\t" << mTimeTaken << " millisecs" << std::endl;
		}

		
		// Execute Class Queries asynchronously in multiple threads
#if 1
		mLogger.out() << "Executing multi-threaded family queries" << endl ;

		mFinalResult = 0;
		const int sNumThreads = 2;
		{
			int i = 0;
			HTHREAD lThreads[sNumThreads];
			for (i = 0; i < sNumThreads; i++)
				createThread(&threadFamily, new CSession(*this,NULL), lThreads[i]);
			MVTestsPortability::threadsWaitFor(sNumThreads, lThreads);
		}
		if(mFinalResult != 0) lSuccess = false;
#endif
		
		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Failed to open store"); }
	return lSuccess	? 0	: 1;
}

bool TestFamilies::createPINs(ISession *pSession){
	bool lSuccess = true;
	IPIN * lPIN = NULL;
	static int const sNumProps  = sizeof(mPropIds) / sizeof(mPropIds[0]);
	static int const sMaxVals = 10;
	Value lPVs[sMaxVals * sNumProps];
	string lStr[6]= {"Asia", "Africa", "Europe", "America", "Australia", "Antartica"};
	int i, j, iV;
	mLogger.out() << "Creating lots of pins..." << std::endl;
	for (i = 0; i < mNumPINs; i++)
	{
		if (0 == i % 100)
			mLogger.out() << ".";

		// Remember time at which 1/3rd of pins were created
		if(i == mNumPINs/3) {TIMESTAMP dt; getTimestamp(dt); mUI64a = dt;}

		int lNumProps = (int)((float)(sNumProps + 2) * rand() / RAND_MAX); // 1 or more properties.
		lNumProps = lNumProps == 0?1:lNumProps;
		RefVID lRef;
		for (j = 0, iV = 0; j < lNumProps; j++)
		{
			switch(j){
				case 0:// VT_STRING
					{
						int lIndex = (int)((float)5 * rand() / RAND_MAX); 
						lIndex = lIndex == 0?1:lIndex;
						SETVALUE(lPVs[iV], mPropIds[j], lStr[lIndex].c_str(), OP_SET);iV++;						
					}
					break;
				case 1: // VT_INT
					{
						int lRand = (int)((float)mNumPINs * rand() / RAND_MAX); 
						SETVALUE(lPVs[iV], mPropIds[j], lRand, OP_SET);iV++;
					}
					break;
				case 2: // VT_DOUBLE
					{
						int lRand = (int)((float)mNumPINs * rand() / RAND_MAX); 
						SETVALUE(lPVs[iV], mPropIds[j], (double)lRand, OP_SET);iV++;
					}
					break;
				case 3: // VT_REF
					{
						bool lAdd = (int)((float)mNumPINs * rand()/RAND_MAX) > mNumPINs/2;
						if(i > 0 && lAdd){
							// Set reference to the previously created PIN
							lPVs[iV].set(mPID[i-1]);lPVs[iV].setPropID(mPropIds[j]);iV++;
						}
					}					
					break;
				case 4: // VT_REFIDPROP
					{
						bool lAdd = (int)((float)mNumPINs * rand()/RAND_MAX) > mNumPINs/2;
						if(i > 0 && lAdd){
							// Set reference to the VT_STRING property of the previously created PIN
							lRef.id = mPID[i-1]; lRef.pid=mPropIds[0]; lRef.eid=STORE_COLLECTION_ID; lRef.vid=STORE_CURRENT_VERSION;
							lPVs[iV].set(lRef);lPVs[iV].setPropID(mPropIds[j]);iV++;
						}
					}
					break;
				case 5: // PROP_SPEC_UPDATED
					{
						lPVs[iV].set(0);lPVs[iV].setPropID(PROP_SPEC_UPDATED);iV++;						
					}
					break;	
			}
		}	
		if(RC_OK != pSession->createPIN(lPVs,iV,&lPIN,MODE_PERSISTENT|MODE_COPY_VALUES)){
			mLogger.out() << " Failed to commit the pin " << std::endl;
			lSuccess = false;
		}else{
			mPID[i] = lPIN->getPID();
			/*MVTApp::output(*lPIN,mLogger.out(),pSession);
			mLogger.out() << "  ------------------------------ " << std::endl;*/
			lPIN->destroy();
		}
	}
	mLogger.out() << std::endl;
	return lSuccess;
}

IExprNode * TestFamilies::createExpr(ISession *pSession, IStmt &lQ, const int pCase, unsigned char const lVar){
	// Note: This test outputs many "WARNING: Full scan query!!!" warnings
	// even though the queries are sometimes assigned to classes.  This is because
	// of the presence of OP_IN and other non-indexable operations
	
	IExprNode *lE = NULL;
	switch(pCase){
		case 0:
			{
				// Query (on int prop): prop2 > param0   AND  ( prop2 % param1 ) == param2

				Value val[2];			
				val[0].setVarRef(0,mPropIds[2]);
				val[1].setParam(0);
				TExprTreePtr lE1 = EXPRTREEGEN(pSession)(OP_GT, 2, val, 0);

				val[0].setVarRef(0,mPropIds[2]);
				val[1].setParam(1);
				TExprTreePtr lE2 = EXPRTREEGEN(pSession)(OP_MOD, 2, val, 0);
				
				val[0].set(lE2);
				val[1].setParam(2);
				TExprTreePtr lE3 = EXPRTREEGEN(pSession)(OP_EQ, 2, val, 0);

				val[0].set(lE1);
				val[1].set(lE3);
				lE = EXPRTREEGEN(pSession)(OP_LAND, 2, val, 0);
				mNumParams[pCase] = 3;
			}
			break;
		case 1:
			{			
				// Query:   (prop0=param0 OR prop2>param1) OR (prop1=param2 OR prop3<param3)
				Value val[2];			
				val[0].setVarRef(0,mPropIds[0]);
				val[1].setParam(0);
				TExprTreePtr lE1 = EXPRTREEGEN(pSession)(OP_EQ, 2, val, 0);

				val[0].setVarRef(0,mPropIds[2]);
				val[1].setParam(1);
				TExprTreePtr lE2 = EXPRTREEGEN(pSession)(OP_GT, 2, val, 0);
				
				val[0].set(lE1);
				val[1].set(lE2);
				TExprTreePtr lE3 = EXPRTREEGEN(pSession)(OP_LOR, 2, val, 0);

				val[0].setVarRef(0,mPropIds[1]);
				val[1].setParam(2);
				TExprTreePtr lE4 = EXPRTREEGEN(pSession)(OP_EQ, 2, val, 0);

				val[0].setVarRef(0,mPropIds[3]);
				val[1].setParam(3);
				TExprTreePtr lE5 = EXPRTREEGEN(pSession)(OP_LT, 2, val, 0);

				val[0].set(lE4);
				val[1].set(lE5);
				TExprTreePtr lE6 = EXPRTREEGEN(pSession)(OP_LOR, 2, val, 0);

				val[0].set(lE3);
				val[1].set(lE6);
				lE = EXPRTREEGEN(pSession)(OP_LOR, 2, val, 0);

				mNumParams[pCase] = 4;				
			}
			break;
		case 2:
			{	
				// Query ( prop0 in param0 OR prop1 in param1 ) OR prop2 in param2
				// (where param0 and param1 will be sets of possible string values and
				// param2 will be a numeric range)

				Value val[2];
				val[0].setVarRef(0,mPropIds[0]);
				val[1].setParam(0);				
				TExprTreePtr lE1 = EXPRTREEGEN(pSession)(OP_IN, 2, val, 0);

				val[0].setVarRef(0,mPropIds[1]);
				val[1].setParam(1);
				TExprTreePtr lE2 = EXPRTREEGEN(pSession)(OP_IN, 2, val, 0);
			
				val[0].set(lE1);
				val[1].set(lE2);
				TExprTreePtr lE3 = EXPRTREEGEN(pSession)(OP_LOR, 2, val, 0);
				
				val[0].setVarRef(0,mPropIds[2]);
				val[1].setParam(2);
				TExprTreePtr lE4 = EXPRTREEGEN(pSession)(OP_IN, 2, val, 0);
				
				val[0].set(lE3);
				val[1].set(lE4);
				lE = EXPRTREEGEN(pSession)(OP_LOR, 2, val, 0);
				mNumParams[pCase] = 3;						
			}				
			break;
		case 3:
			{
				// Query: Prop3 exists AND prop0=param0

				Value val[2];
				val[0].setVarRef(0,mPropIds[3]);
				TExprTreePtr lE1 = EXPRTREEGEN(pSession)(OP_EXISTS, 1, val, 0);
				
				val[0].setVarRef(0,mPropIds[0]);
				val[1].setParam(0);
				TExprTreePtr lE2 = EXPRTREEGEN(pSession)(OP_EQ, 2, val, 0);

				val[0].set(lE1);
				val[1].set(lE2);
				lE = EXPRTREEGEN(pSession)(OP_LAND, 2, val, 0);

				mNumParams[pCase] = 1;				
			}
			break;
		case 4:
			{
				//QUERY: ( prop2 >= param0 OR prop2 != param1 ) OR 				
				//       ( prop3 < param0 OR prop3 != param1 )
				// Note that the same parameter is used in multiple expression nodes
				Value val[2];			
				val[0].setVarRef(0,mPropIds[2]);
				val[1].setParam(0);
				TExprTreePtr lE1 = EXPRTREEGEN(pSession)(OP_GE, 2, val, 0);

				val[0].setVarRef(0,mPropIds[2]);
				val[1].setParam(1);
				TExprTreePtr lE2 = EXPRTREEGEN(pSession)(OP_NE, 2, val, 0);
				
				val[0].set(lE1);
				val[1].set(lE2);
				TExprTreePtr lE3  = EXPRTREEGEN(pSession)(OP_LOR, 2, val, 0);

				val[0].setVarRef(0,mPropIds[3]);
				val[1].setParam(0);
				TExprTreePtr lE4 = EXPRTREEGEN(pSession)(OP_LE, 2, val, 0);

				val[0].setVarRef(0,mPropIds[3]);
				val[1].setParam(1);
				TExprTreePtr lE5 = EXPRTREEGEN(pSession)(OP_NE, 2, val, 0);
				
				val[0].set(lE4);
				val[1].set(lE5);
				TExprTreePtr lE6  = EXPRTREEGEN(pSession)(OP_LOR, 2, val, 0);

				val[0].set(lE3);
				val[1].set(lE6);
				lE  = EXPRTREEGEN(pSession)(OP_LOR, 2, val, 0);
				mNumParams[pCase] = 4; // Review: param 2 and 3 are not actually 
									   // referenced
			}
			break;
		case 5:
			{
				// Query: ( param0 exists AND param1 > param2 ) OR
				//		  ( param3 in param4 AND prop1 contains param5 )				
				//
				// This shows how even the property to be considered can be stored
				// in a parameter to be resolved later.

				Value val[2];			
				val[0].setParam(0);
				TExprTreePtr lE1 = EXPRTREEGEN(pSession)(OP_EXISTS, 1, val, 0);

				val[0].setParam(1);
				val[1].setParam(2);
				TExprTreePtr lE2 = EXPRTREEGEN(pSession)(OP_GT, 2, val, 0);
				
				val[0].set(lE1);
				val[1].set(lE2);
				TExprTreePtr lE3  = EXPRTREEGEN(pSession)(OP_LAND, 2, val, 0);

				val[0].setParam(3);
				val[1].setParam(4);
				TExprTreePtr lE4 = EXPRTREEGEN(pSession)(OP_IN, 2, val, 0);

				val[0].setVarRef(0,mPropIds[1]);
				val[1].setParam(5);
				TExprTreePtr lE5 = EXPRTREEGEN(pSession)(OP_CONTAINS, 2, val, 0);
				
				val[0].set(lE4);
				val[1].set(lE5);
				TExprTreePtr lE6  = EXPRTREEGEN(pSession)(OP_LAND, 2, val, 0);

				val[0].set(lE3);
				val[1].set(lE6);
				lE  = EXPRTREEGEN(pSession)(OP_LOR, 2, val, 0);
				mNumParams[pCase] = 6;
			}break;
		case 6:
			{
				// Query: PROP_SPEC_UPDATED in (time range) of param0
				Value val[2];				
				PropertyID lPropID[1] = {PROP_SPEC_UPDATED};
				val[0].setVarRef(0,*lPropID);
				val[1].setParam(0,VT_DATETIME);
				lE  = EXPRTREEGEN(pSession)(OP_IN, 2, val, 0);
				mNumParams[pCase] = 1;
			}break;	
	}
	return lE;
}
bool TestFamilies::executeFamilies(ISession *pSession, const int pCase){
	bool lSuccess = true;
	long lBef, lAft;
	Value lParam[10];
	SourceSpec lCS;
	IStmt * lQ = pSession->createStmt();
	getParams(lParam, pCase);
	lCS.objectID = mCLSIDs[pCase];
	lCS.nParams = mNumParams[pCase];
	lCS.params = lParam;
	lQ->addVariable(&lCS, 1);
	uint64_t lCount = 0;
	lBef = getTimeInMs();
	lQ->count(lCount);
	lAft = getTimeInMs();
	mTimeTaken = lAft-lBef;
	if(lCount == 0)
	{
		// Because the input data is generated randomly, there is some small potential
		// that a query will fail without there being any bug
		mLogger.out() << " WARNING :(executeFamilies) Zero result returned for case " << pCase << ". BUG ?!? " << std::endl;
	}
	lQ->destroy();
	TVERIFY((int)lCount == mExpResult[pCase]);
	return lSuccess;	
}

void TestFamilies::getParams(Value *pParam, const int pCase){
	switch(pCase){
		case 0:
			// Query (on int prop): prop2 > param0   AND  ( prop2 % param1 ) == param2
			pParam[0].set(5);
			pParam[1].set(2);
			pParam[2].set(0);
			break;
		case 1:	
			// Query:   (prop0=param0 OR prop2>param1) OR (prop1=param2 OR prop3<param3)
			pParam[0].set("Asia");
			pParam[1].set(0);
			pParam[2].set("Grasshopper");
			pParam[3].set((double)300);			
			break;
		case 2:
			{
				// Query ( prop0 in param0 OR prop1 in param1 ) OR prop2 in param2

				static Value lV0[4];
				lV0[0].set("Antartica");			
				lV0[1].set("No mans land");
				lV0[2].set("America");
				lV0[3].set("Australia");
				pParam[0].set(lV0,4);
				
				static Value lV1[4];
				lV1[0].set("Warhawk");
				lV1[1].set("Bolo");
				lV1[2].set("Vigilant");
				lV1[3].set("Sandpiper");
				INav *lCollNav = new CTestCollNav(lV1,4);
				pParam[1].set(lCollNav);				

				static Value lV2[2];
				lV2[0].set(80);			
				lV2[1].set(150);				
				pParam[2].setRange(lV2);
			}
			break;
		case 3:
			// Query: Prop3 exists AND prop0=param0
			pParam[0].set("Europe");
			break;
		case 4:
			//QUERY: ( prop2 >= param0 OR prop2 != param1 ) OR 				
			//       ( prop3 < param0 OR prop3 != param1 )

			//In this case:
			// ( prop2 >= 15 OR prop2 != 16 ) OR 				
			// ( prop3 < 15 OR prop3 != 16 )

			pParam[0].set(15);
			pParam[1].set(16);
			pParam[2].set((double)10.0); // Ignored
			pParam[3].set((double)9.0); // Ignored
			break;
		case 5:
			{
			// Query: ( param0 exists AND param1 > param2 ) OR
			//		  ( param3 in param4 AND prop1 contains param5 )				
			
			// In this case:
			// ( prop4 exists AND prop2 > 5 ) OR ( prop3 in range 5-60 AND prop1 contains "a" )

			pParam[0].setVarRef(0,mPropIds[4]);
			pParam[1].setVarRef(0,mPropIds[2]);
			pParam[2].set(5);
			pParam[3].setVarRef(0,mPropIds[3]);
			static Value lV[2];
			lV[0].set((double)5);			
			lV[1].set((double)60);	
			pParam[4].setRange(lV);
			pParam[5].set("a");
			}break;	
		case 6:
			{
				// Query: PROP_SPEC_UPDATED in (time range) of param0

				// In this case query for all pins created/updated since mUI64a time 
				// and now
				TIMESTAMP dt; getTimestamp(dt); mUI64b = dt;
				static Value lV[2];
				lV[0].setDateTime(mUI64a);			
				lV[1].setDateTime(mUI64b);	
				pParam[0].setRange(lV);
			}break;	
	}	
}

bool TestFamilies::quickTest(ISession *pSession){

	// This test is independent of the rest of the test

	RC rc ;
	bool lSuccess = true;
	Value lV[3];
	SETVALUE(lV[0],mPropIds[0],"quickTest",OP_SET);
	SETVALUE(lV[1],mPropIds[1],"quickTest",OP_SET);
	SETVALUE(lV[2],mPropIds[2],1000,OP_SET);
	if(RC_OK!=pSession->createPIN(lV,3,NULL,MODE_PERSISTENT|MODE_COPY_VALUES)){
		mLogger.out() << "ERROR(quickTest): Failed to create PIN" << std::endl;
		return false;
	}

	// Query "(prop0 exists AND prop1=param0) OR prop2=param1"
	IStmt *lQ = pSession->createStmt();
	unsigned const char lVar = lQ->addVariable();
	IExprNode *lET;
	{
		Value val[2];
		val[0].setVarRef(0,mPropIds[0]);
		TExprTreePtr lE1 = EXPRTREEGEN(pSession)(OP_EXISTS, 1, val, 0);
		
		val[0].setVarRef(0,mPropIds[1]);
		val[1].setParam(0);
		TExprTreePtr lE2 = EXPRTREEGEN(pSession)(OP_EQ, 2, val, 0);

		val[0].set(lE1);
		val[1].set(lE2);
		TExprTreePtr lE3 = EXPRTREEGEN(pSession)(OP_LAND, 2, val, 0);

		val[0].setVarRef(0,mPropIds[2]);
		val[1].setParam(1);
		TExprTreePtr lE4 = EXPRTREEGEN(pSession)(OP_EQ, 2, val, 0);

		val[0].set(lE3);
		val[1].set(lE4);
		lET = EXPRTREEGEN(pSession)(OP_LOR, 2, val, 0);
	}
	lQ->addCondition(lVar,lET);

	// Register a class (with random name) based on the query
	char lB[64];
	ClassID lCLSID = STORE_INVALID_CLASSID;
	Tstring lClassName; MVTRand::getString(lClassName,10,10,false,false);
	sprintf(lB, "TestFamilies.quickTest%s.%d", lClassName.c_str(), 1000);
	defineClass(pSession, lB, lQ, &lCLSID);

	// Fill in candiate values for the query's parameters
	Value lParam[2];
	{
		lParam[0].set("quickTest");
		lParam[1].set(1000);
	}

	// Test the query directly 
	uint64_t lQCount = 0;
	lQ->count(lQCount,lParam,2);
	lET->destroy();

	// Test the query as a class

	uint64_t lCCount = 0;
	{
		SourceSpec lCS;
		IStmt * lCQ = pSession->createStmt();
		lCS.objectID = lCLSID;
		lCS.nParams = 2;
		lCS.params = lParam;
		lCQ->addVariable(&lCS, 1);
		lCQ->count(lCCount);
		lCQ->destroy();
	}

	if(lQCount == 0 || lCCount == 0 || lQCount != lCCount) lSuccess = false;

	if ( lSuccess )
	{
		// Further verification
		lParam[0].set("somethingelse");
		lParam[1].set(1000); // Should match pin because of the OR

		uint64_t cnt ;
		rc = lQ->count(cnt,lParam,2);
		if ( cnt == 0 ) 
		{
			lSuccess = false ;
		}
	}

	if ( lSuccess )
	{
		// Further verification
		lParam[0].set("somethingelse");
		lParam[1].set(999); 

		uint64_t cnt ;
		rc = lQ->count(cnt,lParam,2);
		if ( cnt != 0 ) 
		{
			lSuccess = false ;
		}
	}

	lQ->destroy();
	return lSuccess;
}
