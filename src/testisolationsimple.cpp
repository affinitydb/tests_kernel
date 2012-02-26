/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

// These macros could be deleted later on; their purpose is to
// help read the output of the test.  They signal which errors
// are "expected", on the assumption that isolation is actually
// not implemented.  These macros should start lying as and when
// isolation starts being implemented.
#define EXPECTED_FAILURE_WITH_NO_ISOLATION { if (!rcverify(false, false)) { mLogger.out() << "[Expecting failure at line " << __LINE__ << " due to absence of isolation]" << std::endl; } }
#define EXPECTED_FAILURE_WITH_NO_ISOLATION_INDEXOP { if (!rcverify(false, true)) { mLogger.out() << "[Expecting failure at line " << __LINE__ << " due to absence of isolation]" << std::endl; } }

class CWithSession
{
		ISession * mSession;
	public:
		CWithSession(ISession * pSession) : mSession(pSession) { if (mSession) mSession->attachToCurrentThread(); }
		~CWithSession() { if (mSession) mSession->detachFromCurrentThread(); }
		ISession * operator->() const { return mSession; }
		ISession * p() const { return mSession; }
		void clear() { mSession->terminate(); mSession = NULL; }
};

class CWithIFM
{
		ISession * const mSession;
		unsigned mOldIFM;
		bool const mAttach;
	public:
		CWithIFM(ISession * pSession, unsigned pNewIFM, bool pCond=true, bool pAttach=false)
			: mSession(pCond ? pSession : NULL)
			, mOldIFM(0)
			, mAttach(pAttach)
		{
			if (!mSession)
				return;
			CWithSession const lWS(mAttach ? mSession : NULL);
			mOldIFM = mSession->getInterfaceMode();
			mSession->setInterfaceMode(pNewIFM);
		}
		~CWithIFM()
		{
			if (!mSession)
				return;
			CWithSession const lWS(mAttach ? mSession : NULL);
			mSession->setInterfaceMode(mOldIFM);
		}
};

template <class T> void valueSet(AfyDB::Value & pV, T v) { pV.set(v); } // generic version.
template <> void valueSet(AfyDB::Value & pV, int64_t v) { pV.setI64(v); } // special because of the special name...
template <> void valueSet(AfyDB::Value & pV, uint64_t v) { pV.setU64(v); } // special because of the special name... (n.b. equivalent to setDateTime)
template <> void valueSet(AfyDB::Value & pV, float v) { pV.set(v); } // special because of the extra arguments...
template <> void valueSet(AfyDB::Value & pV, PID const & v) { pV.set(v); } // special because by reference...

