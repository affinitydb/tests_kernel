/**************************************************************************************

Copyright 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"
#include "md5stream.h"
#include <limits>
#include <math.h>

#define PART1 1
#define PART2 1
#define PART3 1
#define PART4 1
#define PART5 1
#define DEFAULT_KEY_DENSITY 100
#define DEFAULT_KEY_MAXLEN 255

// Publish this test.
class TestPGTree : public ITest
{
	public:
		// Note:
		//   When this test was written, the store used to coerce keys to a uniform type at insertion time;
		//   the type was determined either by the very first insertion (what we use in this test - for convenience),
		//   or by a specification when defining the family; if a value couldn't be coerced to an index's
		//   type, insertion in the index would fail. Now, the store supports mixed-type indexes natively:
		//   . you either force the type of index specifying the type of parameter, e.g. v[1].setParam(0,VT_DATETIME)
		//   . or the type of index is 'variable' - which will retain the type of data being indexed
		//   . unless you specify ORD_NCASE in flags, or the lhs of index spec contains functions UPPER, LOWER, SUBSTR (in which case the type of index is forced to VT_STRING).
		template <ValueType Type> class CompareValues
		{
			public:
				static ValueType getType() { return Type; }
				static bool isEquivalentType(ValueType pVT)
				{
					if (isInteger(pVT)) return isInteger(Type);
					if (isNumber(pVT)) return isNumber(Type);
					if (isString(Type)) return true;
					return false;
				}
				static bool isInteger(ValueType pVT) { return pVT == VT_INT || pVT == VT_INT64 || pVT == VT_UINT || pVT == VT_UINT64; }
				static bool isNumber(ValueType pVT) { return pVT >= VT_INT && pVT <= VT_DOUBLE; }
				static bool isString(ValueType pVT) { return pVT >= VT_STRING && pVT <= VT_BSTR; }
			protected:
				bool isInteger(Value const & pV) const { return isInteger((ValueType)pV.type); }
				int64_t getInteger(Value const & pV) const
				{
					switch (pV.type)
					{
						case VT_INT: return int64_t(pV.i);
						case VT_INT64: return pV.i64;
						case VT_UINT: return int64_t(pV.ui);
						case VT_UINT64: return int64_t(pV.ui64);
						default: assert(false); break;
					}
					return 0;
				}
				char * coerceToString(Value const & pV, char * pBuf) const
				{
					switch (pV.type)
					{
						case VT_INT: sprintf(pBuf, "%d", pV.i); return pBuf;
						case VT_INT64: sprintf(pBuf, _LD_FM, pV.i64); return pBuf;
						case VT_UINT: sprintf(pBuf, "%u", pV.ui); return pBuf;
						case VT_UINT64: sprintf(pBuf, _LU_FM, pV.ui64); return pBuf;
						default: assert(false); break;
					}
					return NULL;
				}
				double coerceToDouble(Value const & pV) const
				{
					switch (pV.type)
					{
						case VT_DOUBLE: return pV.d;
						case VT_INT: return (double)pV.i;
						case VT_INT64: return (double)pV.i64;
						case VT_UINT: return (double)pV.ui;
						case VT_UINT64: return (double)pV.ui64;
						case VT_FLOAT: return (double)pV.f;
						default: assert(false); break;
					}
					return 0.0;
				}
			public:
				bool operator()(Value const & pV1, Value const & pV2) const
				{
					char lS1[256], lS2[256];
					switch (Type)
					{
						case VT_STRING:
						{
							assert(Type == VT_STRING);
							char const * const lS1p = (VT_STRING == pV1.type ? pV1.str : coerceToString(pV1, lS1));
							char const * const lS2p = (VT_STRING == pV2.type ? pV2.str : coerceToString(pV2, lS2));
							return strcmp(lS1p, lS2p) < 0;
						}
						case VT_INT64:
						{
							assert(Type == VT_INT64);
							assert(isInteger(pV1));
							assert(isInteger(pV2));
							return getInteger(pV1) < getInteger(pV2);
						}
						case VT_DOUBLE:
						{
							assert(Type == VT_DOUBLE);
							return coerceToDouble(pV1) < coerceToDouble(pV2);
						}
						default: assert(false); return false;
					}
				}
				static bool eq(Value const & pV1, Value const & pV2) { return !CompareValues<Type>()(pV1, pV2) && !CompareValues<Type>()(pV2, pV1); }
		};
		typedef std::vector<Tstring> Tstrings;
		typedef std::vector<Value> Tvalues;
		typedef std::set<Value, CompareValues<VT_STRING> > Tkeys_str;
		typedef std::set<Value, CompareValues<VT_INT64> > Tkeys_int;
		typedef std::set<Value, CompareValues<VT_DOUBLE> > Tkeys_dbl;
		typedef std::vector<PID> TPIDsV;
	public:
		TEST_DECLARE(TestPGTree);
		virtual char const * getName() const { return "testpgtree"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "trying to test a wider array of key permutations, with less pins"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { return false; }
		virtual bool isPerformingFullScanQueries() const { return true; } // Note (maxw): I do it mostly to run on a clean store, to be able to control things like ISession::listWords; should improve the fwk to allow this without forcing fullscan.
		virtual bool includeInPerfTest() const { return true; }
		virtual bool includeInBashTest(char const *& pReason) const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		struct PARTCtx
		{
			ISession * const mSession;
			Tkeys_str mKeys_str;
			Tkeys_int mKeys_int;
			Tkeys_dbl mKeys_dbl;
			PARTCtx(ISession * pSession)
				: mSession(pSession)
			{
				defineKeys_str(*mSession, mKeys_str);
				defineKeys_int(mKeys_int, mKeys_str);
				defineKeys_dbl(mKeys_dbl);
			}
			~PARTCtx()
			{
				TestPGTree::freeKeys(*mSession, mKeys_str);
			}
		};
		void testPART1(PARTCtx & pCtx);
		void testPART2(PARTCtx & pCtx);
		void testPART3(PARTCtx & pCtx);
		void testPART4(PARTCtx & pCtx);
		void testPART5(PARTCtx & pCtx);
	protected:
		// REVIEW: ascii vs unicode (would be trivial now that everything is templatized)...
		void insertKey(ISession & pSession, Value const & pKey, PropertyID pProp);
		void getPIDsFor(ISession & pSession, PropertyID pProp, TPIDsV & pPIDs);
		bool checkCount(size_t pExpected, size_t pActual);
		enum eCheckKeysFlags { kCKFCheckOrder = (1 << 0), kCKFSubset = (1 << 1), };
		void checkKeys(Tkeys_str const & pExpected, Tstrings const & pActual, long pFlags=kCKFCheckOrder);
		template <class Keys> void checkKeys(Keys const & pExpected, Tvalues const & pActual, long pFlags=kCKFCheckOrder);
		template <class Keys> void checkKeys(Keys const & pExpected, Keys const & pActual, long pFlags=0);
		template <class Values> static void freeKeys(ISession & pSession, Values & pKeys);
	protected:
		void simpleInsertRepeatedValue(ISession & pSession, Value const & pValue, size_t pRepetition, PropertyID pProp);
		template <class Keys> void simpleInsertForward(ISession & pSession, Keys const & pKeys, PropertyID pProp);
		template <class Keys> void simpleInsertBackward(ISession & pSession, Keys const & pKeys, PropertyID pProp);
		template <class Keys> void simpleInsertRandom(ISession & pSession, Keys const & pKeys, PropertyID pProp);
		void simpleInsertBackwardByLen_str(ISession & pSession, Tkeys_str const & pKeys, PropertyID pProp);
		void mergeAllInOneSeries(ISession & pSession, Tkeys_str const & pKeys, PropertyID);
		void morph1(ISession & pSession, Tkeys_str const & pKeys, PropertyID);
		void defineFamilies(ISession & pSession, size_t pPassIndex=0);
		void enumKeysFT_str(ISession & pSession, Tstrings & pResult, char pLetter);
		void enumKeysFT_str(ISession & pSession, Tstrings & pResult, char pLetter, size_t pPropIndex);
		void checkAllKeysFT_str(ISession & pSession, Tkeys_str const & pKeys);
		void checkAllKeysFT_str(ISession & pSession, Tkeys_str const & pKeys, size_t pPropIndex);
		template <class Keys> void checkAllKeysByFamily1(ISession & pSession, Keys const & pKeys, size_t pPropIndex, size_t pPassIndex, bool pSubset=false);
		template <class Keys> void checkAllKeysByFamily2(ISession & pSession, Keys const & pKeys, size_t pPropIndex, size_t pPassIndex, bool pSubset=false);
		template <class Keys> void checkAllKeysByFamily(ISession & pSession, Keys const & pKeys, size_t pPropIndex, size_t pPassIndex, bool pSubset=false) { checkAllKeysByFamily1(pSession, pKeys, pPropIndex, pPassIndex, pSubset); checkAllKeysByFamily2(pSession, pKeys, pPropIndex, pPassIndex, pSubset); }
		template <class Keys> void checkAllKeysFullScan(ISession & pSession, Keys const & pKeys, size_t pPropIndex, bool pSubset=false);
		static void defineKeys_str(ISession & pSession, Tkeys_str & pKeys, size_t pKeyLenMax=DEFAULT_KEY_MAXLEN, size_t pDensity=DEFAULT_KEY_DENSITY);
		static void defineKeys_int(Tkeys_int & pKeys, Tkeys_str const & pKeysStr);
		static void defineKeys_dbl(Tkeys_dbl & pKeys);
	protected:
		enum eCases
		{
			kCFirst_str,
			kCSimpleForward_str = kCFirst_str,
			kCSimpleBackward_str,
			kCSimpleRandom_str,
			kCSimpleBackwardByLen_str,
			kCEnd_str,
			// ---
			kCFirst_int = kCEnd_str,
			kCSimpleForward_int = kCFirst_int,
			kCSimpleBackward_int,
			kCSimpleRandom_int,
			kCEnd_int, 
			// ---
			kCFirst_dbl = kCEnd_int,
			kCSimpleForward_dbl = kCFirst_dbl,
			kCSimpleBackward_dbl,
			kCSimpleRandom_dbl,
			kCEnd_dbl,
			// ---
			kCFirst_mixed = kCEnd_dbl,
			kCMorph1,
			kCEnd_mixed,
			kCTotal = kCEnd_mixed
		};
		enum ePasses
		{
			kPFirst,
			kPSecond,
			kPThird,
			kPTotal
		};
	protected:
		typedef void (TestPGTree::*TFunc_str) (ISession &, Tkeys_str const &, PropertyID);
		typedef void (TestPGTree::*TFunc_int) (ISession &, Tkeys_int const &, PropertyID);
		typedef void (TestPGTree::*TFunc_dbl) (ISession &, Tkeys_dbl const &, PropertyID);
		static TFunc_str const sFuncs_str[kCEnd_str - kCFirst_str];
		static TFunc_int const sFuncs_int[kCEnd_int - kCFirst_int];
		static TFunc_dbl const sFuncs_dbl[kCEnd_dbl - kCFirst_dbl];
		// TODO: more insertion/modif/del patterns (e.g. various types of random modifs until converge on lKeys, etc.)
	protected:
		RC mRCUpdates;
		PropertyID mProps[kCTotal];
		DataEventID mFamilies[kPTotal][kCTotal];
};
TEST_IMPLEMENT(TestPGTree, TestLogger::kDStdOut);
TestPGTree::TFunc_str const TestPGTree::sFuncs_str[kCEnd_str - kCFirst_str] = { &TestPGTree::simpleInsertForward, &TestPGTree::simpleInsertBackward, &TestPGTree::simpleInsertRandom, &TestPGTree::simpleInsertBackwardByLen_str, };
TestPGTree::TFunc_int const TestPGTree::sFuncs_int[kCEnd_int - kCFirst_int] = { &TestPGTree::simpleInsertForward, &TestPGTree::simpleInsertBackward, &TestPGTree::simpleInsertRandom, };
TestPGTree::TFunc_dbl const TestPGTree::sFuncs_dbl[kCEnd_dbl - kCFirst_dbl] = { &TestPGTree::simpleInsertForward, &TestPGTree::simpleInsertBackward, &TestPGTree::simpleInsertRandom, };

// Implement this test.
int TestPGTree::execute()
{
	mRCUpdates = RC_FALSE;
	if (MVTApp::startStore())
	{
		mRCUpdates = RC_OK;
		ISession * const lSession = MVTApp::startSession();
		MVTApp::mapURIs(lSession, "testpgtree.prop", kCTotal, mProps);
		defineFamilies(*lSession, kPFirst);
		{
			PARTCtx lCtx(lSession);
			if (PART1) testPART1(lCtx);
			if (PART2) testPART2(lCtx);
			if (PART3) testPART3(lCtx);
			if (PART4) testPART4(lCtx);
			if (PART5) testPART5(lCtx);
		}
		lSession->terminate();
		MVTApp::stopStore();
	}
	else
	{
		TVERIFY(!"Couldn't open store"); return RC_FALSE;
	}
	// REVIEW: Do an explicit durability test as well?
	return mRCUpdates;
}

void TestPGTree::testPART1(PARTCtx & pCtx)
{
	// Run the update scenarios for string keys, and do validations.
	mLogger.out() << std::endl << "STRING-ONLY KEYS" << std::endl;
	size_t iC;
	for (iC = 0; iC < kCFirst_int && RC_OK == mRCUpdates; iC++)
	{
		(this->*sFuncs_str[iC])(*pCtx.mSession, pCtx.mKeys_str, mProps[iC]);
		if (RC_OK == mRCUpdates)
		{
			checkAllKeysFullScan(*pCtx.mSession, pCtx.mKeys_str, iC);
			checkAllKeysFT_str(*pCtx.mSession, pCtx.mKeys_str);
			checkAllKeysFT_str(*pCtx.mSession, pCtx.mKeys_str, iC);
			checkAllKeysByFamily(*pCtx.mSession, pCtx.mKeys_str, iC, kPFirst);
		}
	}

	defineFamilies(*pCtx.mSession, kPSecond);
	for (iC = 0; iC < kCFirst_int; iC++)
		checkAllKeysByFamily(*pCtx.mSession, pCtx.mKeys_str, iC, kPSecond);

	// Merge all properties (corresponding to different insertion order) into a single series of pins, and redo the validations.
	mergeAllInOneSeries(*pCtx.mSession, pCtx.mKeys_str, STORE_INVALID_URIID);
	if (RC_OK == mRCUpdates)
	{
		checkAllKeysFT_str(*pCtx.mSession, pCtx.mKeys_str);
		size_t iC;
		for (iC = 0; iC < kCFirst_int; iC++)
		{
			checkAllKeysFullScan(*pCtx.mSession, pCtx.mKeys_str, iC);
			checkAllKeysFT_str(*pCtx.mSession, pCtx.mKeys_str, iC);
			for (size_t iPass = kPFirst; iPass <= kPSecond; iPass++)
				checkAllKeysByFamily(*pCtx.mSession, pCtx.mKeys_str, iC, iPass);
		}
	}

	defineFamilies(*pCtx.mSession, kPThird); // Note: bug #126.
	for (iC = 0; iC < kCFirst_int; iC++)
		checkAllKeysByFamily(*pCtx.mSession, pCtx.mKeys_str, iC, kPThird);
}

void TestPGTree::testPART2(PARTCtx & pCtx)
{
	// Run the update scenarios for integer keys, and do validations.
	mLogger.out() << std::endl << "INTEGER-ONLY KEYS" << std::endl;
	size_t iC;
	for (iC = kCFirst_int; iC < kCEnd_int && RC_OK == mRCUpdates; iC++)
	{
		(this->*sFuncs_int[iC - kCFirst_int])(*pCtx.mSession, pCtx.mKeys_int, mProps[iC]);
		if (RC_OK == mRCUpdates)
		{
			checkAllKeysFullScan(*pCtx.mSession, pCtx.mKeys_int, iC);
			checkAllKeysByFamily(*pCtx.mSession, pCtx.mKeys_int, iC, kPFirst);
		}
	}
}

void TestPGTree::testPART3(PARTCtx & pCtx)
{
	// Run the update scenarios for double keys, and do validations.
	mLogger.out() << std::endl << "DOUBLE-ONLY KEYS" << std::endl;
	size_t iC;
	for (iC = kCFirst_dbl; iC < kCEnd_dbl && RC_OK == mRCUpdates; iC++)
	{
		(this->*sFuncs_dbl[iC - kCFirst_dbl])(*pCtx.mSession, pCtx.mKeys_dbl, mProps[iC]);
		if (RC_OK == mRCUpdates)
		{
			checkAllKeysFullScan(*pCtx.mSession, pCtx.mKeys_dbl, iC);
			checkAllKeysByFamily(*pCtx.mSession, pCtx.mKeys_dbl, iC, kPFirst);
		}
	}
}

void TestPGTree::testPART4(PARTCtx & pCtx)
{
	// Mix string and double keys in the same property, and see what happens.
	mLogger.out() << std::endl << "MIXED STRING & DOUBLE KEYS" << std::endl;
	simpleInsertRandom(*pCtx.mSession, pCtx.mKeys_str, mProps[kCFirst_mixed]);
	simpleInsertRandom(*pCtx.mSession, pCtx.mKeys_dbl, mProps[kCFirst_mixed]);
	if (RC_OK == mRCUpdates)
	{
		mLogger.out() << "  total expected number of keys: " << (pCtx.mKeys_str.size() + pCtx.mKeys_dbl.size()) << std::endl;
		checkAllKeysFullScan(*pCtx.mSession, pCtx.mKeys_dbl, kCFirst_mixed, true);
		checkAllKeysFullScan(*pCtx.mSession, pCtx.mKeys_str, kCFirst_mixed, true);
		// issue: listValues doesn't seem to return everything... see if normal, maybe use another querying method (range?)
		// issue: my 'checkKeys' methods don't handle 'subset' correctly (should reverse the loops)
		// checkAllKeysByFamily(*pCtx.mSession, pCtx.mKeys_dbl, kCFirst_mixed, kPFirst, true);
		// checkAllKeysByFamily(*pCtx.mSession, pCtx.mKeys_str, kCFirst_mixed, kPFirst, true);
	}
}

void TestPGTree::testPART5(PARTCtx & pCtx)
{
	// Start from a single key repeated N times, and morph into N different keys.
	// issue: at the moment this generates tons of "Error 1 updating(1) index 87" errors...
	mLogger.out() << std::endl << "MORPH STRING KEYS" << std::endl;
	Value lV;
	SETVALUE(lV, STORE_INVALID_URIID, "hello", OP_SET);
	simpleInsertRepeatedValue(*pCtx.mSession, lV, pCtx.mKeys_str.size(), mProps[kCMorph1]);
	morph1(*pCtx.mSession, pCtx.mKeys_str, mProps[kCMorph1]);
	if (RC_OK == mRCUpdates)
	{
		checkAllKeysFullScan(*pCtx.mSession, pCtx.mKeys_str, kCMorph1);
		checkAllKeysFT_str(*pCtx.mSession, pCtx.mKeys_str, kCMorph1);
		checkAllKeysByFamily(*pCtx.mSession, pCtx.mKeys_str, kCMorph1, kPFirst);
	}
}

/**
 * Helpers.
 */
