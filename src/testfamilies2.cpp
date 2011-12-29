/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "TestCollNav.h"
#include "teststream.h"
using namespace std;

//#define VERBOSE

#define TEST_IMPORT_PERF 0
#define REPRO_BUG_3292 0
#define USE_LESS_STRINGS 0
#define USE_MD5 0

class TestFamilies2 : public ITest{
	public:
		static const int mNumProps = 6;
		PropertyID mPropIds[mNumProps];
		int mNumPINsImportTestStr[mNumProps];
		int mNumPINsImportTestWStr[mNumProps];		
		std::vector<Tstring> mStr;
		int mNumPINsWithStr;
		std::vector<Tstring> mWStr;
		int mNumPINsWithUStr;
		std::vector<PID> mPIDs;
		int mRunTimes;

		// testHistogramFamily() specific
		int mNumPINsWithINT;
		int mStartHistogram;
		int mEndHistogram;
		int mBoundaryHistogram;
		MVStoreKernel::StoreCtx *mStoreCtx;

		// For performance test of import
#if TEST_IMPORT_PERF
		static const int mImageNumProps = 21;
		static const int mImageNumPINs = 20000;
		std::vector<PID> mImagePIDs;
		PropertyID mImagePropIds[mImageNumProps];

		#if USE_LESS_STRINGS
			static const int mImageNumStr = 1000;
		#else
			static const int mImageNumStr = mImageNumPINs;
		#endif

		std::vector<uint64_t> mImageMd5;
		std::vector<Tstring> mImageStr;
		clock_t mTimeTaken;
#endif

	public:
		TEST_DECLARE(TestFamilies2);
		virtual char const * getName() const { return "testfamilies2"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "second test for class families"; }
		//virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Until all other tests use mapURIs()..."; return false; }
		
		virtual int execute();
		virtual void destroy() { delete this; }	

		// Taking 7 minutes or so in debug
		//virtual bool isLongRunningTest()const {return true;}

public:
		bool createPINs(ISession *pSession, const int pNumPINs = 10000);
		bool modifyPINs(ISession *pSession, int pNumPINs = 10);
		bool deletePINs(ISession *pSession, int pNumPINs = 100);
		bool testImportFamily1(ISession *pSession);// For VT_STR
		bool testImportFamily3(ISession *pSession);// For OP_IN with VT_RANGE
		bool testHistogramFamily(ISession *pSession, bool pBoundaryIncluded = false);
		bool testHistogramDateFamily(ISession *pSession);
		bool testHistogramDateFamily2(ISession *pSession);
		bool testFamilyWithMultiConditions(ISession *pSession,bool pUseClass = false, bool pUseStaticValue = true, bool pCreatePINsBeforefamily = false);
		bool testAsyncPINCreationWithFamily(ISession *pSession);
		bool testClassInClassFamily(ISession *pSession, bool pUseClass = false);
		bool testFamilyWithOrderBy(ISession *pSession,int pOrderBy = 2);
		bool testComplexFamily(ISession *pSession, bool pMakeCollection = true, bool pMore = false);
		bool testArrayFamily(ISession *pSession); 
		static THREAD_SIGNATURE createPINsAsync(void *);
		static THREAD_SIGNATURE getStreamAync(void *);
		static THREAD_SIGNATURE RunFamilyAync(void *);		
		// For Testing Image Import performance
		#if TEST_IMPORT_PERF
			bool createImagePINs(ISession *pSession, const int pNumPINs = 50000, uint64_t pTime = 0);
			bool testImageImportPerf(ISession *pSession);
			bool testCountPerfOnFamily(ISession *pSession,bool pCollWithCommit = false);
			uint64_t axtoi(unsigned char hexStg[16]); 
		#endif
	private:
		void PrintQuery( ISession* S, char * inClassName, IStmt * inQ ) ;
		void AddRandomTagCollection( ISession* pSession, const std::vector<PID> & pids, PropertyID prop ) ;
		void AddKnownTag( ISession* pSession, const std::vector<PID> & pids, PropertyID prop, const char * inTag ) ;

};
TEST_IMPLEMENT(TestFamilies2, TestLogger::kDStdOut);

struct CreatePINThreadInfo
{
	int mNumPINsToCreate;
	Tstring mImageStr;
	Tstring mRandStr;
	Tstring mImageSubStr;
	Tstring mRandSubStr;
	PropertyID *mPropIds;
	int mNumProps;
	MVTestsPortability::Mutex * mLock;
	MVStoreKernel::StoreCtx *mStoreCtx;
	ITest* mTest ;
};
struct GetStreamThreadInfo
{
	std::vector<PID> mPIDs;
	PropertyID mPropID;
	MVStoreKernel::StoreCtx *mStoreCtx;
};

struct RunFamilyThreadInfo
{
	uint64_t mStartDate;
	uint64_t mEndDate;
	ClassID mFamilyID;
	int mExpNumPINs;
	PropertyID mPropID;
	MVStoreKernel::StoreCtx *mStoreCtx;
};
THREAD_SIGNATURE TestFamilies2::createPINsAsync(void * pInfo)
{
	CreatePINThreadInfo *lInfo = (CreatePINThreadInfo *)pInfo;
	ISession *lSession = MVTApp::startSession(lInfo->mStoreCtx);
	int i = 0;
	for(i = 0; i < lInfo->mNumPINsToCreate; i++)
	{		
		TIMESTAMP ui64 = MVTRand::getDateTime(lSession);
		bool lRand = ( rand() > RAND_MAX/2 ) ; // 50% chance of being true
		Value lV[3];		
		SETVALUE(lV[0],lInfo->mPropIds[0], lInfo->mImageStr.c_str(), OP_SET);
		SETVALUE(lV[1],lInfo->mPropIds[1], lRand?lInfo->mRandSubStr.c_str():lInfo->mRandStr.c_str(),OP_SET);
		lV[2].setDateTime(ui64); lV[2].setPropID(lInfo->mPropIds[2]);
		IPIN *lPIN = lSession->createUncommittedPIN(lV,3,MODE_COPY_VALUES);
		TV_R(lPIN!=NULL,lInfo->mTest);
		RC rc;
		lInfo->mLock->lock();
		if(RC_OK!=(rc=lSession->commitPINs(&lPIN,1))) 
		{
			TV_R(!"ERROR(createPINsAsync): CommitPINs failed", lInfo->mTest ) ;
			lInfo->mTest->getLogger().out() << "Pin " << i << "RC " << rc << " value0: " << lV[0].str << " value1: " << lV[1].str << " value2: " << lV[2].ui64 << endl ;
		}
		else
			lPIN->destroy();	
		lInfo->mLock->unlock();
	}	
	return 0;
}
THREAD_SIGNATURE TestFamilies2::getStreamAync(void * pInfo)
{
	GetStreamThreadInfo *lInfo = (GetStreamThreadInfo *) pInfo;
	ISession *lSession = MVTApp::startSession(lInfo->mStoreCtx);
	size_t i = 0;
	for(i = 0; i < lInfo->mPIDs.size(); i++)
	{		
		PID lPID = lInfo->mPIDs[i];
		IPIN *lPIN = lSession->getPIN(lPID);
		if(lPIN)
		{
			lPIN->refresh();
			lPIN->destroy();
		}
	}
	lSession->terminate();
	return 0;
}

THREAD_SIGNATURE TestFamilies2::RunFamilyAync(void * pInfo)
{
	RunFamilyThreadInfo *lInfo = (RunFamilyThreadInfo *) pInfo;
	ISession *lSession = MVTApp::startSession(lInfo->mStoreCtx);
	//mLogger.out() << "Executing Family " << lBuf << " ... ";
	{		
		Value lParam[1];
		Value lParam1[2];
		lParam1[0].setDateTime(lInfo->mStartDate);				
		lParam1[1].setDateTime(lInfo->mEndDate);			
		lParam[0].setRange(lParam1);
				
		ClassSpec lCS;
		IStmt * lQ = lSession->createStmt();
		lCS.classID = lInfo->mFamilyID;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);
		//uint64_t lCount = 0;
		//clock_t lBef = getTimeInMs();
		//lQ->count(lCount);
		//clock_t lAft = getTimeInMs();
		OrderSeg lOrder = {NULL,lInfo->mPropID,ORD_DESC,0,0};
		lQ->setOrder(&lOrder,1);
		ICursor *lR = NULL;
		lQ->execute(&lR);
		if(lR)
		{
			for(IPIN *lPIN = lR->next();lPIN!=NULL;lPIN=lR->next())
				lPIN->destroy();
			lR->destroy();
		}
		//mLogger.out() << std::endl << "Time taken to count " << lCount << " PINs -- " << std::dec << (lAft - lBef) << "ms" << std::endl;
		lQ->destroy();
	}
	lSession->terminate();
	return 0;
}
class MyFamilyStream : public IStream
{
	/* generate a stream with no specific content 
	   based on the size specified in constructor */

protected:
	size_t size;
	size_t remaining;
public:
	MyFamilyStream(size_t psize):size(psize), remaining(psize){}
	virtual size_t read(void * buf, size_t maxLength) {
		size_t consumed = remaining < maxLength ? remaining:maxLength;
		remaining-=consumed;
		//REVIEW: code was this
		//memset(buf,remaining,maxLength );
		//but I think this makes more sense:
		memset(buf,'e',consumed) ; // set buffer to all bytes to a specific byte, e.g. 'e'
		return consumed;
	} 
	virtual ValueType dataType() const { return VT_STRING; }
	virtual size_t readChunk(uint64_t pSeek, void * buf, size_t maxLength) {
		cout << "Read chunk!";return 0;
	}
	virtual	IStream * clone() const { return new MyFamilyStream(size); }
	virtual	RC reset() { remaining=size; return RC_OK;  }
	virtual void destroy() { delete this; }
	virtual	uint64_t length() const { return size;}
};

