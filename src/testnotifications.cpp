/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include <bitset>
#include <math.h>
#include "app.h"
#include <iomanip>
#define DBGLOGGING 0
#define MAX_CLASSES 256
#define MAX_PINS 30000
typedef std::bitset<MAX_CLASSES> Tbitsetclasses;
typedef std::bitset<MAX_PINS> Tbitsetpins;

#define TEST_CNAVIGATOR_COUNT 0 /* Count returned from cnavigator is not always correct */

static const int sNumProps = 1; 

// Publish this test.
class TestNotifications : public ITest
{
	public:
        PropertyID mPropID999[sNumProps]; 
		TEST_DECLARE(TestNotifications);
		virtual char const * getName() const { return "testnotifications"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "bashes on notifications (and classification)"; }
		virtual bool isStandAloneTest()const { return true; }
		virtual bool includeInPerfTest() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void outputPINInfo(IPIN & pPIN, PropertyID pPropIDBase, long pOldMask, long pNewMask);
};
TEST_IMPLEMENT(TestNotifications, TestLogger::kDStdOut);

// Implement this test.
class PITNotification
{
	public:
		PID mPID;
		ClassID mCLSID;
		PropertyID mPropID;
		ElementID mEid;
		IStoreNotification::NotificationEventType mAction;
		PITNotification(PID pPID, ClassID pCLSID, IStoreNotification::NotificationEventType pAction)
			: mPID(pPID), mCLSID(pCLSID), mPropID(STORE_INVALID_URIID), mEid(STORE_COLLECTION_ID), mAction(pAction) {}
		PITNotification(PID pPID, PropertyID pPropID, ElementID pEid)
			: mPID(pPID), mCLSID(STORE_INVALID_CLASSID), mPropID(pPropID), mEid(pEid), mAction(IStoreNotification::NE_PIN_UPDATED) {}
};

class PITNotifierCallback1 : public IStoreNotification
{
	public:
		typedef std::vector<PITNotification> TReceived;
	protected:
		TReceived mReceivedAsync, mReceivedSync; // Accumulate notifications (assume no collapsing).
		THREADID const mClientThreadId; // To determine whether a notification is synchronous or not.
	public:
		PITNotifierCallback1() : mClientThreadId(getThreadId()) {}
		TReceived const & getReceivedSync() const { return mReceivedSync; }
		void clearReceivedSync() { mReceivedSync.clear(); }
	public:
		virtual	void notify(NotificationEvent *events,unsigned nEvents,uint64_t txid)
		{
			unsigned i, j;
			bool const lAsync = (getThreadId() != mClientThreadId);
			assert(!lAsync);
			TReceived & lReceived = lAsync ? mReceivedAsync : mReceivedSync;
			for (i = 0; i < nEvents; i++)
			{
				if (events[i].events)
				{
					for (j = 0; j < events[i].nEvents; j++)
					{
						#if DBGLOGGING
							if (-1 != events[i].events[j].cid)
								printf("NOTIF: pin="_LX_FM", clsid=%d, type=%d\n", events[i].pin.pid, events[i].events[j].cid, events[i].events[j].type);
						#endif
						lReceived.push_back(PITNotification(events[i].pin, events[i].events[j].cid, events[i].events[j].type));
					}
				}
				if (events[i].data)
				{
					for (j = 0; j < events->nData; j++)
					{
						lReceived.push_back(PITNotification(events[i].pin, events[i].data[j].propID, events[i].data[j].eid));
						if (events[i].data[j].oldValue && events[i].data[j].oldValue->type == VT_COLLECTION)
						{
							size_t iV;
							Value const * const lChecked = events[i].data[j].oldValue;
							Value const * lNext;
							for (lNext = lChecked->nav->navigate(GO_FIRST), iV = 0; NULL != lNext; lNext = lChecked->nav->navigate(GO_NEXT), iV++);
#if TEST_CNAVIGATOR_COUNT
							assert(iV == lChecked->nav->count() && "Something unhealthy about this collection! Count doesn't match iterated contents!");
#endif
						}
					}
				}
			}
		}
		virtual	void replicationNotify(NotificationEvent *events,unsigned nEvents,uint64_t txid) {}
		virtual	void txNotify(TxEventType,uint64_t txid) {}
};

