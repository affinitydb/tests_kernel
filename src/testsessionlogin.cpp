/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;
class CSessionInfo;
// Publish this test.
class TestSessionLogin : public ITest
{
	public:
		// sNumIdentities > 3
		static int const sNumIdentities = 5;
		std::vector<CSessionInfo *> lSIs;		
		std::vector<Tstring> mIdentitiesStr;
		std::vector<IdentityID> mIdentities;
		std::vector<Tstring> mPwd;
		std::vector<Tstring> mCert;
		std::vector<bool> mMayInsert;
		URIMap lPM[4];		
		std::vector<PID> mPID;
		MVStoreKernel::StoreCtx *mStoreCtx;

		TEST_DECLARE(TestSessionLogin);
		virtual char const * getName() const { return "testsessionlogin"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test's session authentication"; }
		virtual bool includeInPerfTest() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
		MVStoreKernel::Mutex mLock;
		long volatile mFinalResult;
		TestSessionLogin() : mFinalResult(0) {}
	public:
		void createIdentities(int sNumIdentities,long volatile &lStop);
		bool testsessionlogin(int pIdentityIndex,int sNumIdentities);
		bool runSampleQuery(ISession *lSession,int pIndex,int sNumIdentities);
		bool insertPIN(ISession *pSession, bool pMayInsert);
		bool changePwdCert();
		void postRun();
};
TEST_IMPLEMENT(TestSessionLogin, TestLogger::kDStdOut);

class CSessionInfo
{
	public:
		TestSessionLogin & mContext;
		int const mIdentityIndex;
		long volatile * const mStop;
		int mNumIdentities;
		ISession * mSession;
		CSessionInfo(TestSessionLogin & pContext, int pIdentityIndex,long volatile * pStop,int pNumIdentities)
			: mContext(pContext), mIdentityIndex(pIdentityIndex),mStop(pStop),mNumIdentities(pNumIdentities){}
		~CSessionInfo()
		{
			if (mSession){
				mSession->terminate();
				mSession = NULL;
			}
		}
		void onStart()
		{
			mSession = MVTApp::startSession(mContext.mStoreCtx, mContext.mIdentitiesStr[mIdentityIndex].c_str(),mContext.mPwd[mIdentityIndex].c_str());
			assert(NULL != mSession);
		}
		void onFalseStart()
		{
			int pIndex = mIdentityIndex + 1;
			if(mContext.mIdentitiesStr[pIndex].c_str() == NULL) pIndex = 0;
			if(mSession == NULL){
				mSession = MVTApp::startSession(mContext.mStoreCtx, mContext.mIdentitiesStr[mIdentityIndex].c_str(),mContext.mPwd[pIndex].c_str());
			}
		}
};

THREAD_SIGNATURE threadTestSessionLogin(void * pSI)
{
	bool fakepwd = (int)(10 * rand()/RAND_MAX) > 5?true:false;
	int count = 0;
	CSessionInfo * const lSI = (CSessionInfo *)pSI;	
	while (!*lSI->mStop)
	{		
		lSI->onStart();
		lSI->mContext.mLock.lock();
		if(lSI->mSession == NULL || !lSI->mContext.runSampleQuery(lSI->mSession,lSI->mIdentityIndex,lSI->mNumIdentities) || !lSI->mContext.insertPIN(lSI->mSession,lSI->mContext.mMayInsert[lSI->mIdentityIndex])){
			lSI->mContext.getLogger().out() << "Failed to get the session/insert/query failed for "<< lSI->mContext.mIdentitiesStr[lSI->mIdentityIndex].c_str()<<std::endl;
			INTERLOCKEDI(&lSI->mContext.mFinalResult);
			INTERLOCKEDI(lSI->mStop); lSI->mContext.mLock.unlock(); continue;		
		}		
		lSI->mContext.mLock.unlock();
		MVStoreKernel::threadSleep(10);			

		if(lSI->mSession != NULL) {lSI->mSession->terminate(); lSI->mSession = NULL;}
		
		bool impersonate = (int)(100.0 * rand()/RAND_MAX)>50?true:false;
		if(impersonate && lSI->mNumIdentities > 0){
			ISession * const session =	MVTApp::startSession(lSI->mContext.mStoreCtx);	
			if (session) {
				TVRC_R(session->impersonate(lSI->mContext.mIdentitiesStr[lSI->mIdentityIndex].c_str()), &lSI->mContext);
				if(!lSI->mContext.runSampleQuery(session,lSI->mIdentityIndex,0)){
					lSI->mContext.getLogger().out() << "Failed to impersonate the session for "<< lSI->mContext.mIdentitiesStr[lSI->mIdentityIndex].c_str()<<std::endl;
					INTERLOCKEDI(&lSI->mContext.mFinalResult);
					INTERLOCKEDI(lSI->mStop);
					lSI->mSession->terminate(); continue;
				}else{
					session->terminate();
				}
			}else{
				lSI->mContext.getLogger().out() << "Couldn't get a session!"<<std::endl;
			}
		}

		if(fakepwd){
			lSI->onFalseStart();
			lSI->mContext.mLock.lock();
			if(lSI->mSession != NULL) {
				lSI->mContext.getLogger().out() << "Returned a session for invalid password for "<< lSI->mContext.mIdentitiesStr[lSI->mIdentityIndex].c_str()<<std::endl;
				INTERLOCKEDI(&lSI->mContext.mFinalResult);
				INTERLOCKEDI(lSI->mStop);
				lSI->mSession->terminate(); lSI->mContext.mLock.unlock(); continue;
			}
			lSI->mContext.mLock.unlock();
		}

		if(count++ == 10) INTERLOCKEDI(lSI->mStop);
	}
	return 0;
}

int	TestSessionLogin::execute()
{
	bool lSuccess =	true;
	long volatile lStop = 0;
	
	if (MVTApp::startStore()){
		createIdentities(sNumIdentities,lStop);
	}else{
		return 1;
	}
	int i=0;
	mStoreCtx = MVTApp::getStoreCtx();

	// Concurrent Sessions
	HTHREAD lThreads[sNumIdentities];
	for (i = 0; i < sNumIdentities; i++)
			createThread(&threadTestSessionLogin, lSIs[i], lThreads[i]);

	MVStoreKernel::threadsWaitFor(sNumIdentities, lThreads);	
//#if 0

	if(changePwdCert()) {
		for (i = 0; i < sNumIdentities; i++)
			createThread(&threadTestSessionLogin, lSIs[i], lThreads[i]);
	}else{
		mLogger.out() << "Failed to change Password/Certificate/Mayinsert " << std::endl;
		lSuccess = false;
	}

	MVStoreKernel::threadsWaitFor(sNumIdentities, lThreads);
//#endif
	// Terminate the sessions, before stopping the store.
	for (i = 0; i < (int)lSIs.size(); i++)
		delete lSIs[i];
	lSIs.clear();	

	// Serial sessions
	if(mFinalResult == 0) {
		for (i=0;i<sNumIdentities;i++)	
			if(!testsessionlogin(i,sNumIdentities)) {
				lSuccess = false; 
				INTERLOCKEDI(&mFinalResult);
				INTERLOCKEDI(&lStop);
				break;
			}	
	}
	else{
		lSuccess = false;
	}

	//For running the test multiple times
	postRun();

	mIdentitiesStr.clear();
	mIdentities.clear();
	mPwd.clear();
	mCert.clear();
	mMayInsert.clear();
	mPID.clear();

	MVTApp::stopStore();
	return lSuccess	? 0	: 1;
}

bool TestSessionLogin::testsessionlogin(int pIndex,int sNumIdentities)
{
	bool lSuccess = true;
	//if (MVTApp::startStore())
	//{

		ISession *lSession = MVTApp::startSession(mStoreCtx, mIdentitiesStr[pIndex].c_str(),mPwd[pIndex].c_str());
		if(lSession == NULL || !runSampleQuery(lSession,pIndex,sNumIdentities) || !insertPIN(lSession, mMayInsert[pIndex])) {lSuccess = false;}
		else{
			lSession->terminate();
			lSession = NULL;
		}
		if(lSession != NULL){lSession->terminate();}
		//MVTApp::stopStore();
	//}
	return lSuccess;
}

bool TestSessionLogin::runSampleQuery(ISession *lSession,int pIndex,int sNumIdentities)
{
	bool lSuccess = true;

	IStmt * lQ = lSession->createStmt();			
	unsigned char const var = lQ->addVariable();
	lQ->setConditionFT(var,"login");
	ICursor * lR = NULL;
	TVERIFYRC(lQ->execute(&lR));
	IPIN *pin = lR->next();
	int count = 0;
	while(pin != NULL){
		pin->destroy();	pin = lR->next();count++;
	}
	if(sNumIdentities > 0 && (((pIndex == 0 || pIndex == (int)(sNumIdentities/2)) && count == 0) || (count == 0 && lR == NULL)))
		lSuccess = false;	
	lR->destroy();
	lQ->destroy();
	
	return lSuccess;
}

void TestSessionLogin::createIdentities(int sNumIdentities,long volatile &lStop)
{
	ISession * const lSession =	MVTApp::startSession();	
	int i;
	/*
	lPM[0].URI="testsessionlogin.IdentityPwd";lPM[0].displayName="IdentityPwd";
	lPM[1].URI="testsessionlogin.IdentityID";lPM[1].displayName="IdentityID";
	lPM[2].URI="testsessionlogin.Certificate";lPM[2].displayName="Certificate";
	lPM[3].URI="testsessionlogin.MayInsert";lPM[3].displayName="MayInsert";
	lSession->mapURIs(4,lPM);
	*/
	MVTApp::mapURIs(lSession,"TestSessionLogin.prop",4,lPM);
	for (i = 0; i < sNumIdentities; i++)
	{
		char lIdentity[255];
		Tstring lPwd,lCert,lRandStr;
		MVTRand::getString(lRandStr,10,10,false,false);
		sprintf(lIdentity, "testsessionlogin.%s.identity%d", lRandStr.c_str(), i);
		IdentityID const lTempIID = lSession->getIdentityID(lIdentity);
		if(lTempIID != STORE_INVALID_IDENTITY) 
		// Doing this for running the test multiple times without failure
		{
			IStmt *lQ = lSession->createStmt();
			unsigned char lVar = lQ->addVariable();
			Value lVal[2];
			lVal[0].setVarRef(0,lPM[1].uid);
			lVal[1].set((unsigned int)lTempIID);
			IExprTree *lET = lSession->expr(OP_EQ,2,lVal);
			lQ->addCondition(lVar,lET);
			ICursor * lR = NULL;
			TVERIFYRC(lQ->execute(&lR));
			IPIN *lPIN = lR->next();
			mPID.push_back(lPIN->getPID());
			lPwd = lPIN->getValue(lPM[0].uid)->str;
			lCert = lPIN->getValue(lPM[2].uid)->str;
			bool lMayInsert = lPIN->getValue(lPM[3].uid)->b;
			
			lPIN->destroy();
			lR->destroy();
			lQ->destroy();
			lET->destroy();

			mIdentities.push_back(lTempIID);
			mPwd.push_back(lPwd);
			mCert.push_back(lCert);
			mMayInsert.push_back(lMayInsert);
		}else{
			MVTRand::getString(lPwd, (int)(100.0 * rand() /RAND_MAX), (int)(150.0 * rand() /RAND_MAX), false);
			MVTRand::getString(lCert, (int)(100.0 * rand() /RAND_MAX), (int)(150.0 * rand() /RAND_MAX), false);
			int lCertLen = (int) lCert.length();
			bool lMayInsert = (int)1000.0 * rand()/RAND_MAX > 500;
			IdentityID const lIID = lSession->storeIdentity(lIdentity, lPwd.c_str(), lMayInsert, (const unsigned char *) lCert.c_str(), lCertLen);
			{
				Value lVal[4]; PID lPID;
				SETVALUE(lVal[0], lPM[0].uid, lPwd.c_str(), OP_SET);
				SETVALUE(lVal[1], lPM[1].uid, (unsigned int)lIID, OP_SET);
				SETVALUE(lVal[2], lPM[2].uid, lCert.c_str(), OP_SET);
				SETVALUE(lVal[3], lPM[3].uid, lMayInsert, OP_SET);
				lSession->createPIN(lPID,lVal,4);
				mPID.push_back(lPID);
			}
			mPwd.push_back(lPwd);
			mCert.push_back(lCert);
			mMayInsert.push_back(lMayInsert);	
			mIdentities.push_back(lIID);				
		}
		mIdentitiesStr.push_back(lIdentity);
		lSIs.push_back(new CSessionInfo(*this, i,&lStop,sNumIdentities));
	}		
	
	// Create a pin for runSamplequery()
	URIMap pmap[2];Value pvs[3];PID pid;
	pmap[0].URI="testsessionlogin.VT_STRING";
	lSession->mapURIs(1,pmap);		
	SETVALUE(pvs[0], pmap[0].uid, "login test", OP_SET);
	RC rc = lSession->createPIN(pid,pvs,1);
	
	SETVALUE(pvs[0], pmap[0].uid, "session login test2", OP_SET);
	pvs[1].setIdentity(mIdentities[(int)(sNumIdentities/2)]);
	SETVATTR_C(pvs[1], PROP_SPEC_ACL, OP_ADD, STORE_LAST_ELEMENT);
	pvs[1].meta = ACL_READ | ACL_WRITE;
	pvs[2].setIdentity(mIdentities[0]);
	SETVATTR_C(pvs[2], PROP_SPEC_ACL, OP_ADD, STORE_LAST_ELEMENT);
	pvs[2].meta = ACL_READ | ACL_WRITE;
	rc = lSession->createPIN(pid,pvs,3);

	lSession->terminate();
}

bool TestSessionLogin::changePwdCert()
{
	bool lSuccess = true;
	ISession * const lSession =	MVTApp::startSession();
	TVERIFY(NULL != lSession);
	int i = 0;
	for(i = 0; i < sNumIdentities; i++){
		bool lChangeInsertPerm = (int)10 * rand()/RAND_MAX > 5;
		if(lChangeInsertPerm){
			if(RC_OK != lSession->setInsertPermission(mIdentities[i],!mMayInsert[i])) { lSuccess = false; break;}
			mMayInsert[i] = !mMayInsert[i];
		}
		bool lChangePwd = (int)10 * rand()/RAND_MAX > 5;		
		if(lChangePwd){
			Tstring lPwd;
			MVTRand::getString(lPwd, (int)(100.0 * rand() /RAND_MAX), (int)(150.0 * rand() /RAND_MAX), false);
			if(RC_OK!=lSession->changePassword(mIdentities[i], mPwd[i].c_str(), lPwd.c_str())){ lSuccess = false; break;}
			mPwd[i] = lPwd;
		}
		bool lChangeCert = (int)10 * rand()/RAND_MAX > 5;
		if(lChangeCert){
			Tstring lCert;
			MVTRand::getString(lCert, (int)(100.0 * rand() /RAND_MAX), (int)(150.0 * rand() /RAND_MAX), false);
			int lCertLen = (int) lCert.length();			
			if(RC_OK!=lSession->changeCertificate(mIdentities[i],mPwd[i].c_str(),(const unsigned char *) lCert.c_str(), lCertLen)){ lSuccess = false; break;}
			mCert[i] = lCert;
		}
	}
	lSession->terminate();
	return lSuccess;
}

bool TestSessionLogin::insertPIN(ISession *pSession, bool pMayInsert)
{
	bool lSuccess = true;
	Value lVal[1]; PID lPID;
	char lBuf[255];
	IdentityID lIID = pSession->getCurrentIdentityID();
	pSession->getIdentityName(lIID,lBuf,255);
	char *lStr = strcat(lBuf," Created PIN");
	PropertyID prop=MVTApp::getProp(pSession,"TestSessionLogininsertPIN");
	lVal[0].set(lStr);lVal[0].setPropID(prop);
	if(RC_OK!=pSession->createPIN(lPID,lVal,1)){
		if(pMayInsert) lSuccess = false;
	}else{
		if(!pMayInsert) lSuccess = false;
	}
	return lSuccess;
}

void TestSessionLogin::postRun()
{
	ISession * const lSession =	MVTApp::startSession();	
	int i = 0;
	for (i = 0; i < sNumIdentities; i++){
		Value lVal[3];
		PID lPID = mPID[i];
		SETVALUE(lVal[0], lPM[2].uid, mCert[i].c_str(), OP_SET);
		SETVALUE(lVal[1], lPM[3].uid, mMayInsert[i] , OP_SET);
		SETVALUE(lVal[2], lPM[0].uid, mPwd[i].c_str() , OP_SET);
		IPIN *lPIN = lSession->getPIN(lPID);
		lPIN->modify(lVal,3);
		lPIN->destroy();
	}
	lSession->terminate();	
}