int	TestFamilies2::execute(){
	mRunTimes = 0;
	if (MVTApp::startStore()){
		ISession * const lSession =	MVTApp::startSession();
		mStoreCtx = MVTApp::getStoreCtx();
		
		#if TEST_IMPORT_PERF
			// create PINs along with a collection n.b: This is faster than the next one by 12 times
			TVERIFY(testCountPerfOnFamily(lSession,true));
			
			// creates PINs and later modifies a property to a collection
			TVERIFY(testCountPerfOnFamily(lSession)) ;

			TVERIFY(testImageImportPerf(lSession)) ;
			mImagePIDs.clear();
			mImageStr.clear();
			mImageMd5.clear();
		#else
			mNumPINsWithINT = 0;
			mStartHistogram = 0;
			mEndHistogram = 100;
			mBoundaryHistogram = 0;
			mNumPINsWithStr = mNumPINsWithUStr = 0;
			URIMap lData;
			int i;
			Tstring lPropStr; MVTRand::getString(lPropStr,10,10,false,true);
			for (i = 0; i < mNumProps; i++)
			{
				char lB2[64];
				sprintf(lB2, "TestFamilies2.prop%s.%d", lPropStr.c_str(), i);
				lData.URI = lB2; lData.uid = STORE_INVALID_PROPID; lSession->mapURIs(1, &lData);
				mPropIds[i] = lData.uid;
				Tstring lTStr;MVTRand::getString(lTStr, 10, 50, false);mStr.push_back(lTStr);
				//mLogger.out() << "mStr[" << i << "]=" << lTStr.c_str() << std::endl;
				Tstring lWStr;MVTRand::getString(lWStr, 10, 50, false);mWStr.push_back(lWStr);
				mNumPINsImportTestStr[i] = 0;
				mNumPINsImportTestWStr[i] = 0;
			}
            
			// Family with VT_ARRAY and VT_COLLECTION as params
			TVERIFY(testArrayFamily(lSession));
			
			/*	#1
				class() = /pin[prop1 contains 'abc']
				class2() = /pin[pin is class() and (pin has prop2 and prop2 != 'xyz')]
				family() = /pin[pin is class2() and date in $var]
			*/
			TVERIFY(testComplexFamily(lSession, true /*make collection*/, false /*longer expression*/)) ;

			/*	#2
				class() = /pin[prop1 contains 'abc']
				class2() = /pin[pin is class() and ((pin has prop2 and prop2 != 'xyz') or !(pin has prop2))]
				family() = /pin[pin is class2() and date in $var]
			*/
			TVERIFY(testComplexFamily(lSession, true, true)) ;

			// same as #1 but without collection
			TVERIFY(testComplexFamily(lSession,false,false)) ;

			// same as #2 but without collection
			TVERIFY(testComplexFamily(lSession, false, true)) ;

			// family() = /pin[prop1 = $var] order by prop1
			// order by VT_STR
			TVERIFY(testFamilyWithOrderBy(lSession, 0)) ;
			
			// family() = /pin[prop1 = $var] order by prop1
			// order by VT_INT
			TVERIFY(testFamilyWithOrderBy(lSession, 1)) ;

			// family() = /pin[prop1 = $var] order by prop1
			// order by VT_DATETIME
			TVERIFY(testFamilyWithOrderBy(lSession, 2)) ;
			/* 
				Family declassification issue:
				defineclass:	imageW() = /pin[prop2 = "abc"]
				definefamily:   family() = /pin[pin is imageW() and date in $var]
				modify pins, change few pins prop2 to 'xyz'
				run family() again, Zero pins are returned
			*/
			TVERIFY(testClassInClassFamily(lSession)) ;

			/* 
				Family declassification issue:
				defineclass:	image() = /pin[pin contains 'image']
				defineclass:	imageW() = /pin[pin is image() and prop2 = "abc"]
				definefamily:   family() = /pin[pin is imageW() and date in $var]
				modify pins, change few pins prop2 to 'xyz'
				run family() again. Zero pins are returned
			*/
			TVERIFY(testClassInClassFamily(lSession, true)) ;

			/*
				Asyncronously creates PINs belonging to a family and executes the family.
			*/
			TVERIFY(testAsyncPINCreationWithFamily(lSession)) ;
			/* #1		
				. Without a class in the family definition and with a static value
				. AND PINs are created after family definition
				. /pin[prop1 = "xyz" and prop2 = $var]
			*/
			TVERIFY(testFamilyWithMultiConditions(lSession, false, true)) ;
			
			/* #2
				. With a class in the family definition and without a static value
				. AND PINs are created after family definition
				. /pin[pin is image() and prop2 = $var]
			*/
			TVERIFY(testFamilyWithMultiConditions(lSession, true, false)) ;
	
			//#3    Same as #1 but PINs are created before family definition
			TVERIFY(testFamilyWithMultiConditions(lSession, false, true, true)) ;

			//#4    Same as #2 but PINs are created before family definition	
			TVERIFY(testFamilyWithMultiConditions(lSession, true, false, true)) ;

			TVERIFY(testHistogramDateFamily2(lSession)) ;

			TVERIFY(testHistogramDateFamily(lSession)) ;
			
			// family() = /pin[prop1 >= $startdate && prop1 <= $enddate]			
			TVERIFY(testHistogramFamily(lSession, true)) ;			
			
			// family() = /pin[prop1 > $startdate && prop1 < $enddate]
			TVERIFY(testHistogramFamily(lSession, false)) ;
			
			TVERIFY(testImportFamily3(lSession)) ;			

			TVERIFY(testImportFamily1(lSession));

			// Create PINs with random number of Properties
			//if(!lSuccess || !createPINs(lSession))
			//	lSuccess = false;
		
			mStr.clear();
			mWStr.clear();
			mPIDs.clear();
		#endif

		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Failed to open store"); }
	return RC_OK  ;
}
#define REPRO_BUG_3227 0
bool TestFamilies2::createPINs(ISession *pSession, const int pNumPINs){
	bool lSuccess = true;
	IPIN * lPIN = NULL;
	PID lPID; INITLOCALPID(lPID); lPID.pid = STORE_INVALID_PID;
	bool lCluster = false;
	std::vector<IPIN *> lClusterPINs;
	int lClusterSize = (int) pNumPINs/10 ;
	static int const sMaxVals = 100;
	Value lPVs[sMaxVals * mNumProps];
	int i, j, iV, k;
	mLogger.out() << "Creating " << pNumPINs << " pins ";
	for (i = 0,k = 1; i < pNumPINs; i++, k++)
	{
		if (0 == i % 100)
			mLogger.out() << ".";
		lPIN = pSession->createUncommittedPIN();
		RefVID *lRef = (RefVID *)pSession->alloc(1*sizeof(RefVID));
		int lNumProps = (int)((float)(mNumProps + 1) * rand() / RAND_MAX); // 1 or more properties.
		lNumProps = lNumProps == 0?1:lNumProps;
		for (j = 0, iV = 0; j < lNumProps; j++)
		{
			switch(j){
				case 0: // VT_STRING
					{
						int lIndex = (int)((float)mNumProps * rand() / RAND_MAX); 
						lIndex = lIndex == mNumProps?mNumProps-1:lIndex;
						SETVALUE(lPVs[iV], mPropIds[j], mStr[lIndex].c_str(), OP_SET);iV++;
						mNumPINsImportTestStr[lIndex]++; mNumPINsWithStr++;
					}
					break;
				case 1: // VT_STRING
					{
						int lIndex = (int)((float)mNumProps * rand() / RAND_MAX); 
						lIndex = lIndex == mNumProps?mNumProps-1:lIndex;
						SETVALUE(lPVs[iV], mPropIds[j], mWStr[lIndex].c_str(), OP_SET);iV++;
						mNumPINsImportTestWStr[lIndex]++; mNumPINsWithUStr++;
					}
					break;
				case 2: // VT_INT
					{
						int lRand = (int)((float)pNumPINs * rand() / RAND_MAX); 
						SETVALUE(lPVs[iV], mPropIds[j], lRand, OP_SET);iV++;
						if(lRand > mStartHistogram && lRand < mEndHistogram) mNumPINsWithINT++;
						if(lRand == mStartHistogram || lRand == mEndHistogram)mBoundaryHistogram++;
					}
					break;
				case 3: // VT_DOUBLE
					{
						double lRand = (double)((double)pNumPINs * rand() / RAND_MAX); 
						SETVALUE(lPVs[iV], mPropIds[j], lRand, OP_SET);iV++;
					}
					break;
				case 4: // VT_REF
					{
						bool lAdd = (int)((float)pNumPINs * rand()/RAND_MAX) > pNumPINs/2;
						if(i > 1 && lAdd && !lCluster){
							lPVs[iV].set(lPID);lPVs[iV].setPropID(mPropIds[j]);iV++;
						}
					}					
					break;
				case 5: // VT_REFIDPROP
					{
						bool lAdd = (int)((float)pNumPINs * rand()/RAND_MAX) > pNumPINs/2;
						if(i > 1 && lAdd && !lCluster){							
							RefVID lRef1 = {lPID,mPropIds[0],STORE_COLLECTION_ID,0};
							*lRef = lRef1;
							lPVs[iV].set(*lRef);lPVs[iV].setPropID(mPropIds[j]);iV++;
						}
					}
					break;
				case 6: // PROP_SPEC_UPDATED
					{
						lPVs[iV].set(0);lPVs[iV].setPropID(PROP_SPEC_UPDATED);iV++;						
					}
					break;	
			}
		}	
		if(RC_OK != lPIN->modify(lPVs,iV)){
			mLogger.out() << "ERROR (TestFamilies2::createPINs): Failed to modify uncommitted pin " << std::endl;
			lSuccess = false;
		}
		pSession->free(lRef);
		if(lCluster){
			lClusterPINs.push_back(lPIN);
		}else{
			RC lRC;
			if(RC_OK != (lRC=pSession->commitPINs(&lPIN,1))){
				mLogger.out() << "ERROR (TestFamilies2::createPINs): Failed to commit the pin  with RC = " << lRC << std::endl;
				lSuccess = false;
			}
			else
			{
				if(i > 0) lPID = lPIN->getPID();
				mPIDs.push_back(lPIN->getPID());
				lPIN->destroy();
			}
		}
		if(lCluster && k == lClusterSize){
			if(RC_OK != pSession->commitPINs(&lClusterPINs[0],lClusterSize)){
				mLogger.out() << "ERROR (TestFamilies2::createPINs): Failed to commit cluster of pins " << std::endl;
				lSuccess = false;
			}
			else
			{
				for(k = 0; k < lClusterSize; k++){
					mPIDs.push_back(lClusterPINs[k]->getPID());
					lClusterPINs[k]->destroy();
				}
				k = 1;
			}
		}
	}	
	mLogger.out() << " DONE" << std::endl;
	return lSuccess;
}

bool TestFamilies2::modifyPINs(ISession *pSession, int pNumPINs){
	bool lSuccess = true;	
	static int const sMaxVals = 100;
	int lNumPINsCreated = (int)mPIDs.size();
	int lPIDsIndex = (int)((float)(lNumPINsCreated/2) * rand() /RAND_MAX);
	pNumPINs = pNumPINs <= lNumPINsCreated? pNumPINs: lNumPINsCreated/2;
	Value lPVs[sMaxVals * mNumProps];
	int i, j, iV;
	RefVID lRef;
	
	char const * lBefMod = "tf2_beforemodif.txt";Tofstream lOs1(lBefMod, std::ios::ate);
	char const * lAftMod = "tf2_aftermodif.txt";Tofstream lOs2(lAftMod, std::ios::ate);	

	mLogger.out() << "Modifying " << pNumPINs << " pins from Index " << lPIDsIndex << " ...";
	for (i = 0; i < pNumPINs;i++,lPIDsIndex++)
	{
		if (0 == i % 50)
			mLogger.out() << ".";
		IPIN * lPIN = pSession->getPIN(mPIDs[lPIDsIndex]);
		if(lPIN)
		{
			#if REPRO_BUG_3227
				MVTApp::output(*lPIN, lOs1, NULL);
			#endif
			int lNumProps = (int)((float)(mNumProps + 1) * rand() / RAND_MAX); // 1 or more properties.
			lNumProps = lNumProps == 0?1:lNumProps;
			for (j = 0, iV = 0; j < lNumProps; j++)
			{
				switch(j){
					case 0: // VT_STRING
						{
							int lIndex = (int)((float)mNumProps * rand() / RAND_MAX);
							lIndex = lIndex == mNumProps?mNumProps-1:lIndex;
							SETVALUE(lPVs[iV], mPropIds[j], mStr[lIndex].c_str(), OP_SET);iV++;
							if(lPIN->defined(&mPropIds[j],1)){
								const Value *lVal = lPIN->getValue(mPropIds[j]);
								int k;
								for(k = 0; k < mNumProps; k++) 
									if(strcmp(mStr[k].c_str(), lVal->str) == 0) break;
								mNumPINsImportTestStr[k]--;
							}
							mNumPINsImportTestStr[lIndex]++;
						}
						break;
					case 1: // VT_STRING
						{
							int lIndex = (int)((float)mNumProps * rand() / RAND_MAX);
							lIndex = lIndex == mNumProps?mNumProps-1:lIndex;
							SETVALUE(lPVs[iV], mPropIds[j], mWStr[lIndex].c_str(), OP_SET);iV++;
							if(lPIN->defined(&mPropIds[j],1)){
								const Value *lVal = lPIN->getValue(mPropIds[j]);
								int k;
								for(k = 0; k < mNumProps; k++) 
									if(strcmp(mWStr[k].c_str(), lVal->str) == 0) break;
								mNumPINsImportTestWStr[k]--;
							}
							mNumPINsImportTestWStr[lIndex]++;
						}
						break;
					case 2: // VT_INT
						{
							int lRand = (int)((float)pNumPINs * rand() / RAND_MAX); 
							SETVALUE(lPVs[iV], mPropIds[j], lRand, OP_SET);iV++;
						}
						break;
					case 3: // VT_DOUBLE
						{
							double lRand = (double)((double)pNumPINs * rand() / RAND_MAX); 
							SETVALUE(lPVs[iV], mPropIds[j], lRand, OP_SET);iV++;
						}
						break;
					case 4: // VT_REF
						{
							bool lAdd = (int)((float)pNumPINs * rand()/RAND_MAX) > pNumPINs/2;
							if(lAdd){
								lPVs[iV].set(mPIDs[lPIDsIndex-1]);lPVs[iV].setPropID(mPropIds[j]);iV++;
							}
						}					
						break;
					case 5: // VT_REFIDPROP
						{
							bool lAdd = (int)((float)pNumPINs * rand()/RAND_MAX) > pNumPINs/2;
							if(lAdd){
								RefVID lRef2 = {mPIDs[lPIDsIndex-1],mPropIds[0],STORE_COLLECTION_ID,0};
								lRef=lRef2;
								lPVs[iV].set(lRef);lPVs[iV].setPropID(mPropIds[j]);iV++;
							}
						}
						break;
					case 6: // PROP_SPEC_UPDATED
						if (lPIN->getValue(PROP_SPEC_UPDATED)==NULL) {
							lPVs[iV].set(0);lPVs[iV].setPropID(PROP_SPEC_UPDATED); lPVs[iV].op=OP_ADD; iV++;						
						}
						break;	
				}
			}	
			if(RC_OK != lPIN->modify(lPVs,iV)){
				mLogger.out() << "ERROR (TestFamilies2::modifyPINs): Failed to modify pin " << std::endl;
				MVTApp::output(*lPIN,mLogger.out(),pSession);
				MVTApp::output(*lPVs,mLogger.out(),pSession);

				lSuccess = false;
			}else{
				MVTApp::output(*lPIN, lOs2, NULL);			
				lPIN->destroy();
			}
		}
	}
	mLogger.out() << " DONE" << std::endl;
	return lSuccess;
}


bool TestFamilies2::deletePINs(ISession *pSession, int pNumPINs)
{
	bool lSuccess = true;	
	int lNumPINsCreated = (int)mPIDs.size();
	std::vector<PID> lPIDList;
	int lPIDsIndex = (int)((float)(lNumPINsCreated/2) * rand() /RAND_MAX);
	pNumPINs = pNumPINs <= lNumPINsCreated? pNumPINs: lNumPINsCreated/2;
	int i, j;	
	
	mLogger.out() << "Deleting " << pNumPINs << " pins from Index " << lPIDsIndex << " ...";
	for (i = 0; i < pNumPINs;i++,lPIDsIndex++)
	{
		if (0 == i % 50)
			mLogger.out() << ".";
		IPIN * lPIN = pSession->getPIN(mPIDs[lPIDsIndex]);
		if(lPIN)
		{
			lPIDList.push_back(mPIDs[lPIDsIndex]);
			for (j = 0; j < mNumProps; j++)
			{
				switch(j){
					case 0: // VT_STRING
						{
							if(lPIN->defined(&mPropIds[j],1)){
								const Value *lVal = lPIN->getValue(mPropIds[j]);
								int k;
								for(k = 0; k < mNumProps; k++) 
									if(strcmp(mStr[k].c_str(), lVal->str) == 0) break;
								mNumPINsImportTestStr[k]--;
							}						
						}
						break;
					case 1: // VT_STRING
						{
							if(lPIN->defined(&mPropIds[j],1)){
								const Value *lVal = lPIN->getValue(mPropIds[j]);
								int k;
								for(k = 0; k < mNumProps; k++) 
									if(strcmp(mWStr[k].c_str(), lVal->str) == 0) break;
								mNumPINsImportTestWStr[k]--;
							}						
						}
						break;
					case 2: // VT_INT
						{
							//int lRand = (int)((float)pNumPINs * rand() / RAND_MAX); 
							//SETVALUE(lPVs[iV], mPropIds[j], lRand, OP_SET);iV++;
						}
						break;
					case 3: // VT_DOUBLE
						{
							//double lRand = (double)((double)pNumPINs * rand() / RAND_MAX); 
							//SETVALUE(lPVs[iV], mPropIds[j], lRand, OP_SET);iV++;
						}
						break;
					case 4: // VT_REF
						{
							//bool lAdd = (int)((float)pNumPINs * rand()/RAND_MAX) > pNumPINs/2;
							//if(lAdd){
								//lPVs[iV].set(mPIDs[lPIDsIndex-1]);lPVs[iV].setPropID(mPropIds[j]);iV++;
							//}
						}					
						break;
					case 5: // VT_REFIDPROP
						{
							//bool lAdd = (int)((float)pNumPINs * rand()/RAND_MAX) > pNumPINs/2;
							//if(lAdd){
								//RefVID lRef = {mPIDs[lPIDsIndex-1],mPropIds[0],STORE_COLLECTION_ID};
								//lPVs[iV].set(lRef);lPVs[iV].setPropID(mPropIds[j]);iV++;
							//}
						}
						break;
					case 6: // PROP_SPEC_UPDATED
						if (lPIN->getValue(PROP_SPEC_UPDATED)==NULL) {
							//lPVs[iV].set(0);lPVs[iV].setPropID(PROP_SPEC_UPDATED); lPVs[iV].op=OP_ADD; iV++;						
						}
						break;	
				}
			}
			lPIN->destroy();
		}
	}
	if(RC_OK != pSession->deletePINs(&lPIDList[0],(unsigned int)lPIDList.size())){//(lPVs,iV)){
		mLogger.out() << "ERROR (TestFamilies2::deletePINs): Failed to delete pin " << std::endl;			
		lSuccess = false;
	}
	
	mLogger.out() << " DONE" << std::endl;
	return lSuccess;
}


#define CREATE_PINS_BEFORE_CLASS 0
#define CREATE_MORE_PINS_AFTER_CLASS 1
#define MODIFY_PINS_AFTER_CLASS 1
#define CONFIRM_WITH_FULLSCAN 1

bool TestFamilies2::testImportFamily1(ISession *pSession){
	bool lSuccess = true;
	static const int lNumPINsToCreate = 5000;
	mLogger.out() << "Running testImportFamily1() for VT_STR: " << std::endl;
	#if CREATE_PINS_BEFORE_CLASS
		if(!createPINs(pSession, lNumPINsToCreate)) return false;
	#endif

	IStmt *lQ = pSession->createStmt();
	unsigned const char lVar = lQ->addVariable();
	IExprTree *lET;
	{
		Value lV[2];
		lV[0].setVarRef(0,mPropIds[0]);
		lV[1].setParam(0);
		lET = EXPRTREEGEN(pSession)(OP_EQ, 2, lV, 0);
	}	
	lQ->addCondition(lVar,lET);
	char lB[100];
	ClassID lCLSID1 = STORE_INVALID_CLASSID;
	Tstring lFamilyStr; MVTRand::getString(lFamilyStr,10,10,false,false);
	sprintf(lB, "TestFamilies2.importTest%s.%d", lFamilyStr.c_str(), 1);
	defineClass(pSession, lB, lQ, &lCLSID1);

	#if !CREATE_PINS_BEFORE_CLASS
		if(!createPINs(pSession, lNumPINsToCreate)) return false;
	#endif

	uint64_t lCount = 0;
	{
		Value lParam[1];
		int lIndex = (int)((float)mNumProps * rand() / RAND_MAX);
		lParam[0].set(mStr[lIndex].c_str());		
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID1;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		lQ->destroy();
		if((int)lCount!=mNumPINsImportTestStr[lIndex]) {
			mLogger.out() << "ERROR(testImportFamily1): Not all PINs were returned" << std::endl;
			lSuccess = false;
		}
	}

	#if CREATE_MORE_PINS_AFTER_CLASS
		if(!createPINs(pSession, lNumPINsToCreate)) return false;	
		lCount = 0;
		{
			Value lParam[1];
			int lIndex = (int)((float)(mNumProps - 1) * rand() / RAND_MAX);
			lParam[0].set(mStr[lIndex].c_str());		
			ClassSpec lCS;
			IStmt * lQ = pSession->createStmt();
			lCS.classID = lCLSID1;
			lCS.nParams = 1;
			lCS.params = lParam;
			lQ->addVariable(&lCS, 1);		
			lQ->count(lCount);
			lQ->destroy();
			if((int)lCount!=mNumPINsImportTestStr[lIndex]) {
				mLogger.out() << "ERROR(testImportFamily1): Not all PINs were returned (1)" << std::endl;
				lSuccess = false;
			}
		}
	#endif

	#if MODIFY_PINS_AFTER_CLASS
		if(!modifyPINs(pSession, (int)lNumPINsToCreate/3)) return false;	
		lCount = 0;
		{
			Value lParam[1];
			#if REPRO_BUG_3227
				int lIndex = 0;
			#else
				int lIndex = (int)((float)(mNumProps - 1) * rand() / RAND_MAX);
			#endif
			lParam[0].set(mStr[lIndex].c_str());		
			ClassSpec lCS;
			IStmt * lQ = pSession->createStmt();
			lCS.classID = lCLSID1;
			lCS.nParams = 1;
			lCS.params = lParam;
			lQ->addVariable(&lCS, 1);
			lQ->count(lCount);
			unsigned long lResultCnt = 0;
			ICursor *lR = NULL;
			TVERIFYRC(lQ->execute(&lR));
			if(lR)
			{
				for(IPIN *lPIN = lR->next(); lPIN != NULL; lPIN = lR->next())
				{					
					const Value *lVal = lPIN->getValue(mPropIds[0]);
					if(strcmp(mStr[lIndex].c_str(), lVal->str) != 0) 
					{						
						mLogger.out() << "*** Incorrect PIN in family *** " <<std::endl;
						mLogger.out() << " Value being searched for " << mStr[lIndex].c_str() << " of PropertyID = 0 " << std::endl;
						//mLogger.out() << std::hex << LOCALPID(lPIN->getPID()) << std::endl;
						MVTApp::output(*lPIN,mLogger.out(), pSession);
						mLogger.out() << " Check tf2_beforemodif.txt and tf2_aftermodif.txt for more info " << std::endl;
					}
					lPIN->destroy();
					lResultCnt++;
				}
				lR->destroy();
			}			
			lQ->destroy();
			if(lResultCnt != (unsigned long)lCount || (int)lCount!=mNumPINsImportTestStr[lIndex]) {
			#if CONFIRM_WITH_FULLSCAN
				// Just to confirm, run a Full Scan and check
				IStmt * lQ = pSession->createStmt();
				unsigned char lVar = lQ->addVariable();
				IExprTree *lET;
				{
					Value lV[2];
					lV[0].setVarRef(0,mPropIds[0]);
					lV[1].set(mStr[lIndex].c_str());
					lET = EXPRTREEGEN(pSession)(OP_EQ, 2, lV, 0);
				}
				lQ->addCondition(lVar,lET);
				uint64_t lCnt = 0;
				lQ->count(lCnt);
				if(lCnt!=lCount){
					mLogger.out() << "ERROR(testImportFamily1): Not all PINs were returned or more PINs were returned" << std::endl;
					lSuccess = false;
				}
				lET->destroy();
				lQ->destroy();
			#else
				mLogger.out() << "ERROR(testImportFamily1): Not all PINs were returned or more PINs were returned" << std::endl;
				lSuccess = false;
			#endif
			}
		}
	#endif

	if(!deletePINs(pSession, (int)lNumPINsToCreate/3)) return false;
	lCount = 0;
		{
			Value lParam[1];
			int lIndex = (int)((float)(mNumProps - 1) * rand() / RAND_MAX);
			lParam[0].set(mStr[lIndex].c_str());		
			ClassSpec lCS;
			IStmt * lQ = pSession->createStmt();
			lCS.classID = lCLSID1;
			lCS.nParams = 1;
			lCS.params = lParam;
			lQ->addVariable(&lCS, 1);
			lQ->count(lCount);
			unsigned long lResultCnt = 0;
			ICursor *lR = NULL;
			TVERIFYRC(lQ->execute(&lR));
			if(lR)
			{
				for(IPIN *lPIN = lR->next(); lPIN != NULL; lPIN = lR->next())
				{					
					lPIN->getValue(mPropIds[0]);
					lPIN->destroy();
					lResultCnt++;
				}
				lR->destroy();
			}			
			lQ->destroy();
			if(lResultCnt != (unsigned long)lCount || (int)lCount!=mNumPINsImportTestStr[lIndex]) {
			#if CONFIRM_WITH_FULLSCAN_1
				// Just to confirm, run a Full Scan and check
				IStmt * lQ = pSession->createStmt();
				unsigned char lVar = lQ->addVariable();
				IExprTree *lET;
				{
					Value lV[2];
					lV[0].setVarRef(0,mPropIds[0]);
					lV[1].set(mStr[lIndex].c_str());
					lET = EXPRTREEGEN(pSession)(OP_EQ, 2, lV, 0);
				}
				lQ->addCondition(lVar,lET);
				uint64_t lCnt = 0;
				lQ->count(lCnt);
				if(lCnt!=lCount){
					mLogger.out() << "ERROR(testImportFamily1): Not all PINs were returned or more PINs were returned" << std::endl;
					lSuccess = false;
				}
				lET->destroy();
				lQ->destroy();
			#else
				mLogger.out() << "ERROR(testImportFamily1): Not all PINs were returned or more PINs were returned" << std::endl;
				lSuccess = false;
			#endif
			}
		}

	return lSuccess;
}

bool TestFamilies2::testImportFamily3(ISession *pSession){
	bool lSuccess = true;
	static const int lNumPINsToCreate = 5000;
	
	if(!createPINs(pSession, lNumPINsToCreate)) return false;	

	IStmt *lQ = pSession->createStmt();
	unsigned const char lVar = lQ->addVariable();
	IExprTree *lET;
	{
		Value lV[2];
		lV[0].setVarRef(0,mPropIds[2]);
		lV[1].setParam(0);
		lET = EXPRTREEGEN(pSession)(OP_IN, 2, lV, 0);
	}	
	lQ->addCondition(lVar,lET);
	char lB[100];
	ClassID lCLSID1 = STORE_INVALID_CLASSID;
	Tstring lFamilyStr; MVTRand::getString(lFamilyStr,10,10,false,false);
	sprintf(lB, "TestFamilies2.importTest%s.%d",lFamilyStr.c_str(), 3);
	mLogger.out() << "Creating Family " << lB << " ...";
	if(RC_OK!=defineClass(pSession,lB, lQ, &lCLSID1)){
		mLogger.out() << "ERROR(testImportFamily2): Failed to create Class " << lB << std::endl;	
		return false;
	}else{
		mLogger.out() << " DONE" << std::endl;
	}

	mLogger.out() << "Executing Family " << lB << " ... ";
	uint64_t lCount = 0;
	{
		Value lParams[2];
		lParams[0].set(0);				
		lParams[1].set(200);
		Value lParam[1];
		lParam[0].setRange(lParams);
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID1;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		lQ->destroy();	
//		if(lCount == 0)
		{
			// Just to confirm, run a Full Scan and check
			IStmt * lQ = pSession->createStmt();
			unsigned char lVar = lQ->addVariable();
			IExprTree *lET;
			{
				Value lV1[2];
				lV1[0].setVarRef(0,mPropIds[2]);
				lV1[1].setParam(0);
				lET = EXPRTREEGEN(pSession)(OP_IN, 2, lV1, 0);
			}
			lQ->addCondition(lVar,lET);
			uint64_t lCnt = 0;
			Value lParam[2];
			lParam[0].set(0);
			lParam[1].set(200);				
			Value lParam1[1];
			lParam1[0].setRange(lParam);
			lQ->count(lCnt,lParam1,1);
			if(lCnt == 0 || lCnt!=lCount)
			{
				mLogger.out() << "ERROR(testImportFamily3): Not all PINs were returned or more PINs were returned. " ;//<< std::endl;
				mLogger.out() << "Family returned = " << lCount << " Full Scan Returned = " << lCnt << std::endl;
				lSuccess = false;
			}
			lET->destroy();
			lQ->destroy();
		}
	}
	return lSuccess;
}

#define COND_1 0

bool TestFamilies2::testHistogramFamily(ISession *pSession, bool pBoundaryIncluded){
	bool lSuccess = true;
	static const int lNumPINsToCreate = 5000;	
	IStmt *lQ = pSession->createStmt();
	unsigned const char lVar = lQ->addVariable();
	IExprTree *lET;
	{
		if(pBoundaryIncluded)
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIds[2]);
			lV[1].setParam(0);
			IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_GT, 2, lV, 0);
			lV[0].setVarRef(0,mPropIds[2]);
			lV[1].setParam(1);
			IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_LT, 2, lV, 0);
			lV[0].set(lET1);
			lV[1].set(lET2);
			lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV, 0);
		}
		else
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIds[2]);
			lV[1].setParam(0);
			IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_GE, 2, lV, 0);
			lV[0].setVarRef(0,mPropIds[2]);
			lV[1].setParam(1);
			IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_LE, 2, lV, 0);
			lV[0].set(lET1);
			lV[1].set(lET2);
			lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV, 0);
		}
	}	
	lQ->addCondition(lVar,lET);
	char lB[100];
	ClassID lCLSID = STORE_INVALID_CLASSID;
	Tstring lStr;MVTRand::getString(lStr,10,10,false,true);
	sprintf(lB, "TestFamilies2.histogram%s.%d",lStr.c_str(), 1);	
	mLogger.out() << "Creating Family " << lB << " ...";
	if(RC_OK!=defineClass(pSession,lB, lQ, &lCLSID)){
		mLogger.out() << "ERROR(testHistogramFamily): Failed to create Class " << lB << std::endl;	
		return false;
	}else{
		mLogger.out() << " DONE" << std::endl;
	}
	
	int lExpNumPINs = 0;
	if(!createPINs(pSession, lNumPINsToCreate)) return false;

	if(pBoundaryIncluded)
		lExpNumPINs = mNumPINsWithINT;
	else
		lExpNumPINs = mNumPINsWithINT + mBoundaryHistogram;

	mLogger.out() << "Executing Family " << lB << " ... ";
	uint64_t lCount = 0;
	{
		Value lParam[2];
		lParam[0].set(mStartHistogram);				
		lParam[1].set(mEndHistogram);				
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID;
		lCS.nParams = 2;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		lQ->destroy();
		if((int)lCount != lExpNumPINs)
		{			
			mLogger.out() << "ERROR(testHistogramFamily): Not all PINs were returned or more PINs were returned" << std::endl;
			mLogger.out() << "Expected = " << lExpNumPINs << " Got = " << lCount << std::endl;
			lSuccess = false;					
		}else{
			mLogger.out() << " PASSED!" << std::endl;
		}
	}
	return lSuccess;
}