void TestPGTree::insertKey(ISession & pSession, Value const & pKey, PropertyID pProp)
{
	Value lV;
	switch (pKey.type)
	{
		case VT_INT: SETVALUE(lV, pProp, pKey.i, OP_SET); break;
		case VT_INT64: lV.setI64(pKey.i64); lV.property = pProp; lV.op = OP_SET; break;
		case VT_STRING: SETVALUE(lV, pProp, pKey.str, OP_SET); lV.meta = META_PROP_FTINDEX; break;
		case VT_DOUBLE: SETVALUE(lV, pProp, pKey.d, OP_SET); break;
		default: TVERIFY2(false, "Unexpected value type"); mRCUpdates = RC_OTHER; return;
	}
	if (RC_OK != (mRCUpdates = pSession.createPIN(&lV, 1, NULL, MODE_PERSISTENT|MODE_COPY_VALUES)))
		mLogger.out() << "  RC=" << mRCUpdates << std::endl;
}

void TestPGTree::getPIDsFor(ISession & pSession, PropertyID pProp, TPIDsV & pPIDs)
{
	// For now, play safe, use fullscan here.
	CmvautoPtr<IStmt> lQ(pSession.createStmt());
	unsigned char const lVar = lQ->addVariable();
	TVERIFYRC(lQ->setPropCondition(lVar, &pProp, 1));
	ICursor* lC = NULL;
	TVERIFYRC(lQ->execute(&lC));
	CmvautoPtr<ICursor> lR(lC);
	if (!lR.IsValid())
		{ TVERIFY2(false, "Couldn't execute query."); return; }
	IPIN * lP;
	for (lP = lR->next(); NULL != lP; lP = lR->next())
	{
		pPIDs.push_back(lP->getPID());
		lP->destroy();
	}
}