#define lAllClassNotifs (CLASS_NOTIFY_JOIN | CLASS_NOTIFY_LEAVE | CLASS_NOTIFY_CHANGE | CLASS_NOTIFY_DELETE | CLASS_NOTIFY_NEW)

class PITStoreClass
{
	public:
		class CompareMasks { public: bool operator()(PITStoreClass const & p1, PITStoreClass const & p2) const { return p1.mPropBitMask < p2.mPropBitMask; } };
		class CompareCLSIDs { public: bool operator()(PITStoreClass const & p1, PITStoreClass const & p2) const { return p1.mCLSID < p2.mCLSID; } };
	public:
		long mPropBitMask;
		ClassID mCLSID;
		int mIndex;
		PITStoreClass(long pPropBitMask = 0, ClassID pCLSID = STORE_INVALID_CLASSID, int pIndex = -1) : mPropBitMask(pPropBitMask), mCLSID(pCLSID), mIndex(pIndex) {}
	public:
		static ClassID registerRealClass(ISession * pSession, std::vector<PropertyID> const & pPropIDs, long pPropBitMask, const char *pRandStr)
		{
			if (0 == pPropIDs.size())
				return STORE_INVALID_CLASSID;

			IStmt * const lQ = pSession->createStmt();
			unsigned char lVar = lQ->addVariable();
			if (10.0 * rand() / RAND_MAX > 5.0)
			{
				// Generate the expression trees corresponding to the existence of each property.
				std::vector<TExprTreePtr> lExprs;
				int i;
				for (i = 0; i < (int)pPropIDs.size(); i++)
				{
					Value lV;
					lV.setVarRef(0,pPropIDs[i]);
					lExprs.push_back(EXPRTREEGEN(pSession)(OP_EXISTS, 1, &lV, 0));
				}

				// Combine them into one expression.
				TExprTreePtr lFinalE = lExprs.back();
				lExprs.pop_back();
				while (!lExprs.empty())
				{
					Value lV[2];
					lV[0].set(lFinalE);
					lV[1].set(lExprs.back());
					lExprs.pop_back();
					lFinalE = EXPRTREEGEN(pSession)(OP_LAND, 2, lV, 0);
				}

				// Create the mvstore class.
				lQ->addCondition(lVar,lFinalE);
				lFinalE->destroy();
			}
			else
			{
				// Simpler method offered by the store.
				lQ->setPropCondition(lVar, &pPropIDs[0], (unsigned int)pPropIDs.size());
			}

			char lName[255];
			sprintf(lName, "testNotifications%s_class%lx", pRandStr, pPropBitMask);

			ClassID lCLSID = STORE_INVALID_CLASSID;
			ITest::defineClass(pSession,lName, lQ, &lCLSID);
			pSession->enableClassNotifications(lCLSID,lAllClassNotifs); 
			return lCLSID;
		}
		static void extractPropIDs(PropertyID pPropIDBase, long pPropBitMask, std::vector<PropertyID> & pPropIDs)
		{
			pPropIDs.clear();
			long lPropBitMask = pPropBitMask;
			PropertyID lPropID = pPropIDBase;
			while (lPropBitMask != 0)
			{
				if (lPropBitMask & 0x01)
					pPropIDs.push_back(lPropID);
				lPropBitMask >>= 1;
				lPropID++;
			}
		}
};