bool TestFamilies2::testHistogramDateFamily(ISession *pSession){
	bool lSuccess = true;
	static const int lNumPINsToCreate = 5000;
	static const int lNumProps = 2;
	PropertyID lPropIDs[lNumProps];
	Tstring lPropStr; MVTRand::getString(lPropStr,10,10,false,true);
	int i = 0;
	URIMap lData;
	for (i = 0; i < lNumProps; i++)
	{
		char lB[100];
		sprintf(lB, "TestFamilies2.testHistogramDateFamily%s.%d", lPropStr.c_str(), i);
		lData.URI = lB; lData.uid = STORE_INVALID_PROPID; pSession->mapURIs(1, &lData);
		lPropIDs[i] = lData.uid;
	}
	uint64_t lui641, lui642, lui643;	
	lui641 = MVTRand::getDateTime(pSession);
	lui642 = MVTRand::getDateTime(pSession);
	if(lui641 > lui642){
		lui643 = lui641; lui641 = lui642; lui642 = lui643;
	}

	int lNumPINsINRange = 0;
	/*
	int i = 0;
	TIMESTAMP ui64;
	mLogger.out() << "Creating " << lNumPINsToCreate << " PINs ...";
	for(i = 0; i < lNumPINsToCreate; i++){
		if( i % 100 == 0) mLogger.out() << ".";
		ui64 = MVTRand::getDateTime(pSession);
		if(ui64 > lui641 && ui64 < lui642) lNumPINsINRange++;
		Value lV[2];
		SETVALUE(lV[0],lPropIDs[0], i, OP_SET);
		lV[1].setDateTime(ui64); lV[1].setPropID(lPropIDs[1]);
		IPIN *lPIN = pSession->createUncommittedPIN(lV,2,MODE_COPY_VALUES);
		TVERIFYRC(pSession->commitPINs(&lPIN,1));
		lPIN->destroy();
	}
	*/
	IStmt *lQ = pSession->createStmt();
	unsigned const char lVar = lQ->addVariable();
	IExprTree *lET;
	{
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[1]);
		lV[1].setParam(0);
		IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_GT, 2, lV, 0);
		lV[0].setVarRef(0,lPropIDs[1]);
		lV[1].setParam(1);
		IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_LT, 2, lV, 0);
		lV[0].set(lET1);
		lV[1].set(lET2);
		lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV, 0);
	}	
	lQ->addCondition(lVar,lET);
	char lB[100];
	ClassID lCLSID = STORE_INVALID_CLASSID;
	Tstring lFamilyStr; MVTRand::getString(lFamilyStr,10,10,false,false);
	sprintf(lB, "TestFamilies2.HistogramDateFamily%s.%d", lFamilyStr.c_str(), 1);
	mLogger.out() << "Creating Family " << lB << " ...";
	if(RC_OK!=defineClass(pSession,lB, lQ, &lCLSID)){
		mLogger.out() << "ERROR(testHistogramDateFamily): Failed to create Class " << lB << std::endl;	
		return false;
	}else{
		mLogger.out() << " DONE" << std::endl;
	}

	
	TIMESTAMP ui64;
	mLogger.out() << "Creating " << lNumPINsToCreate << " PINs ...";
	for(i = 0; i < lNumPINsToCreate; i++){
			if( i % 100 == 0) mLogger.out() << ".";
			ui64 = MVTRand::getDateTime(pSession);
			if(ui64 > lui641 && ui64 < lui642) lNumPINsINRange++;
			Value lV[2];
			SETVALUE(lV[0],lPropIDs[0], i, OP_SET);
			lV[1].setDateTime(ui64); lV[1].setPropID(lPropIDs[1]);
			IPIN *lPIN = pSession->createUncommittedPIN(lV,2,MODE_COPY_VALUES);
			TVERIFY(lPIN!=NULL);
			if (lPIN!=NULL) {
				TVERIFYRC(pSession->commitPINs(&lPIN,1));
				lPIN->destroy();
			}
	}
	mLogger.out() << std::endl;
	
	mLogger.out() << "Executing Family " << lB << " ... ";
	uint64_t lCount = 0;
	{		
		Value lParam[2];
		lParam[0].setDateTime(lui641);				
		lParam[1].setDateTime(lui642);
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID;
		lCS.nParams = 2;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		lQ->destroy();	
		if((int)lCount != lNumPINsINRange){
			#if 1
				//Confirm with a FULL SCAN
				IStmt *lQ = pSession->createStmt();
				unsigned const char lVar = lQ->addVariable();
				IExprTree *lET;
				{
					Value lV[2];
					lV[0].setVarRef(0,lPropIDs[1]);
					lV[1].setParam(0);
					IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_GT, 2, lV, 0);
					lV[0].setVarRef(0,lPropIDs[1]);
					lV[1].setParam(1);
					IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_LT, 2, lV, 0);
					lV[0].set(lET1);
					lV[1].set(lET2);
					lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV, 0);
				}	
				lQ->addCondition(lVar,lET);
				Value lParam[2];
				lParam[0].setDateTime(lui641);
				lParam[1].setDateTime(lui642);
				uint64_t lCnt = 0;
				lQ->count(lCnt, lParam, 2);
				if((int)lCnt!= lNumPINsINRange || lCnt!=lCount){
					mLogger.out() << "ERROR(testHistogramDateFamily): Not all PINs were returned or more PINs were returned" << std::endl;
					lSuccess = false;
				}else{
					mLogger.out() << " PASSED!" << std::endl;
				}
				lET->destroy();
				lQ->destroy();
			#else
				mLogger.out() << "ERROR(testHistogramDateFamily): Not all PINs were returned or more PINs were returned" << std::endl;
				lSuccess = false;			
			#endif
		}else{
			mLogger.out() << " PASSED!" << std::endl;
		}
	}
	return lSuccess;
}