bool TestPGTree::checkCount(size_t pExpected, size_t pActual)
{
	if (pExpected == pActual)
	{
		mLogger.out() << "  count ok: " << pActual << std::endl;
		return true;
	}
	mLogger.out() << "  count mismatch: expected " << pExpected << " but got " << pActual << std::endl;
	TVERIFY2(false, "Count mismatch.");
	return false;
}

void TestPGTree::checkKeys(Tkeys_str const & pExpected, Tstrings const & pActual, long pFlags)
{
	if (0 == (pFlags & kCKFSubset))
		checkCount(pExpected.size(), pActual.size());
	bool const lCheckOrder = (0 != (pFlags & kCKFCheckOrder));
	size_t lNumErrors = 0;
	Tstrings::const_iterator iK;
	Tkeys_str::const_iterator iKe;
	for (iK = pActual.begin(), iKe = pExpected.begin(); pActual.end() != iK && pExpected.end() != iKe; iK++, iKe++)
	{
		Value lV;
		SETVALUE(lV, STORE_INVALID_URIID, (*iK).c_str(), OP_SET);
		if (pExpected.end() == pExpected.find(lV) || (lCheckOrder && !Tkeys_str::key_compare::eq(lV, *iKe)))
		{
			lNumErrors++;
			#if 1
				if (lNumErrors < 5)
				{
					mLogger.out() << "  unexpected value" << std::endl;
					MVTApp::output(lV, mLogger.out());
				}
			#endif
		}
	}
	mLogger.out() << "  " << lNumErrors << " unexpected values (/" << pActual.size() << " tested)" << std::endl;
}

