/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include <bitset>
#include "app.h"
class PINInfo;
typedef std::set<PINInfo *> TPINsInfo;
#define DBGLOGGING 0
#define MAX_IDENTITIES 256
typedef std::bitset<MAX_IDENTITIES> Tbitset;

// See also the testaclsbasic test for a 
// an overview of ACL expected behavior

// Quick hack to convert this old test to using official properties.
static const int sNumProps = 50; 
static PropertyID mPropIDs[sNumProps]; 

// Publish this test.
class TestAcls : public ITest
{		
	public:
		TEST_DECLARE(TestAcls);
		virtual char const * getName() const { return "testacls"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test for ACLs"; }
		virtual bool includeInPerfTest() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }
		TestAcls() : mFinalResult(0) {}
	public:
		std::vector<Tstring> mIdentitiesStr;
		std::vector<IdentityID> mIdentities;
		TPINsInfo mPINs;
		MVTestsPortability::Mutex mLock;
		Afy::IAffinity *mStoreCtx;
		long volatile mFinalResult; // Non-zero means test failed
};
TEST_IMPLEMENT(TestAcls, TestLogger::kDStdOut);

// Implement this test.
class SessionInfo
{
	// Context information for one of the test threads
	public:
		TestAcls & mContext;
		int const mIdentityIndex;
		long volatile * const mStop; // Signals time to stop
		ISession * mSession;
		SessionInfo(TestAcls & pContext, int pIdentityIndex, long volatile * pStop)
			: mContext(pContext), mIdentityIndex(pIdentityIndex), mStop(pStop) {}
		SessionInfo(TestAcls & pContext, int pIdentityIndex, long volatile * pStop, const char * pwd, const char * storeident)
			: mContext(pContext), mIdentityIndex(pIdentityIndex), mStop(pStop), ppwd(pwd), pstoreident(storeident) {}
		void onStart()
		{
			mSession = MVTApp::startSession(mContext.mStoreCtx, mContext.mIdentitiesStr[mIdentityIndex].c_str(),ppwd.c_str());
			if(!mSession) {  
				mContext.getLogger().out() << "!ERROR: failed to create session while trying to log into the store!" << endl; 
				exit(RC_FALSE); 
			} 
		}
		
		string ppwd;
		string pstoreident;
};

class PINGrab
{
	public:
		IPIN * const mPIN;
		bool const mDestroy;
		PINGrab(SessionInfo & pSI, PID const & pPID, IPIN * pPIN) : mPIN(pPIN ? pPIN : pSI.mSession->getPIN(pPID)), mDestroy(NULL == pPIN) {}
		~PINGrab() { if (mDestroy && mPIN) mPIN->destroy(); }
		IPIN * operator->() { return mPIN; }
		operator IPIN *() { return mPIN; }
};