bool TestFamilies2::testHistogramDateFamily2(ISession *pSession){

	bool lSuccess = true;
	static const int lNumPINsToCreate = 100;
	static const int lNumProps = 2;
	PropertyID lPropIDs[lNumProps];
	Tstring lPropStr; MVTRand::getString(lPropStr,10,10,false,true);
	int i = 0;
	URIMap lData;
	for (i = 0; i < lNumProps; i++)
	{
		char lB[100];
		sprintf(lB, "TestFamilies2.testHistogramDateFamily2%s.%d", lPropStr.c_str(), i);
		lData.URI = lB; lData.uid = STORE_INVALID_PROPID; pSession->mapURIs(1, &lData);
		lPropIDs[i] = lData.uid;
	}
	uint64_t mStartDate, mEndDate;
	mStartDate = 0;
	mEndDate = MVTRand::getDateTime(pSession);
	uint64_t lMinDate = mEndDate;
	int lNumExpectedPINs = 0;
	TIMESTAMP ui64;
	mLogger.out() << "Creating " << lNumPINsToCreate << " PINs ...";
	for(i = 0; i < lNumPINsToCreate; i++){
		if( i % 100 == 0) mLogger.out() << ".";
		ui64 = MVTRand::getDateTime(pSession);
		if(lNumExpectedPINs==0) ui64=mStartDate; // Ensure at least one match (see 15571)
		if(ui64  < lMinDate) lMinDate = ui64;
		if(mStartDate <= ui64 && ui64 <= mEndDate) lNumExpectedPINs++;
		Value lV[2];
		SETVALUE(lV[0],lPropIDs[0], i, OP_SET);
		lV[1].setDateTime(ui64); lV[1].setPropID(lPropIDs[1]);
		IPIN *lPIN = pSession->createUncommittedPIN(lV,2,MODE_COPY_VALUES);
		TVERIFY(lPIN!=NULL);
		if (lPIN!=NULL) {
			TVERIFYRC(pSession->commitPINs(&lPIN,1));
			lPIN->destroy();
		}
	}

	//mLogger.out() << "Number of PINs created " << lNumPINsToCreate << " StartDate = " << mStartDate << " EndDate = " << mEndDate << std::endl;
	IStmt *lQ = pSession->createStmt();
	unsigned const char lVar = lQ->addVariable();

	#define DONT_USE_OP_IN 0

	IExprTree *lET;
	{
		#if DONT_USE_OP_IN
			Value lV[2];
			lV[0].setVarRef(0,lPropIDs[1]);
			lV[1].setParam(0);
			IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_GT, 2, lV, 0);		
			lV[0].setVarRef(0,lPropIDs[1]);
			lV[1].setParam(1);
			IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_LT, 2, lV, 0);
			lV[0].set(lET1);
			lV[1].set(lET2);
			lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV, 0);
		#else
			Value lV[2];
			lV[0].setVarRef(0,lPropIDs[1]);
			lV[1].setParam(0);
			lET = EXPRTREEGEN(pSession)(OP_IN, 2, lV, 0);
		#endif
	}	
	lQ->addCondition(lVar,lET);
	char lB[64];
	ClassID lCLSID = STORE_INVALID_CLASSID;
	Tstring lFamilyStr; MVTRand::getString(lFamilyStr,10,10,false,false);
	sprintf(lB, "TestFamilies2.HistogramDateFamily%s.%d",lFamilyStr.c_str(), 2);
	mLogger.out() << "Creating Family " << lB << " ...";
	if(RC_OK!=defineClass(pSession,lB, lQ, &lCLSID)){
		TVERIFY(false);
		mLogger.out() << "Failed to create Class " << lB << std::endl;	
		return false;
	}else{
		mLogger.out() << " DONE" << std::endl;
	}
	
	mLogger.out() << "Executing Family " << lB << " ... ";
	uint64_t lResDT= 0;
	{		
		Value lParam[2];
		int lNumParams = 0;
		#if DONT_USE_OP_IN
			lParam[0].setDateTime(mStartDate);				
			lParam[1].setDateTime(mEndDate);			
			lNumParams = 2;
		#else
			Value lParam1[2];
			lParam1[0].setDateTime(mStartDate);				
			lParam1[1].setDateTime(mEndDate);
			lParam[0].setRange(lParam1);
			lNumParams = 1;
		#endif		
		
		ClassSpec lCS;
		int lNumActualPINs = 0;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID;
		lCS.nParams = lNumParams;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		ICursor *lR = NULL;
		TVERIFYRC(lQ->execute(&lR));
		if(lR){
			IPIN *lPIN = lR->next();
			if(lPIN){
				lResDT = lPIN->getValue(lPropIDs[1])->ui64;
				// *** PROBLEM ****
				// Runtime error is thrown at pgtree.cpp TreePageMgr::TreePage::findKey() line 1030
				#if 1
					for(;lPIN!=NULL; lPIN=lR->next()){
						lNumActualPINs++;
						uint64_t dt=lPIN->getValue(lPropIDs[1])->ui64;
						if (dt<lResDT) lResDT=dt;
						//MVTApp::output(*lPIN,mLogger.out(),pSession);
						lPIN->destroy();
						if(lNumActualPINs > lNumPINsToCreate)
						{
							TVERIFY(!"More PINs returned than expected !!! " );
							break;
						}						
					}
				#else
					lPIN->destroy();
				#endif
			}else{
				TVERIFY(!"NO PIN was returned");
			}
			lR->destroy();	
		}else{		
			TVERIFY(!"NO results were returned");
			lResDT = 0;
		}
		lQ->destroy();
		//mLogger.out() << "Expected PINs = " << lNumExpectedPINs << " Actual PINs = " << lNumActualPINs << std::endl;
		if(lResDT != lMinDate || lNumExpectedPINs != lNumActualPINs){
			#if 1
				//Confirm with a FULL SCAN
				IStmt *lQ = pSession->createStmt();
				unsigned const char lVar = lQ->addVariable();
				IExprTree *lET;
				{
					#if DONT_USE_OP_IN
						Value lV[2];
						lV[0].setVarRef(0,lPropIDs[1]);
						lV[1].setParam(0);
						IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_GT, 2, lV, 0);		
						lV[0].setVarRef(0,lPropIDs[1]);
						lV[1].setParam(1);
						IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_LT, 2, lV, 0);
						lV[0].set(lET1);
						lV[1].set(lET2);
						lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV, 0);
					#else
						Value lV[2];
						lV[0].setVarRef(0,lPropIDs[1]);
						lV[1].setParam(0);
						lET = EXPRTREEGEN(pSession)(OP_IN, 2, lV, 0);
					#endif
				}	
				lQ->addCondition(lVar,lET);
				Value lParam[2];
				int lNumParams = 0;
				#if DONT_USE_OP_IN
					lParam[0].setDateTime(mStartDate);				
					lParam[1].setDateTime(mEndDate);			
					lNumParams = 2;
				#else
					Value lParam1[2];
					lParam1[0].setDateTime(mStartDate);				
					lParam1[1].setDateTime(mEndDate);
					lParam[0].setRange(lParam1);
					lNumParams = 1;
				#endif	
				uint64_t lResultDT = 0;
				ICursor *lR = NULL;
				TVERIFYRC(lQ->execute(&lR, lParam,lNumParams));
				lResultDT = lR->next()->getValue(lPropIDs[1])->ui64;				
				lR->destroy();	
				if(lResultDT!= lMinDate || lResultDT!=lResDT){
					TVERIFY(!"Minimum value is not returned");
					lSuccess = false;
				}else{
					mLogger.out() << " PASSED!" << std::endl;
				}
				lET->destroy();
				lQ->destroy();
			#else
				TVERIFY(!"Minimum value is not returned");
				lSuccess = false;			
			#endif
		}else{
			mLogger.out() << " PASSED!" << std::endl;
		}
	}
	return lSuccess;
}

bool TestFamilies2::testFamilyWithMultiConditions(ISession *pSession, bool pUseClass, bool pUseStaticValue, bool pCreatePINsBeforefamily)
{
	bool lSuccess = true;
	static const int lNumPINsToCreate = 1000;
	static const int lNumProps = 3;
	PropertyID lPropIDs[lNumProps];
	Tstring lPropStr; MVTRand::getString(lPropStr,10,10,false,true);
	int i = 0;
	URIMap lData;
	for (i = 0; i < lNumProps; i++)
	{
		char lB[100];
		sprintf(lB, "TestFamilies2.testFamilyWithMultiConditions%s.%d", lPropStr.c_str(), i);
		lData.URI = lB; lData.uid = STORE_INVALID_PROPID; pSession->mapURIs(1, &lData);
		lPropIDs[i] = lData.uid;
	}
	/*
	static const int lBasePropID = 27688;
	static const int lNumProps = 3;
	int lStartPropID;
	lStartPropID = lBasePropID + (lNumProps * mRunTimes);
	PropertyID lPropIDs[lNumProps] = {lStartPropID , lStartPropID + 1, lStartPropID +  2};
	mRunTimes++;
	*/
	uint64_t lStartDate, lEndDate;
	lStartDate = 0;
	lEndDate = MVTRand::getDateTime(pSession);
	int lNumExpectedPINs = 0;
	int lNumExpPINs = 0;
	Tstring lImageStr; MVTRand::getString(lImageStr,10,50,false,false);
	Tstring lRandStr; MVTRand::getString(lRandStr,20,20,false,true);
	Tstring lImageSubStr = lImageStr.substr(0,6);
	Tstring lRandSubStr = lRandStr.substr(0,5);

	if(pCreatePINsBeforefamily)
	{
		mLogger.out() << "Creating " << lNumPINsToCreate << " PINs...";		
		for(i = 0; i < lNumPINsToCreate; i++){
			if( i % 100 == 0) mLogger.out() << ".";
			TIMESTAMP ui64 = MVTRand::getDateTime(pSession);
			bool lRand = (int)((float)rand() * lNumPINsToCreate/RAND_MAX) > (int)lNumPINsToCreate/2;
			if(ui64 >= lStartDate && ui64 <= lEndDate && lRand) lNumExpectedPINs++;
			if(lRand) lNumExpPINs++;
			Value lV[3];		
			SETVALUE(lV[0],lPropIDs[0], lImageStr.c_str(), OP_SET);
			SETVALUE(lV[1],lPropIDs[1], lRand?lRandSubStr.c_str():lRandStr.c_str(),OP_SET);
			lV[2].setDateTime(ui64); lV[2].setPropID(lPropIDs[2]);
			IPIN *lPIN = pSession->createUncommittedPIN(lV,3,MODE_COPY_VALUES);
			TVERIFY(lPIN!=NULL);
			if (lPIN!=NULL) {
				TVERIFYRC(pSession->commitPINs(&lPIN,1));
				lPIN->destroy();
			}
		}
		mLogger.out() << std::endl;
	}

	IStmt *lQ;
	unsigned char lVar;
	if(pUseClass)
	{
		ClassID lCLSImageID = STORE_INVALID_CLASSID;
		{
			IStmt *lQ = pSession->createStmt();
			unsigned const char lVar = lQ->addVariable();
			Value lV[2];
			lV[0].setVarRef(0,lPropIDs[0]);
			lV[1].set(lImageSubStr.c_str());
			IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_CONTAINS, 2, lV, CASE_INSENSITIVE_OP);		
			lQ->addCondition(lVar,lET1);
			char lB[100];			
			sprintf(lB, "TestFamilies2.image%s.%d", lImageSubStr.c_str(), mRunTimes);
			RC rc=defineClass(pSession,lB, lQ, &lCLSImageID);
			lET1->destroy();
			lQ->destroy();
			if(RC_OK!=rc){
				mLogger.out() << "ERROR(testFamilyWithMultiConditions): Failed to create Class " << lB << std::endl;	
				return false;
			}
		}


		lQ = pSession->createStmt();
		ClassSpec lRange[1];
		lRange[0].classID = lCLSImageID;
		lRange[0].nParams = 0;
		lRange[0].params = NULL;

		lVar = lQ->addVariable(lRange,1);
	}
	else
	{
		lQ = pSession->createStmt();
		lVar = lQ->addVariable();
	}

	IExprTree *lET;
	if(pUseStaticValue)
	{
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[1]);
		lV[1].set(lRandSubStr.c_str());
		IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_EQ, 2, lV, 0);
		lV[0].setVarRef(0,lPropIDs[2]);
		lV[1].setParam(0);
		IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_IN, 2, lV, 0);
		lV[0].set(lET1);
		lV[1].set(lET2);
		lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV, 0);		
	}
	else
	{
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[1]);
		lV[1].setParam(0);
		lET = EXPRTREEGEN(pSession)(OP_EQ, 2, lV, 0);	
	}	
	lQ->addCondition(lVar,lET);
	char lB[200];
	ClassID lCLSID = STORE_INVALID_CLASSID;
	sprintf(lB, "TestFamilies2.testFamilyWithMultiConditions%s.%d", lRandSubStr.c_str(), mRunTimes);
	if(RC_OK!=defineClass(pSession,lB, lQ, &lCLSID)){
		mLogger.out() << "ERROR(testFamilyWithMultiConditions): Failed to create Class " << lB << std::endl;	
		return false;
	}

	if(!pCreatePINsBeforefamily)
	{
		mLogger.out() << "Creating " << lNumPINsToCreate << " PINs...";
		for(i = 0; i < lNumPINsToCreate; i++){
			if( i % 100 == 0) mLogger.out() << ".";
			TIMESTAMP ui64 = MVTRand::getDateTime(pSession);
			bool lRand = (int)((float)rand() * lNumPINsToCreate/RAND_MAX) > (int)lNumPINsToCreate/2;
			if(ui64 >= lStartDate && ui64 <= lEndDate && lRand) lNumExpectedPINs++;
			if(lRand) lNumExpPINs++;
			Value lV[3];		
			SETVALUE(lV[0],lPropIDs[0], lImageStr.c_str(), OP_SET);
			SETVALUE(lV[1],lPropIDs[1], lRand?lRandSubStr.c_str():lRandStr.c_str(),OP_SET);
			lV[2].setDateTime(ui64); lV[2].setPropID(lPropIDs[2]);
			IPIN *lPIN = pSession->createUncommittedPIN(lV,3,MODE_COPY_VALUES);
			if(RC_OK!=pSession->commitPINs(&lPIN,1)) 
				mLogger.out() << "Hit 1730 assert ";
			else
				lPIN->destroy();
		}
		mLogger.out() << std::endl;
	}

	mLogger.out() << "Executing Family " << lB << " ... ";
	{		
		Value lParam[1],lParam1[2];
		if(pUseStaticValue)
		{
			lParam1[0].setDateTime(lStartDate);				
			lParam1[1].setDateTime(lEndDate);			
			lParam[0].setRange(lParam1);
		}
		else
		{
			lNumExpectedPINs = lNumExpPINs;			
			lParam[0].set(lRandSubStr.c_str());
		}		
		ClassSpec lCS;
		int lNumActualPINs = 0;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		uint64_t lCount = 0;
		lQ->count(lCount);
		ICursor *lR = NULL;
		TVERIFYRC(lQ->execute(&lR));
		if(lR){
			for(IPIN *lPIN = lR->next(); lPIN!=NULL; lPIN = lR->next())
			{
				lNumActualPINs++;
				lPIN->destroy();
			}
			lR->destroy();		
		}
		lQ->destroy();
		if((int)lCount != lNumExpectedPINs || lNumActualPINs != lNumExpectedPINs)
		{
			mLogger.out() << " ERROR(testFamilyWithMultiConditions): Family did not return expected number of PINs" << std::endl;
			mLogger.out() << " Expected " << lNumExpectedPINs << " Got " << lNumActualPINs << std::endl;
			mLogger.out() << " Test FAILED " << std::endl;
			lSuccess = false;	
		}
	}
	mLogger.out() << std::endl;
	return lSuccess;
}

#define CREATE_FAMILY_BEFORE_PINS 0
	