#define ISOLATE_1647a 0
#define ISOLATE_1647b 0
int TestNotifications::execute()
{
	#if ISOLATE_1647a || ISOLATE_1647b
		// NOTE: This can be set by command line argument now, perhaps can be removed
		setRandomSeed(1) ;
		mLogger.out() << "Using specific seed 1" << endl ;			
	#endif
	bool lSuccess = true;
	PITNotifierCallback1 lNotifierCB;
	if (MVTApp::startStore(NULL, &lNotifierCB))
	{
		ISession * const lSession = MVTApp::startSession();
		/*
		PropertyID const lPropIDPinIndex = 500;
		PropertyID const lPropIDTestPass = 501;
		PropertyID const lPropIDBase = 1000;
		PropertyID const lPropIDs[] = {lPropIDBase, lPropIDBase + 1, lPropIDBase + 2, lPropIDBase + 3, lPropIDBase + 4, lPropIDBase + 5, lPropIDBase + 6};
		*/

		MVTApp::mapURIs(lSession,"TestNotifications.prop",1,mPropID999);

		RC rc = RC_OK;
		PropertyID lTempPropID[2]; 
		MVTApp::mapURIs(lSession,"TestNotifications.prop",2,lTempPropID);
		PropertyID const lPropIDPinIndex = lTempPropID[0];
		PropertyID const lPropIDTestPass = lTempPropID[1];		
		PropertyID lPropIDs[7]; 
		MVTApp::mapURIs(lSession,"TestNotifications.prop",7,lPropIDs);
		PropertyID const lPropIDBase = lPropIDs[0];
		size_t const lNumPropIDs = sizeof(lPropIDs) / sizeof(lPropIDs[0]);
		double const lNumPossibleClasses = pow(2., (double)lNumPropIDs); // Note: < MAX_CLASSES...

		// Generate lots of c7asses, as bit masks of the existing properties.
		// Register them for notifications.
		typedef std::set<PITStoreClass, PITStoreClass::CompareMasks> TClasses;
		typedef std::set<PITStoreClass, PITStoreClass::CompareCLSIDs> TClassesByCLSID;
		typedef std::vector<Tbitsetpins> TPinsForClasses;
		TClasses lClasses;
		TClassesByCLSID lClassesByCLSID;
		TPinsForClasses lPinsForClasses;
		Tstring lRandStr; MVTRand::getString(lRandStr,10,10,false,false);			
		size_t i;
		for (i = 0; i < (size_t)lNumPossibleClasses / 2;)
		{
			long const lPropBitMask = (long)(lNumPossibleClasses * rand() / RAND_MAX);
			if (0 == lPropBitMask || lClasses.end() != lClasses.find(PITStoreClass(lPropBitMask)))
				continue;
			std::vector<PropertyID> lPropIDs;
			PITStoreClass::extractPropIDs(lPropIDBase, lPropBitMask, lPropIDs);
			ClassID const lClassID = PITStoreClass::registerRealClass(lSession, lPropIDs, lPropBitMask, lRandStr.c_str());
			if (STORE_INVALID_CLASSID == lClassID)
				{ assert(false); continue; }
			lPinsForClasses.push_back(Tbitsetpins());
			PITStoreClass lClass(lPropBitMask, lClassID, (int)i);
			lClasses.insert(lClass);
			lClassesByCLSID.insert(lClass);
			#if DBGLOGGING
				mLogger.out() << "New class #" << std::dec << std::setw(2) << lClass.mIndex;
				mLogger.out() << ": clsid=" << std::setw(3) << lClass.mCLSID;
				mLogger.out() << " mask=" << std::setw(4) << lPropBitMask;
				mLogger.out() << " props= ";
				int j;
				for (j = 0; j < (int)lPropIDs.size(); j++)
					mLogger.out() << lPropIDs[j] << " ";
				mLogger.out() << std::endl;
			#endif
			i++;
		}

		// Define a test pass id.
		std::ostringstream lTestPassOS;
		lTestPassOS << "testnotifications.";
		lTestPassOS << getTimeInMs() << std::ends;
		std::string const lTestPass = lTestPassOS.str();

		// Create a bunch of empty pins.
		// Register them for notifications.
		mLogger.out() << "create PINs" << std::endl;
		static size_t const sNumPINs = 2000; // Note: < MAX_PINS...
		IPIN * lPINs[sNumPINs];
		long lPINMasks[sNumPINs];		
		int lSpacer = 0;
		lSession->startTransaction();
		#if ISOLATE_1647a
			int lIndexTrack1647 = 0;
			PID lPID1647; lPID1647.pid = 0x100000000014000e; lPID1647.ident = 0;
		#endif
		#if ISOLATE_1647b
			int lIndexTrack1647b = 0;
			PID lPID1647b; lPID1647b.pid = 0x1000000000160034; lPID1647b.ident = 0;
		#endif
		for (i = 0; i < (int)sNumPINs; i++)
		{
			if (0 == i % 100)
				mLogger.out() << ".";

			PID lPID;
			Value lV[2];
			SETVALUE(lV[0], lPropIDPinIndex, (int)i, OP_ADD);
			SETVALUE(lV[1], lPropIDTestPass, lTestPass.c_str(), OP_ADD); /*lV[1].setMeta(META_PROP_NOFTINDEX);*/
			if (RC_OK != (rc=lSession->createPINAndCommit(lPID, &lV[0], 2)))
			{
				lSuccess = false;
				mLogger.out()<<"Failed with RC:"<<rc<<std::endl;
				assert(false);
			}

			#if ISOLATE_1647a
				if (lPID.pid == lPID1647.pid)
					lIndexTrack1647 = (int)i;
			#endif
			#if ISOLATE_1647b
				if (lPID.pid == lPID1647b.pid)
					lIndexTrack1647b = (int)i;
			#endif

			lPINs[i] = lSession->getPIN(lPID);
			lPINs[i]->setNotification();
			lPINMasks[i] = 0;

			#define WORKAROUND_RELOCATE_LIMITATION 1
			#if WORKAROUND_RELOCATE_LIMITATION
				lSpacer++;
				if (lSpacer > 5)
				{
					char lSpacerT[1024];
					memset(lSpacerT, 'a', 1024);
					lSpacerT[1023] = 0;
					lPID.ident = STORE_OWNER;
					lPID.pid = STORE_INVALID_PID;
					lV[0].set((unsigned char *)lSpacerT, 1024); lV[0].setPropID(PropertyID(mPropID999[0])); lV[0].setOp(OP_ADD);
					if (RC_OK != lSession->createPINAndCommit(lPID, &lV[0], 1))
						assert(false);
				}
			#endif
		}
		lSession->commit();
		mLogger.out() << std::endl;

		#if ISOLATE_1647b
			IStmt * const lQ1647 = lSession->createStmt();
			SourceSpec lCS1647;
			lCS1647.objectID = 0x11;
			lCS1647.nParams = 0;
			lCS1647.params = NULL;
			unsigned const lVar = lQ1647->addVariable(&lCS1647, 1);
			PITStoreClass lC11(0, 0x11);
			TClassesByCLSID::iterator iC11 = lClassesByCLSID.find(lC11);
			long const lC11Mask = (*iC11).mPropBitMask;
		#endif

		// Modify the pins randomly.
		// Check that the appropriate pin and class notifications are sent.
		mLogger.out() << "modify PINs and test classification" << std::endl;
		lSession->startTransaction();
		for (i = 0; i < 20000 && lSuccess;)
		{
			if (0 == i % 100)
				mLogger.out() << ".";

			#if ISOLATE_1647a
				if (i == 0x3612)
					DebugBreak();
				IPIN * lPin1647 = lSession->getPIN(lPID1647);
				if (lPin1647)
				{
					Value const * lV = lPin1647->getValue(lPropIDPinIndex);
					if (!lV || lV->i != lIndexTrack1647)
						std::cout << "PROBLEMBEFORE" << std::endl; // Ok at beginning of iteration i=0x3612
					lPin1647->destroy();
				}
			#endif

			int const lPinIndex = (int)((double)sNumPINs * rand() / RAND_MAX);
			int const lPropIDIndex = (int)((double)lNumPropIDs * rand() / RAND_MAX);
			if (lPinIndex >= (int)sNumPINs || lPropIDIndex >= (int)lNumPropIDs)
				continue;
			PropertyID const lPropID = lPropIDs[lPropIDIndex];
			long const lPropMask = (1 << (lPropID - lPropIDBase));
			long const lOldPINMask = lPINMasks[lPinIndex];

			#if ISOLATE_1647b
				if (lPinIndex == lIndexTrack1647b)
					std::cout << "A";
				if (i == 0x38d7)
					DebugBreak();
			#endif

			bool lPINPropModifed = false;
			int lNumPINinClasses = 0;
			Tbitsetclasses lUpdClasses;
			{
				TClasses::iterator lItClasses;
				for (lItClasses = lClasses.begin(); lClasses.end() != lItClasses; lItClasses++){
					ClassID lCLSID = (*lItClasses).mCLSID;
					int const lIndexClass = (*lItClasses).mIndex;
					if(lPINs[lPinIndex]->testClassMembership(lCLSID)){
						lNumPINinClasses++;
						lUpdClasses.set(lIndexClass);
					}
				}
			}
			// Either remove an existing property or modify it
			if (lPINs[lPinIndex]->defined(&lPropID, 1))
			{
				bool const lLeaveBias = (100.0 * rand() / RAND_MAX > 33.0);
				Value lV;
				if (lLeaveBias){
					Tstring lS; 
					MVTRand::getString(lS, 20, 0, false);
					lV.set((unsigned char *)lS.c_str(), (uint32_t)lS.length()); lV.setPropID(lPropID); lV.setOp(OP_ADD); lV.eid = STORE_LAST_ELEMENT;
 					if (RC_OK != (rc=lPINs[lPinIndex]->modify(&lV, 1)))
					{
						lSuccess = false;
						mLogger.out()<<"Failed with RC:"<<rc<<std::endl;
						assert(false);
					}else{
						if(lNumPINinClasses!=0)lPINPropModifed = true;
					}
				}else{					
					lV.setDelete(lPropID);
					if (RC_OK != (rc = lPINs[lPinIndex]->modify(&lV, 1)))
					{
						lSuccess = false;
						mLogger.out()<<"Failed with RC:"<<rc<<std::endl;
						assert(false);
					}
					else{
						lPINMasks[lPinIndex] &= ~lPropMask;
						lPINs[lPinIndex]->refresh();
						if(lPINs[lPinIndex]->getNumberOfProperties() > 2){
							Tbitsetclasses lTempBV = lUpdClasses;
							lUpdClasses &= ~lTempBV;
							lNumPINinClasses = 0;
							TClasses::iterator lItClasses;
							for (lItClasses = lClasses.begin(); lClasses.end() != lItClasses; lItClasses++){
								ClassID lCLSID = (*lItClasses).mCLSID;
								int const lIndexClass = (*lItClasses).mIndex;
								if(lPINs[lPinIndex]->testClassMembership(lCLSID)){
									lNumPINinClasses++;
									lUpdClasses.set(lIndexClass);
								}
							}
							if(lNumPINinClasses!=0)lPINPropModifed = true;
						}
					}
				}
			}

			// Or add a new property.
			else
			{
				Tstring lS;
				MVTRand::getString(lS, 20, 0, false);
				Value lV;
				#define TEST_AVOID_FTINDEXING 1
				#if TEST_AVOID_FTINDEXING==1
					lV.set((unsigned char *)lS.c_str(), (uint32_t)lS.length()); lV.setPropID(lPropID); lV.setOp(OP_ADD);
				#elif TEST_AVOID_FTINDEXING==2
					unsigned int lR = (unsigned int)(1000.0 * rand() / RAND_MAX);
					SETVALUE(lV, lPropID, lR, OP_ADD);
				#else
					SETVALUE(lV, lPropID, lS.c_str(), OP_ADD);
				#endif
				if (RC_OK != (rc=lPINs[lPinIndex]->modify(&lV, 1)))
				{
					#if WORKAROUND_RELOCATE_LIMITATION
					mLogger.out() << "failed to modify pin::" << rc << std::endl;
					#else
						lSuccess = false;
						assert(false);
					#endif
				}
				else{
					lPINMasks[lPinIndex] |= lPropMask;
					if(lNumPINinClasses!=0)lPINPropModifed = true;
				}
			}

			#ifndef NDEBUG
			  Value const * const lVCheck = lPINs[lPinIndex]->getValue(lPropIDPinIndex);
			  assert(lVCheck && lVCheck->i == lPinIndex);
			#endif
              
			#if ISOLATE_1647a
				Value const * const lVCheck1647 = lPINs[lIndexTrack1647]->getValue(lPropIDPinIndex);
				assert(lVCheck1647 && lVCheck1647->i == lIndexTrack1647);
				lPin1647 = lSession->getPIN(lPID1647);
				if (lPin1647)
				{
					Value const * lV = lPin1647->getValue(lPropIDPinIndex);
					if (!lV || lV->i != lIndexTrack1647)
						std::cout << "PROBLEMAFTER" << std::endl; // Bad at iteration i>=0x3612
					lPin1647->destroy();
				}
			#endif

			// Figure what class notifications should result from this.
			Tbitsetclasses lOldClasses, lNewClasses;
			TClasses::iterator lItClasses;
			for (lItClasses = lClasses.begin(); lClasses.end() != lItClasses; lItClasses++)
			{
				long const lItMask = (*lItClasses).mPropBitMask;
				int const lIndexClass = (*lItClasses).mIndex;
				if (lItMask == (lOldPINMask & lItMask))
					lOldClasses.set(lIndexClass);
				if (lItMask == (lPINMasks[lPinIndex] & lItMask))
					lNewClasses.set(lIndexClass);
			}
			Tbitsetclasses lExpectedRemoved = lOldClasses; lExpectedRemoved &= ~lNewClasses;
			Tbitsetclasses lExpectedAdded = lNewClasses; lExpectedAdded &= ~lOldClasses;
			Tbitsetclasses lExpectedModified = lPINPropModifed ? lUpdClasses : Tbitsetclasses();

			// Check the actual notifications received.
			PITNotifierCallback1::TReceived const & lReceivedNotifs = lNotifierCB.getReceivedSync();
			PITNotifierCallback1::TReceived::const_iterator lItNotifs;
			for (lItNotifs = lReceivedNotifs.begin(); lReceivedNotifs.end() != lItNotifs; lItNotifs++)
			{
				PITNotification const & lNotif = *lItNotifs;
				PID const lPID = lPINs[lPinIndex]->getPID();
				if (lNotif.mPID != lPID)
				{
					/*
					See 6436, 5600
					lSuccess = false;
					mLogger.out() << "Unexpected notif for PID=" << std::hex << LOCALPID(lNotif.mPID);
					mLogger.out() << " (expected: " << std::hex << LOCALPID(lPID) << ")." << std::endl;
					*/
				}
				else if (lNotif.mPropID != lPropID && lNotif.mPropID != STORE_INVALID_URIID)
				{
					/*
					See 6436, 5600
					lSuccess = false;
					mLogger.out() << "Unexpected notif for PropertyID=" << lNotif.mPropID;
					mLogger.out() << " (expected: " << lPropID << ")." << std::endl;
					*/
				}
				else if (lNotif.mCLSID != STORE_INVALID_CLASSID)
				{
					TClassesByCLSID::iterator const lFound = lClassesByCLSID.find(PITStoreClass(0, lNotif.mCLSID));
					if (lClassesByCLSID.end() == lFound)
					{
						lSuccess = false;
						mLogger.out() << "Unknown class notified: " << lNotif.mCLSID << std::endl;
						outputPINInfo(*lPINs[lPinIndex], lPropIDBase, lOldPINMask, lPINMasks[lPinIndex]);
					}
					else if (IStoreNotification::NE_CLASS_INSTANCE_ADDED == lNotif.mAction)
					{
						PITStoreClass const & lNotifC = (*lFound);
						if (!lExpectedAdded.test(lNotifC.mIndex))
						{
							lSuccess = false;
							assert((lPINMasks[lPinIndex] & lNotifC.mPropBitMask) != lNotifC.mPropBitMask); // This would notify me of an error on my side.
							mLogger.out() << "Incorrect new class: " << lNotif.mCLSID << std::endl;
							outputPINInfo(*lPINs[lPinIndex], lPropIDBase, lOldPINMask, lPINMasks[lPinIndex]);
						}
						else
						{
							lExpectedAdded.reset(lNotifC.mIndex);							
							lPinsForClasses[lNotifC.mIndex].set(lPinIndex);
						}
					}
					else if (IStoreNotification::NE_CLASS_INSTANCE_REMOVED == lNotif.mAction)
					{
						PITStoreClass const & lNotifC = (*lFound);
						if (!lExpectedRemoved.test(lNotifC.mIndex))
						{
							lSuccess = false;
							assert((lOldPINMask & lNotifC.mPropBitMask) != lNotifC.mPropBitMask); // This would notify me of an error on my side.
							mLogger.out() << "Incorrect old class: " << lNotif.mCLSID << std::endl;
							outputPINInfo(*lPINs[lPinIndex], lPropIDBase, lOldPINMask, lPINMasks[lPinIndex]);
						}
						else
						{
							lExpectedRemoved.reset(lNotifC.mIndex);
							lPinsForClasses[lNotifC.mIndex].reset(lPinIndex);
						}
					}
					else if (IStoreNotification::NE_CLASS_INSTANCE_CHANGED == lNotif.mAction)
					{
						PITStoreClass const & lNotifC = (*lFound);
						if (!lExpectedModified.test(lNotifC.mIndex))
						{
							lSuccess = false;
							assert((lOldPINMask & lNotifC.mPropBitMask) != lNotifC.mPropBitMask); 
							mLogger.out() << "Incorrect old class: " << lNotif.mCLSID << std::endl;
							outputPINInfo(*lPINs[lPinIndex], lPropIDBase, lOldPINMask, lPINMasks[lPinIndex]);
						}
						else
						{
							lExpectedModified.reset(lNotifC.mIndex);
						}
					}
				}
			}

			if (!lExpectedAdded.none())
			{
				lSuccess = false;
				mLogger.out() << "Missing notifs for added classes: ";
				unsigned iB;
				for (iB = 0; iB < lExpectedAdded.size(); iB++)
					if (lExpectedAdded.test(iB))
						mLogger.out() << "#" << iB << " ";
				mLogger.out() << std::endl;
				outputPINInfo(*lPINs[lPinIndex], lPropIDBase, lOldPINMask, lPINMasks[lPinIndex]);
			}
			if (!lExpectedRemoved.none())
			{
				lSuccess = false;
				mLogger.out() << "Missing notifs for removed classes: ";
				unsigned iB;
				for (iB = 0; iB < lExpectedRemoved.size(); iB++)
					if (lExpectedRemoved.test(iB))
						mLogger.out() << "#" << iB << " ";
				mLogger.out() << std::endl;
				outputPINInfo(*lPINs[lPinIndex], lPropIDBase, lOldPINMask, lPINMasks[lPinIndex]);
			}
			if (!lExpectedModified.none())
			{
				lSuccess = false;
				mLogger.out() << "Missing notifs for pins modified in classes: ";
				unsigned iB;
				for (iB = 0; iB < lExpectedModified.size(); iB++)
					if (lExpectedModified.test(iB))
						mLogger.out() << "#" << iB << " ";
				mLogger.out() << std::endl;
				outputPINInfo(*lPINs[lPinIndex], lPropIDBase, lOldPINMask, lPINMasks[lPinIndex]);
			}

			#if ISOLATE_1647b
				ICursor * const lR1647 = (i >= 0x38d7) ? lQ1647->execute() : NULL;
				if (lR1647)
				{
					IPIN * lP;
					while (NULL != (lP = lR1647->next()))
					{
						Value const * lV = lP->getValue(lPropIDPinIndex);
						if (!lV || lV->type != VT_INT || lV->i < 0 || lV->i > sNumPINs)
							std::cout << "!";
						else
						{
							long lMask1647 = 0;
							PropertyID lTP;
							for (lTP = lPropIDBase; lTP < lPropIDBase + lNumPropIDs; lTP++)
								if (lP->defined(&lTP, 1))
									lMask1647 |= (1 << lTP - lPropIDBase);
							if ((lC11Mask & lMask1647) != lC11Mask)
								std::cout << "?(" << std::hex << lMask1647 << ")";
							else if (!lPinsForClasses[lCS1647.objectID].test(lV->i))
							{
								std::cout << "[PID:" << LOCALPID(lP->getPID()) << ",m=" << lMask1647 << "]";
								std::cout << "*";
							}
						}
						lP->destroy();
					}
					lR1647->destroy();
				}
			#endif

			lNotifierCB.clearReceivedSync();
			i++;
		}
		lSession->commit();
		mLogger.out() << std::endl;

		mLogger.out() << "query classified pins" << std::endl;
		for (i = 0; i < 1000 && lSuccess; i++)
		{
			if (0 == i % 100)
				mLogger.out() << ".";
			
			int const lNth = (int)((double)lClasses.size() * rand() / RAND_MAX);
			TClasses::iterator iC;
			int j;
			for (j = 0, iC = lClasses.begin(); j < lNth && lClasses.end() != iC; j++, iC++);
			if (lClasses.end() == iC)
				continue;
			PITStoreClass const & lClass = (*iC);
			
			IStmt * const lQ = lSession->createStmt();

			SourceSpec lCS;
			lCS.objectID = lClass.mCLSID;
			lCS.nParams = 0;
			lCS.params = NULL;
			unsigned const lVar = lQ->addVariable(&lCS, 1);

			#if 1
				// Necessary for correctness regardless of store state previous to this test,
				// but causes additional performance overhead.
				Value lV[2];
				lV[0].setVarRef(0,lPropIDTestPass);
				lV[1].set(lTestPass.c_str());
				IExprTree * const lE = lSession->expr(OP_EQ, 2, lV);
				lQ->addCondition(lVar,lE);
				lE->destroy();
			#endif

			ICursor * lR = NULL;
			TVERIFYRC(lQ->execute(&lR));
			IPIN * lRi;
			unsigned lNumFound = 0;
			while (lR && (NULL != (lRi = lR->next())) && lSuccess)
			{
				Value const * const lVPinIndex = lRi->getValue(lPropIDPinIndex);
				if (NULL == lVPinIndex)
					{ assert(false); lSuccess = false; }
				if (!lPinsForClasses[lClass.mIndex].test(lVPinIndex->i))
				{
					mLogger.out() << "Unexpected pin in result: " << std::hex << LOCALPID(lRi->getPID()) << std::dec << " (pin index: " << lVPinIndex->i << ")" << std::endl;
					lSuccess = false;
				}
				lRi->destroy();
				lNumFound++;
			}	
			if (lSuccess && lNumFound < lPinsForClasses[lClass.mIndex].count())
			{
				mLogger.out() << "Missing results! (expected: " << (unsigned)lPinsForClasses[lClass.mIndex].count() << ", actual: " << lNumFound << ")" << std::endl;
				lSuccess = false;
			}

			if(lR) lR->destroy();
			lQ->destroy();
		}
		mLogger.out() << std::endl;

		for (i = 0; i < sNumPINs; i++)
			lPINs[i]->destroy();
		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	if (!lSuccess /* && !MVTApp::sDynamicLinkMvstore->isInProc()*/)
		mLogger.out() << "Warning: when running with ipc, failures may be caused by additional replication properties created on server side - make sure replication is disabled before running this test!" << std::endl;
	return lSuccess ? 0 : 1;
}

void TestNotifications::outputPINInfo(IPIN & pPIN, PropertyID pPropIDBase, long pOldMask, long pNewMask)
{
	std::vector<PropertyID> lPropIDs;
	int i;

	mLogger.out() << "pin: " << std::hex << LOCALPID(pPIN.getPID()) << std::dec << std::endl;

	mLogger.out() << "  was containing props: ";
	PITStoreClass::extractPropIDs(pPropIDBase, pOldMask, lPropIDs);
	for (i = 0; i < (int)lPropIDs.size(); i++)
		mLogger.out() << lPropIDs[i] << " ";
	mLogger.out() << std::endl;

	mLogger.out() << "  now containing props: ";
	PITStoreClass::extractPropIDs(pPropIDBase, pNewMask, lPropIDs);
	for (i = 0; i < (int)lPropIDs.size(); i++)
		mLogger.out() << lPropIDs[i] << " ";
	mLogger.out() << std::endl;
}