// Publish this test.
class TestIsolationSimple : public ITest
{
	protected:
		// Note:
		// . none of the levels supported by mvstore should produce the kIIDirtyRead
		// . TXI_READ_COMMITTED may have kIIReadCommitted and kIIReadCommittedOnIndex
		// . TXI_REPEATABLE_READ may only have kIIReadCommittedOnIndex
		// . TXI_SERIALIZABLE should have none of the issues.
		enum eReadOnlyType { kROTExplicit=0, kROTImplicit, kROTEnd };
		enum eIsolationIssues { kIIDirtyRead=1, kIIReadCommitted, kIIReadCommittedOnIndex, kIIEnd };
		enum eBaseProps { kBPInt=0, kBPSmallStr, kBPLargeStr, kBPDeletedProp, kBPSmallColl, kBPLargeColl, kBPEnd, kBPInsertedProp=kBPEnd };
		enum ePid { kPNormal=0, kPReplicated, kPEnd };
		enum ePidRequirements { kPRBasics = (1 << 0), kPRLargeString = (1 << 1), kPRLargeCollection = (1 << 2), kPRAll = (kPRBasics | kPRLargeString | kPRLargeCollection) };
	protected:
		static int const sValueInt;
		static char const * const sValueStr;
		static int const sSmallCollSize;
		static int const sLargeCollSize;
		static char const * const sReadOnlyTypes[];
		static char const * const sTxiLevels[];
		static char const * const sSimulatedIssues[];
		static char const * const sPid[];
	protected:
		AfyKernel::StoreCtx *mStoreCtx;
		ISession * mSession1, * mSession2;
		PropertyID mPropIDs[215];
		PID mPIDs[kPEnd];
		ClassID mCLSID;
		Tstring mLargeString;
		int mCurrentReadOnlyType; // eReadOnlyType
		int mCurrentTxiLevel; // AfyDB::TXI_LEVEL
		int mCurrentlySimulatedIssue; // eIsolationIssues
		int mCurrentPid; // ePid
	public:
		TEST_DECLARE(TestIsolationSimple);
		virtual char const * getName() const { return "testisolationsimple"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "runs basic isolation tests"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Until the test passes..."; return false; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testUpdateInt();
		void testUpdateSmallString();
		void testUpdateLargeString();
		void testUpdateSmallCollection();
		void testUpdateLargeCollection();
		void testTransformSmallStringIntoLargeString(); // Review (maxw): will that cover all possible SSV configs? META_PROP_SSTORAGE?
		void testTransformLargeStringIntoSmallString();
		void testTransformSmallCollectionIntoLargeCollection();
		void testTransformLargeCollectionIntoSmallCollection();
		void testTransformSmallCollectionIntoScalar();
		void testTransformLargeCollectionIntoScalar();
		void testTransformScalarIntoSmallCollection();
		void testTransformScalarIntoLargeCollection();
		void testTransformScalarType();
		void testCreate1Pin(bool pTxNesting, bool pCommit = false);
		void testCreate1Property(bool pTxNesting, bool pCommit = false);
		void testDelete1Pin();
		void testDelete1Property();
		void testCreateLotsofPins();
		void testCreateLotsofProperties();
	protected:
		void createPIN(ePid pPID, long pRequirements=kPRAll, bool pAttach=false);
		PID getCurrentPID() { return mPIDs[mCurrentPid]; }
		void createBlankReplicatedPID(ISession & pSession, PID & pResult);
	protected:
		static bool findPin_session(ISession * pS, PID const & pPID);
		static bool findPin_fullscan(ISession * pS, ClassID pClsid, PID const & pPID);
		static bool findPin_class(ISession * pS, ClassID pClsid, PID const & pPID);
		static bool findPin_ft(ISession * pS, PID const & pPID, Tstring const & pTxt, unsigned long pMode = 0);
		static bool findPin_result(ICursor * pCursor, PID const & pPID);
		static Tstring readStream(AfyDB::IStream & pStream);
		static bool compareStreamToString(AfyDB::PropertyID pPropID, AfyDB::IPIN & pPIN, Tstring const & pExpected);
		static void extractSomeWords(Tstring const & pFrom, int pNum, Tstring & pExtracted);
	protected:
		bool isCurrentCaseExcluded(char const *& pReason) const
		{
			// With snapshot isolation (mCurrentReadOnlyType==kROTExplicit), it's
			// sufficient to just start a transaction in rcbeg(), to enforce the
			// same assumptions for read-committed as for simple dirty read.  But with
			// 2PL isolation, this is insufficient: we'd need to also read the pin.
			// However, if we did, because the current test is entirely single-threaded,
			// we'd freeze.  In a truly concurrent environment, the write tx would just
			// have to wait until the read-only tx is finished.  I consider that these
			// cases are already covered in tests that focus on locking, so for the moment,
			// I'm disabling these cases here.
			if (kROTImplicit == mCurrentReadOnlyType && simulatingrc())
			{
				pReason = "testisolationsimple doesn't handle simulations of read-committed with implicit read-only transactions";
				return true;
			}

			// Note:
			//   This function will allow to exclude unsupported cases, if any,
			//   by returning true for combinations of {mCurrentReadOnlyType, mCurrentTxiLevel,
			//   mCurrentlySimulatedIssue, mCurrentPid}.  Please document briefly each
			//   new case.
			return false;
		}
	protected:
		bool simulatingrc() const { return (kIIReadCommitted == mCurrentlySimulatedIssue) || (kIIReadCommittedOnIndex == mCurrentlySimulatedIssue); }
		void rcbeg(long pPinRequirements)
		{
			if (simulatingrc())
			{
				// Note:
				//   Since we're _committing_ changes in these versions of the tests,
				//   we recreate new pins every time (in their expected state);
				//   pPinRequirements allows to skip the expensive large properties
				//   when they're not needed.
				createPIN((ePid)mCurrentPid, pPinRequirements, true);

				// Note:
				//   See isCurrentCaseExcluded()...
				CWithSession const lWS(mSession2);
				TVERIFYRC(lWS->startTransaction(mCurrentReadOnlyType==kROTExplicit?TXT_READONLY:TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
			}
		}
		void rccommit()
		{
			if (simulatingrc())
			{
				CWithSession const lWS(mSession1);
				TVERIFYRC(lWS->commit());
			}
		}
		void rcend()
		{
			if (simulatingrc())
			{
				CWithSession const lWS(mSession2);
				TVERIFYRC(lWS->rollback());
			}
		}
		bool rcverify(bool pDirtyReadAssumption, bool pIndexOp = false)
		{
			// Note: This function transforms the final verification assumption of a test on the basis of
			//       a simulation of dirty read, adapted to the actual issue simulated (and
			//       the actual transaction isolation level used); when the assumption is inverted,
			//       the result is a weaker verification (it doesn't verify a specific value),
			//       but for now I think it's sufficient.
			switch (mCurrentlySimulatedIssue)
			{
				case kIIDirtyRead:
				{
					// If we're simulating dirty-read, then all assumptions hold, never mind the isolation level.
					return pDirtyReadAssumption;
				}
				case kIIReadCommitted:
				{
					// If we're simulating read-committed, then read-committed issues should be tolerated
					// (and actually expected) in TXI_READ_COMMITTED; otherwise, they should be treated
					// as if they were dirty-read.
					if (mCurrentTxiLevel == TXI_READ_COMMITTED)
						return !pDirtyReadAssumption;
					return pDirtyReadAssumption;
				}
				case kIIReadCommittedOnIndex:
				{
					// Since throughout this file we're repurposing the same tests to simulate all issues,
					// here we simply ignore the results of all tests not related to indexes.
					if (!pIndexOp)
						return true;
					// It is expected that the dirty-read-assumption will be relaxed for index operations,
					// both in TXI_READ_COMMITTED (the more general case)
					// and in TXI_REPEATABLE_READ (the case that applies specifically to index operations).
					if (mCurrentTxiLevel == TXI_READ_COMMITTED || mCurrentTxiLevel == TXI_REPEATABLE_READ)
						return !pDirtyReadAssumption;
					return pDirtyReadAssumption;
				}
			}
			TVERIFY(false); // Should never happen.
			return pDirtyReadAssumption;
		}
		void logSubtest(char const * pName)
		{
			mLogger.out() << "-> " << pName << std::endl;
			mLogger.out() << "   [isolation:" << sTxiLevels[mCurrentTxiLevel - TXI_READ_COMMITTED];
			mLogger.out() << " issue:" << sSimulatedIssues[mCurrentlySimulatedIssue - kIIDirtyRead];
			mLogger.out() << std::endl;
			mLogger.out() << "    pid:" << sPid[mCurrentPid];
			mLogger.out() << " rotx:" << sReadOnlyTypes[mCurrentReadOnlyType];
			mLogger.out() << "]" << std::endl;
		}
	protected:
		template <class T> class ScalarTypeChange
		{
			public:
				typedef T vt;
				AfyDB::ValueType const mVT;
				T const mV;
				ScalarTypeChange(AfyDB::ValueType pVT, T const & pV) : mVT(pVT), mV(pV) {}
		};
		template <class TT> void testScalarTypeChange(TT pTT, ISession * pS1, ISession * pS2)
		{
			// Convert to new type...
			logSubtest("testScalarTypeChange");
			mLogger.out() << "type=" << pTT.mVT << std::endl;
			rcbeg(kPRBasics);
			PropertyID const lPropID = mPropIDs[kBPInt];
			PID lPID = getCurrentPID();
			IPIN * lP1;
			{
				CWithSession const lWS(pS1);
				lP1 = lWS->getPIN(lPID);
				TVERIFY(AfyDB::VT_INT == lP1->getValue(lPropID)->type);
				TVERIFY(sValueInt == lP1->getValue(lPropID)->i);
				Value lV;
				valueSet(lV, pTT.mV); lV.type = pTT.mVT;/*For cases like u64 vs datetime...*/ lV.property = lPropID; lV.op = OP_SET;
				TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
				TVERIFYRC(lP1->modify(&lV, 1));
				TVERIFY(pTT.mVT == lP1->getValue(lPropID)->type);
				TVERIFY(pTT.mV == *((typename TT::vt *)&lP1->getValue(lPropID)->str));
			}
			rccommit();
			{
				CWithSession const lWS(pS2);
				CmvautoPtr<IPIN> lP2(lWS->getPIN(lPID));
				Value const * lV = lP2->getValue(lPropID);
				EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(AfyDB::VT_INT == lV->type));
				EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(sValueInt == lV->i));
			}
			// And back to old type...
			rcend(); rcbeg(kPRBasics);
			lPID = getCurrentPID();
			{
				CWithSession const lWS(mSession1);
				if (!simulatingrc())
					{ TVERIFYRC(lWS->commit()); }
				lP1->refresh();
				TVERIFY(pTT.mVT == lP1->getValue(lPropID)->type);
				TVERIFY(pTT.mV == *((typename TT::vt *)&lP1->getValue(lPropID)->str));
				Value lV;
				SETVALUE(lV, lPropID, sValueInt, OP_SET);
				TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
				TVERIFYRC(lP1->modify(&lV, 1));
				TVERIFY(AfyDB::VT_INT == lP1->getValue(lPropID)->type);
				TVERIFY(sValueInt == lP1->getValue(lPropID)->i);
			}
			rccommit();
			{
				CWithSession const lWS(pS2);
				CmvautoPtr<IPIN> lP2(lWS->getPIN(lPID));
				Value const * lV = lP2->getValue(lPropID);
				EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(pTT.mVT == lV->type));
				EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(pTT.mV == *((typename TT::vt *)&lV->str)));
			}
			// Cleanup.
			CWithSession const lWS(pS1);
			if (!simulatingrc())
				{ TVERIFYRC(lWS->commit()); }
			lP1->destroy();
			rcend();
		}
};
TEST_IMPLEMENT(TestIsolationSimple, TestLogger::kDStdOut);
int const TestIsolationSimple::sValueInt = 1;
char const * const TestIsolationSimple::sValueStr = "Once upon a time";
int const TestIsolationSimple::sSmallCollSize = 5;
int const TestIsolationSimple::sLargeCollSize = 5000;
char const * const TestIsolationSimple::sReadOnlyTypes[] = {"kROTExplicit", "kROTImplicit"};
char const * const TestIsolationSimple::sTxiLevels[] = {"TXI_READ_COMMITTED", "TXI_REPEATABLE_READ", "TXI_SERIALIZABLE"};
char const * const TestIsolationSimple::sSimulatedIssues[] = {"kIIDirtyRead", "kIIReadCommitted", "kIIReadCommittedOnIndex"};
char const * const TestIsolationSimple::sPid[] = {"kPNormal", "kPReplicated"};