bool TestFamilies2::testAsyncPINCreationWithFamily(ISession *pSession)
{
	bool lSuccess = true;
	static const int lNumPINsToCreate = 2000;
	static int const lNumThreads = 3;
	static const int lNumProps = 3;

	PropertyID lPropIDs[lNumProps];
	Tstring lPropStr; MVTRand::getString(lPropStr,10,10,false,true);
	int i = 0;
	URIMap lData;
	for (i = 0; i < lNumProps; i++)
	{
		char lB[100];
		sprintf(lB, "TestFamilies2.testAsyncPINCreationWithFamily%s.%d", lPropStr.c_str(), i);
		lData.URI = lB; lData.uid = STORE_INVALID_PROPID; pSession->mapURIs(1, &lData);
		lPropIDs[i] = lData.uid;
	}
	/*
	static const int lBasePropID = 32008;
	static const int lNumProps = 3;
	int lStartPropID;
	lStartPropID = lBasePropID + (lNumProps * mRunTimes);
	PropertyID lPropIDs[lNumProps] = {lStartPropID , lStartPropID + 1, lStartPropID +  2};	
	*/
	Tstring lImageStr; MVTRand::getString(lImageStr,10,50,false,false);
	Tstring lRandStr; MVTRand::getString(lRandStr,20,20,false,true);
	Tstring lImageSubStr = lImageStr.substr(0,6);
	Tstring lRandSubStr = lRandStr.substr(0,5);
	
	#if CREATE_FAMILY_BEFORE_PINS
		ClassID lCLSImageID = STORE_INVALID_CLASSID;
		{
			IStmt *lQ = pSession->createStmt();
			unsigned const char lVar = lQ->addVariable();
			Value lV[2];
			lV[0].setVarRef(0,lPropIDs[0]);
			lV[1].set(lImageSubStr.c_str());
			IExprTree *lET = EXPRTREEGEN(pSession)(OP_CONTAINS, 2, lV, CASE_INSENSITIVE_OP);		
			lQ->addCondition(lVar,lET);
			char lB[100];			
			sprintf(lB, "TestFamilies2.testAsyncPINCreationWithFamily%s.%d", lImageSubStr.c_str(), mRunTimes);
			RC rc=defineClass(pSession,lB, lQ, &lCLSImageID);
			lET->destroy();
			lQ->destroy();
			if(RC_OK!=rc){
				mLogger.out() << "ERROR(testAsyncPINCreationWithFamily): Failed to create Class " << lB << std::endl;	
				return false;
			}
		}
	#endif

	MVTestsPortability::Mutex lLock;
	CreatePINThreadInfo lInfo = {lNumPINsToCreate, lImageStr, lRandStr, lImageSubStr, lRandSubStr, lPropIDs, lNumProps , &lLock, mStoreCtx, this};	
	HTHREAD lThreads[lNumThreads];
	for (i = 0; i < lNumThreads; i++)			
		createThread(&TestFamilies2::createPINsAsync, &lInfo, lThreads[i]);	

	#if !CREATE_FAMILY_BEFORE_PINS
		MVTestsPortability::threadSleep(2000);
		ClassID lCLSImageID = STORE_INVALID_CLASSID;
		{
			IStmt *lQ = pSession->createStmt();
			unsigned const char lVar = lQ->addVariable();
			Value lV[2];
			lV[0].setVarRef(0,lPropIDs[0]);
			lV[1].set(lImageSubStr.c_str());
			IExprTree *lET = EXPRTREEGEN(pSession)(OP_CONTAINS, 2, lV, CASE_INSENSITIVE_OP);		
			lQ->addCondition(lVar,lET);
			char lB[100];			
			sprintf(lB, "TestFamilies2.testAsyncPINCreationWithFamily%s.%d", lImageSubStr.c_str(), mRunTimes);
			RC rc=defineClass(pSession,lB, lQ, &lCLSImageID);
			lET->destroy();
			lQ->destroy();
			if(RC_OK!=rc){
				mLogger.out() << "ERROR(testAsyncPINCreationWithFamily): Failed to create Class " << lB << std::endl;	
				return false;
			}
		}
	#endif


	IStmt *lQ = pSession->createStmt();
	ClassSpec lRange[1];
	lRange[0].classID = lCLSImageID;
	lRange[0].nParams = 0;
	lRange[0].params = NULL;
	unsigned char lVar = lQ->addVariable(lRange,1);
	
	IExprTree *lET;
	{
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[2]);
		lV[1].setParam(0);
		lET = EXPRTREEGEN(pSession)(OP_IN, 2, lV, 0);		
	}
	lQ->addCondition(lVar,lET);
	char lB[200];
	ClassID lCLSID = STORE_INVALID_CLASSID;
	sprintf(lB, "TestFamilies2.testAsyncPINCreationWithFamily%s.%d", lRandSubStr.c_str(), mRunTimes);
	if(RC_OK!=defineClass(pSession,lB, lQ, &lCLSID)){
		mLogger.out() << "ERROR(testAsyncPINCreationWithFamily): Failed to create Class " << lB << std::endl;	
		return false;
	}
	mLogger.out() << "Executing Family " << lB << " in a loop... " << std::endl;
	uint64_t lStartDate = 0;
	uint64_t lEndDate = MVTRand::getDateTime(pSession);
	for(i = 0; i < (int)lNumPINsToCreate/8; i++)
	{
		Value lParam[1];
		Value lParam1[2];
		lParam1[0].setDateTime(lStartDate);				
		lParam1[1].setDateTime(lEndDate);			
		lParam[0].setRange(lParam1);
		int lNumPINsInFamily = 0;
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		ICursor *lR = NULL;
		TVERIFYRC(lQ->execute(&lR));
		if(lR){
			for(IPIN *lPIN = lR->next();lPIN!=NULL; lPIN=lR->next())
			{
				lNumPINsInFamily++;
				lPIN->destroy();
			}
		}
		lR->destroy();		
		lQ->destroy();
		if(i%50 == 0)
			mLogger.out() << "Iteration " << i << ": Number of PINs in family = " << lNumPINsInFamily << std::endl;
	}	
	mLogger.out() << "INFO(testAsyncPINCreationWithFamily): Completed. Waiting for threads to exit" << std::endl;
	MVTestsPortability::threadsWaitFor(lNumThreads, lThreads);
	return lSuccess;
}
bool TestFamilies2::testClassInClassFamily(ISession *pSession, bool pUseClass)
{
	bool lSuccess = true;
	static const int lNumPINsToCreate = 100;
	std::vector<PID> lPIDs;
	static const int lNumProps = 3;
	PropertyID lPropIDs[lNumProps];
	Tstring lPropStr; MVTRand::getString(lPropStr,10,10,false,true);
	int i = 0;
	URIMap lData;
	for (i = 0; i < lNumProps; i++)
	{
		char lB[100];
		sprintf(lB, "TestFamilies2.testClassInClassFamily%s.%d", lPropStr.c_str(), i);
		lData.URI = lB; lData.uid = STORE_INVALID_PROPID; pSession->mapURIs(1, &lData);
		lPropIDs[i] = lData.uid;
	}
	/*
	static const int lBasePropID = 29688;
	static const int lNumProps = 3;
	int lStartPropID;
	lStartPropID = lBasePropID + (lNumProps * mRunTimes);
	PropertyID lPropIDs[lNumProps] = {lStartPropID , lStartPropID + 1, lStartPropID +  2};
	mRunTimes++;
	*/
	uint64_t lStartDate, lEndDate;
	lStartDate = 0;
	lEndDate = MVTRand::getDateTime(pSession);
	int lNumPINsBelonging = 0;
	int lNumPINsNotBelonging = 0;	
	Tstring lImageStr; MVTRand::getString(lImageStr,10,50,false,false);
	Tstring lRandStr; MVTRand::getString(lRandStr,20,20,false,true);
	Tstring lImageSubStr = lImageStr.substr(0,6);
	Tstring lRandSubStr = lRandStr.substr(0,5);

	ClassID lCLSImageID = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = pSession->createStmt();
		unsigned const char lVar = lQ->addVariable();
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[0]);
		lV[1].set(lImageSubStr.c_str());
		IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_CONTAINS, 2, lV, CASE_INSENSITIVE_OP);		
		lQ->addCondition(lVar,lET1);
		char lB[100];			
		sprintf(lB, "TestFamilies2.testClassInClassImage%s.%d", lImageSubStr.c_str(), mRunTimes);
		RC rc=defineClass(pSession,lB, lQ, &lCLSImageID);
		lET1->destroy();
		lQ->destroy();
		if(RC_OK!=rc){
			mLogger.out() << "ERROR(testClassInClassFamily): Failed to create Class " << lB << std::endl;	
			return false;
		}
	}

	ClassID lCLSBelongID = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = pSession->createStmt();
		lQ = pSession->createStmt();
		unsigned char lVar;
		if(pUseClass)
		{
			ClassSpec lRange[1];
			lRange[0].classID = lCLSImageID;
			lRange[0].nParams = 0;
			lRange[0].params = NULL;
			lVar = lQ->addVariable(lRange,1);
		}
		else
		{
			lVar = lQ->addVariable();
		}

		Value lV[2];		
		lV[0].setVarRef(0,lPropIDs[1]);
		lV[1].set(lRandSubStr.c_str());
		IExprTree *lET = EXPRTREEGEN(pSession)(OP_EQ, 2, lV);		
		lQ->addCondition(lVar,lET);
		char lB[100];			
		sprintf(lB, "TestFamilies2.testClassInClassBelong%s.%d", lRandSubStr.c_str(), mRunTimes);
		RC rc = defineClass(pSession,lB, lQ, &lCLSBelongID);
		lET->destroy();
		lQ->destroy();
		if(RC_OK!=rc){
			mLogger.out() << "ERROR(testClassInClassFamily): Failed to create Class " << lB << std::endl;	
			return false;
		}
	}

	ClassID lCLSNotBelongID = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = pSession->createStmt();
		lQ = pSession->createStmt();
		unsigned char lVar;
		if(pUseClass)
		{
			ClassSpec lRange[1];
			lRange[0].classID = lCLSImageID;
			lRange[0].nParams = 0;
			lRange[0].params = NULL;
			lVar = lQ->addVariable(lRange,1);
		}
		else
		{		
			lVar = lQ->addVariable();
		}
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[1]);
		lV[1].set(lRandSubStr.c_str());
		IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_NE, 2, lV);
		lV[0].set(lET1);
		lV[1].setVarRef(0,lPropIDs[1]);
		IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_EXISTS, 1, &lV[1]);
		lV[1].set(lET2);
		IExprTree *lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV);
		lQ->addCondition(lVar,lET);
		char lB[100];			
		sprintf(lB, "TestFamilies2.testClassInClassNotBelong%s.%d", lRandSubStr.c_str(), mRunTimes);
		RC rc=defineClass(pSession,lB, lQ, &lCLSNotBelongID);
		lET->destroy();
		lQ->destroy();
		if(RC_OK!=rc){
			mLogger.out() << "ERROR(testClassInClassFamily): Failed to create Class " << lB << std::endl;	
			return false;
		}
	}
	// Create the Family with lCLSBelongID's class
	ClassID lCLSIDBelong = STORE_INVALID_CLASSID;
	char lBuf1[200];
	{
		IStmt *lQ = pSession->createStmt();
		lQ = pSession->createStmt();
		ClassSpec lRange[1];
		lRange[0].classID = lCLSBelongID;
		lRange[0].nParams = 0;
		lRange[0].params = NULL;
		unsigned const char lVar = lQ->addVariable(lRange,1);		

		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,lPropIDs[2]);
			lV[1].setParam(0);
			lET = EXPRTREEGEN(pSession)(OP_IN, 2, lV, 0);	
		}
			
		lQ->addCondition(lVar,lET);		
		sprintf(lBuf1, "TestFamilies2.testClassInClassFamily1%s.%d", lRandSubStr.c_str(), mRunTimes);
		if(RC_OK!=defineClass(pSession,lBuf1, lQ, &lCLSIDBelong)){
			mLogger.out() << "ERROR(testClassInClassFamily): Failed to create Class " << lBuf1 << std::endl;	
			return false;
		}
	}

	// Create the Family with lCLSBelongID's class
	ClassID lCLSIDNotBelong = STORE_INVALID_CLASSID;
	char lBuf2[200];
	{
		IStmt *lQ = pSession->createStmt();
		lQ = pSession->createStmt();
		ClassSpec lRange[1];
		lRange[0].classID = lCLSNotBelongID;
		lRange[0].nParams = 0;
		lRange[0].params = NULL;
		unsigned const char lVar = lQ->addVariable(lRange,1);		

		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,lPropIDs[2]);
			lV[1].setParam(0);
			lET = EXPRTREEGEN(pSession)(OP_IN, 2, lV, 0);	
		}
			
		lQ->addCondition(lVar,lET);		
		sprintf(lBuf2, "TestFamilies2.testClassInClassFamily2%s.%d", lRandSubStr.c_str(), mRunTimes);
		if(RC_OK!=defineClass(pSession,lBuf2, lQ, &lCLSIDNotBelong)){
			mLogger.out() << "ERROR(testClassInClassFamily): Failed to create Class " << lBuf2 << std::endl;	
			return false;
		}
	}
	
	mLogger.out() << "Creating " << lNumPINsToCreate << " PINs...";
	for(i = 0; i < lNumPINsToCreate; i++){
		if( i % 100 == 0) mLogger.out() << ".";
		TIMESTAMP ui64 = MVTRand::getDateTime(pSession);
		bool lRand = (int)((float)rand() * lNumPINsToCreate/RAND_MAX) > (int)lNumPINsToCreate/2;		
		Value lV[3];		
		SETVALUE(lV[0],lPropIDs[0], lImageStr.c_str(), OP_SET);
		SETVALUE(lV[1],lPropIDs[1], lRand?lRandSubStr.c_str():lRandStr.c_str(),OP_SET);
		lV[2].setDateTime(ui64); lV[2].setPropID(lPropIDs[2]);
		IPIN *lPIN = pSession->createUncommittedPIN(lV,3,MODE_COPY_VALUES);
		TVERIFY(lPIN!=NULL);
		if (lPIN!=NULL) {
			TVERIFYRC(pSession->commitPINs(&lPIN,1));
			if(ui64 >= lStartDate && ui64 <= lEndDate && lRand) lNumPINsBelonging++;
			if(ui64 >= lStartDate && ui64 <= lEndDate && !lRand) lNumPINsNotBelonging++;
			lPIDs.push_back(lPIN->getPID());
			lPIN->destroy();
		}
	}
	mLogger.out() << std::endl;

	mLogger.out() << "Executing Family " << lBuf1 << " ... ";
	{		
		Value lParam[1];
		Value lParam1[2];
		lParam1[0].setDateTime(lStartDate);				
		lParam1[1].setDateTime(lEndDate);			
		lParam[0].setRange(lParam1);
				
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSIDBelong;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);
		uint64_t lCount = 0;
		lQ->count(lCount);
		lQ->destroy();
		if((int)lCount != lNumPINsBelonging)
		{
			mLogger.out() << " ERROR(testClassInClassFamily): Family did not return expected number of PINs" << std::endl;
			mLogger.out() << " Expected " << lNumPINsBelonging << " Got " << lCount << std::endl;
			lSuccess = false;
		}
	}
	mLogger.out() << std::endl;

	mLogger.out() << "Executing Family " << lBuf2 << " ... ";
	{		
		Value lParam[1];
		Value lParam1[2];
		lParam1[0].setDateTime(lStartDate);				
		lParam1[1].setDateTime(lEndDate);			
		lParam[0].setRange(lParam1);
				
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSIDNotBelong;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);
		uint64_t lCount = 0;
		lQ->count(lCount);			
		lQ->destroy();
		if((int)lCount != lNumPINsNotBelonging)
		{
			mLogger.out() << " ERROR(testClassInClassFamily): Family did not return expected number of PINs" << std::endl;
			mLogger.out() << " Expected " << lNumPINsNotBelonging << " Got " << lCount << std::endl;
			lSuccess = false;	
		}
	}
	mLogger.out() << std::endl;

	// Modify the lPropIDs[2] here
	mLogger.out() << "Modifying " << lNumPINsToCreate << " PINs...";
	for(i = 0; i < lNumPINsToCreate; i++){
		if( i % 100 == 0) mLogger.out() << ".";		
		IPIN *lPIN = pSession->getPIN(lPIDs[i]); 
		TVERIFY(lPIN!=NULL);
		uint64_t ui64 = lPIN->getValue(lPropIDs[2])->ui64;
		bool lRand = strcmp(lPIN->getValue(lPropIDs[1])->str,lRandSubStr.c_str()) == 0;
		Value lV[1];		
		SETVALUE(lV[0],lPropIDs[1], lRand?lRandStr.c_str():lRandSubStr.c_str(),OP_SET);		
		TVERIFYRC(lPIN->modify(lV,1));
		if(ui64 >= lStartDate && ui64 <= lEndDate && lRand)
		{ 
			lNumPINsBelonging--; lNumPINsNotBelonging++;
		}
		if(ui64 >= lStartDate && ui64 <= lEndDate && !lRand)
		{
			lNumPINsNotBelonging--;	lNumPINsBelonging++;
		}
		lPIN->destroy();
	}
	mLogger.out() << std::endl;

	mLogger.out() << "Executing Family " << lBuf1 << " ... ";
	{		
		Value lParam[1];
		Value lParam1[2];
		lParam1[0].setDateTime(lStartDate);				
		lParam1[1].setDateTime(lEndDate);			
		lParam[0].setRange(lParam1);
				
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSIDBelong;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);
		uint64_t lCount = 0;
		lQ->count(lCount);		
		lQ->destroy();
		if((int)lCount != lNumPINsBelonging)
		{
			IStmt *lQ = pSession->createStmt();
			unsigned char lVar = lQ->addVariable();
			IExprTree *lET;
			{
				Value lV[2];		
				lV[0].setVarRef(0,lPropIDs[1]);
				lV[1].set(lRandSubStr.c_str());
				IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_EQ, 2, lV);
				lV[0].setVarRef(0,lPropIDs[2]);
				Value lR[2];
				lR[0].setDateTime(lStartDate);
				lR[1].setDateTime(lEndDate);
				lV[1].setRange(lR);
				IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_IN, 2, lV);
				lV[0].set(lET1);
				lV[1].set(lET2);
				lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV);
			}
			lQ->addCondition(lVar,lET);
			uint64_t lCnt = 0;
			lQ->count(lCnt);
			lET->destroy();
			lQ->destroy();
			if(lCount != lCnt)
			{
				mLogger.out() << " ERROR(testClassInClassFamily): Family did not return expected number of PINs" << std::endl;
				mLogger.out() << " Expected " << lNumPINsBelonging << " Got " << lCount << std::endl;
				lSuccess = false;	
			}
		}
	}
	mLogger.out() << std::endl;

	mLogger.out() << "Executing Family " << lBuf2 << " ... ";
	{		
		Value lParam[1];
		Value lParam1[2];
		lParam1[0].setDateTime(lStartDate);				
		lParam1[1].setDateTime(lEndDate);			
		lParam[0].setRange(lParam1);
				
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSIDNotBelong;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);
		uint64_t lCount = 0;
		lQ->count(lCount);		
		lQ->destroy();
		if((int)lCount != lNumPINsNotBelonging)
		{
			mLogger.out() << " ERROR(testClassInClassFamily): Family did not return expected number of PINs" << std::endl;
			mLogger.out() << " Expected " << lNumPINsNotBelonging << " Got " << lCount << std::endl;
			lSuccess = false;	
		}
	}
	mLogger.out() << std::endl;

	lPIDs.clear();
	return lSuccess;
}
bool TestFamilies2::testFamilyWithOrderBy(ISession *pSession, int pOrderBy)
{
	bool lSuccess = true;
	static const int lNumPINsToCreate = 100;
	static const int lNumProps = 5;
	int lNumExpPINs = 0;
	PropertyID lPropIDs[lNumProps];
	Tstring lPropStr; MVTRand::getString(lPropStr,10,10,false,true);
	int i = 0;
	URIMap lData;
	for (i = 0; i < lNumProps; i++)
	{
		char lB[100];
		sprintf(lB, "TestFamilies2.testFamilyWithOrderBy%s.%d", lPropStr.c_str(), i);
		lData.URI = lB; lData.uid = STORE_INVALID_PROPID; pSession->mapURIs(1, &lData);
		lPropIDs[i] = lData.uid;
	}
	Tstring lImageStr; MVTRand::getString(lImageStr,10,50,false,false);
	Tstring lRandStr; MVTRand::getString(lRandStr,20,20,false,true);	
	
	IStmt *lQ = pSession->createStmt();
	unsigned const char lVar = lQ->addVariable();
	IExprTree *lET;
	{
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[1]);
		lV[1].setParam(0);
		lET = EXPRTREEGEN(pSession)(OP_EQ, 2, lV, 0);
	}	
	lQ->addCondition(lVar,lET);
	char lB[64];
	ClassID lCLSID = STORE_INVALID_CLASSID;
	MVTRand::getString(lPropStr,10,10,false,true);
	sprintf(lB, "TestFamilies2.testFamilyWithOrderBy%s.%d", lPropStr.c_str(), 1);
	if(RC_OK!=defineClass(pSession,lB, lQ, &lCLSID)){
		mLogger.out() << "ERROR(testFamilyWithOrderBy): Failed to create family " << lB << std::endl;	
		return false;
	}

	mLogger.out() << "Creating " << lNumPINsToCreate << " PINs...";
	for(i = 0; i < lNumPINsToCreate; i++){
		if( i % 100 == 0) mLogger.out() << ".";
		TIMESTAMP ui64 = MVTRand::getDateTime(pSession);
		bool lRand = (int)((float)rand() * lNumPINsToCreate/RAND_MAX) > (int)lNumPINsToCreate/2;		
		Value lV[5];
		Tstring lPropStr; MVTRand::getString(lPropStr,10,50,true,false);
		SETVALUE(lV[0],lPropIDs[0], lPropStr.c_str(), OP_SET);
		SETVALUE(lV[1],lPropIDs[1], lRand?lRandStr.c_str():lImageStr.c_str(),OP_SET);
		lV[2].setDateTime(ui64); lV[2].setPropID(lPropIDs[2]);
		SETVALUE(lV[3],lPropIDs[3],i,OP_SET);
		int lSize = (int) ((float)rand() * 20000/RAND_MAX);
		IStream *lStream1 = MVTApp::wrapClientStream(pSession, new MyFamilyStream(lSize > 0? lSize:2000));
		lV[4].set(lStream1);lV[4].setPropID(lPropIDs[4]);lV[4].setMeta(META_PROP_NOFTINDEX |META_PROP_SSTORAGE);

		IPIN *lPIN = pSession->createUncommittedPIN(lV,5,MODE_COPY_VALUES);
		TVERIFYRC(pSession->commitPINs(&lPIN,1));
		if(lRand) lNumExpPINs++;
		lPIN->destroy();
	}
	mLogger.out() << std::endl;
	uint64_t lCount = 0;
	{
		Value lParam[1];
		lParam[0].set(lRandStr.c_str());		
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		if((int)lCount!=lNumExpPINs) {
			mLogger.out() << "ERROR(testFamilyWithOrderBy): Not all PINs were returned" << std::endl;
			lSuccess = false;
		}

		OrderSeg ord = {NULL,0,0,0,0};
		ord.flags = (int)((float)rand() * lNumPINsToCreate/RAND_MAX) > (int)lNumPINsToCreate/2?0:ORD_DESC;		
		bool lOrderAscen = ord.flags == 0;

		if(pOrderBy == 0)
			ord.pid = lPropIDs[0];
		else if(pOrderBy == 1)
			ord.pid = lPropIDs[3];
		else
			ord.pid = lPropIDs[2];

		lQ->setOrder( &ord, 1);
		ICursor *lR = NULL;
		TVERIFYRC(lQ->execute(&lR));
		if(lR)
		{
			char lsPrev[100] = ""; int liPrev = 0; uint64_t luPrev = 0;
			IPIN *lPIN = lR->next();
			strcpy(lsPrev,lPIN->getValue(lPropIDs[0])->str);
			liPrev = lPIN->getValue(lPropIDs[3])->i;
			luPrev = lPIN->getValue(lPropIDs[2])->ui64;
			lPIN->destroy();

			bool fInOrder = true;
			for(lPIN = lR->next();lPIN!=NULL; lPIN=lR->next())
			{
				if(pOrderBy == 0)
				{
					fInOrder = lOrderAscen?strcmp(lsPrev,lPIN->getValue(lPropIDs[0])->str)<=0:strcmp(lsPrev,lPIN->getValue(lPropIDs[0])->str)>=0;
					strcpy(lsPrev,lPIN->getValue(lPropIDs[0])->str);
				}
				else if(pOrderBy == 1)
				{
					fInOrder = lOrderAscen?liPrev<=lPIN->getValue(lPropIDs[3])->i:liPrev>=lPIN->getValue(lPropIDs[3])->i;
					liPrev=lPIN->getValue(lPropIDs[3])->i;
				}
				else
				{
					fInOrder = lOrderAscen?luPrev<=lPIN->getValue(lPropIDs[2])->ui64:luPrev>=lPIN->getValue(lPropIDs[2])->ui64;
					luPrev=lPIN->getValue(lPropIDs[2])->ui64;
				}
				lPIN->destroy();
				if(!fInOrder)
				{
					mLogger.out() << " ERROR(testFamilyWithOrderBy): Incorrect order of pins in the family" << std::endl;
					lSuccess = false;
					break;
				}
			}
			lR->destroy();
		}
		lQ->destroy();
	}
	return lSuccess;
}
bool TestFamilies2::testComplexFamily(ISession *pSession, bool pMakeCollection, bool pMore /*more complex expression*/)
{
	bool lSuccess = true;	
	static const int lNumPINsToCreate = 1000;
	static const int lNumProps = 5;
	PropertyID lPropIDs[lNumProps];
	uint64_t lStartDate, lEndDate;
	std::vector<PID> lPIDList;
	lStartDate = 0; lEndDate = MVTRand::getDateTime(pSession);
	int lExpNumPINs = 0; /* Expected number of pins in query results */
	int i = 0;
	MVTApp::mapURIs(pSession,"TestFamilies2.testComplexFamily.",lNumProps,lPropIDs);
	Tstring lImageStr; MVTRand::getString(lImageStr,10,50,false,false);
	Tstring lRandStr; MVTRand::getString(lRandStr,20,20,false,true);
	Tstring lImageSubStr = lImageStr.substr(0,6);
	Tstring lRandSubStr = lRandStr.substr(0,5);
	mRunTimes++;  // Used to build easy to recognize class names

	ClassID lCLSImageID = STORE_INVALID_CLASSID;
	{
		// pins where prop0 contains a specific string, which is equivalent to the "image"
		// class in MVPhoto
		IStmt *lQ = pSession->createStmt();
		unsigned const char lVar = lQ->addVariable();
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[0]);
		lV[1].set(lImageSubStr.c_str());
		IExprTree *lET = EXPRTREEGEN(pSession)(OP_CONTAINS, 2, lV, CASE_INSENSITIVE_OP);		
		TVERIFYRC(lQ->addCondition(lVar,lET));

		char lB[100];			
		sprintf(lB, "TestFamilies2.testComplexFamilyClass.%s.%d", lImageSubStr.c_str(), 0);
		TVERIFYRC(defineClass(pSession,lB, lQ, &lCLSImageID));
		PrintQuery( pSession, lB, lQ ) ;
		lET->destroy();
		lQ->destroy();
	}

	ClassID lCLSSubImageID = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = pSession->createStmt();
		unsigned char lVar;
		ClassSpec lRange[1];
		lRange[0].classID = lCLSImageID;
		lRange[0].nParams = 0;
		lRange[0].params = NULL;
		lVar = lQ->addVariable(lRange,1);		
	
		// class imagetag() = pin[pin is image()and ((pin has prop1 and prop1 != 'xyz') or !(pin has prop1))]
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[1]);
		IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_EXISTS, 1, lV);
        lV[0].setVarRef(0,lPropIDs[1]);
		lV[1].set(lRandStr.c_str());
		IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_NE, 2, lV);		
		lV[0].set(lET1);
		lV[1].set(lET2);
		IExprTree *lET;
		if(!pMore)
			lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV);
		else
		{
			IExprTree *lET3 = EXPRTREEGEN(pSession)(OP_LAND, 2, lV);

			lV[0].setVarRef(0,lPropIDs[1]);
			IExprTree *lET4 = EXPRTREEGEN(pSession)(OP_EXISTS, 1, lV);
			lV[0].set(lET4);
			IExprTree *lET5 = EXPRTREEGEN(pSession)(OP_LNOT, 1, lV);

			lV[0].set(lET3);
			lV[1].set(lET5);
			lET = EXPRTREEGEN(pSession)(OP_LOR, 2, lV);
		}
		
		TVERIFYRC(lQ->addCondition(lVar,lET));
		char lB[100];			
		sprintf(lB, "TestFamilies2.testComplexFamily.%s.%d", lRandSubStr.c_str(), 1);
		TVERIFYRC(defineClass(pSession,lB, lQ, &lCLSSubImageID));
		PrintQuery( pSession, lB, lQ ) ;
		lET->destroy();
		lQ->destroy();
	}
	
	// Create the Family with lCLSSubImageID's class
	ClassID lFamilyID = STORE_INVALID_CLASSID;
	char lBuf[200];
	{
		// pin is lCLSSubImageID and prop2 IN var1 (range of dates)

		IStmt *lQ = pSession->createStmt();
		ClassSpec lRange[1];
		lRange[0].classID = lCLSSubImageID;
		lRange[0].nParams = 0;
		lRange[0].params = NULL;
		unsigned const char lVar = lQ->addVariable(lRange,1);		

		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,lPropIDs[2]);
			lV[1].setParam(0);
			lET = EXPRTREEGEN(pSession)(OP_IN, 2, lV, 0);	
		}

		lQ->addCondition(lVar,lET);		
		sprintf(lBuf, "TestFamilies2.testComplexFamily.%s.%d", lRandSubStr.c_str(), 2);
		TVERIFYRC(defineClass(pSession,lBuf, lQ, &lFamilyID));
		PrintQuery( pSession, lBuf, lQ ) ;
	}
	
	mLogger.out() << "Creating " << lNumPINsToCreate << " PINs...";
	for(i = 0; i < lNumPINsToCreate; i++){
		if( i % 100 == 0) mLogger.out() << ".";
		
		// Random time that might be in the range 
		TIMESTAMP ui64 = MVTRand::getDateTime(pSession);
		bool bNoMatchOnExcludeString = ( rand() > RAND_MAX / 2 ) ;
		Value lV[10];     		
		SETVALUE(lV[0],lPropIDs[0], lImageStr.c_str(), OP_SET);

		if ( bNoMatchOnExcludeString )
		{
			SETVALUE(lV[1],lPropIDs[1], lRandSubStr.c_str(),OP_SET);	
		}
		else
		{
			/* Put full string in, so that this PIN will drop out of the class (because of prop1 != 'xyz' clause) */
			SETVALUE(lV[1],lPropIDs[1], lRandStr.c_str(),OP_SET);	
		}

		lV[2].setDateTime(ui64); lV[2].setPropID(lPropIDs[2]);		

		/* Use stream to simulate creation of a large binary blob, similar to photo data on a photo */
		IStream *lStream = MVTApp::wrapClientStream(pSession,new MyFamilyStream(1000));
		SETVALUE(lV[3],lPropIDs[3], lStream, OP_SET); lV[3].meta = META_PROP_NOFTINDEX | META_PROP_SSTORAGE;		

		IPIN *lPIN = pSession->createUncommittedPIN(lV,4,MODE_COPY_VALUES);
		TVERIFYRC(pSession->commitPINs(&lPIN,1)); 
		if(ui64 >= lStartDate && ui64 <= lEndDate && bNoMatchOnExcludeString) ++lExpNumPINs;
		lPIDList.push_back(lPIN->getPID());
		lPIN->destroy();
	}
	mLogger.out() << std::endl;
	
	if(pMakeCollection)
	{
		AddKnownTag( pSession, lPIDList, lPropIDs[1], "national geographic" ) ;
		AddKnownTag( pSession, lPIDList, lPropIDs[1], "national" ) ;
		AddKnownTag( pSession, lPIDList, lPropIDs[1], "geographic" ) ;
		AddKnownTag( pSession, lPIDList, lPropIDs[1], "graphics" ) ;
		AddRandomTagCollection( pSession, lPIDList, lPropIDs[1] ) ;
	}
	lPIDList.clear();

	{		
		// Execute the query and verify expected number of matches are found
		Value lParam[1];
		Value lParam1[2];
		lParam1[0].setDateTime(lStartDate);				
		lParam1[1].setDateTime(lEndDate);			
		lParam[0].setRange(lParam1);
				
		ClassSpec lCS;
		lCS.classID = lFamilyID;
		lCS.nParams = 1;
		lCS.params = lParam;

		// Basic query
		mLogger.out() << "Executing Family " << lBuf << " ... ";
		IStmt * lQ = pSession->createStmt();
		unsigned char var = lQ->addVariable(&lCS, 1);
		uint64_t lCount = 0;
		long lBef = getTimeInMs();

		char lFile[100]; sprintf(lFile,"tf2_testComplexFamily%d.txt",mRunTimes);
		TVERIFY( !MVTUtil::findDuplicatePins( lQ,mLogger.out() ) ) ;
		lQ->count(lCount);		
		long lAft = getTimeInMs();

		if((int)lCount != lExpNumPINs)
		{
			std::ostringstream errmsg ;
			errmsg<< " ERROR(testComplexFamily): Family did not return expected number of PINs" << std::endl;
			errmsg<< " Expected " << lExpNumPINs << " Got " << lCount << std::endl;
			TVERIFY2(0,errmsg.str().c_str()) ;
			lSuccess = false ;
		}
		else
		{
			mLogger.out() << "PASSED (" << std::dec << lAft-lBef << " ms, " << std::dec << lCount << " matches )" << std::endl;
		}

		if(pMakeCollection)
		{
			mLogger.out() << "Executing Family with FT " << lBuf << " ... ";

			// same query with FT and different sort order (See 4976). 
			// We added this tag to ALL the pins so we expect 
			// same number are found as before
			lQ->setConditionFT( var, "National" ) ;
			OrderSeg ord = {NULL,lPropIDs[2],ORD_DESC,0,0};
			lQ->setOrder( &ord, 1);


			lBef = getTimeInMs();
			lQ->count(lCount);		
			lAft = getTimeInMs();

			if((int)lCount != lExpNumPINs)
			{
				std::ostringstream errmsg ;
				errmsg<< " ERROR(testComplexFamily): Family with FT Cond did not return expected number of PINs," ;
				errmsg<< " Expected " << lExpNumPINs << " Got " << lCount ;
				TVERIFY2(0,errmsg.str().c_str()) ;

				char lFile[100]; sprintf(lFile,"tf2_testComplexFamily_FT%d.txt",mRunTimes);

				TVERIFY( !MVTUtil::findDuplicatePins( lQ,mLogger.out()));
				lSuccess = false ;
			}
			else
			{
				mLogger.out() << "PASSED (" << std::dec << lAft-lBef << " ms, " << std::dec << lCount << " matches )" << std::endl;
			}
		}

		lQ->destroy();
	}
	mLogger.out() << std::endl;	
	return lSuccess;
}