template <class Keys>
void TestPGTree::checkKeys(Keys const & pExpected, Tvalues const & pActual, long pFlags)
{
	if (0 == (pFlags & kCKFSubset))
		checkCount(pExpected.size(), pActual.size());
	bool const lCheckOrder = (0 != (pFlags & kCKFCheckOrder));
	size_t lNumErrors = 0;
	Tvalues::const_iterator iK;
	typename Keys::const_iterator iKe;
	for (iK = pActual.begin(), iKe = pExpected.begin(); pActual.end() != iK && pExpected.end() != iKe; iK++, iKe++)
	{
		if (pExpected.end() == pExpected.find(*iK) || (lCheckOrder && !Keys::key_compare::eq(*iK, *iKe)))
		{
			lNumErrors++;
			#if 1
				if (lNumErrors < 5)
				{
					mLogger.out() << "  unexpected value" << std::endl;
					MVTApp::output(*iK, mLogger.out());
				}
			#endif
		}
	}
	mLogger.out() << "  " << lNumErrors << " unexpected values (/" << pActual.size() << " tested)" << std::endl;
}

template <class Keys>
void TestPGTree::checkKeys(Keys const & pExpected, Keys const & pActual, long pFlags)
{
	if (0 == (pFlags & kCKFSubset))
		checkCount(pExpected.size(), pActual.size());
	typename Keys::const_iterator iK1, iK2;
	size_t lNumErrors = 0;
	for (iK1 = pExpected.begin(), iK2 = pActual.begin(); pExpected.end() != iK1 && pActual.end() != iK2; iK1++, iK2++)
	{
		if (!Keys::key_compare::eq(*iK1, *iK2))
		{
			lNumErrors++;
			mLogger.out() << "  expected value" << std::endl;
			MVTApp::output(*iK1, mLogger.out());
			mLogger.out() << "  but found value" << std::endl;
			MVTApp::output(*iK2, mLogger.out());
		}
	}
	mLogger.out() << "  " << lNumErrors << " unexpected values (/" << pActual.size() << " tested)" << std::endl;
}

template <class Values>
void TestPGTree::freeKeys(ISession & pSession, Values & pKeys)
{
	typename Values::iterator iK;
	for (iK = pKeys.begin(); pKeys.end() != iK; iK++)
	{
		Value lK = *iK;
		pSession.freeValue(lK);
	}
	pKeys.clear();
}

/**
 * Index update scenarii.
 */
void TestPGTree::simpleInsertRepeatedValue(ISession & pSession, Value const & pValue, size_t pRepetition, PropertyID pProp)
{
	mLogger.out() << std::endl << ">>> Inserting a single repeated key..." << std::flush;
	size_t i;
	for (i = 0; i < pRepetition && RC_OK == mRCUpdates; i++)
	{
		if (i % 100 == 0)
			mLogger.out() << "." << std::flush;
		insertKey(pSession, pValue, pProp);
	}
	mLogger.out() << " done." << std::endl;
}

template <class Keys>
void TestPGTree::simpleInsertForward(ISession & pSession, Keys const & pKeys, PropertyID pProp)
{
	mLogger.out() << std::endl << ">>> Inserting keys in simple forward key order..." << std::flush;
	typename Keys::const_iterator iK;
	size_t i;
	for (iK = pKeys.begin(), i = 0; pKeys.end() != iK && RC_OK == mRCUpdates; iK++, i++)
	{
		if (i % 100 == 0)
			mLogger.out() << "." << std::flush;
		insertKey(pSession, *iK, pProp);
	}
	mLogger.out() << " done." << std::endl;
}