class PINInfo
{
	public:
		enum eRight { kRFirst = 0, kRRead = kRFirst, kRWrite, kRTOT };
		static uint8_t getMeta(eRight pRight) { if (kRRead == pRight) return META_PROP_READ; else if (kRWrite == pRight) return META_PROP_WRITE; else return 0; }
		static char const * getMetaTxt(uint8_t pMeta) { return META_PROP_READ == pMeta ? "R" : (META_PROP_WRITE == pMeta ? "W" : (0 == pMeta ? "none" : "RW")); }
		static char const * getMetaTxt(eRight pRight) { return kRRead == pRight ? "R" : (kRWrite == pRight ? "W" : "none"); }
	protected:
		PID const mPID;
		Tbitset mAllow[kRTOT], mOverride[kRTOT];
		PINInfo * mParent;
		bool mIsDoc;
	public:
		PINInfo(PID const & pPID) : mPID(pPID), mParent(NULL), mIsDoc(false) {}
		bool eval(eRight pRight, int pIdentityIndex) const { return mOverride[pRight].test(pIdentityIndex) ? mAllow[pRight].test(pIdentityIndex) : (mParent ? mParent->eval(pRight, pIdentityIndex) : false); }
		void override(eRight pRight, int pIdentityIndex, bool pAllow) { mOverride[pRight].set(pIdentityIndex); if (pAllow) mAllow[pRight].set(pIdentityIndex); else mAllow[pRight].reset(pIdentityIndex); }
		void removeOverride(eRight pRight, int pIdentityIndex) { mOverride[pRight].reset(pIdentityIndex); mAllow[pRight].reset(pIdentityIndex); }
		void setParent(PINInfo * pParent) { assert(pParent->isDoc()); mParent = pParent; }
		void setIsDoc(bool pIsDoc) { mIsDoc = pIsDoc; }
	public:
		PINInfo * getParent() const { return mParent; }
		bool isDoc() const { return mIsDoc; }
		PID const & getPID() const { return mPID; }
		static PINInfo * pickRandom(TestAcls & pContext);
	public:
		void showState(char const * pTitle, Tstring & pOutput, SessionInfo & pSI, IPIN * pPIN = NULL);
		bool changeRights(bool * pChangedSomething, SessionInfo & pSI, IPIN * pPIN = NULL);
		bool changeRights(eRight pRight, bool * pChangedSomething, SessionInfo & pSI, IPIN * pPIN = NULL);
		bool checkRights(SessionInfo & pSI, IPIN * pPIN = NULL);
		static void grabRights(eRight pRight, TestAcls & pContext, Value const & pACL, Tbitset & pOverrideAllow);
		static void grabRights(eRight pRight, TestAcls & pContext, IPIN * pPIN, Tbitset & pOverrideAllow);
		static Value const * findACL(IdentityID pIID, Value const * pV);
	public:
		static bool tryRead(IPIN * pPIN);
		static bool tryWrite(IPIN * pPIN, RC & pRC);
};

static inline void declareError(SessionInfo & pSI, char const * pMessage, PINInfo * pPINInfo, IPIN * pPIN)
{
	INTERLOCKEDI(&pSI.mContext.mFinalResult);
	INTERLOCKEDI(pSI.mStop);
	Tstring lStr;
	pPINInfo->showState(pMessage, lStr, pSI, pPIN);
	pSI.mContext.getLogger().out() << std::endl << lStr.c_str();
	int i = 0;
	PINInfo * lParent = pPINInfo->getParent();
	while (lParent)
	{
		char lMessage[32];
		sprintf(lMessage, "parent%d", i);

		lStr.clear();
		lParent->showState(lMessage, lStr, pSI);
		pSI.mContext.getLogger().out() << lStr.c_str();

		lParent = lParent->getParent();
		i++;
	}
}