int TestIsolationSimple::execute()
{
	// Open the store.
	int i;
	bool lSuccess = false;
	if (MVTApp::startStore())
	{
		lSuccess = true;

		// Create two sessions.
		mSession1 = MVTApp::startSession();
		mSession1->detachFromCurrentThread();
		mSession2 = MVTApp::startSession();
		mSession2->detachFromCurrentThread();
		mSession1->attachToCurrentThread();

		// Setup some properties, pins, classes for the test.
		mLogger.out() << "Preparing..." << std::endl;
		{
			// Properties.
			MVTUtil::mapStaticProperty(mSession1, "testisolationsimple.prop", mPropIDs[0]);
			MVTApp::mapURIs(mSession1, "testisolationsimple.prop", sizeof(mPropIDs) / sizeof(mPropIDs[0]) - 1, &mPropIDs[1]);
			MVTApp::randomString(mLargeString, 32000, 40000);

			// Pins.
			INITLOCALPID(mPIDs[kPNormal]);
			INITLOCALPID(mPIDs[kPReplicated]);
			createPIN(kPNormal);
			createPIN(kPReplicated);

			// Class.
			mCLSID = STORE_INVALID_CLASSID;
			Tstring const lClassName("testisolationsimple.class.1");
			if (RC_OK != mSession1->getClassID(lClassName.c_str(), mCLSID))
			{
				CmvautoPtr<IStmt> lQ(mSession1->createStmt());
				PropertyID  lProps[1];
				lProps[0] = mPropIDs[kBPInt];
				TVERIFYRC(lQ->setPropCondition(lQ->addVariable(), lProps, 1));
				TVERIFYRC(defineClass(mSession1, lClassName.c_str(), lQ, &mCLSID));
			}
		}
		mSession1->detachFromCurrentThread();

		// Run a series of simple tests, all following the same pattern:
		//   1) in session1, modify/create/delete something within a transaction
		//   2) in session2, verify that the modification done at step 1 is not visible
		//   3) cleanup
		// Note:
		//   Because all this is single-threaded, completion of step 2 relies on
		//   the snapshot isolation of read-only transactions; with strict 2PL,
		//   this step would just be stalled by the lock acquired at step 1;
		//   currently this goes through because 2PL is ~disabled; when 2PL
		//   is re-enabled, this might break until snapshot isolation is
		//   implemented.
		// More specifically, this pattern is adapted to produce all the specific
		// "problems" that apply to different transaction isolation levels, i.e.:
		//   a) kIIDirtyRead: plain as steps 1-2-3 above
		//   b) kIIReadCommitted: same as a), but before step 1 session2 starts a transaction, and after step 1 session1 commits
		//   c) kIIReadCommittedOnIndex: same as b), but only for subtests that perform an index scan (other tests just return in that case)
		// In other words, the dirty-read assumption is repurposed/reframed
		// within a different transaction context, but using ~the same code,
		// to simulate all other desired isolation "issues".
		// Note:
		//   The test verifies not only that these "problems" don't occur when the
		//   isolation level protects against them, but also verifies that they do occur
		//   when the level permits them.
		// Review:
		//   When it all works, we could decide to randomize a bit, mix commits and rollbacks,
		//   and run a longer loop; until then, no point clogging the output.
		// Review:
		//   Add a case for very long explicit read-only tx concurrent with lots of rw activity.

		for (mCurrentPid = kPNormal/*kPReplicated*/; mCurrentPid < kPEnd; mCurrentPid++)
		{
			mLogger.out() << std::endl;
			mLogger.out() << "***************************************" << std::endl;
			mLogger.out() << "* pid: " << sPid[mCurrentPid] << std::endl;
			mLogger.out() << "***************************************" << std::endl << std::endl;;

			CWithIFM const lwifm1(mSession1, ITF_REPLICATION, kPReplicated == mCurrentPid, true);
			CWithIFM const lwifm2(mSession2, ITF_REPLICATION, kPReplicated == mCurrentPid, true); // Review: we could fine-tune this one...

			#if 0 // To focus on a specific case.
			  mCurrentTxiLevel = TXI_REPEATABLE_READ; // TXI_READ_COMMITTED;
			#else
			  for (mCurrentTxiLevel = TXI_SERIALIZABLE; mCurrentTxiLevel >= TXI_READ_COMMITTED; mCurrentTxiLevel--)
			#endif
			{
				mLogger.out() << std::endl;
				mLogger.out() << " ***************************************" << std::endl;
				mLogger.out() << " * isolation level: " << sTxiLevels[mCurrentTxiLevel - TXI_READ_COMMITTED] << std::endl;
				mLogger.out() << " ***************************************" << std::endl << std::endl;;

				#if 0 // To focus on a specific case.
				  mCurrentlySimulatedIssue = kIIReadCommitted; // kIIReadCommittedOnIndex;
				#else
				  for (mCurrentlySimulatedIssue = kIIDirtyRead; mCurrentlySimulatedIssue < kIIEnd; mCurrentlySimulatedIssue++)
				#endif
				{
					mLogger.out() << std::endl;
					mLogger.out() << "  ---------------------------------------" << std::endl;
					mLogger.out() << "  - simulated issue: " << sSimulatedIssues[mCurrentlySimulatedIssue - kIIDirtyRead] << std::endl;
					mLogger.out() << "  ---------------------------------------" << std::endl << std::endl;;

					for (mCurrentReadOnlyType = kROTExplicit/*kROTImplicit*/; mCurrentReadOnlyType < kROTEnd; mCurrentReadOnlyType++)
					{
						mLogger.out() << std::endl;
						mLogger.out() << "   ---------------------------------------" << std::endl;
						mLogger.out() << "   - read-only type: " << sReadOnlyTypes[mCurrentReadOnlyType - kROTExplicit] << std::endl;
						mLogger.out() << "   ---------------------------------------" << std::endl << std::endl;;

						for (i = 0; i < 1; i++)
						{
							char const * lReason = NULL;
							if (isCurrentCaseExcluded(lReason))
							{
								mLogger.out() << "case excluded (" << sTxiLevels[mCurrentTxiLevel - TXI_READ_COMMITTED] << "+" << sSimulatedIssues[mCurrentlySimulatedIssue - kIIDirtyRead] << "+" << sPid[mCurrentPid] << "+" << sReadOnlyTypes[mCurrentReadOnlyType - kROTExplicit] << "):" << std::endl;
								mLogger.out() << "  reason: " << lReason << std::endl;
								continue;
							}
							rcbeg(kPRBasics); testUpdateInt(); rcend();
							rcbeg(kPRBasics); testUpdateSmallString(); rcend();
							rcbeg(kPRLargeString); testUpdateLargeString(); rcend();
							rcbeg(kPRBasics); testUpdateSmallCollection(); rcend();
							rcbeg(kPRLargeCollection); testUpdateLargeCollection(); rcend();
							rcbeg(kPRBasics); testTransformSmallStringIntoLargeString(); rcend();
							rcbeg(kPRLargeString); testTransformLargeStringIntoSmallString(); rcend();
							rcbeg(kPRBasics); testTransformSmallCollectionIntoLargeCollection(); rcend();
							rcbeg(kPRLargeCollection); testTransformLargeCollectionIntoSmallCollection(); rcend();
							rcbeg(kPRBasics); testTransformSmallCollectionIntoScalar(); rcend();
							rcbeg(kPRLargeCollection); testTransformLargeCollectionIntoScalar(); rcend();
							rcbeg(kPRBasics); testTransformScalarIntoSmallCollection(); rcend();
							rcbeg(kPRBasics); testTransformScalarIntoLargeCollection(); rcend();
							testTransformScalarType(); // Note: does rcbeg-rcend internally.
							rcbeg(kPRBasics); testCreate1Pin(false); rcend();
							rcbeg(kPRBasics); testCreate1Pin(true); rcend();
							rcbeg(kPRBasics); testCreate1Property(false); rcend();
							rcbeg(kPRBasics); testCreate1Property(true); rcend();
							rcbeg(kPRBasics); testDelete1Pin(); rcend();
							rcbeg(kPRBasics); testDelete1Property(); rcend();
							rcbeg(kPRBasics); testCreateLotsofPins(); rcend();
							rcbeg(kPRBasics); testCreateLotsofProperties(); rcend();
						}
					}
				}
			}
		}

		// Cleanup.
		{ CWithSession lWS(mSession1); lWS->deletePINs(mPIDs, sizeof(mPIDs) / sizeof(mPIDs[0]), MODE_PURGE); lWS.clear(); }
		{ CWithSession lWS(mSession2); lWS.clear(); }
		MVTApp::stopStore();
	}

	return lSuccess ? 0 : 1;
}