template <class Keys>
void TestPGTree::simpleInsertBackward(ISession & pSession, Keys const & pKeys, PropertyID pProp)
{
	mLogger.out() << std::endl << ">>> Inserting keys in simple backward key order..." << std::flush;
	typename Keys::const_reverse_iterator iK;
	size_t i;
	for (iK = pKeys.rbegin(), i = 0; pKeys.rend() != iK && RC_OK == mRCUpdates; iK++, i++)
	{
		if (i % 100 == 0)
			mLogger.out() << "." << std::flush;
		insertKey(pSession, *iK, pProp);
	}
	mLogger.out() << " done." << std::endl;
}

template <class Keys>
void TestPGTree::simpleInsertRandom(ISession & pSession, Keys const & pKeys, PropertyID pProp)
{
	mLogger.out() << std::endl << ">>> Inserting keys in simple random key order..." << std::flush;
	Tvalues lShuffledKeys;
	{
		typename Keys::const_iterator iK;
		for (iK = pKeys.begin(); pKeys.end() != iK; iK++)
			lShuffledKeys.push_back(*iK);
		random_shuffle(lShuffledKeys.begin(), lShuffledKeys.end());
	}
	Tvalues::const_iterator iK;
	size_t i;
	for (iK = lShuffledKeys.begin(), i = 0; lShuffledKeys.end() != iK && RC_OK == mRCUpdates; iK++, i++)
	{
		if (i % 100 == 0)
			mLogger.out() << "." << std::flush;
		insertKey(pSession, *iK, pProp);
	}
	mLogger.out() << " done." << std::endl;
}

bool bwCmpKeyLengths(Tstring const & p1, Tstring const & p2) { return p1.length() > p2.length(); }
void TestPGTree::simpleInsertBackwardByLen_str(ISession & pSession, Tkeys_str const & pKeys, PropertyID pProp)
{
	mLogger.out() << std::endl << ">>> Inserting keys in simple backward key length order..." << std::flush;
	Tstrings lOrderedKeys;
	{
		Tkeys_str::const_iterator iK;
		for (iK = pKeys.begin(); pKeys.end() != iK; iK++)
			lOrderedKeys.push_back((*iK).str);
		stable_sort(lOrderedKeys.begin(), lOrderedKeys.end(), bwCmpKeyLengths);
	}
	Tstrings::const_iterator iK;
	size_t i;
	for (iK = lOrderedKeys.begin(), i = 0; lOrderedKeys.end() != iK && RC_OK == mRCUpdates; iK++, i++)
	{
		if (i % 100 == 0)
			mLogger.out() << "." << std::flush;
		Value lV;
		SETVALUE(lV, pProp, (*iK).c_str(), OP_SET);
		insertKey(pSession, lV, pProp);
	}
	mLogger.out() << " done." << std::endl;
}

void TestPGTree::mergeAllInOneSeries(ISession & pSession, Tkeys_str const & pKeys, PropertyID)
{
	TPIDsV lKept;
	getPIDsFor(pSession, mProps[0], lKept);
	if (pKeys.size() != lKept.size())
	{
		mLogger.out() << "  expected " << pKeys.size() << " but found " << lKept.size() << " pins to keep." << std::endl;
		TVERIFY2(false, "Unexpected lKept.size()");
	}

	TPIDsV lRemoved;
	size_t iC;
	for (iC = 1; iC < kCFirst_int; iC++)
		getPIDsFor(pSession, mProps[iC], lRemoved);
	if (pKeys.size() * (kCFirst_int - 1) != lRemoved.size())
	{
		mLogger.out() << "  expected " << (pKeys.size() * (kCFirst_int - 1)) << " but found " << lRemoved.size() << " pins to delete." << std::endl;
		TVERIFY2(false, "Unexpected lRemoved.size()");
	}
	random_shuffle(lRemoved.begin(), lRemoved.end());

	mLogger.out() << std::endl << ">>> Merging all keys/properties into a single series..." << std::flush;

	size_t iNext[kCFirst_int];
	memset(iNext, 0, sizeof(iNext));
	while (!lRemoved.empty())
	{
		IPIN * lPdel = pSession.getPIN(lRemoved.back());
		if (!lPdel)
		{
			TVERIFY2(false, "Unexpected: couldn't find pin from lRemoved.");
			continue;
		}
		Value const * lVp = lPdel->getValueByIndex(0);
		TVERIFY(lVp->property > mProps[0] && lVp->property <= mProps[kCFirst_int - 1]);
		Value lV;
		TVERIFYRC(pSession.copyValue(*lVp, lV));
		lV.op = OP_SET; lV.eid = STORE_COLLECTION_ID; lV.flags = 0;
		size_t const lPropIndex = lVp->property - mProps[0]; // Note: Assumes that mProps is a sequence...
		size_t const lIndexToUpdate = iNext[lPropIndex];
		TVERIFY(lIndexToUpdate < lKept.size());
		iNext[lPropIndex] += 1;
		CmvautoPtr<IPIN> lPupd(pSession.getPIN(lKept[lIndexToUpdate]));
		if (!lPupd.IsValid())
		{
			TVERIFY2(false, "Unexpected: couldn't find pin from lKept.");
			lPdel->destroy();
			pSession.freeValue(lV);
			continue;
		}
		TVERIFYRC(lPupd->modify(&lV, 1));
		TVERIFYRC(lPdel->deletePIN());
		pSession.freeValue(lV);
		lRemoved.pop_back();
		if (lRemoved.size() % 100 == 0)
			mLogger.out() << "." << std::flush;
	}
	mLogger.out() << " done." << std::endl;
	for (iC = 1; iC < kCFirst_int; iC++)
		TVERIFY(iNext[iC] == lKept.size());
}