bool TestFamilies2::testArrayFamily(ISession *pSession)
{
	bool lSuccess = true;
	static const int lNumPINsToCreate = 200;
	
	Tstring lFamilyStr; MVTRand::getString(lFamilyStr,10,10,false,false);		
	ClassID lCLSID = STORE_INVALID_CLASSID;
	char lB[100];
	{
		IStmt *lQ = pSession->createStmt();
		unsigned const char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIds[2]);
			lV[1].setParam(0);
			lET = EXPRTREEGEN(pSession)(OP_EQ, 2, lV, 0);
		}	
		lQ->addCondition(lVar,lET);
		sprintf(lB, "TestFamilies2.testArrayFamily%s.%d",lFamilyStr.c_str(), 0);
		mLogger.out() << "Creating Family " << lB << " ...";
		if(RC_OK!=defineClass(pSession,lB, lQ, &lCLSID)){
			mLogger.out() << "ERROR(testArrayFamily): Failed to create Class " << lB << std::endl;	
			return false;
		}else{
			mLogger.out() << " DONE" << std::endl;
		}
	}
	ClassID lCLSID1 = STORE_INVALID_CLASSID;
	char lB1[100];
	{
		IStmt *lQ = pSession->createStmt();
		unsigned const char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIds[0]);
			lV[1].setParam(0);
			lET = EXPRTREEGEN(pSession)(OP_IN, 2, lV, 0);
		}	
		lQ->addCondition(lVar,lET);
		sprintf(lB1, "TestFamilies2.testArrayFamily%s.%d",lFamilyStr.c_str(), 1);
		mLogger.out() << "Creating Family " << lB1 << " ...";
		if(RC_OK!=defineClass(pSession,lB1, lQ, &lCLSID1)){
			mLogger.out() << "ERROR(testArrayFamily): Failed to create Class " << lB1 << std::endl;	
			return false;
		}else{
			mLogger.out() << " DONE" << std::endl;
		}
	}

	ClassID lCLSID2 = STORE_INVALID_CLASSID;
	char lB2[100];
	{
		IStmt *lQ = pSession->createStmt();
		unsigned const char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIds[1]);
			lV[1].setParam(0);
			lET = EXPRTREEGEN(pSession)(OP_BEGINS, 2, lV, 0);
		}	
		lQ->addCondition(lVar,lET);
		sprintf(lB2, "TestFamilies2.testArrayFamily%s.%d",lFamilyStr.c_str(), 2);
		mLogger.out() << "Creating Family " << lB2 << " ...";
		if(RC_OK!=defineClass(pSession,lB2, lQ, &lCLSID2)){
			mLogger.out() << "ERROR(testArrayFamily): Failed to create Class " << lB2 << std::endl;	
			return false;
		}else{
			mLogger.out() << " DONE" << std::endl;
		}
	}

	if(!createPINs(pSession, lNumPINsToCreate)) return false;	

	// Create the VT_ARRAY and VT_COLLECTION Value structures
	int i = 0, lINT = 0, lSTRING = 0, lUSTR = 0;
	int lExpNumPINsWithINT = mNumPINsWithINT + mBoundaryHistogram;

	Value *lVArrayINT = new Value[lNumPINsToCreate]; 
	for(i = mStartHistogram; i <= mEndHistogram; i++) lVArrayINT[i].set(i);	lINT = i;
	CTestCollNav *lCNAVIGATORINT = new CTestCollNav(lVArrayINT, lINT);

	Value *lVArrayString = new Value[mNumProps];
	for(i = 0; i < (int)mStr.size(); i++) lVArrayString[i].set(mStr[i].c_str()); lSTRING = i;
	CTestCollNav *lCNAVIGATORSTRING = new CTestCollNav(lVArrayString, lSTRING);

	Value *lVArrayUString = new Value[mNumProps];
	for(i = 0; i < (int)mWStr.size(); i++) lVArrayUString[i].set(mWStr[i].c_str()); lUSTR = i;
	CTestCollNav *lCNAVIGATORUSTR = new CTestCollNav(lVArrayUString, lUSTR);

	uint64_t lCount = 0;	
	mLogger.out() << "Executing Family " << lB << " with OP_EQ of T_ARRAY params on VT_INT... ";
	{
		Value lParam[1];
		lParam[0].set(lVArrayINT, lINT);
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		lQ->destroy();	
		TVERIFY((int)lCount == lExpNumPINsWithINT && "ERROR(testArrayFamily): NOT all PINs were returned for VT_ARRAY on VT_INT");		
	}
	mLogger.out() << "DONE" << std::endl;

	mLogger.out() << "Executing Family " << lB1 << " with OP_IN of VT_ARRAY params on VT_STRING... ";
	{
		Value lParam[1];
		lParam[0].set(lVArrayString, lSTRING);
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID1;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		lQ->destroy();	
		TVERIFY((int)lCount == mNumPINsWithStr && "ERROR(testArrayFamily): NOT all PINs were returned for VT_ARRAY on VT_STRING");		
	}
	mLogger.out() << "DONE" << std::endl;

	mLogger.out() << "Executing Family " << lB2 << " with OP_BEGINS of VT_ARRAY params on VT_STRING... ";
	{
		Value lParam[1];
		lParam[0].set(lVArrayUString, lUSTR);
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID2;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		lQ->destroy();	
		TVERIFY((int)lCount == mNumPINsWithUStr && "ERROR(testArrayFamily): NOT all PINs were returned for VT_ARRAY on VT_STRING");		
	}
	mLogger.out() << "DONE" << std::endl;

	mLogger.out() << "Executing Family " << lB << " with OP_EQ of VT_COLLECTION params on VT_INT... ";
	{
		Value lParam[1];
		lParam[0].set(lCNAVIGATORINT);
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		lQ->destroy();	
		TVERIFY((int)lCount == lExpNumPINsWithINT && "ERROR(testArrayFamily): NOT all PINs were returned for VT_COLLECTION on VT_INT");		
	}
	mLogger.out() << "DONE" << std::endl;

	mLogger.out() << "Executing Family " << lB1 << " with OP_IN of VT_COLLECTION params on VT_STRING... ";
	{
		Value lParam[1];
		lParam[0].set(lCNAVIGATORSTRING);
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID1;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		lQ->destroy();	
		TVERIFY((int)lCount == mNumPINsWithStr && "ERROR(testArrayFamily): NOT all PINs were returned for VT_COLLECTION on VT_STRING");		
	}
	mLogger.out() << "DONE" << std::endl;
	
	mLogger.out() << "Executing Family " << lB2 << " with OP_BEGINS of VT_COLLECTION params on VT_STRING... ";
	{
		Value lParam[1];
		lParam[0].set(lCNAVIGATORUSTR);
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lCLSID2;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);		
		lQ->count(lCount);
		lQ->destroy();	
		TVERIFY((int)lCount == mNumPINsWithUStr && "ERROR(testArrayFamily): NOT all PINs were returned for VT_COLLECTION on VT_STRING");		
	}
	mLogger.out() << "DONE" << std::endl;

	delete lVArrayINT;
	delete lVArrayString;
	delete lVArrayUString;
	lCNAVIGATORINT->destroy();
	lCNAVIGATORSTRING->destroy();
	lCNAVIGATORUSTR->destroy();
	return lSuccess;
}
void TestFamilies2::PrintQuery( ISession* S, char * inClassName, IStmt * inQ )
{
#ifdef VERBOSE
	char * strQ = inQ->toString() ;
	mLogger.out() << inClassName << " definition: " << strQ << endl ;
	S->free( strQ ) ;
#endif
}