void TestIsolationSimple::testUpdateInt()
{
	logSubtest("testUpdateInt");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV;
		SETVALUE(lV, mPropIDs[kBPInt], 1000, OP_SET);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
	}
	rccommit();
	{
		// Note:
		//   All this is expected to be treated as explicitly read-only, i.e.
		//   either an explicit read-only transaction was opened
		//   by the caller (in rcbeg), or, in other modes, there's no explicit
		//   transaction in this session but the code below is expected to
		//   be handled as a single-op read-only tx.
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		Value const * lV = lP2->getValue(mPropIDs[kBPInt]);
		TVERIFY(AfyDB::VT_INT == lV->type);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(sValueInt == lV->i));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testUpdateSmallString()
{
	logSubtest("testUpdateSmallString");
	char const * const lNewStr = "there were three little pigs";
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		TVERIFY(findPin_ft(lWS.p(), getCurrentPID(), sValueStr, MODE_ALL_WORDS));
		Value lV;
		SETVALUE(lV, mPropIDs[kBPSmallStr], lNewStr, OP_SET);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
		TVERIFY(findPin_ft(lWS.p(), getCurrentPID(), lNewStr, MODE_ALL_WORDS));
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		Value const * lV = lP2->getValue(mPropIDs[kBPSmallStr]);
		TVERIFY(AfyDB::VT_STRING == lV->type);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(0 == strcmp(lV->str, sValueStr)));
		EXPECTED_FAILURE_WITH_NO_ISOLATION_INDEXOP; TVERIFY(rcverify(findPin_ft(lWS.p(), getCurrentPID(), sValueStr, MODE_ALL_WORDS), true));
		EXPECTED_FAILURE_WITH_NO_ISOLATION_INDEXOP; TVERIFY(rcverify(!findPin_ft(lWS.p(), getCurrentPID(), lNewStr, MODE_ALL_WORDS), true));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testUpdateLargeString()
{
	logSubtest("testUpdateLargeString");
	char const * const lNewSegment = "Jamie Lee Curtis";
	Tstring lSomeWords;
	extractSomeWords(mLargeString, 3, lSomeWords);
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		TVERIFY(compareStreamToString(mPropIDs[kBPLargeStr], *lP1, mLargeString));
		TVERIFY(findPin_ft(lWS.p(), getCurrentPID(), lSomeWords.c_str(), MODE_ALL_WORDS));
		Value lV;
		Tstring lNS(" "); lNS += lNewSegment; lNS += " ";
		lV.setEdit(lNS.c_str(), lNS.length(), 20, 0); lV.property = mPropIDs[kBPLargeStr];
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
		TVERIFY(findPin_ft(lWS.p(), getCurrentPID(), lNewSegment, MODE_ALL_WORDS)); // This fails... is it a bug? (unrelated with isolation)
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(compareStreamToString(mPropIDs[kBPLargeStr], *lP2.Get(), mLargeString)));
		TVERIFY(rcverify(findPin_ft(lWS.p(), getCurrentPID(), lSomeWords.c_str(), MODE_ALL_WORDS), true)); // This actually doesn't fail... is it a bug?
		TVERIFY(rcverify(!findPin_ft(lWS.p(), getCurrentPID(), lNewSegment, MODE_ALL_WORDS), true)); // This actually doesn't fail... is it a bug?
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testUpdateSmallCollection()
{
	logSubtest("testUpdateSmallCollection");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		TVERIFY(size_t(sSmallCollSize) == MVTApp::getCollectionLength(*lP1->getValue(mPropIDs[kBPSmallColl])));
		Value lV;
		SETVALUE_C(lV, mPropIDs[kBPSmallColl], 1000, OP_ADD, STORE_LAST_ELEMENT);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
		TVERIFY(size_t(sSmallCollSize + 1) == MVTApp::getCollectionLength(*lP1->getValue(mPropIDs[kBPSmallColl])));
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(size_t(sSmallCollSize) == MVTApp::getCollectionLength(*lP1->getValue(mPropIDs[kBPSmallColl]))));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testUpdateLargeCollection()
{
	logSubtest("testUpdateLargeCollection");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		TVERIFY(size_t(sLargeCollSize) == MVTApp::getCollectionLength(*lP1->getValue(mPropIDs[kBPLargeColl]))); // See line 130...
		Value lV;
		SETVALUE_C(lV, mPropIDs[kBPLargeColl], 1000, OP_ADD, STORE_LAST_ELEMENT);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
		TVERIFY(size_t(sLargeCollSize + 1) == MVTApp::getCollectionLength(*lP1->getValue(mPropIDs[kBPLargeColl]))); // This fails... is it a bug? (unrelated with isolation)
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		TVERIFY(size_t(sLargeCollSize) == MVTApp::getCollectionLength(*lP1->getValue(mPropIDs[kBPLargeColl]))); // This actually doesn't fail... is it a bug?
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testTransformSmallStringIntoLargeString()
{
	logSubtest("testTransformSmallStringIntoLargeString");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV;
		SETVALUE(lV, mPropIDs[kBPSmallStr], mLargeString.c_str(), OP_SET);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
		TVERIFY(compareStreamToString(mPropIDs[kBPSmallStr], *lP1, mLargeString));
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		Value const * lV = lP2->getValue(mPropIDs[kBPSmallStr]);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(AfyDB::VT_STRING == lV->type && 0 == strcmp(lV->str, sValueStr)));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testTransformLargeStringIntoSmallString()
{
	logSubtest("testTransformLargeStringIntoSmallString");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV;
		SETVALUE(lV, mPropIDs[kBPLargeStr], sValueStr, OP_SET);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
		Value const * lVv = lP1->getValue(mPropIDs[kBPLargeStr]);
		TVERIFY(AfyDB::VT_STRING == lVv->type && 0 == strcmp(lVv->str, sValueStr));
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(compareStreamToString(mPropIDs[kBPLargeStr], *lP2.Get(), mLargeString)));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testTransformSmallCollectionIntoLargeCollection()
{
	logSubtest("testTransformSmallCollectionIntoLargeCollection");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV; int i;
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		for (i = sSmallCollSize; i < sLargeCollSize; i++)
		{
			SETVALUE_C(lV, mPropIDs[kBPSmallColl], 1 + i, OP_ADD, STORE_LAST_ELEMENT);
			TVERIFYRC(lP1->modify(&lV, 1));
		}
		TVERIFY(size_t(sLargeCollSize) == MVTApp::getCollectionLength(*lP1->getValue(mPropIDs[kBPSmallColl])));
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(size_t(sSmallCollSize) == MVTApp::getCollectionLength(*lP2->getValue(mPropIDs[kBPSmallColl]))));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testTransformLargeCollectionIntoSmallCollection()
{
	logSubtest("testTransformLargeCollectionIntoSmallCollection");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV; int i;
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		for (i = sSmallCollSize; i < sLargeCollSize; i++)
		{
			lV.setDelete(mPropIDs[kBPLargeColl], STORE_LAST_ELEMENT);
			TVERIFYRC(lP1->modify(&lV, 1));
		}
		TVERIFY(size_t(sSmallCollSize) == MVTApp::getCollectionLength(*lP1->getValue(mPropIDs[kBPLargeColl]))); // Note: failure related to the issue at line 130...
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		// Review (maxw): Not 100% sure what this kind of transition means in terms of TXI_REPEATABLE_READ...
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(size_t(sLargeCollSize) == MVTApp::getCollectionLength(*lP2->getValue(mPropIDs[kBPLargeColl]))));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testTransformSmallCollectionIntoScalar()
{
	logSubtest("testTransformSmallCollectionIntoScalar");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV;
		SETVALUE(lV, mPropIDs[kBPSmallColl], sValueInt, OP_SET);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
		Value const * lVv = lP1->getValue(mPropIDs[kBPSmallColl]);
		TVERIFY(AfyDB::VT_INT == lVv->type && sValueInt == lVv->i);
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		Value const * lV = lP2->getValue(mPropIDs[kBPSmallColl]);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(size_t(sSmallCollSize) == MVTApp::getCollectionLength(*lV)));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testTransformLargeCollectionIntoScalar()
{
	logSubtest("testTransformLargeCollectionIntoScalar");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV;
		SETVALUE(lV, mPropIDs[kBPLargeColl], sValueInt, OP_SET);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
		Value const * lVv = lP1->getValue(mPropIDs[kBPLargeColl]);
		TVERIFY(AfyDB::VT_INT == lVv->type && sValueInt == lVv->i);
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		Value const * lV = lP2->getValue(mPropIDs[kBPLargeColl]);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(size_t(sLargeCollSize) == MVTApp::getCollectionLength(*lV)));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testTransformScalarIntoSmallCollection()
{
	logSubtest("testTransformScalarIntoSmallCollection");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV; int i;
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		for (i = 1; i < sSmallCollSize; i++)
		{
			SETVALUE_C(lV, mPropIDs[kBPInt], 1 + i, OP_ADD, STORE_LAST_ELEMENT);
			TVERIFYRC(lP1->modify(&lV, 1));
		}
		TVERIFY(size_t(sSmallCollSize) == MVTApp::getCollectionLength(*lP1->getValue(mPropIDs[kBPInt])));
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		Value const * lV = lP2->getValue(mPropIDs[kBPInt]);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(AfyDB::VT_INT == lV->type && sValueInt == lV->i));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testTransformScalarIntoLargeCollection()
{
	logSubtest("testTransformScalarIntoLargeCollection");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV; int i;
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		for (i = 1; i < sLargeCollSize; i++)
		{
			SETVALUE_C(lV, mPropIDs[kBPInt], 1 + i, OP_ADD, STORE_LAST_ELEMENT);
			TVERIFYRC(lP1->modify(&lV, 1));
		}
		TVERIFY(size_t(sLargeCollSize) == MVTApp::getCollectionLength(*lP1->getValue(mPropIDs[kBPInt])));
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		Value const * lV = lP2->getValue(mPropIDs[kBPInt]);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(AfyDB::VT_INT == lV->type && sValueInt == lV->i));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testTransformScalarType()
{
	// This is maybe a little bit maniac, but while we're at it, might as well check just in case...
	// Review (maxw): Could add more types...
	testScalarTypeChange(ScalarTypeChange<unsigned int>(VT_UINT, 0x1000), mSession1, mSession2);
	testScalarTypeChange(ScalarTypeChange<int64_t>(VT_INT64, 0x1000000000000000LL), mSession1, mSession2);
	testScalarTypeChange(ScalarTypeChange<float>(VT_FLOAT, 123.456789f), mSession1, mSession2);
	testScalarTypeChange(ScalarTypeChange<bool>(VT_BOOL, false), mSession1, mSession2);
	testScalarTypeChange(ScalarTypeChange<uint64_t>(VT_DATETIME, 0x1000100010001000LL), mSession1, mSession2);
	testScalarTypeChange(ScalarTypeChange<PID>(VT_REFID, getCurrentPID()), mSession1, mSession2);
}

void TestIsolationSimple::testCreate1Pin(bool pTxNesting, bool pCommit)
{
	logSubtest("testCreate1Pin");
	mLogger.out() << (pTxNesting ? "nested" : "plain") << " tx)" << std::endl;
	PID lPID;
	{
		CWithSession const lWS(mSession1);
		if (pTxNesting)
			{ TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel)); }
		Value lPVs[1];
		SETVALUE(lPVs[0], mPropIDs[kBPInt], 1, OP_SET);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lWS->createPIN(lPID, lPVs, 1));
		TVERIFY(findPin_session(lWS.p(), lPID));
		TVERIFY(findPin_fullscan(lWS.p(), mCLSID, lPID));
		TVERIFY(findPin_class(lWS.p(), mCLSID, lPID));
		// TODO: also track by reference (e.g. add a reference to lPID in a large coll)
		if (pTxNesting)
			{ TVERIFYRC(lWS->commit()); }
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(!findPin_session(lWS.p(), lPID))); // I presume that this is not indexop...
		EXPECTED_FAILURE_WITH_NO_ISOLATION_INDEXOP; TVERIFY(rcverify(!findPin_fullscan(lWS.p(), mCLSID, lPID), true));
		EXPECTED_FAILURE_WITH_NO_ISOLATION_INDEXOP; TVERIFY(rcverify(!findPin_class(lWS.p(), mCLSID, lPID), true));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ if (pCommit) { TVERIFYRC(lWS->commit()); } else { TVERIFYRC(lWS->rollback()); } }
}