void TestPGTree::morph1(ISession & pSession, Tkeys_str const & pKeys, PropertyID pProp)
{
	TPIDsV lPIDs;
	getPIDsFor(pSession, pProp, lPIDs);
	if (pKeys.size() != lPIDs.size())
	{
		mLogger.out() << "  expected " << pKeys.size() << " but found " << lPIDs.size() << " pins to morph." << std::endl;
		TVERIFY2(false, "Unexpected lPIDs.size()");
	}

	mLogger.out() << std::endl << ">>> Morphing all identical pins into the basic string series..." << std::flush;

	Tkeys_str::iterator iK;
	for (iK = pKeys.begin(); pKeys.end() != iK && !lPIDs.empty(); iK++)
	{
		CmvautoPtr<IPIN> lPmorph(pSession.getPIN(lPIDs.back()));
		if (!lPmorph.IsValid())
		{
			TVERIFY2(false, "Unexpected: couldn't find pin from lPIDs.");
			continue;
		}
		Value const * lVp = lPmorph->getValue(pProp);
		TVERIFY(lVp->type == VT_STRING && 0 == strcmp(lVp->str, "hello"));
		Value lV = *iK;
		lV.op = OP_SET; lV.eid = STORE_COLLECTION_ID; lV.flags = 0; lV.property = pProp;
		TVERIFYRC(lPmorph->modify(&lV, 1));
		lPIDs.pop_back();
		if (lPIDs.size() % 100 == 0)
			mLogger.out() << "." << std::flush;
	}
	mLogger.out() << " done." << std::endl;
}

/**
 * Validations.
 */
void TestPGTree::defineFamilies(ISession & pSession, size_t pPassIndex)
{
	mLogger.out() << std::endl << ">>> Defining families (pass " << pPassIndex << ")" << std::endl;
	size_t iC;
	for (iC = 0; iC < kCTotal; iC++)
	{
		char lClassName[256];
		sprintf(lClassName, "testpgtree_class%lu_%lu", (unsigned long)iC, (unsigned long)pPassIndex);
		mFamilies[pPassIndex][iC] = STORE_INVALID_CLASSID;
		if (RC_OK != pSession.getDataEventID(lClassName, mFamilies[pPassIndex][iC]))
		{
			CmvautoPtr<IStmt> lQ(pSession.createStmt());
			unsigned char const lVar = lQ->addVariable();
			Value lV[2];
			lV[0].setVarRef(0,mProps[iC]);
			lV[1].setParam(0);
			bool const lIntegerOnly = (iC >= kCFirst_int && iC <= kCEnd_int);
			bool const lFloatOnly = (iC >= kCFirst_dbl && iC <= kCEnd_dbl);
			bool const lNumberOnly = lIntegerOnly || lFloatOnly;
			CmvautoPtr<IExprNode> lET(pSession.expr(OP_IN, 2, lV, lNumberOnly ? 0 : CASE_INSENSITIVE_OP));
			TVERIFYRC(lQ->addCondition(lVar, lET));
			TVERIFYRC(defineClass(&pSession, lClassName, lQ, &mFamilies[pPassIndex][iC]));
		}
	}
}

void TestPGTree::enumKeysFT_str(ISession & pSession, Tstrings & pResult, char pLetter)
{
	// Enumerate keys in the FT index, using ISession::listWords.
	// This is case-insensitive by design; words are returned in std lexicographical order.

	char lBuf[2];
	lBuf[0] = pLetter;
	lBuf[1] = 0;

	Afy::StringEnum * lSE = NULL;
	TVERIFYRC(pSession.listWords(lBuf, lSE));
	if (lSE)
	{
		// Note: Individual values obtained through next() belong to the store.
		char const * lS;
		for (lS = lSE->next(); NULL != lS; lS = lSE->next())
			pResult.push_back(lS); // Note: An implicit string copy happens here.
		lSE->destroy();
	}
}

void TestPGTree::enumKeysFT_str(ISession & pSession, Tstrings & pResult, char pLetter, size_t pPropIndex)
{
	// Enumerate keys in the FT index, using a FT condition on a specific property.
	// All FT-related functions are case-insensitive by design; words are returned in std lexicographical order.

	char lBuf[2];
	lBuf[0] = pLetter;
	lBuf[1] = 0;

	CmvautoPtr<IStmt> lQ(pSession.createStmt());
	unsigned char const lVar = lQ->addVariable();
	TVERIFYRC(lQ->addConditionFT(lVar, lBuf, 0, &mProps[pPropIndex], 1));
	OrderSeg const lOrder = {NULL, mProps[pPropIndex], ORD_NCASE, 0, 0};
	TVERIFYRC(lQ->setOrder(&lOrder, 1));
	ICursor* lC = NULL;
	TVERIFYRC(lQ->execute(&lC));
	CmvautoPtr<ICursor> lR(lC);
	if (!lR.IsValid())
		{ TVERIFY2(false, "Couldn't execute query."); return; }
	IPIN * lP;
	for (lP = lR->next(); NULL != lP; lP = lR->next())
	{
		pResult.push_back(lP->getValue(mProps[pPropIndex])->str);
		lP->destroy();
	}
}

void TestPGTree::checkAllKeysFT_str(ISession & pSession, Tkeys_str const & pKeys)
{
	mLogger.out() << "  === checkAllKeysFT_str (ISession::listWords)" << std::endl;
	Tstrings lKeysStr;

	// Gather the state of the FT index through listWords.
	char iLetter;
	for (iLetter = 'a'; iLetter <= 'z'; iLetter++)
		enumKeysFT_str(pSession, lKeysStr, iLetter);

	checkKeys(pKeys, lKeysStr);
}

void TestPGTree::checkAllKeysFT_str(ISession & pSession, Tkeys_str const & pKeys, size_t pPropIndex)
{
	mLogger.out() << "  === checkAllKeysFT_str (IStmt::addConditionFT)" << std::endl;
	Tstrings lKeysStr;

	// Gather the state of the FT index through a query on pPropIndex.
	char iLetter;
	for (iLetter = 'a'; iLetter <= 'z'; iLetter++)
		enumKeysFT_str(pSession, lKeysStr, iLetter, pPropIndex);

	checkKeys(pKeys, lKeysStr);
}

template <class Keys>
void TestPGTree::checkAllKeysByFamily1(ISession & pSession, Keys const & pKeys, size_t pPropIndex, size_t pPassIndex, bool pSubset)
{
	mLogger.out() << "  === checkAllKeysByFamily1 (ISession::listValues) (prop " << pPropIndex << ", pass " << pPassIndex << ", type " << Keys::key_compare::getType() << ")" << std::endl;
	Tvalues lKeys;

	Afy::IndexNav * lVE;
	TVERIFYRC(pSession.listValues(mFamilies[pPassIndex][pPropIndex], mProps[pPropIndex], lVE));
	if (lVE)
	{
		// Note: Individual values obtained through next() belong to the store.
		Value const * lVp;
		for (lVp = lVE->next(); NULL != lVp; lVp = lVE->next())
		{
			if (pSubset && !Keys::key_compare::isEquivalentType((ValueType)lVp->type))
				continue;
			TVERIFY(Keys::key_compare::isEquivalentType((ValueType)lVp->type));
			Value lV;
			TVERIFYRC(pSession.copyValue(*lVp, lV));
			lKeys.push_back(lV);
		}
		lVE->destroy();
	}

	checkKeys(pKeys, lKeys, pSubset ? kCKFSubset : kCKFCheckOrder);
	freeKeys(pSession, lKeys);
}