void TestFamilies2::AddRandomTagCollection( ISession* pSession, const std::vector<PID> & pids, PropertyID prop ) 
{
	// add a collection of random strings to the requested property on each pin.  
	// This approximately simulates the tag attributes on a photo

	size_t cntPins = pids.size() ;

	mLogger.out() << "Adding random tags to property " << prop << " on "<< (unsigned int)cntPins << " PINs...";
	std::vector<Tstring> lStrList; Tstring lStr;
	for(size_t i = 0; i < cntPins; i++)
	{
		if(i%100 == 0) mLogger.out() << ".";
		Value lV[20]; // Up to 20 collection items
		int lNumElements = (int)((float)rand() * 20/RAND_MAX);
		lNumElements = lNumElements == 0?5:lNumElements;
		int j = 0;
		lStrList.resize( lNumElements ) ;

		for(j = 0; j < lNumElements; j++) 
		{
			// REVIEW: By constantly generating new random words we don't have the same
			// situation as real "tags", which would often be repeated on different pins
			MVTRand::getString(lStr,10,10,false/*words*/,false/*keepcase*/);
			lStrList[j] =lStr;
			SETVALUE_C(lV[j],prop, lStrList[j].c_str(),OP_ADD, STORE_LAST_ELEMENT);
		}
		IPIN *lPIN = pSession->getPIN(pids[i]); TVERIFY(lPIN != NULL) ;
		TVERIFYRC(lPIN->modify(lV,lNumElements));
		lPIN->destroy();
	}
	mLogger.out() << std::endl;
}

void TestFamilies2::AddKnownTag( ISession* pSession, const std::vector<PID> & pids, PropertyID prop, const char * inTag ) 
{
	// add a specific string to the property
	// This approximately simulates adding a tag attribute on a photo

	size_t cntPins = pids.size() ;

	for(size_t i = 0; i < cntPins; i++)
	{
		Value val ;
		val.set( inTag ) ; val.property = prop ; val.op = OP_ADD ;

		IPIN *lPIN = pSession->getPIN(pids[i]); TVERIFY(lPIN != NULL) ;
		TVERIFYRC(lPIN->modify(&val,1));
		lPIN->destroy();
	}
}




#if TEST_IMPORT_PERF
bool TestFamilies2::testImageImportPerf(ISession *pSession){
	bool lSuccess = true;
	URIMap lData;
	int i;
	for (i = 0; i < mImageNumProps; i++)
	{
		char lB1[64], lB2[64];
		sprintf(lB1, "prop%d", i);
		sprintf(lB2, "TestFamilies2.imageimportprop%d", i);

		lData.URI = lB2; lData.uid = STORE_INVALID_PROPID; pSession->mapURIs(1, &lData);
		mImagePropIds[i] = lData.uid;
	}
	Tstring lStr, lBaseStr; int lIter = 1000, k;
	lBaseStr = "c:\\snaps\\copy of reptile wins vitality\\Totally weird (";
	for(i =0, k = 0; i < mImageNumStr; i++, k++) {
		lStr = lBaseStr;
		#if !REPRO_BUG_3292
			MVTRand::getString(lStr,15,20,true,false);	
		#else
			if(k < lIter){
				char lB[10];
				sprintf(lB, "%d", k);
				lStr.append(lB);
				lStr.append(").jpg");
			}else{
				k = 0;
				Tstring lTempStr;MVTRand::getString(lTempStr,10,10,false,false);
				lBaseStr = "c:\\snaps\\copy of reptiles\\" + lTempStr + " (";
			}
		#endif		
		mImageStr.push_back(lStr);
		#if USE_MD5
			Md5TestStream lS;unsigned char lM[16];
			lS.write(lStr.c_str(),std::streamsize(lStr.length()));
			lS.flush_md5(lM);
			uint64_t lValue = axtoi(lM);
			mImageMd5.push_back(lValue);
		#endif
	}
	
	IStmt *lQ = pSession->createStmt();
	unsigned const char lVar = lQ->addVariable();
	IExprTree *lET;
	{
		Value lV[2];
		lV[0].setVarRef(0,mImagePropIds[18]);
		lV[1].setParam(0);
		lET = EXPRTREEGEN(pSession)(OP_EQ, 2, lV, 0);
	}	
	lQ->addCondition(lVar,lET);
	char lB[64];
	ClassID lCLSID = STORE_INVALID_CLASSID;
	sprintf(lB, "TestFamilies2.imageimportperf%d", 1);
	if(RC_OK!=defineClass(pSession,lB, lQ, &lCLSID)){
		mLogger.out() << "ERROR (testImageImportPerf): Failed to create Class " << lB << std::endl;	
		lSuccess = false;
	}

	if(lSuccess && !createImagePINs(pSession, mImageNumPINs)) lSuccess = false;

	if(lSuccess){
		long lBef, lAft;
		int lFamilyCnt = 0;
		int lFTCnt = 0;
		int lFSCnt = 0;
		int lStrIndex = (int)((float)mImageNumStr * rand()/ RAND_MAX);
		
		#if USE_MD5
			mLogger.out() << " Property value used : " << mImageMd5[lStrIndex] << std::endl;
		#else
			mLogger.out() << " Property value used : " << mImageStr[lStrIndex].c_str() << std::endl;
		#endif

		// Execute the family
		{
			mLogger.out() << "Executing Family " << lB << " ... ";			
			Value lParam[1];
			int lIndex = (int)((float)mNumProps * rand() / RAND_MAX);
			#if USE_MD5
				lParam[0].setU64(mImageMd5[lStrIndex]);
			#else
				lParam[0].set(mImageStr[lStrIndex].c_str());
			#endif
			ClassSpec lCS;
			IStmt * lQ = pSession->createStmt();
			lCS.classID = lCLSID;
			lCS.nParams = 1;
			lCS.params = lParam;
			lQ->addVariable(&lCS, 1);
			lBef = getTimeInMs();
        		ICursor *lR = NULL;
        		TVERIFYRC(lQ->execute(&lR));
			lAft = getTimeInMs();
			lQ->destroy();
			if(lR){
				mLogger.out() << "\n Time taken to execute family (" << lB << ") = " << lAft-lBef << std::endl;
				mLogger.out() << "PINs part of the family (" << lB << "):" << std::endl;
				IPIN *lPIN = lR->next();
				for(;lPIN!=NULL; lPIN = lR->next(), lFamilyCnt++){
					mLogger.out() << "PIN " << std::hex << LOCALPID(lPIN->getPID()) << std::endl; 
					lPIN->destroy();
				}		
			}
			lR->destroy();
		}
		// Execute FT search
		{
			lBef = lAft = 0;
			IStmt * lQ = pSession->createStmt();
			unsigned char lVar = lQ->addVariable();
			lQ->setConditionFT(lVar, mImageStr[lStrIndex].c_str(), 0, &mImagePropIds[18], 1);
			lBef = getTimeInMs();
        		ICursor *lR = NULL;
        		TVERIFYRC(lQ->execute(&lR));
			lAft = getTimeInMs();
			lQ->destroy();
			if(lR){
				mLogger.out() << "\n Time taken to FT = " << lAft-lBef << std::endl;
				mLogger.out() << "PINs part of the FT search " << std::endl;
				IPIN *lPIN = lR->next();
				for(;lPIN!=NULL; lPIN = lR->next(), lFTCnt++){
					mLogger.out() << "PIN " << std::hex << LOCALPID(lPIN->getPID()) << std::endl; 
					lPIN->destroy();
				}		
			}
			lR->destroy();
		}
	#if CONFIRM_WITH_FULLSCAN
		// Execute Full Scan query
		{
			IStmt *lQ = pSession->createStmt();
			unsigned const char lVar = lQ->addVariable();
			IExprTree *lET;
			{
				Value lV[2];
				lV[0].setVarRef(0,mImagePropIds[18]);
				#if USE_MD5
					lV[1].setU64(mImageMd5[lStrIndex]);
				#else
					lV[1].set(mImageStr[lStrIndex].c_str());
				#endif
				lET = EXPRTREEGEN(pSession)(OP_EQ, 2, lV, 0);
			}	
			lQ->addCondition(lVar,lET);
        		ICursor *lR = NULL;
        		TVERIFYRC(lQ->execute(&lR));
			lQ->destroy();
			lET->destroy();
			if(lR){
				mLogger.out() << "PINs part of the FullScan search " << std::endl;
				IPIN *lPIN = lR->next();
				for(;lPIN!=NULL; lPIN = lR->next(), lFSCnt++){
					mLogger.out() << "PIN " << std::hex << LOCALPID(lPIN->getPID()) << std::endl; 
					lPIN->destroy();
				}		
			}
			lR->destroy();

		}
	#endif

		if(lFamilyCnt != lFTCnt || lFamilyCnt != lFSCnt) lSuccess = false;
	}
	return lSuccess;
}
bool TestFamilies2::createImagePINs(ISession *pSession, const int pNumPINs, uint64_t pTime){
	bool lSuccess = true;
	bool lCluster = true;
	std::vector<IPIN *> lClusterPINs;
	int lClusterSize = 125;//(int) pNumPINs/10 ;	

	IPIN * lPIN = NULL;
	Tstring lNullCheck; bool fNull = false;
	if(mImageStr.size() == 0) 
	{
		MVTRand::getString(lNullCheck,30,30,true,false);
		fNull = true;
	}
	Value lPVs[mImageNumProps+2];
	mLogger.out() << "Creating " << pNumPINs << " pins ";
	int i, k;
	for(i = 0, k = 0;i < pNumPINs && lSuccess; i++, k++){
		if (0 == i % 100)
			mLogger.out() << ".";		
		//lPIN = pSession->createUncommittedPIN(0,0,MODE_COPY_VALUES);

				// 'fs_path_index' property
		#if USE_LESS_STRINGS
			int lStrIndex = (int)((float)mImageNumStr * rand()/RAND_MAX);
			lStrIndex = lStrIndex == mImageNumStr?(int)mImageNumStr/2:lStrIndex;
		#else
			int lStrIndex = i;
		#endif
		if(fNull) mImageStr.push_back(lNullCheck);
		#if USE_MD5
			lPVs[0].setU64(mImageMd5[lStrIndex]);lPVs[0].setPropID(mImagePropIds[18]);//,(uint64_t),OP_SET);
		#else
			SETVALUE(lPVs[0],mImagePropIds[18],mImageStr[lStrIndex].c_str(),OP_SET);
		#endif

		// 'mime' property		
		SETVALUE(lPVs[1],mImagePropIds[1],"image/jpeg",OP_SET);lPVs[1].setMeta(META_PROP_NOFTINDEX);

		// 'name' property
		Tstring lStr;MVTRand::getString(lStr,15,50,false,false);
		SETVALUE(lPVs[2],mImagePropIds[0],lStr.c_str(),OP_SET);

		// 'lastmodified' property
		lPVs[3].setNow();lPVs[3].setPropID(mImagePropIds[6]);

		// 'created' property
		lPVs[4].setNow();lPVs[4].setPropID(mImagePropIds[7]);

		// 'binary' property
		int lSize = (int) ((float)1000 * rand()/RAND_MAX);
		IStream *lStream1 = MVTApp::wrapClientStream(pSession, new MyFamilyStream(lSize > 0? lSize:80));
		lPVs[5].set(lStream1);lPVs[5].setPropID(mImagePropIds[1]);lPVs[5].setMeta(META_PROP_NOFTINDEX |META_PROP_SSTORAGE);

		// 'date' property
		if(pTime!=0)
		{
			int lRand = (int)((float)100 * rand()/RAND_MAX);
			bool fRand = (int)((float)100 * rand()/RAND_MAX) > 50;
			if(fRand)
				lPVs[6].setDateTime(pTime+lRand);
			else
				lPVs[6].setDateTime(pTime-lRand);				
		}
		else
			lPVs[6].setNow();
		lPVs[6].setPropID(mImagePropIds[5]);

		// 'exif_mode1' property
		SETVALUE(lPVs[7],mImagePropIds[8],"canon",OP_SET);

		// 'exif_fnumber' property
		SETVALUE(lPVs[8],mImagePropIds[9],float(1.0),OP_SET);

		// 'exif_picturetaken' property
		lPVs[9].setNow();lPVs[9].setPropID(mImagePropIds[10]);

		// 'exif_exposuretime' proeprty
		SETVALUE(lPVs[10],mImagePropIds[11],float(0.0),OP_SET);

		// 'exif_isospeedratings' property
		SETVALUE(lPVs[11],mImagePropIds[12],0,OP_SET);

		// 'exif_flash' property
		SETVALUE(lPVs[12],mImagePropIds[13],0,OP_SET);

		// 'width' property
		SETVALUE(lPVs[13],mImagePropIds[14],0,OP_SET);

		// 'height' property
		SETVALUE(lPVs[14],mImagePropIds[15],0,OP_SET);

		// 'preview' property
		lSize = (int) ((float)1000 * rand()/RAND_MAX);
		IStream *lStream2 = MVTApp::wrapClientStream(pSession, new MyFamilyStream(lSize > 0? lSize:40));
		lPVs[15].set(lStream2);lPVs[15].setPropID(mImagePropIds[16]);lPVs[15].setMeta(META_PROP_NOFTINDEX|META_PROP_SSTORAGE);

		// 'exif_make' property
		SETVALUE(lPVs[16],mImagePropIds[17],"",OP_SET);

		// 'refresh-node-id' property
		SETVALUE(lPVs[17],mImagePropIds[2],"007-pinode.mvstore.org",OP_SET);lPVs[17].setMeta(META_PROP_NOFTINDEX);

		// 'exif_width' property
		SETVALUE(lPVs[18],mImagePropIds[19],665,OP_SET);

		// 'exif_height' property
		SETVALUE(lPVs[19],mImagePropIds[20],480,OP_SET);
		
		// 'fs_path' property
		MVTRand::getString(lStr,15,200,false,false);
		SETVALUE(lPVs[20],mImagePropIds[4],lStr.c_str(),OP_SET);		

		// 'PROP_SPEC_ACL' property
		SETVALUE(lPVs[21],PROP_SPEC_ACL,1,OP_SET);

		// 'PROP_SPEC_CREATED' property
		SETVALUE(lPVs[22],PROP_SPEC_CREATED,1,OP_SET);

	/*	if(RC_OK != lPIN->modify(lPVs,mImageNumProps+2)){
			mLogger.out() << "ERROR (TestFamilies2::createImagePINs): Failed to modify uncommitted pin " << std::endl;
			lSuccess = false;
		}
		*/
		lPIN = pSession->createUncommittedPIN(lPVs,mImageNumProps+2,MODE_COPY_VALUES);
		if(lCluster){
			lClusterPINs.push_back(lPIN);
		}else{
			RC lRC = RC_OK;
			if(RC_OK != (lRC = pSession->commitPINs(&lPIN,1))){
				mLogger.out() << "ERROR (TestFamilies2::createImagePINs): Failed to commit the pin. RC returned = " << lRC << std::endl;
				lSuccess = false;
			}else{
				mImagePIDs.push_back(lPIN->getPID());
				lPIN->destroy();
			}
		}
		if(lCluster && k == lClusterSize){
			if(RC_OK != pSession->commitPINs(&lClusterPINs[0],lClusterSize)){
				mLogger.out() << "ERROR (TestFamilies2::createPINs): Failed to commit cluster of pins " << std::endl;
				lSuccess = false;
			}
			else
			{
				for(k = 0; k < lClusterSize; k++){
					mPIDs.push_back(lClusterPINs[k]->getPID());
					lClusterPINs[k]->destroy();
				}
				k = 0;
				lClusterPINs.clear();
			}
		}
	}
	if(lSuccess) mLogger.out() << " DONE" << std::endl;
	return lSuccess;
}  
uint64_t TestFamilies2::axtoi(unsigned char pChar[16]) 
{
  uint64_t lResult = 0;
  int i = 0;
  while (i < 4) {
	 lResult*=256;
	 lResult+=pChar[i];
	 i++;
  }
  return lResult;
}