void TestIsolationSimple::testCreate1Property(bool pTxNesting, bool pCommit)
{
	logSubtest("testCreate1Property");
	mLogger.out() << (pTxNesting ? "nested" : "plain") << " tx)" << std::endl;
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		if (pTxNesting)
			{ TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel)); }
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV;
		SETVALUE(lV, mPropIDs[kBPInsertedProp], 1, OP_SET);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
		Value const * lVt = lP1->getValue(mPropIDs[kBPInsertedProp]);
		TVERIFY(lVt && lVt->i == 1);
		if (pTxNesting)
			{ TVERIFYRC(lWS->commit()); }
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		Value const * lV = lP2->getValue(mPropIDs[kBPInsertedProp]);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(!lV));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ if (pCommit) { TVERIFYRC(lWS->commit()); } else { TVERIFYRC(lWS->rollback()); } }
	lP1->destroy();
}

void TestIsolationSimple::testDelete1Pin()
{
	logSubtest("testDelete1Pin");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->deletePIN());
		TVERIFY(!lWS->getPIN(getCurrentPID())); // Review (maxw): not sure what's the intended behavior here...
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		IPIN * lP2 = lWS->getPIN(getCurrentPID());
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(lP2!=NULL)); // I presume that this is not indexop...
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testDelete1Property()
{
	logSubtest("testDelete1Property");
	IPIN * lP1;
	{
		CWithSession const lWS(mSession1);
		lP1 = lWS->getPIN(getCurrentPID());
		Value lV;
		lV.setDelete(mPropIDs[kBPDeletedProp]);
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		TVERIFYRC(lP1->modify(&lV, 1));
		Value const * lVt = lP1->getValue(mPropIDs[kBPDeletedProp]);
		TVERIFY(!lVt);
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		Value const * lV = lP2->getValue(mPropIDs[kBPDeletedProp]);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(lV!=NULL));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();
}