THREAD_SIGNATURE threadTestAcls(void * pSI)
{
	enum eTested { kTWrite = (1 << 0), kTRead = (1 << 1), kTACL = (1 << 2) };
	int lTested = 0;
	SessionInfo * const lSI = (SessionInfo *)pSI;
	lSI->onStart();
	while (!*lSI->mStop)
	{
		if (lSI->mContext.mPINs.empty())
			{ MVTestsPortability::threadSleep(50); continue; }

		lSI->mContext.mLock.lock();

		// Get whatever pin.
		PINInfo * const lPINInfo = PINInfo::pickRandom(lSI->mContext);
		bool const lCanR = lPINInfo->eval(PINInfo::kRRead, lSI->mIdentityIndex);
		IPIN * const lPIN = lSI->mSession->getPIN(lPINInfo->getPID()); // todo: maintain local map of pid to pin, to not reload always [option]
		if (!lPIN)
		{
			if (lCanR)
				declareError(*lSI, "Error: PIN expected to be readable was not!", lPINInfo, lPIN);
			lSI->mContext.mLock.unlock();
			MVTestsPortability::threadSleep(10);
			continue;
		}
		IPIN * lPINParent = NULL;
		if (lPINInfo->getParent())
		{
			lPINParent = lSI->mSession->getPIN(lPINInfo->getParent()->getPID());
			static bool lWarned = false;
			if (!lPINParent && !lWarned)
			{
				lSI->mContext.getLogger().out() << "WARNING: Store returned child pin of a read-only parent (PROP_SPEC_DOCUMENT)!" << std::endl;
				lWarned = true;
			}
		}

		// At all times our own redundant copy of ACLs should be in sync with the store's.
		if (!lPINInfo->checkRights(*lSI, lPIN))
			declareError(*lSI, "Error: Store's ACLs were not in sync with redundant info maintained by the test!", lPINInfo, lPIN);
		if (lPINInfo->getParent() && !lPINInfo->getParent()->checkRights(*lSI, lPINParent))
			declareError(*lSI, "Error: Store's ACLs were not in sync with redundant info maintained by the test!", lPINInfo->getParent(), NULL);

		// Get rights.
		bool const lCanW = lPINInfo->eval(PINInfo::kRWrite, lSI->mIdentityIndex);
		bool const lCanChangeRights = lCanW; // Note: Might evolve...

		// Check write access.
		RC lRCWrite = RC_OK;
		if (lCanW && !PINInfo::tryWrite(lPIN, lRCWrite))
		{
			char lMsg[512];
			sprintf(lMsg, "Error: Failed to modify a pin expected to have write access, with RC=%d!", lRCWrite);
			declareError(*lSI, lMsg, lPINInfo, lPIN);
		}
		else if (!lCanW && PINInfo::tryWrite(lPIN, lRCWrite))
			declareError(*lSI, "Error: Succeeded to modify a pin not expected to have write access!", lPINInfo, lPIN);
		else
			lTested |= kTWrite;

		// Check read access.
		if (lCanR && !PINInfo::tryRead(lPIN))
			declareError(*lSI, "Error: Failed to read a pin expected to have read access!", lPINInfo, lPIN);
		else if (!lCanR && PINInfo::tryRead(lPIN))
			declareError(*lSI, "Error: Succeeded to read a pin not expected to have read access!", lPINInfo, lPIN);
		else
			lTested |= kTRead;

		// Check ACL access.
		bool lChangedRights = false;
		lSI->mSession->startTransaction();
		if (lCanChangeRights)
		{
			if ((10.0 * rand() / RAND_MAX) > 5.0)
				;
			else if (!lPINInfo->changeRights(&lChangedRights, *lSI, lPIN))
				declareError(*lSI, "Error: Failed changing rights on a pin expected to allow it!", lPINInfo, lPIN);
		}
		else if (lPINInfo->changeRights(&lChangedRights, *lSI, lPIN) && lChangedRights)
			declareError(*lSI, "Error: Succeeded changing rights on a pin not expected to allow it!", lPINInfo, lPIN);
		else
			lTested |= kTACL;
		lSI->mSession->commit();

		lPIN->destroy(); // not if put in map
		if (lPINParent)
			lPINParent->destroy();
		lSI->mContext.mLock.unlock();
		MVTestsPortability::threadSleep(10);
	}
	lSI->mSession->terminate();
	if (0 == (lTested & kTWrite) || 0 == (lTested & kTRead) || 0 == (lTested & kTACL))
		lSI->mContext.getLogger().out() << "WARNING: Did not even test all basic scenarios!" << std::endl;
	return 0;
}