template <class Keys>
void TestPGTree::checkAllKeysByFamily2(ISession & pSession, Keys const & pKeys, size_t pPropIndex, size_t pPassIndex, bool pSubset)
{
	mLogger.out() << "  === checkAllKeysByFamily2 (prop " << pPropIndex << ", type " << Keys::key_compare::getType() << ")" << std::endl;
	Tvalues lKeys;

	Value * lParam = NULL;
	#define CHECKALLKEYSBYFAMILY2OPTION 2
	#if CHECKALLKEYSBYFAMILY2OPTION==1
		char lStmtStr[256];
		sprintf(lStmtStr, "SELECT * FROM testpgtree_class%lu_%lu(:0) ORDER BY $0;", (unsigned long)pPropIndex, (unsigned long)pPassIndex);
		CmvautoPtr<IStmt> lQ(pSession.createStmt(lStmtStr, &mProps[pPropIndex], 1));
		Value lParams[3];
		SETVALUE(lParams[0], mProps[pPropIndex], "a", OP_SET);
		SETVALUE(lParams[1], mProps[pPropIndex], "zzzzzzzzzzzzz", OP_SET);
		lParams[2].setRange(&lParams[0]);
		lParam = &lParams[2];
	#elif CHECKALLKEYSBYFAMILY2OPTION==2
		char lStmtStr[256];
		sprintf(lStmtStr, "SELECT * FROM testpgtree_class%lu_%lu ORDER BY $0;", (unsigned long)pPropIndex, (unsigned long)pPassIndex);
		CmvautoPtr<IStmt> lQ(pSession.createStmt(lStmtStr, &mProps[pPropIndex], 1));
	#else
		CmvautoPtr<IStmt> lQ(pSession.createStmt());
		SourceSpec lCS;
		lCS.objectID = mFamilies[pPassIndex][pPropIndex];
		lCS.nParams = 0; lCS.params = NULL;
		lQ->addVariable(&lCS, 1);
		OrderSeg const lOrder = {NULL, mProps[pPropIndex], ORD_NCASE, 0, 0};
		TVERIFYRC(lQ->setOrder(&lOrder, 1));
	#endif
	uint64_t lTotalCnt = 0;
	TVERIFYRC(lQ->count(lTotalCnt, lParam, lParam ? 1 : 0));
	mLogger.out() << "  total query count: " << lTotalCnt << std::endl;
	ICursor* lC = NULL;
	TVERIFYRC(lQ->execute(&lC, lParam, lParam ? 1 : 0));
	CmvautoPtr<ICursor> lR(lC);
	if (!lR.IsValid())
		{ TVERIFY2(false, "Couldn't execute query."); return; }
	IPIN * lP;
	for (lP = lR->next(); NULL != lP; lP = lR->next())
	{
		Value const * lVp = lP->getValue(mProps[pPropIndex]);
		if (!lVp)
			{ TVERIFY2(false, "Unexpected null value"); continue; }
		if (pSubset && !Keys::key_compare::isEquivalentType((ValueType)lVp->type))
			continue;
		TVERIFY(Keys::key_compare::isEquivalentType((ValueType)lVp->type));
		Value lV;
		TVERIFYRC(pSession.copyValue(*lVp, lV));
		lKeys.push_back(lV);
		lP->destroy();
	}

	checkKeys(pKeys, lKeys, pSubset ? kCKFSubset : kCKFCheckOrder);
	freeKeys(pSession, lKeys);
}

template <class Keys>
void TestPGTree::checkAllKeysFullScan(ISession & pSession, Keys const & pKeys, size_t pPropIndex, bool pSubset)
{
	mLogger.out() << "  === checkAllKeysFullScan (prop " << pPropIndex << ", type " << Keys::key_compare::getType() << ")" << std::endl;
	Keys lKeys;

	CmvautoPtr<IStmt> lQ(pSession.createStmt());
	unsigned char const lVar = lQ->addVariable();
	TVERIFYRC(lQ->setPropCondition(lVar, &mProps[pPropIndex], 1));
	uint64_t lTotalCnt = 0;
	TVERIFYRC(lQ->count(lTotalCnt));
	mLogger.out() << "  total query count: " << lTotalCnt << std::endl;
	// Note:
	//   Here I don't specify order, by design; I prefer to use the most plain,
	//   unquestionable flavor of query here, as a reference/confirmation
	//   of what's in the store.
	ICursor* lC = NULL;
	TVERIFYRC(lQ->execute(&lC));
	CmvautoPtr<ICursor> lR(lC);
	if (!lR.IsValid())
		{ TVERIFY2(false, "Couldn't execute query."); return; }
	IPIN * lP;
	for (lP = lR->next(); NULL != lP; lP = lR->next())
	{
		Value const * lVp = lP->getValue(mProps[pPropIndex]);
		if (!lVp)
			{ TVERIFY2(false, "Unexpected null value"); continue; }
		if (pSubset && !Keys::key_compare::isEquivalentType((ValueType)lVp->type))
			continue;
		TVERIFY(Keys::key_compare::isEquivalentType((ValueType)lVp->type));
		Value lV;
		TVERIFYRC(pSession.copyValue(*lVp, lV));
		lKeys.insert(lV);
		lP->destroy();
	}

	checkKeys(pKeys, lKeys, pSubset ? kCKFSubset : kCKFCheckOrder);
	freeKeys(pSession, lKeys);
}

/**
 * Core scenario definition.
 */