void TestIsolationSimple::testCreateLotsofPins()
{
	logSubtest("testCreateLotsofPins");
	typedef std::vector<PID> TPIDs;
	typedef std::vector<IPIN *> TPINs;
	TPINs::iterator iPin;
	TPIDs::iterator iPid;
	TPIDs lPIDs;
	bool lOk;
	{
		CWithSession const lWS(mSession1);
		Value lPVs[2]; Tstring lStr; int j; TPINs lBucket;
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		for (j = 0; j < 1000; j++)
		{
			MVTApp::randomString(lStr, 200, 300);
			SETVALUE(lPVs[0], mPropIDs[kBPInt], j, OP_SET);
			SETVALUE(lPVs[1], mPropIDs[kBPSmallStr], lStr.c_str(), OP_SET);
			IPIN * const lPIN = lWS->createUncommittedPIN(lPVs, 2, MODE_COPY_VALUES);
			lBucket.push_back(lPIN);
		}
		TVERIFYRC(lWS->commitPINs(&lBucket[0], (unsigned)lBucket.size()));
		for (iPin = lBucket.begin(); iPin != lBucket.end(); iPin++) { lPIDs.push_back((*iPin)->getPID()); }
		for (iPid = lPIDs.begin(), lOk = true; iPid != lPIDs.end() && lOk; iPid++) { lOk = findPin_session(lWS.p(), *iPid); } TVERIFY(lOk);
		for (iPid = lPIDs.begin(), lOk = true; iPid != lPIDs.end() && lOk; iPid++) { lOk = findPin_fullscan(lWS.p(), mCLSID, *iPid); } TVERIFY(lOk);
		for (iPid = lPIDs.begin(), lOk = true; iPid != lPIDs.end() && lOk; iPid++) { lOk = findPin_class(lWS.p(), mCLSID, *iPid); } TVERIFY(lOk);
		// Review: Could add ft also...
		for (iPin = lBucket.begin(); iPin != lBucket.end(); iPin++) { (*iPin)->destroy(); }
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; for (iPid = lPIDs.begin(), lOk = true; iPid != lPIDs.end() && lOk; iPid++) { lOk = !findPin_session(lWS.p(), *iPid); } TVERIFY(rcverify(lOk)); // I presume that this is not indexop...
		EXPECTED_FAILURE_WITH_NO_ISOLATION_INDEXOP; for (iPid = lPIDs.begin(), lOk = true; iPid != lPIDs.end() && lOk; iPid++) { lOk = !findPin_fullscan(lWS.p(), mCLSID, *iPid); } TVERIFY(rcverify(lOk, true));
		EXPECTED_FAILURE_WITH_NO_ISOLATION_INDEXOP; for (iPid = lPIDs.begin(), lOk = true; iPid != lPIDs.end() && lOk; iPid++) { lOk = !findPin_class(lWS.p(), mCLSID, *iPid); } TVERIFY(rcverify(lOk, true));
		// Review: Could add ft also...
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	// Note: I'm also getting
	//   mvstore: HeapPage 00000005 corrupt after read: invalid total allocated length
	//   pgheap.cpp:951: void AfyKernel::HeapPageMgr::HeapPage::compact(AfyKernel::PageIdx, bool): Assertion `page->freeSpaceLength==freeSpaceLength+scatteredFreeSpace' failed.

	// Review: Could also commit, then do the inverse (delete lots of pins); but since I get the failure above, I'll wait...
}

void TestIsolationSimple::testCreateLotsofProperties()
{
	logSubtest("testCreateLotsofProperties");
	typedef std::vector<Tstring> Tstrings;
	Tstrings lStrings;
	IPIN * lP1;
	bool lOk;
	{
		CWithSession const lWS(mSession1);
		Value lV; Tstring lStr; int j;
		lP1 = lWS->getPIN(getCurrentPID());
		TVERIFYRC(lWS->startTransaction(TXT_READWRITE, (AfyDB::TXI_LEVEL)mCurrentTxiLevel));
		for (j = 0; j < 200; j++)
		{
			MVTApp::randomString(lStr, 50, 100);
			lStrings.push_back(lStr);
			SETVALUE(lV, mPropIDs[kBPEnd + j], lStr.c_str(), OP_SET);
			TVERIFYRC(lP1->modify(&lV, 1));
		}
		for (j = 0, lOk = true; j < 200 && lOk; j++)
		{
			Value const * lVt = lP1->getValue(mPropIDs[kBPEnd + j]);
			lOk = (lVt && AfyDB::VT_STRING == lVt->type && lStrings[j] == lVt->str);
		}
		TVERIFY(lOk);
	}
	rccommit();
	{
		CWithSession const lWS(mSession2);
		CmvautoPtr<IPIN> lP2(lWS->getPIN(getCurrentPID()));
		int j;
		for (j = 0, lOk = true; j < 200 && lOk; j++)
			lOk = !lP2->getValue(mPropIDs[kBPEnd + j]);
		EXPECTED_FAILURE_WITH_NO_ISOLATION; TVERIFY(rcverify(lOk));
	}
	CWithSession const lWS(mSession1);
	if (!simulatingrc())
		{ TVERIFYRC(lWS->rollback()); }
	lP1->destroy();

	// Review: Could also commit, then do the inverse (delete lots of properties).
}

// Misc helpers...
void TestIsolationSimple::createPIN(ePid pPID, long pRequirements, bool pAttach)
{
	CWithIFM const lwifm(mSession1, ITF_REPLICATION, kPReplicated == pPID, pAttach);
	CWithSession const lWS(pAttach ? mSession1 : NULL);
	Value lPVs[kBPEnd];
	SETVALUE(lPVs[0], mPropIDs[kBPInt], sValueInt, OP_SET);
	SETVALUE(lPVs[1], mPropIDs[kBPSmallStr], sValueStr, OP_SET);
	SETVALUE(lPVs[2], mPropIDs[kBPLargeStr], (0 != (pRequirements & kPRLargeString)) ? mLargeString.c_str() : "undefined", OP_SET);
	SETVALUE(lPVs[3], mPropIDs[kBPDeletedProp], 1, OP_SET);
	SETVALUE(lPVs[4], mPropIDs[kBPSmallColl], 1, OP_SET);
	SETVALUE(lPVs[5], mPropIDs[kBPLargeColl], "value1", OP_SET);
	IPIN * lP = NULL;
	switch (pPID)
	{
		case kPNormal:
		{
			if (STORE_INVALID_PID != mPIDs[kPNormal].pid)
			{
				mSession1->deletePINs(&mPIDs[kPNormal], 1, MODE_PURGE);
				INITLOCALPID(mPIDs[kPNormal]);
			}

			TVERIFYRC(mSession1->createPIN(mPIDs[kPNormal], lPVs, 6));
			lP = mSession1->getPIN(mPIDs[kPNormal]);
			break;
		}
		case kPReplicated:
		{
			if (STORE_INVALID_PID != mPIDs[kPNormal].pid)
			{
				mSession1->deletePINs(&mPIDs[kPReplicated], 1, MODE_PURGE);
				INITLOCALPID(mPIDs[kPReplicated]);
			}

			createBlankReplicatedPID(*mSession1, mPIDs[kPReplicated]);
			lP = mSession1->getPIN(mPIDs[kPReplicated]);
			TVERIFYRC(lP->modify(lPVs, 6));
			break;
		}
		default:
			break;
	}
	if (NULL == lP)
	{
		TVERIFY(false);
		return;
	}
	TVERIFYRC(mSession1->startTransaction());
	int i;
	for (i = 1; i < sSmallCollSize; i++)
	{
		SETVALUE_C(lPVs[0], mPropIDs[kBPSmallColl], 1 + i, OP_ADD, STORE_LAST_ELEMENT);
		TVERIFYRC(lP->modify(&lPVs[0], 1));
	}
	if (0 != (pRequirements & kPRLargeCollection))
	{
		char lStr[32];
		for (i = 1; i < sLargeCollSize; i++)
		{
			sprintf(lStr, "value%d", 1 + i);
			SETVALUE_C(lPVs[0], mPropIDs[kBPLargeColl], lStr, OP_ADD, STORE_LAST_ELEMENT);
			TVERIFYRC(lP->modify(&lPVs[0], 1));
		}
	}
	TVERIFYRC(mSession1->commit());
	TVERIFY(size_t(sSmallCollSize) == MVTApp::getCollectionLength(*lP->getValue(mPropIDs[kBPSmallColl])));
	if (0 != (pRequirements & kPRLargeCollection))
		TVERIFY(size_t(sLargeCollSize) == MVTApp::getCollectionLength(*lP->getValue(mPropIDs[kBPLargeColl])));
	lP->destroy();
}
void TestIsolationSimple::createBlankReplicatedPID(ISession & pSession, PID & pResult)
{
	ushort const lStoreid1 = pSession.getLocalStoreID() + 0x1000;
	int lStartIndex;
	for (lStartIndex = 1; ; lStartIndex++)
	{
		PID lPID;
		INITLOCALPID(lPID); LOCALPID(lPID) = (uint64_t(lStoreid1) << STOREID_SHIFT) + lStartIndex;
		IPIN * const lPIN = pSession.getPIN(lPID);
		if (lPIN)
			lPIN->destroy();
		else
			break;
	}

	CWithIFM const lwifm(&pSession, ITF_REPLICATION);
	INITLOCALPID(pResult);
	LOCALPID(pResult) = (uint64_t(lStoreid1) << STOREID_SHIFT) + lStartIndex;
	IPIN * const lP = pSession.createUncommittedPIN(NULL, 0, PIN_REPLICATED, &pResult);
	TVERIFYRC(pSession.commitPINs(&lP, 1));
	lP->destroy();
}
bool TestIsolationSimple::findPin_session(ISession * pS, PID const & pPID)
{
	CmvautoPtr<IPIN> lP(pS->getPIN(pPID));
	return lP.IsValid();
}
bool TestIsolationSimple::findPin_fullscan(ISession * pS, ClassID pClsid, PID const & pPID)
{
	IPIN * lCls;
	if (RC_OK != pS->getClassInfo(pClsid, lCls))
		return false;
	Value const * const lClsV = lCls->getValue(PROP_SPEC_PREDICATE);
	if (!lClsV || lClsV->type != AfyDB::VT_STMT)
		return false;
	CmvautoPtr<IStmt> lQ(lClsV->stmt->clone());
	ICursor* lC = NULL;
	lQ->execute(&lC);
	CmvautoPtr<ICursor> lR(lC);
	lCls->destroy();
	return findPin_result(lR, pPID);
}
bool TestIsolationSimple::findPin_class(ISession * pS, ClassID pClsid, PID const & pPID)
{
	// Note (maxw):
	//   There is a known issue in the store presently, causing the classes to
	//   become dysfunctional after shutdown... as a result, this is not returning
	//   the expected result when run the second time on an existing store.

	CmvautoPtr<IStmt> lQ(pS->createStmt());
	ClassSpec cs; cs.classID = pClsid; cs.nParams = 0; cs.params = NULL;
	lQ->addVariable(&cs, 1);
	ICursor* lC = NULL;
	lQ->execute(&lC);
	CmvautoPtr<ICursor> lR(lC);
	return findPin_result(lR, pPID);
}
bool TestIsolationSimple::findPin_ft(ISession * pS, PID const & pPID, Tstring const & pTxt, unsigned long pMode)
{
	CmvautoPtr<IStmt> lQ(pS->createStmt());
	lQ->setConditionFT(lQ->addVariable(), pTxt.c_str());
	ICursor* lC = NULL;
	lQ->execute(&lC, NULL, 0, ~0, 0, pMode);
	CmvautoPtr<ICursor> lR(lC);
	return findPin_result(lR, pPID);
}
bool TestIsolationSimple::findPin_result(ICursor * pCursor, PID const & pPID)
{
	bool lFound = false;
	IPIN * lP;
	for (lP = pCursor->next(); lP && !lFound; lP = pCursor->next())
	{
		lFound = lP->getPID() == pPID;
		lP->destroy();
	}
	return lFound;
}
Tstring TestIsolationSimple::readStream(AfyDB::IStream & pStream)
{
	std::stringstream lResult;
	char lBuf[0x1000];
	size_t lRead;
	while (0 != (lRead = pStream.read(lBuf, 0x1000)))
		lResult.write(lBuf, lRead);
	lResult << std::ends;
	return lResult.str().c_str();
}
bool TestIsolationSimple::compareStreamToString(AfyDB::PropertyID pPropID, AfyDB::IPIN & pPIN, Tstring const & pExpected)
{
	Value const * lV = pPIN.getValue(pPropID);
	if (AfyDB::VT_STREAM != lV->type)
		return false;
	return (readStream(*lV->stream.is) == pExpected);
}
void TestIsolationSimple::extractSomeWords(Tstring const & pFrom, int pNum, Tstring & pExtracted)
{
	char * lCpy = new char[1 + pFrom.length()];
	strcpy(lCpy, pFrom.c_str());
	strtok(lCpy, " "); // skip the first word
	int i, j;
	for (i = 0; i < pNum; i++)
	{
		char * const lE = strtok(NULL, " ");
		if (!lE)
			break;
		pExtracted += lE;
		pExtracted += " ";
		for (j = 0; j < i; j++) // skip some words (could be further randomized)
			strtok(NULL, " ");
	}
	delete [] lCpy;
}