int TestAcls::execute()
{
	if (!MVTApp::startStore()) { return RC_FALSE; }
	ISession * const lSession =	MVTApp::startSession();
	mStoreCtx = MVTApp::getStoreCtx();
	        
	MVTApp::mapURIs(lSession,"TestAcls",sNumProps,mPropIDs);
	
	// Setup a few identities/threads.
	#define TESTS_CONCURRENT_WITH_POPULATE 1
	static int const sNumIdentities = 5; // Note: < MAX_IDENTITIES...
	std::vector<SessionInfo *> lSIs;
	HTHREAD lThreads[sNumIdentities];
	long volatile lStop = 0;
	int i;
	
	string ppwd;        if(!mpArgs->get_param("pwd", ppwd)){ ppwd="";};
	string pidentity;   if(!mpArgs->get_param("identity", pidentity)){ pidentity="";};
	
	for (i = 0; i < sNumIdentities; i++)
	{
		char lIdentity[255];
		sprintf(lIdentity, "testacls.identity%d", i);
		IdentityID const lIID = lSession->storeIdentity(lIdentity, ppwd.c_str());
		mIdentities.push_back(lIID);
		mIdentitiesStr.push_back(lIdentity);
		lSIs.push_back(new SessionInfo(*this, i, &lStop, ppwd.c_str(), pidentity.c_str()));
	}
	#if TESTS_CONCURRENT_WITH_POPULATE
		for (i = 0; i < sNumIdentities; i++)
			createThread(&threadTestAcls, lSIs[i], lThreads[i]);
	#endif

	// Create lots of pins with ACLs.
	for (i = 0; i < 2000 && !lStop; i++)
	{
		if (0 == i % 100)
			mLogger.out() << ".";

		#define ASSIGN_ACL_AT_CREATION 1
		#define CREATE_ACL_HIERARCHIES 1

		// Create the pin.
		PINInfo * lParent = NULL;
		#if CREATE_ACL_HIERARCHIES
			// Determine randomly a parent, if desired.
			if (10.0 * rand() / RAND_MAX > 5.0)
			{
				lParent = PINInfo::pickRandom(*this);
				#if 1 // If enabled, restricts to 1-level deep hierarchies.
					while (lParent && lParent->getParent())
						lParent = lParent->getParent();
				#endif
			}
		#endif
		bool lPINCreated = false;
		PID lPID;
		#if ASSIGN_ACL_AT_CREATION
			int j;
			Value lPV[1 + sNumIdentities];
			for (j = 0; j < sNumIdentities && !lParent; j++)
			{
				lPV[j].setIdentity(mIdentities[j]);
				SETVATTR_C(lPV[j], PROP_SPEC_ACL, OP_ADD, STORE_LAST_ELEMENT);
				lPV[j].meta = META_PROP_READ | META_PROP_WRITE;
			}
			SETVALUE(lPV[j], PropertyID(mPropIDs[0]), "basic property for all pins in this test, for read access check", OP_ADD);
			lPINCreated = (RC_OK == lSession->createPINAndCommit(lPID, lPV, 1 + j));
		#else
			Value lPV;
			SETVALUE(lPV[j], PropertyID(1mPropIDs[0]), "basic property for all pins in this test, for read access check", OP_ADD);
			lPINCreated = (RC_OK == lSession->createPINAndCommit(lPID, &lPV, 1));
		#endif

		// Create redundant info, and adjust the pin's acl and parenting.
		if (lPINCreated)
		{
			mLock.lock();

			// Create redundant info.
			PINInfo * lPINInfo = new PINInfo(lPID);
			bool lAssignAcl = true;

			// Record already assigned acl, if any.
			#if ASSIGN_ACL_AT_CREATION
				lAssignAcl = false;
				int j;
				for (j = 0; j < sNumIdentities && !lParent; j++)
				{
					if (!lPINInfo->eval(PINInfo::kRRead, j))
						lPINInfo->override(PINInfo::kRRead, j, true);
					if (!lPINInfo->eval(PINInfo::kRWrite, j))
						lPINInfo->override(PINInfo::kRWrite, j, true);
				}
			#endif

			// Assign a parent doc to this pin, if desired, to test acl hierarchies.
			if (lParent)
			{
				IPIN * const lPIN = lSession->getPIN(lPID);
				Value lPV;
				SETVALUE_C(lPV, PROP_SPEC_DOCUMENT, lParent->getPID(), OP_ADD, STORE_LAST_ELEMENT);
				if (RC_OK != lPIN->modify(&lPV, 1))
					assert(false);
				else
				{
					lParent->setIsDoc(true);
					lPINInfo->setParent(lParent);
					lAssignAcl = false; // (could be random).
				}
				lPIN->destroy();
			}

			// Assign acl to this pin, if desired.
			if (lAssignAcl)
			{
				IPIN * const lPIN = lSession->getPIN(lPID);
				lSession->startTransaction();
				int j;
				for (j = 0; j < sNumIdentities; j++)
				{
					Value lPV;
					lPV.setIdentity(mIdentities[j]);
					SETVATTR_C(lPV, PROP_SPEC_ACL, OP_ADD, STORE_LAST_ELEMENT);
					lPV.meta = META_PROP_READ | META_PROP_WRITE;
					if (RC_OK != lPIN->modify(&lPV, 1))
						assert(false);
					else
					{
						if (!lPINInfo->eval(PINInfo::kRRead, j))
							lPINInfo->override(PINInfo::kRRead, j, true);
						if (!lPINInfo->eval(PINInfo::kRWrite, j))
							lPINInfo->override(PINInfo::kRWrite, j, true);
					}
				}
				lSession->commit();
				lPIN->destroy();
			}

			mPINs.insert(lPINInfo);
			mLock.unlock();
		}
		else
		{
			INTERLOCKEDI(&mFinalResult);
			INTERLOCKEDI(&lStop);
			mLogger.out() << "Error: Could not create new pin!" << std::endl;
		}
		#if TESTS_CONCURRENT_WITH_POPULATE
			MVTestsPortability::threadSleep(10);
		#endif
	}

	#if !TESTS_CONCURRENT_WITH_POPULATE
		for (i = 0; i < sNumIdentities; i++)
			createThread(&threadTestAcls, lSIs[i], lThreads[i]);
		MVTestsPortability::threadSleep(100000);
	#endif

	// We're done.
	lStop = 1;
	MVTestsPortability::threadsWaitFor(sNumIdentities, lThreads);
	TPINsInfo::iterator iP;
	for (iP = mPINs.begin(); mPINs.end() != iP; iP++)
		delete (*iP);
	mPINs.clear();
	mIdentitiesStr.clear();
	mIdentities.clear();

	lSession->terminate();
	MVTApp::stopStore();
	return mFinalResult;
}