void TestPGTree::defineKeys_str(ISession & pSession, Tkeys_str & pKeys, size_t pKeyLenMax, size_t pDensity)
{
	// Note:
	//   I copied the default stop word list just to be absolutely sure that
	//   stop words were not responsible for some of the errors I encountered.
	const char *lDefaultEnglishStopWords[] =
	{
		"about", "ain't", "all", "also", "am", "an", "and", "any", "anybody", "anyhow", "anyone",
		"anything", "anyway", "anyways", "anywhere", "are", "aren't", "as", "at", "be", "because",
		"been", "being", "both", "but", "by", "can", "can't", "cannot", "cant", "could", "couldn't", 
		"did", "didn't", "do", "does", "doesn't", "doing", "don't", "done", "each",
		"either", "else", "etc", "ever", "every", "for", "from", "get", "gets", "getting",
		"got", "gotten", "had","hadn't", "has", "hasn't", "have", "haven't", "having", "he", "he's", 
		"her", "here", "here's", "hers", "herself", "hi", "him", "himself", "his", "how", "i'd", "i'll", 
		"i'm", "i've", "ie", "if", "in", "into", "is", "isn't", "it", "it'd", "it'll", "it's", "its", 
		"itself", "just", "let's", "like", "mainly", "many", "may", "maybe", "me", "might", "more",
		"much", "my", "myself", "new", "no", "non", "none", "not", "nothing", "now", "of", "off", "ok",
		"okay", "on", "one", "only", "or", "other", "others", "ought", "our", "ours", "ourselves", "out",
		"per", "same", "shall", "she", "she's", "she'll", "should", "shouldn't", "so", "some", "somebody",
		"somehow", "someone", "something", "sometime", "sometimes", "somewhat", "somewhere", "such",
		"th", "than", "that", "that's", "thats", "the", "their", "theirs", "them", "themselves", "then",
		"there", "there's", "these", "they", "they'd", "they'll", "they're", "they've",
		"this", "those", "though", "through", "thru", "to", "too", "until", "unto", "up",
		"upon", "us", "very", "was", "wasn't", "we", "we'd", "we'll","we're", "we've", "were",
		"weren't", "what", "what's", "when", "where", "where's", "whether", "which", "while", 
		"who", "who's", "whoever", "whom", "whose", "why", "will", "with", "within", "without", "won't", 
		"would", "wouldn't", "yes", "yet", "you", "you'd", "you'll", "you're","you've", "your", "yours",
		"yourself", "yourselves",
	};
	Tkeys_str lStopW;
	Tstring lKeyStr;
	Value lKey;
	size_t iLen, iD;
	for (iD = 0; iD < sizeof(lDefaultEnglishStopWords) / sizeof(lDefaultEnglishStopWords[0]); iD++)
	{
		lKey.set(lDefaultEnglishStopWords[iD]);
		lStopW.insert(lKey);
	}
	// Note: Beyond pKeyLenMax=256, some validations (e.g. using listWords) will stop working.
	// Note: Below pKeyLenMax=2, some FT validations don't count 1-letter words and therefore fail.
	for (iLen = 2; iLen < pKeyLenMax; iLen++)
	{
		size_t lMaxD = size_t(MVTRand::getRange(pDensity / 2, pDensity));
		if (iLen * 26 < lMaxD && (size_t)pow(26.0, (double)iLen) < lMaxD) // A quick approximation, just to avoid busting with large keys...
			lMaxD = (size_t)pow(26.0, (double)iLen);
		for (iD = 0; iD < lMaxD;)
		{
			MVTRand::getString(lKeyStr, iLen, 0, false, false); // Note: For now I use case-insensitive strings, because I don't know how to control case in all types of validations that I want to use...
			SETVALUE(lKey, STORE_INVALID_URIID, lKeyStr.c_str(), OP_SET);
			if (pKeys.end() == pKeys.find(lKey) && lStopW.end() == lStopW.find(lKey)) // Note: Just to be very explicit...
			{
				char * lBuf = (char *)pSession.malloc(lKeyStr.length() + 1);
				memcpy(lBuf, lKeyStr.c_str(), lKeyStr.length());
				lBuf[lKeyStr.length()] = 0;

				lKey.set(lBuf);
				pKeys.insert(lKey);
				iD++;
			}
		}
	}
}

void TestPGTree::defineKeys_int(Tkeys_int & pKeys, Tkeys_str const & pKeysStr)
{
	int i;
	for (i = 0; i < int(pKeysStr.size()); i++)
	{
		Value lKey;
		SETVALUE(lKey, STORE_INVALID_URIID, i, OP_SET);
		pKeys.insert(lKey);
	}
	Tkeys_str::const_iterator iK;
	for (iK = pKeysStr.begin(); pKeysStr.end() != iK; iK++)
	{
		Md5Stream lMd5S;
		unsigned char lMd5[16];
		lMd5S << (*iK).str << std::endl;
		lMd5S.flush_md5(lMd5);

		uint64_t lUi64 = 0;
		size_t iDigit;
		for (iDigit = 0; iDigit < 8; iDigit++) // Note: we truncate the 128 bits to 64 bits.
			lUi64 += (uint64_t)lMd5[iDigit] * (uint64_t)pow(256.0, (7.0 - iDigit));

		Value lKey;
		lKey.setI64(int64_t(lUi64)); lKey.property = STORE_INVALID_URIID; lKey.op = OP_SET;
		pKeys.insert(lKey);
	}
}

void TestPGTree::defineKeys_dbl(Tkeys_dbl & pKeys)
{
	// Insert a series of integer values (as doubles).
	int i;
	for (i = 0; i < 3000; i++)
	{
		Value lKey;
		SETVALUE(lKey, STORE_INVALID_URIID, (double)i, OP_SET);
		pKeys.insert(lKey);
	}
	assert(pKeys.size() == 3000);

	// Insert a series of values very close together.
	double d;
	for (d = 1.1, i = 0; i < 3000; i++, d += std::numeric_limits<double>::epsilon())
	{
		Value lKey;
		SETVALUE(lKey, STORE_INVALID_URIID, d, OP_SET);
		pKeys.insert(lKey);
	}
	assert(pKeys.size() == 6000);
	
	// Insert a series of very small values.
	for (d = pow(10.0, std::numeric_limits<double>::min_exponent10 + 3), i = 0; i < 3000; i++, d += std::numeric_limits<double>::epsilon())
	{
		Value lKey;
		SETVALUE(lKey, STORE_INVALID_URIID, d, OP_SET);
		pKeys.insert(lKey);
	}
	assert(pKeys.size() == 9000);

	// Insert a series of very large values.
	for (d = pow(10.0, std::numeric_limits<double>::max_exponent10 - 3), i = 0; i < 3000; i++, d += pow(10.0, std::numeric_limits<double>::max_exponent10 - 8))
	{
		Value lKey;
		SETVALUE(lKey, STORE_INVALID_URIID, d, OP_SET);
		pKeys.insert(lKey);
	}
	assert(pKeys.size() == 12000);
}

// TODO: finish/fix PART4
// TODO: add VT_DATETIME
// TODO: Sonic recommends multi-set/multi-map type of scenario
// TODO: vary the commitpin/tx granularity
// TODO: a bit more scenarios; mixed keys?
// TODO: check coverage of pgtree; iterate until decent