bool TestFamilies2::testCountPerfOnFamily(ISession *pSession, bool pCollWithCommit)
{
	bool lSuccess = true;	
	static const int lNumPINsToCreate = 5000;
	static const int lNumProps = 5;
	PropertyID lPropIDs[lNumProps];
	uint64_t lStartDate, lEndDate;
	std::vector<PID> lPIDList;
	std::vector<PID> lFamilyPIDs;
	lStartDate = 0; getTimestamp(lEndDate);
	//lEndDate = MVTRand::getDateTime(pSession);
	int lExpNumPINs = 0;
	int i = 0;
	MVTApp::mapURIs(pSession,"TestFamilies2.testCountPerfOnFamily.",lNumProps,lPropIDs);
	Tstring lImageStr; MVTRand::getString(lImageStr,10,50,false,false);
	Tstring lRandStr; MVTRand::getString(lRandStr,20,20,false,true);
	Tstring lImageSubStr = lImageStr.substr(0,6);
	Tstring lRandSubStr = lRandStr.substr(0,5);
	
	//mLogger.out() << "Tag being looked for " << lRandStr.c_str() << std::endl;
	ClassID lCLSImageID = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = pSession->createStmt();
		unsigned const char lVar = lQ->addVariable();
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[0]);
		lV[1].set(lImageSubStr.c_str());
		IExprTree *lET = EXPRTREEGEN(pSession)(OP_CONTAINS, 2, lV, CASE_INSENSITIVE_OP);		
		lQ->addCondition(lVar,lET);
		char lB[100];			
		sprintf(lB, "TestFamilies2.testCountPerfImage.%s.%d", lImageSubStr.c_str(), 0);
		RC rc=defineClass(pSession,lB, lQ, &lCLSImageID);
		lET->destroy();
		lQ->destroy();
		if(RC_OK!=rc){
			mLogger.out() << "ERROR(testCountPerfOnFamily): Failed to create Class " << lB << std::endl;	
			return false;
		}
	}

	ClassID lCLSSubImageID = STORE_INVALID_CLASSID;
	{
		IStmt *lQ = pSession->createStmt();
		lQ = pSession->createStmt();
		unsigned char lVar;
		ClassSpec lRange[1];
		lRange[0].classID = lCLSImageID;
		lRange[0].nParams = 0;
		lRange[0].params = NULL;
		lVar = lQ->addVariable(lRange,1);		
		
		// class imagetag() = pin[pin is image()and ((pin has prop1 and prop1 != 'xyz') or !(pin has prop1))]
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[1]);
		IExprTree *lET1 = EXPRTREEGEN(pSession)(OP_EXISTS, 1, lV);
        lV[0].setVarRef(0,lPropIDs[1]);
		lV[1].set(lRandStr.c_str());
		IExprTree *lET2 = EXPRTREEGEN(pSession)(OP_NE, 2, lV);		
		lV[0].set(lET1);
		lV[1].set(lET2);
		//IExprTree *lET = EXPRTREEGEN(pSession)(OP_LAND, 2, lV);
		
		IExprTree *lET3 = EXPRTREEGEN(pSession)(OP_LAND, 2, lV);

		lV[0].setVarRef(0,lPropIDs[1]);
		IExprTree *lET4 = EXPRTREEGEN(pSession)(OP_EXISTS, 1, lV);
        lV[0].set(lET4);
		IExprTree *lET5 = EXPRTREEGEN(pSession)(OP_LNOT, 1, lV);

		lV[0].set(lET3);
		lV[1].set(lET5);
		IExprTree *lET = EXPRTREEGEN(pSession)(OP_LOR, 2, lV);
		
		lQ->addCondition(lVar,lET);
		char lB[100];			
		sprintf(lB, "TestFamilies2.testCountPerfOnFamily.%s.%d", lRandSubStr.c_str(), 1);
		RC rc=defineClass(pSession,lB, lQ, &lCLSSubImageID);
		lET->destroy();
		lQ->destroy();
		if(RC_OK!=rc){
			mLogger.out() << "ERROR(testCountPerfOnFamily): Failed to create Class " << lB << std::endl;	
			return false;
		}
	}
	
	// Create the Family with lCLSSubImageID's class
	ClassID lFamilyID = STORE_INVALID_CLASSID;
	char lBuf[200];
	{
		IStmt *lQ = pSession->createStmt();
		lQ = pSession->createStmt();
		ClassSpec lRange[1];
		lRange[0].classID = lCLSSubImageID;
		lRange[0].nParams = 0;
		lRange[0].params = NULL;
		unsigned const char lVar = lQ->addVariable(lRange,1);		

		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,lPropIDs[2]);
			lV[1].setParam(0);
			lET = EXPRTREEGEN(pSession)(OP_IN, 2, lV, 0);	
		}

		lQ->addCondition(lVar,lET);		
		sprintf(lBuf, "TestFamilies2.testCountPerfOnFamily.%s.%d", lRandSubStr.c_str(), 2);
		if(RC_OK!=defineClass(pSession,lBuf, lQ, &lFamilyID)){
			mLogger.out() << "ERROR(testCountPerfOnFamily): Failed to create Class " << lBuf << std::endl;	
			return false;
		}
	}
	
	mLogger.out() << "Creating " << lNumPINsToCreate << " PINs ";
	if(pCollWithCommit) mLogger.out() << "along with a collection ";
	mLogger.out() << "...";
	for(i = 0; i < lNumPINsToCreate; i++){
		if( i % 100 == 0) mLogger.out() << ".";
		TIMESTAMP ui64 = MVTRand::getDateTime(pSession);
		bool lRand = (int)((float)rand() * lNumPINsToCreate/RAND_MAX) > (int)lNumPINsToCreate/2;
		Value lV[25]; int lIndex = 0;   		
		SETVALUE(lV[lIndex],lPropIDs[0], lImageStr.c_str(), OP_SET);lIndex++;
		SETVALUE(lV[lIndex],lPropIDs[1], lRand?lRandSubStr.c_str():lRandStr.c_str(),OP_SET);lIndex++;
		lV[lIndex].setDateTime(ui64); lV[lIndex].setPropID(lPropIDs[2]);lIndex++;
		SETVALUE(lV[lIndex],lPropIDs[lIndex], "Life is a bitch, FUCK it!", OP_SET);lIndex++;
		std::vector<Tstring> lStrList; Tstring lStr;
		if(pCollWithCommit)
		{
			int lNumElements = (int)((float)rand() * 20/RAND_MAX);
			lNumElements = lNumElements == 0?5:lNumElements;
			int j = 0;			
			for(j = 0; j < lNumElements; j++) 
			{
				MVTRand::getString(lStr,10,10,false,false);
				lStrList.push_back(lStr);
			}
			for(j = 0; j < lNumElements; j++,lIndex++)
			{			
				SETVALUE_C(lV[lIndex],lPropIDs[1], lStrList[j].c_str(),OP_ADD, STORE_LAST_ELEMENT);
			}
		}
		IPIN *lPIN = pSession->createUncommittedPIN(lV,lIndex,MODE_COPY_VALUES);
		TVERIFYRC(pSession->commitPINs(&lPIN,1));
		if(ui64 >= lStartDate && ui64 <= lEndDate && lRand) ++lExpNumPINs;
		lPIDList.push_back(lPIN->getPID());
		lPIN->destroy();
		lStrList.clear();
	}
	mLogger.out() << std::endl;

	if(!pCollWithCommit)
	{
		mLogger.out() << "Making Property " << lPropIDs[1] << " a collection for "<< lNumPINsToCreate << " PINs...";
		for(i = 0; i < lNumPINsToCreate; i++)
		{
			if(i%100 == 0) mLogger.out() << ".";
			Value lV[20];
			int lNumElements = (int)((float)rand() * 20/RAND_MAX);
			lNumElements = lNumElements == 0?5:lNumElements;
			int j = 0;
			std::vector<Tstring> lStrList; Tstring lStr;
			for(j = 0; j < lNumElements; j++) 
			{
				MVTRand::getString(lStr,10,10,false,false);
				lStrList.push_back(lStr);
			}
			for(j = 0; j < lNumElements; j++)
			{			
				SETVALUE_C(lV[j],lPropIDs[1], lStrList[j].c_str(),OP_ADD, STORE_LAST_ELEMENT);
			}
			IPIN *lPIN = pSession->getPIN(lPIDList[i]); TVERIFY(lPIN!=NULL);
			if (lPIN!=NULL) {
				TVERIFYRC(lPIN->modify(lV,lNumElements));
				lStrList.clear();
				lPIN->destroy();
			}
		}
		mLogger.out() << std::endl;	
	}

	mLogger.out() << "Executing Family " << lBuf << " ... ";
	{		
		Value lParam[1];
		Value lParam1[2];
		lParam1[0].setDateTime(lStartDate);				
		lParam1[1].setDateTime(lEndDate);			
		lParam[0].setRange(lParam1);
				
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lFamilyID;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);
		uint64_t lCount = 0;
		long lBef = getTimeInMs();
		lQ->count(lCount);		
		long lAft = getTimeInMs();
		mLogger.out() << std::endl << "Time taken to count " << lCount << " PINs -- " << std::dec << lAft-lBef << "ms";
		lQ->destroy();
	}
	mLogger.out() << std::endl;	
	/*
	static const int sSize = 1000;
	mLogger.out() << "Adding stream of size " << sSize << " to every "<< lNumPINsToCreate << " PINs...";
	for(i = 0; i < lNumPINsToCreate; i++)
	{
		if(i%100 == 0) mLogger.out() << ".";
		Value lV[1];
		IStream *lStream = MVTApp::wrapClientStream(pSession,new MyFamilyStream(sSize));
		SETVALUE(lV[0],lPropIDs[3], lStream, OP_SET); lV[0].meta = META_PROP_NOFTINDEX | META_PROP_SSTORAGE;		
		IPIN *lPIN = pSession->getPIN(lPIDList[i]);
		if (lPIN!=NULL) {
			TVERIFYRC(lPIN->modify(lV,1));
			lPIN->destroy();
		}
	}
	mLogger.out() << std::endl;	

	static const int sNumThreads = 2;
	HTHREAD lThreads[sNumThreads];
	
	RunFamilyThreadInfo lInfo1 = {lStartDate, lEndDate, lFamilyID, lExpNumPINs, lPropIDs[3], mStoreCtx};
	for(i = 0; i < sNumThreads - 1; i++)
		createThread(&TestFamilies2::RunFamilyAync, &lInfo1, lThreads[i]);

	GetStreamThreadInfo lInfo2 = {lPIDList,lPropIDs[3],mStoreCtx};
	createThread(&TestFamilies2::getStreamAync, &lInfo2, lThreads[sNumThreads-1]);

	MVTestsPortability::threadSleep(200);
	mLogger.out() << "Executing Family " << lBuf << " ... ";
	{		
		Value lParam[1];
		Value lParam1[2];
		lParam1[0].setDateTime(lStartDate);				
		lParam1[1].setDateTime(lEndDate);			
		lParam[0].setRange(lParam1);
				
		ClassSpec lCS;
		IStmt * lQ = pSession->createStmt();
		lCS.classID = lFamilyID;
		lCS.nParams = 1;
		lCS.params = lParam;
		lQ->addVariable(&lCS, 1);
		uint64_t lCount = 0;
		long lBef = getTimeInMs();
		lQ->count(lCount);		
		long lAft = getTimeInMs();
		mLogger.out() << std::endl << "Time taken to count " << lCount << " PINs -- " << std::dec << lAft-lBef << "ms" << std::endl;
		lQ->destroy();
	}
	
	MVTestsPortability::threadsWaitFor(sNumThreads, lThreads);
	*/
	lPIDList.clear();
	return lSuccess;
}
#endif