// PINInfo implementation.
PINInfo * PINInfo::pickRandom(TestAcls & pContext)
{
	if (pContext.mPINs.empty())
		return NULL;
	int lPINIndex = (int)(double(pContext.mPINs.size()) * rand() / RAND_MAX);
	if (lPINIndex >= (int)pContext.mPINs.size())
		lPINIndex = 0; // Review: Why is this happening?
	int i;
	TPINsInfo::iterator iPI;
	for (i = 0, iPI = pContext.mPINs.begin(); i < lPINIndex; i++) { iPI++; }
	return *iPI;
}
void PINInfo::showState(char const * pTitle, Tstring & pOutput, SessionInfo & pSI, IPIN * pPIN)
{
	std::basic_ostringstream<char> lOs;
	PINGrab lPIN(pSI, mPID, pPIN);
	eRight lR;
	bool lCanRead = true;
	uint8_t const lCurIdentRights = (mAllow[kRRead].test(pSI.mIdentityIndex) ? META_PROP_READ : 0) | (mAllow[kRWrite].test(pSI.mIdentityIndex) ? META_PROP_WRITE : 0);

	lOs << "[pin:" << std::hex << LOCALPID(mPID) << ",thread:" << getThreadId();
	lOs << ",right:" << getMetaTxt(lCurIdentRights);
	lOs << ",iid#:" << pSI.mIdentityIndex << ",parent=" << (mParent ? LOCALPID(mParent->getPID()) : 0) << "] " << pTitle << std::endl;

	for (lR = kRFirst; lR < kRTOT; lR = eRight(lR + 1))
	{
		// Print the store's version of which identities have lR access to this pin.
		if (lCanRead)
		{		
			Tbitset lBV;
			grabRights(lR, pSI.mContext, lPIN, lBV);
			lOs << "  store(" << getMetaTxt(lR) << "):" << lBV << std::endl;
			if (kRRead == lR && !lBV.test(pSI.mIdentityIndex)) // When I don't have right to read this pin, any further conclusions on other rights may be wrong.
				lCanRead = false;
		}
		else
			lOs << "  store(" << getMetaTxt(lR) << "): can't read..." << std::endl;

		// Print the test's own perception of which identities have lR access to this pin.
		lOs << "   test(" << getMetaTxt(lR) << "):" << mAllow[lR];

		// If the pin has a parent, print the overrides.
		if (mParent)
			lOs << " (overrides: " << mOverride[lR] << ")" << std::endl;
		else
			lOs << std::endl;
	}
	pOutput = lOs.str();
}
bool PINInfo::changeRights(bool * pChangedSomething, SessionInfo & pSI, IPIN * pPIN)
{
	if (10.0 * rand() / RAND_MAX > 5.0)
		return changeRights(kRRead, pChangedSomething, pSI, pPIN);
	return changeRights(kRWrite, pChangedSomething, pSI, pPIN);
}
bool PINInfo::changeRights(eRight pRight, bool * pChangedSomething, SessionInfo & pSI, IPIN * pPIN)
{
	bool lSuccess = true;
	bool lChangedSomething = false;
	if (pChangedSomething)
		*pChangedSomething = false;

	PINGrab lPIN(pSI, mPID, pPIN);
	if (NULL == lPIN.mPIN)
		return lSuccess;

	#if DBGLOGGING
		Tstring lBeforeStr;
		showState("before", lBeforeStr, pSI, lPIN);
	#endif

	assert(lPIN->getPID() == mPID);
	int i;
	// Review: We could maybe reset/remove the acl once in a while for all rights/identities.
	for (i = 0; i < (int)pSI.mContext.mIdentities.size(); i++)
	{
		if (!eval(kRWrite, pSI.mIdentityIndex))
			break;
		bool const lYes = (10.0 * rand() / RAND_MAX) > 3.3;
		IdentityID const lIID = pSI.mContext.mIdentities[i];
		Value const * lACLColl = pPIN->getValue(PROP_SPEC_ACL);
		#ifndef NDEBUG
			size_t const lACLLength = lACLColl ? MVTApp::getCollectionLength(*lACLColl) : 0;
		#endif
		Value const * const lACL = lACLColl ? findACL(lIID, lACLColl) : NULL;
		uint8_t const lOldMeta = lACL ? lACL->meta : ((eval(kRRead, i) ? META_PROP_READ : 0) | (eval(kRWrite, i) ? META_PROP_WRITE : 0));
		if ((lYes && !eval(pRight, i)) || (!lYes && eval(pRight, i)))
		{
			Value lPV;
			lPV.setIdentity(lIID);
			SETVATTR_C(lPV, PROP_SPEC_ACL, (lACL ? OP_SET : OP_ADD), (lACL ? lACL->eid : STORE_LAST_ELEMENT));
			lPV.meta = lYes ? (lOldMeta | getMeta(pRight)) : (lOldMeta & ~getMeta(pRight));
			if (RC_OK == lPIN->modify(&lPV, 1))
			{
				assert(!lACL || lACLLength == MVTApp::getCollectionLength(*pPIN->getValue(PROP_SPEC_ACL)));
				lChangedSomething = true;

				// If there was no acl so far, then we need to set an acl for all rights now (default).
				// Review: And apparently, for all identities... hmm...
				if (!lACLColl && mParent)
				{
					eRight lR;
					int ii;
					for (ii = 0; ii < (int)pSI.mContext.mIdentities.size(); ii++)
						if (ii != i)
							for (lR = kRFirst; lR < kRTOT; lR = eRight(lR + 1))
								override(lR, ii, false);
					for (lR = kRFirst; lR < kRTOT; lR = eRight(lR + 1))
						override(lR, i, mParent->eval(lR, i));
				}
				override(pRight, i, lYes);

				#if DBGLOGGING
					pSI.mContext.getLogger().out() << "[pin:" << std::hex << LOCALPID(mPID) << ",thread:" << getThreadId() << "] ";
					pSI.mContext.getLogger().out() << "changed " << getMetaTxt(lOldMeta) << " to " << getMetaTxt(lPV.meta) << " for iid #" << i << std::endl;
				#endif
			}
			else
			{
				lSuccess = false;
				break;
			}
		}
	}

	#if DBGLOGGING
		if (lChangedSomething)
		{
			Tstring lAfterStr;
			showState("after", lAfterStr, pSI, lPIN);
			pSI.mContext.getLogger().out() << lBeforeStr;
			pSI.mContext.getLogger().out() << lAfterStr;
			pSI.mContext.getLogger().out() << std::endl;
		}
	#endif

	checkRights(pSI, lPIN);
	if (pChangedSomething && lChangedSomething)
		*pChangedSomething = true;
	return lSuccess;
}
bool PINInfo::checkRights(SessionInfo & pSI, IPIN * pPIN)
{
	PINGrab lPIN(pSI, mPID, pPIN);
	if (NULL == lPIN.mPIN)
		return !mAllow[kRRead].test(pSI.mIdentityIndex);

	bool lSuccess = true;
	eRight lR;
	Tbitset lAllow[kRTOT];
	for (lR = kRFirst; lR < kRTOT && lSuccess; lR = eRight(lR + 1))
	{
		grabRights(lR, pSI.mContext, pPIN, lAllow[lR]);
		if (kRFirst == lR && !lAllow[lR].test(pSI.mIdentityIndex))
			return !mAllow[kRRead].test(pSI.mIdentityIndex);

		if (lAllow[lR] != mAllow[lR])
		{
			pSI.mContext.getLogger().out() << "[pin:" << std::hex << LOCALPID(mPID) << ",thread:" << getThreadId() << ",right:" << getMetaTxt(lR) << "] WARNING (unexpected value)!!!" << std::endl << std::endl;
			lSuccess = false;
		}
	}
	return lSuccess;
}
void PINInfo::grabRights(eRight pRight, TestAcls & pContext, Value const & pACL, Tbitset & pOverrideAllow)
{
	if (VT_IDENTITY != pACL.type)
		{ assert(false); return; }
	int i;
	for (i = 0; i < (int)pContext.mIdentities.size(); i++)
		if (pACL.iid == pContext.mIdentities[i])
		{
			if (0 == (pACL.meta & getMeta(pRight)))
				pOverrideAllow.reset(i);
			else
				pOverrideAllow.set(i);
			break;
		}
}
void PINInfo::grabRights(eRight pRight, TestAcls & pContext, IPIN * pPIN, Tbitset & pOverrideAllow)
{
	// Rebuild bitvector of identies from pin.
	// Warning: Not very efficient...
	Value const * const lACL = pPIN ? pPIN->getValue(PROP_SPEC_ACL) : NULL;
	if (lACL && VT_IDENTITY == lACL->type)
		grabRights(pRight, pContext, *lACL, pOverrideAllow);
	else if (lACL && VT_ARRAY == lACL->type)
	{
		size_t i;
		for (i = 0; i < lACL->length; i++)
			grabRights(pRight, pContext, lACL->varray[i], pOverrideAllow);
	}
	else if (lACL && VT_COLLECTION == lACL->type)
	{
		static bool lWarned = false;
		if (!lWarned)
		{
			pContext.getLogger().out() << "WARNING: nav returned, expected varray" << std::endl;
			lWarned = true;
		}

		Value const * lV = lACL->nav->navigate(GO_FIRST);
		while (lV)
		{
			grabRights(pRight, pContext, *lV, pOverrideAllow);
			lV = lACL->nav->navigate(GO_NEXT);
		}
	}
	else
		pOverrideAllow.reset();
}
Value const * PINInfo::findACL(IdentityID pIID, Value const * pV)
{
	size_t i; const Value *lVi;
	if (NULL == pV)
		return NULL;
	switch (pV->type) {
	default: break;
	case VT_IDENTITY:
		return pIID == pV->iid ? pV : (const Value*)0;
	case VT_ARRAY:
		for (i = 0; i < pV->length; i++)
			if (pV->varray[i].type==VT_IDENTITY && pV->varray[i].iid==pIID)
				return &pV->varray[i];
		break;
	case VT_COLLECTION:
		lVi = pV->nav->navigate(GO_FIRST);
		while (lVi!=NULL)
		{
			if (lVi->type==VT_IDENTITY && lVi->iid==pIID)
				return lVi;
			lVi = pV->nav->navigate(GO_NEXT);
		}
		break;
	}
	return NULL;
}
bool PINInfo::tryRead(IPIN * pPIN)
{
	Value const * const lPV = pPIN->getValue(PropertyID(mPropIDs[0]));
	if (NULL == lPV)
		return false;
	if (VT_STRING != lPV->type && VT_ARRAY != lPV->type && VT_COLLECTION != lPV->type)
		return false;
	return true;
}
bool PINInfo::tryWrite(IPIN * pPIN, RC & pRC)
{
	Tstring lStr;
	MVTRand::getString(lStr, 20, 0, false);
	#if 1
		// Note: This used to cause problems due to incomplete code in modify (page relocation issue).
		Value lPV;
		SETVALUE_C(lPV, PropertyID(mPropIDs[MVTRand::getRange(0, sNumProps - 1)]), lStr.c_str(), OP_ADD, STORE_LAST_ELEMENT);
	#else
		Value lPV;
		SETVALUE_C(lPV, PropertyID(mPropIDs[MVTRand::getRange(0, sNumProps - 1)]), lStr.c_str(), OP_SET, STORE_COLLECTION_ID);
	#endif
	pRC = pPIN->modify(&lPV, 1);
	return RC_OK == pRC;
}
