/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#if (!defined(_mvstoreex_serialization_h) && !defined(SERIALIZATION_FOR_IPC)) || (!defined(_mvstoreipc_serialization_h) && defined(SERIALIZATION_FOR_IPC))
#ifdef SERIALIZATION_FOR_IPC
	#define _mvstoreipc_serialization_h
#else
	#define _mvstoreex_serialization_h
#endif

#include "mvstoreexports.h"
#include "mvstoreapi.h"

#ifdef SERIALIZATION_FOR_IPC
	#include "ipcheap.h"
#endif
#include <vector>
#include <string>
#include <ostream>
#include <istream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#ifndef Darwin
#include <malloc.h>
#endif
#include <stdlib.h>
#include <search.h>
#include <assert.h>
using namespace Afy;

#ifdef PSSER_NAMESPACE
	#undef PSSER_NAMESPACE
	#undef PSSER_OSTREAM
	#undef PSSER_ISTREAM
	#undef PSSER_MANIP
#endif
#ifdef SERIALIZATION_FOR_IPC
	#define PSSER_NAMESPACE ipcser
	#define PSSER_OSTREAM IPCostream
	#define PSSER_ISTREAM IPCistream
	#define PSSER_MANIP ipcser
#else
	#define PSSER_NAMESPACE MvStoreSerialization
	#define PSSER_OSTREAM std::ostream
	#define PSSER_ISTREAM std::istream
	#define PSSER_MANIP std
#endif

#ifndef PROP_SPEC_FIRST
	#define PROP_SPEC_FIRST PROP_SPEC_STAMP
#endif

/*
#if defined(WIN32) && defined(_DEBUG) && defined(TRACK_MEMORY)
	#include <crtdbg.h>
	#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
*/

/**
 * External raw serialization/deserialization code for the store.
 * This is our own internal protobuf ancestor.  Was used
 * for replication, remoting, dump&load, logging, pin comparisons,
 * serialization of parameters for async notifications,
 * and Afy::Value-parameter passing in IPC (to support
 * values coming from the stack).  Still used by the tests,
 * and contains useful primitives (e.g. abstraction of
 * collection navigation etc.).
 *
 * Old notes:
 *
 * This separation of "Primitives" is meant to minimize the risk
 * of altering critical raw implementation when fiddling with
 * debugging implementation, while preserving as much as possible
 * a common implementation (to update and fix bugs in one place).
 *
 * Bottom line: HANDLE EVERYTHING CALLED "RAW" WITH EXTREME CAUTION,
 * and play to your heart's content with everything called "Dbg"...
 */
namespace PSSER_NAMESPACE
{
	#ifdef SERIALIZATION_FOR_IPC
		/**
		 * IPCostream/IPCistream
		 * Specialized iostream implementation, completely inlined, with ~no allocation, and no formatting.
		 */
		class IPCostream
		{
			protected:
				typedef std::vector<char *> TBuffers;
				template <class T> inline static T mymin(T t1, T t2) { return (t1 <= t2) ? t1 : t2; } // @#!$
			protected:
				// REVIEW: Should we worry about stack alignment?
				char mDst[IPC_SIZE_COMMAND]; // Main buffer area (typically allocated on the stack, along with the IPCostream object itself).
				TBuffers * mDstExtra; // Additional buffers, in case the main area is insufficient; only supported if mCursorExtra!=-1.
				size_t mCursor; // Current (write) offset in the stream.
				size_t mCursorExtra; // Offset of the last "extra" block in the stream, or -1 if "extra" blocks are not supported by this instance.
			public:
				inline IPCostream(bool pCanGrow = false) : mDstExtra(0), mCursor(0), mCursorExtra(pCanGrow ? 0 : size_t(-1)) {}
				inline ~IPCostream() { if (mDstExtra) { while (!mDstExtra->empty()) { delete [] mDstExtra->back(); mDstExtra->pop_back(); } mDstExtra->clear(); } delete mDstExtra; mDstExtra = 0; }
				inline void imbue(std::locale const &) {}
				inline std::streamoff tellp() const { return (std::streamoff)mCursor; }
				inline void write(char const * pWhat, size_t pLen) { _append(pWhat, pLen); }			
				inline IPCostream & operator <<(IPCostream & (*pFunc)(IPCostream &)) { return (*pFunc)(*this); }
			public:
				char const * getDst() const { return &mDst[0]; }
				inline void grabOutput(char * pOut) const
				{
					memcpy(pOut, mDst, mymin(mCursor, size_t(IPC_SIZE_COMMAND)));
					if (mDstExtra && mDstExtra->size() > 0)
					{
						size_t lOff = IPC_SIZE_COMMAND;
						size_t iB;
						for (iB = 0; iB < mDstExtra->size(); iB++, lOff += IPC_SIZE_COMMAND)
							memcpy(pOut + lOff, mDstExtra->at(iB), mymin(mCursor - lOff, size_t(IPC_SIZE_COMMAND)));
					}
				}
			public:
				inline void _append(char const * pBytes, size_t pNumBytes)
				{
					size_t const lNumBytes = mymin(pNumBytes, IPC_SIZE_COMMAND - (mCursor - mCursorExtra));
					if (lNumBytes)
					{
						if (!mDstExtra)
							memcpy(&mDst[mCursor], pBytes, lNumBytes);
						else
						{
							char * const lDst = mDstExtra->back();
							memcpy(&lDst[mCursor - mCursorExtra], pBytes, lNumBytes);
						}
						mCursor += lNumBytes;
					}
					if (lNumBytes < pNumBytes)
					{
						if (size_t(-1) != mCursorExtra)
						{
							char * lNewBuf = new char[IPC_SIZE_COMMAND];
							if (!mDstExtra)
								mDstExtra = new TBuffers;
							mDstExtra->push_back(lNewBuf);
							mCursorExtra += IPC_SIZE_COMMAND;
							_append(&pBytes[lNumBytes], pNumBytes - lNumBytes);
						}
						else
							assert(false && "pBytes overflows this IPCostream!");
					}
				}
		};
		inline IPCostream & hex(IPCostream & pStr) {return pStr;}
		inline IPCostream & dec(IPCostream & pStr) {return pStr;}
		inline IPCostream & endl(IPCostream & pStr) {pStr._append("\n", 1); return pStr;}
		inline IPCostream & ends(IPCostream & pStr) {pStr._append("\0", 1); return pStr;}
		template <class T> inline IPCostream & operator <<(IPCostream & pStr, T pArg) {pStr._append((char *)&pArg, sizeof(T)); return pStr;}
		template <class T> inline IPCostream & operator <<(IPCostream & pStr, T const * pArg) {pStr._append((char *)&pArg, sizeof(T *)); return pStr;}
		template <> inline IPCostream & operator <<(IPCostream & pStr, char const * pArg) {pStr._append(pArg, strlen(pArg)); return pStr;}

		class IPCistream
		{
			public:
				char const * mSrc;
				size_t mSize, mCursor;
				bool mWhite;
			public:
				inline IPCistream(char const * pSrc, size_t pSize) : mSrc(pSrc), mSize(pSize), mCursor(0), mWhite(false) {}
				inline void imbue(std::locale const &) {}
				inline void get() { mCursor++; mWhite = false; }
				inline std::streamoff tellg() const { return (std::streamoff)mCursor; }
				inline void ignore(size_t pNum/*right type?*/) { mCursor += pNum; mWhite = true; }
				inline bool good() const { return mCursor < mSize; }
				inline void read(char * pDst, size_t pLen) { size_t const lLen = pLen > (mSize - mCursor) ? (mSize - mCursor) : pLen; memcpy(pDst, mSrc + mCursor, lLen); mCursor += lLen; mWhite = true; }
				inline IPCistream & operator >>(IPCistream & (*pFunc)(IPCistream &)) { return (*pFunc)(*this); }
		};
		inline IPCistream & hex(IPCistream & pStr) {return pStr;}
		inline IPCistream & dec(IPCistream & pStr) {return pStr;}
		inline IPCistream & endl(IPCistream & pStr) {pStr.mCursor++; return pStr;}
		inline IPCistream & ends(IPCistream & pStr) {pStr.mCursor++; return pStr;}
		template <class T> inline IPCistream & operator >>(IPCistream & pStr, T & pArg) { if (pStr.mWhite) pStr.mCursor++; memcpy(&pArg, pStr.mSrc + pStr.mCursor, sizeof(T)); pStr.mCursor += sizeof(T); pStr.mWhite = true; return pStr; }
		template <class T> inline IPCistream & operator >>(IPCistream & pStr, T *& pArg) { if (pStr.mWhite) pStr.mCursor++; memcpy(&pArg, pStr.mSrc + pStr.mCursor, sizeof(T *)); pStr.mCursor += sizeof(T *); pStr.mWhite = true; return pStr; }
	#endif

	/**
	 * Out/In
	 * Entry-points to serialize/deserialize mvstore data.
	 * WARNING: HANDLE WITH CARE...
	 */
	template <class TContextOut>
	class Out
	{
		public:
			inline static void value(TContextOut & pCtx, Value const & pValue);
			inline static void valueContent(TContextOut & pCtx, Value const & pValue, uint64_t pPersistedLen);
			inline static void property(TContextOut & pCtx, Value const & pValue);
			inline static void properties(TContextOut & pCtx, IPIN const & pPIN); // Note: Convenient for pin comparisons...
			inline static bool pin(TContextOut & pCtx, IPIN const & pPIN);
	};
	template <class TContextIn>
	class In
	{
		public:
			inline static void value(TContextIn & pCtx, Value & pValue);
			inline static void valueContent(TContextIn & pCtx, uint8_t valtype, Value & pValue);
			inline static void property(TContextIn & pCtx, Value & pValue);
			inline static bool pin(TContextIn & pCtx, IPIN & pPIN, bool pOverwrite = false);
	};

	/**
	 * Services
	 * Helper functions related to serialization.
	 */
	class Services
	{
		public:
			inline static bool clearPIN(IPIN & pPIN, PropertyID const * pExceptions = NULL, long pNumExceptions = 0);
		public:
			inline static uint64_t evaluateLength(Value const & pValue, uint32_t pUnicodeCharSize = sizeof(wchar_t));
			inline static bool containsValueOfType(Value const & pValue, ValueType pVT) { return contains(pValue, &Services::testType, pVT); }
			inline static bool containsValueOfType(IPIN const & pPIN, ValueType pVT) { return contains(pPIN, &Services::testType, pVT); }
			inline static bool containsProperty(Value const & pValue, PropertyID pPropID) { return contains(pValue, &Services::testProperty, pPropID); }
			inline static bool containsProperty(IPIN const & pPIN, PropertyID pPropID) { return contains(pPIN, &Services::testProperty, pPropID); }
		public:
			inline static bool testType(Value const & pValue, ValueType pVT) { return pValue.type == pVT; }
			inline static bool testProperty(Value const & pValue, PropertyID pPropID) { return pValue.property == pPropID; }
			inline static bool isPointerType(Value const & pValue) { return (Afy::VT_STRUCT == pValue.type) || (VT_COLLECTION == pValue.type) || (VT_RANGE == pValue.type) || (Afy::VT_STREAM == pValue.type) || (VT_STRING == pValue.type) || (Afy::VT_BSTR == pValue.type) || (VT_VARREF == pValue.type && pValue.length > 1) || (VT_STMT == pValue.type) || (VT_EXPR == pValue.type) || (VT_EXPRTREE == pValue.type); }
			inline static bool isRefType(Value const & pValue) { return (VT_REF == pValue.type || VT_REFID == pValue.type || VT_REFPROP == pValue.type || VT_REFIDPROP == pValue.type || VT_REFELT == pValue.type || VT_REFIDELT == pValue.type); }
			inline static bool isRefPtrType(Value const & pValue) { return (VT_REF == pValue.type || VT_REFPROP == pValue.type || VT_REFELT == pValue.type); }
			inline static bool isCollectionType(Value const & pValue) { return (VT_COLLECTION == pValue.type); }
			inline static bool isAddOp(ExprOp pOp) { return (OP_ADD == pOp || OP_ADD_BEFORE == pOp); }
			inline static bool isMoveOp(ExprOp pOp) { return (OP_MOVE == pOp || OP_MOVE_BEFORE == pOp); }
			template <class TTest, class TContent> inline static bool contains(Value const & pValue, TTest const & pTest, TContent const & pContent);
			template <class TTest, class TContent> inline static bool contains(IPIN const & pPIN, TTest const & pTest, TContent const & pContent);
		public:
			inline static void cvtUnicode(void * pString, uint32_t pLenInC, uint32_t pDstCharSize, uint32_t pOrgCharSize);
	};

	/**
	 * CollectionIterator
	 * Helper, to provide a uniform iteration interface for various types of collections.
	 */
	class CollectionIterator
	{
		protected:
			Afy::Value const & mCollection;
			unsigned long mI; // Note: May not be defined.
			Afy::ElementID mCurr;
		public:
			CollectionIterator(Afy::Value const & pCollection) : mCollection(pCollection), mI((unsigned long)-1), mCurr(0) {}
			inline Value const * beginAtIndex(unsigned long pIndex);
			inline Value const * beginAtEid(Afy::ElementID pEid);
			inline Value const * next();
			inline Value const * previous();
			inline void reset();
			inline static Afy::Value const * findValue(Afy::Value const & pCollection, Afy::ElementID pEid, unsigned long * pIt = NULL);
		private:
			CollectionIterator(CollectionIterator const &);
			CollectionIterator & operator =(CollectionIterator const &);
	};

	/**
	 * ContextOut/ContextIn
	 * Base classes for contextual information passed to serializers.
	 */
	#define SERIALIZATION_NORMALIZED_UNICODE_CHARSIZE 2
	class ContextOut
	{
		public:
			PSSER_OSTREAM & mOs;
			ISession * const mSession;
			uint32_t const mUnicodeCharSize;
			bool const mStrict;
			bool const mOrderProps;
			std::streamsize mFloatPrecision, mDoublePrecision;
			ContextOut(PSSER_OSTREAM & pOs, ISession * pSession, bool pStrict, bool pOrderProps, std::streamsize pFloatPrecision = 8, std::streamsize pDoublePrecision = 18)
				: mOs(pOs), mSession(pSession), mUnicodeCharSize(SERIALIZATION_NORMALIZED_UNICODE_CHARSIZE)
				, mStrict(pStrict), mOrderProps(pOrderProps)
				, mFloatPrecision(pFloatPrecision), mDoublePrecision(pDoublePrecision) { mOs.imbue(std::locale::classic()); }
			PSSER_OSTREAM & os() const { return mOs; }
			ISession & session() const { return *mSession; }
			virtual ~ContextOut(){};
		private:
			ContextOut & operator=(ContextOut const &);
	};
	class ContextIn
	{
		public:
			enum eVersion { kVFirst = 0, kVVTPARAMFix1, kVOPEDITFix1, kVVTREFIDVALFix1, kVLatest = kVVTREFIDVALFix1 };
		public:
			PSSER_ISTREAM & mIs;
			ISession * const mSession;
			uint32_t const mUnicodeCharSize;
			eVersion mVersion;
		protected:
			uint64_t mStoreIDOrg, mStoreIDOverride;
			unsigned long mPrefixEIDOrg, mPrefixEIDOverride;
		public:
			ContextIn(PSSER_ISTREAM & pIs, ISession * pSession) : mIs(pIs), mSession(pSession), mUnicodeCharSize(2), mVersion(kVLatest), mStoreIDOrg(0), mStoreIDOverride(0), mPrefixEIDOrg(0), mPrefixEIDOverride(0) { mIs.imbue(std::locale::classic()); }
			void setVersion(eVersion pVersion) { mVersion = pVersion; }
			eVersion getVersion() const { return mVersion; }
			void overrideStoreIDs(unsigned int pOrg, unsigned int pOverride) { mStoreIDOrg = uint64_t(pOrg) << 48; mStoreIDOverride = uint64_t(pOverride) << 48; mPrefixEIDOrg = calcPrefix(pOrg); mPrefixEIDOverride = calcPrefix(pOverride); }
			bool overridesStoreIDs() const { return mStoreIDOrg != mStoreIDOverride; }
			uint64_t getStoreIDOrg() const { return mStoreIDOrg; }
			uint64_t getStoreIDOverride() const { return mStoreIDOverride; }
			unsigned long getPrefixEIDOrg() const { return mPrefixEIDOrg; }
			unsigned long getPrefixEIDOverride() const { return mPrefixEIDOverride; }
			static unsigned long calcPrefix(unsigned int pStoreID) { return (unsigned long)((unsigned char)(pStoreID >> 8) ^ (unsigned char)(pStoreID)) << 24; }
			PSSER_ISTREAM & is() const { return mIs; }
			ISession & session() const { return *mSession; }
			virtual ~ContextIn(){};
		private:
			ContextIn & operator=(ContextIn const &);
	};

	/**
	 * ContextOutRaw/ContextInRaw
	 * Contextual information required for raw serialization.
	 */
	class PrimitivesOutRaw;
	class ContextOutRaw : public ContextOut
	{
		public:
			typedef PrimitivesOutRaw TPrimitives;
			ContextOutRaw(PSSER_OSTREAM & pOs, ISession & pSession) : ContextOut(pOs, &pSession, true, false) {}
		protected:
			ContextOutRaw(PSSER_OSTREAM & pOs) : ContextOut(pOs, NULL, true, false) {} // For ContextOutIPC...
			ContextOutRaw(PSSER_OSTREAM & pOs, ISession & pSession, bool pStrict, bool pOrderProps) : ContextOut(pOs, &pSession, pStrict, pOrderProps) {} // For ContextOutComparisons...
	};
	class PrimitivesInRaw;
	class ContextInRaw : public ContextIn
	{
		public:
			typedef PrimitivesInRaw TPrimitives;
			ContextInRaw(PSSER_ISTREAM & pIs, ISession & pSession) : ContextIn(pIs, &pSession) {}
		protected:
			ContextInRaw(PSSER_ISTREAM & pIs) : ContextIn(pIs, NULL) {} // For ContextInIPC...
	};

	typedef Out<ContextOutRaw> OutRaw;
	typedef In<ContextInRaw> InRaw;

	/**
	 * PrimitivesOutRaw/PrimitivesInRaw
	 * Serialization of atomic entities (invoked by Out/In), for replication, dump&load, pin comparisons etc.
	 * WARNING: HANDLE WITH CARE...
	 */
	class PrimitivesOutRaw
	{
		public:
			inline static void outCvtString(ContextOut & pCtx, wchar_t const * pString, uint32_t pLenInB);
			template <class T> inline static void outCvtString(ContextOut & pCtx, T const * pString, uint32_t pLenInB);
			template <class T> inline static void outString(ContextOut & pCtx, T const * pString, uint32_t pLenInB);
			template <class T> inline static void outStream(ContextOut & pCtx, Afy::IStream & pStream, T *, uint64_t pLenInB = uint64_t(~0));
			inline static void outQuery(ContextOut & pCtx, Value const & pValue);
			inline static void outExpr(ContextOut & pCtx, Value const & pValue);
			inline static void outIID(ContextOut & pCtx, IdentityID const & pIID);
			inline static void outRef(ContextOut & pCtx, PID const & pPID);
			inline static void outRef(ContextOut & pCtx, PID const & pPID, PropertyID const & pPropID);
			inline static void outRef(ContextOut & pCtx, PID const & pPID, PropertyID const & pPropID, ElementID const & pEid);
			inline static void outCLSID(ContextOut & pCtx, DataEventID const & pCLSID);
			inline static void outClassSpec(ContextOutRaw & pCtx, SourceSpec const & pClassSpec);
			inline static void outDateTime(ContextOutRaw & pCtx, DateTime const & pDateTime);
			inline static void outPropertyID(ContextOut & pCtx, PropertyID const & pPropID);
			inline static void beginValue(ContextOut & pCtx, Value const & pValue, uint64_t * pLen);
			inline static void endValue(ContextOut &, Value const &) {}
			inline static void beginProperty(ContextOut & pCtx, PropertyID const & pPropID) { outPropertyID(pCtx, pPropID); }
			inline static void endProperty(ContextOut &, PropertyID const &) {}
			inline static bool beginPIN(ContextOut & pCtx, IPIN const & pPIN) { pCtx.os() << pPIN.getNumberOfProperties() << " " << pPIN.getFlags() << " "; return true; }
			inline static void endPIN(ContextOut &, IPIN const &) {}
		public:
			static ValueType normalizeVT(Value const & pValue);
	};
	class PrimitivesInRaw
	{
		public:
			inline static uint32_t inCvtString(ContextIn & pCtx, wchar_t *& pString, uint32_t pLenInB);
			template <class T> inline static uint32_t inCvtString(ContextIn & pCtx, T *& pString, uint32_t pLen);
			template <class T> inline static void inString(ContextIn & pCtx, Value & pValue, T *);
			inline static void inQuery(ContextIn & pCtx, Value & pValue);
			inline static void inExpr(ContextIn & pCtx, Value & pValue);
			inline static void inIID(ContextIn & pCtx, IdentityID & pIID);
			inline static void inRef(ContextIn & pCtx, PID & pPID);
			inline static void inRef(ContextIn & pCtx, PID & pPID, PropertyID & pPropID);
			inline static void inRef(ContextIn & pCtx, PID & pPID, PropertyID & pPropID, ElementID & pEid);
			inline static void inRefIDVal(ContextIn & pCtx,Value & pValue);
			inline static void inRefIDELT(ContextIn & pCtx,Value & pValue);
			inline static void inCLSID(ContextIn & pCtx, DataEventID & pCLSID);
			inline static void inClassSpec(ContextInRaw & pCtx, SourceSpec & pClassSpec);
			inline static void inDateTime(ContextInRaw & pCtx, DateTime & pDateTime);
			template <class TContextIn> inline static void value(TContextIn & pCtx, Value & pValue){In<TContextIn>::value(pCtx,pValue);};
			inline static void inPropertyID(ContextIn & pCtx, PropertyID & pPropID);
			inline static void beginValue(ContextIn & pCtx, Value & pRead);
			inline static void endValue(ContextIn & pCtx, Value & pValue, Value const & pRead);
			inline static void beginProperty(ContextIn & pCtx, PropertyID & pRead) { inPropertyID(pCtx, pRead); }
			inline static void endProperty(ContextIn &, PropertyID & pPropID, PropertyID const & pRead) { pPropID = pRead; }
			inline static void beginPIN(ContextIn & pCtx, unsigned & pNumberOfProperties, uint32_t & pPINFlags, IPIN * = NULL) { pCtx.is() >> pNumberOfProperties >> pPINFlags; }
			inline static void endPIN(ContextIn &, IPIN * = NULL) {}
		public:
			template <class T> inline static void freePtr(T const * pPtr, ISession * pSession) { pSession ? pSession->free((void *)pPtr) : delete pPtr; }
			template <class T> inline static void freeArray(T const * pArray, ISession * pSession) { pSession ? pSession->free((void *)pArray) : delete [] pArray; }
			inline static void freeValue(Value & pValue, ISession * pSession);
			inline static void freeValues(Value * pValue, size_t iNum, ISession * pSession);
			inline static void freeClassSpec(SourceSpec & pClassSpec, ISession * pSession);
	};

	/*************************************************************************/

	/**
	 * Out implementation.
	 * WARNING: HANDLE WITH CARE...
	 */
	template <class TContextOut>
	inline void Out<TContextOut>::valueContent(TContextOut & pCtx, Value const & pValue, uint64_t pPersistedLen)
	{
		static PID const lInvalidPID = {STORE_INVALID_PID, STORE_INVALID_IDENTITY};
		switch (pValue.type)
		{
			// Collections.
			case Afy::VT_STRUCT:
			{
				unsigned long i;
				for (i = 0; i < pValue.length; i++)
					property(pCtx, pValue.varray[i]);
				break;
			}
			case VT_COLLECTION:
				if (pValue.isNav())
				{
					size_t iV;
					Value const * lNext;
					assert(pValue.nav->count() == pPersistedLen && "Unhealthy collection [bug #5599]");
					for (lNext = pValue.nav->navigate(GO_FIRST), iV = 0; NULL != lNext && iV < pPersistedLen; lNext = pValue.nav->navigate(GO_NEXT), iV++)
						value(pCtx, *lNext);
					if (iV < pPersistedLen) // Note: Robustness for #5599 and potential similar issues (e.g. tx isolation issues).
					{
						Value lEmpty; lEmpty.setError(STORE_INVALID_URIID);
						for (; iV < pPersistedLen; iV++)
							value(pCtx, lEmpty);
					}
				} 
				else 
				{
					unsigned long i;
					for (i = 0; i < pValue.length; i++)
						value(pCtx, pValue.varray[i]);
					break;
				}
				break;
			case VT_RANGE:
			{
				unsigned long i;
				for (i = 0; i < pValue.length; i++)
					value(pCtx, pValue.range[i]);
				break;
			}

			// Streams.
			// Note: For replication, a different strategy may be adopted depending on pinet...
			case Afy::VT_STREAM:
			{
				Afy::IStream * lStream = pValue.stream.is;
				bool lCloned = false;
				if (RC_OK != lStream->reset())
					{ lStream = pValue.stream.is->clone(); lCloned = true; }
				switch (lStream->dataType())
				{
					case VT_STRING: case Afy::VT_BSTR: TContextOut::TPrimitives::outStream(pCtx, *pValue.stream.is, (char *)0, pPersistedLen); break;
					default: assert(false && "Unexisting stream format!"); break;
				}
				lCloned ? lStream->destroy() : (void)lStream->reset();
				break;
			}

			// Variable-length.
			case VT_STRING: TContextOut::TPrimitives::outString(pCtx, pValue.str, pValue.length); break;
			case Afy::VT_BSTR: TContextOut::TPrimitives::outString(pCtx, pValue.bstr, pValue.length); break;
			case VT_VARREF:
			{
				pCtx.os() << (int)pValue.refV.refN << " ";
				pCtx.os() << (int)pValue.refV.type << " ";
				if (pValue.length == 1)
					TContextOut::TPrimitives::outPropertyID(pCtx, pValue.refV.id);
				break;
			}
			case VT_STMT: TContextOut::TPrimitives::outQuery(pCtx, pValue); break;
			case VT_EXPR: TContextOut::TPrimitives::outExpr(pCtx, pValue); break;

			// Fixed-length.
			// Review: For VT_FLOAT/VT_DOUBLE, we could force std::fixed only for the hashing usage (#7905),
			//         instead of all usages, e.g. to limit the size of these values in a dump that would
			//         contain lots of them; for the moment it doesn't sound dramatic though, and
			//         backward compatibility should be trivial if we decide to change this later.
			case VT_ENUM: pCtx.os() << pValue.enu.enumid << "#" << pValue.enu.eltid << " "; break;
			case Afy::VT_INT: pCtx.os() << pValue.i << " "; break;
			case Afy::VT_UINT: pCtx.os() << pValue.ui << " "; break;
			case VT_INT64: pCtx.os() << pValue.i64 << " "; break;
			case VT_UINT64: pCtx.os() << pValue.ui64 << " "; break;
			#ifdef SERIALIZATION_FOR_IPC
				case VT_FLOAT: pCtx.os() << pValue.f << " "; break;
				case VT_DOUBLE: pCtx.os() << pValue.d << " "; break;
			#else
				case VT_FLOAT: pCtx.os() << std::fixed << std::setprecision(pCtx.mFloatPrecision) << pValue.f << " "; break;
				case VT_DOUBLE: pCtx.os() << std::fixed << std::setprecision(pCtx.mDoublePrecision) << pValue.d << " "; break;
			#endif
			case Afy::VT_BOOL: pCtx.os() << pValue.b << " "; break;
			case VT_DATETIME: pCtx.os() << pValue.ui64 << " "; break;
			case VT_INTERVAL: pCtx.os() << pValue.i64 << " "; break;
			case VT_CURRENT: break;

			// References.
			// Review: It would be more efficient to persist a table and only index here...
			case VT_REF: TContextOut::TPrimitives::outRef(pCtx, pValue.pin ? pValue.pin->getPID() : lInvalidPID); break;
			case VT_REFID: TContextOut::TPrimitives::outRef(pCtx, pValue.id); break;
			case VT_REFPROP: TContextOut::TPrimitives::outRef(pCtx, pValue.ref.pin ? pValue.ref.pin->getPID() : lInvalidPID, pValue.ref.pid); break;
			case VT_REFIDPROP: TContextOut::TPrimitives::outRef(pCtx, pValue.refId->id, pValue.refId->pid, pValue.refId->eid); break;
			case VT_REFELT: TContextOut::TPrimitives::outRef(pCtx, pValue.ref.pin ? pValue.ref.pin->getPID() : lInvalidPID, pValue.ref.pid, pValue.ref.eid); break;
			case VT_REFIDELT: TContextOut::TPrimitives::outRef(pCtx, pValue.refId->id, pValue.refId->pid, pValue.refId->eid); break;
			case VT_IDENTITY: TContextOut::TPrimitives::outIID(pCtx, pValue.iid); break;
			case VT_URIID: TContextOut::TPrimitives::outPropertyID(pCtx, pValue.uid); break;

			// Delete.
			case Afy::VT_ERROR: break;

			// TODO
			case VT_EXPRTREE:
			default:
				assert(!pCtx.mStrict && "Not yet implemented persistence required in real life!");
				break;
		}
	}

	template <class TContextOut>
	static inline void outValue(TContextOut & pCtx, Value const & pValue)
	{
		#if 0
			std::cout << "out: " << (int)pValue.type << std::endl;
		#endif
		uint64_t lPersistedLen;
		TContextOut::TPrimitives::beginValue(pCtx, pValue, &lPersistedLen);
		Out<TContextOut>::valueContent(pCtx, pValue, lPersistedLen);
		TContextOut::TPrimitives::endValue(pCtx, pValue);
	}

	template <class TContextOut>
	inline void Out<TContextOut>::value(TContextOut & pCtx, Value const & pValue)
	{
		outValue<TContextOut>(pCtx, pValue);
	}

	template <class TContextOut>
	static inline void outProperty(TContextOut & pCtx, Value const & pValue)
	{
		TContextOut::TPrimitives::beginProperty(pCtx, pValue.property);
		Out<TContextOut>::value(pCtx, pValue);
		TContextOut::TPrimitives::endProperty(pCtx, pValue.property);
	}

	template <class TContextOut>
	inline void Out<TContextOut>::property(TContextOut & pCtx, Value const & pValue)
	{
		outProperty<TContextOut>(pCtx, pValue);
	}

	struct PropertyNameAndID
	{
		char * mName;
		PropertyID mPropID;
		static int compare(const void * p1, const void * p2) { return strcmp(((PropertyNameAndID *)p1)->mName, ((PropertyNameAndID *)p2)->mName); }
		static void sortProperties(ISession * pSession, IPIN const & pPIN, unsigned const pNumberOfProperties, PropertyNameAndID * pResult)
		{
			unsigned i;
			assert(pNumberOfProperties == pPIN.getNumberOfProperties());
			for (i = 0; i < pNumberOfProperties; i++)
			{
				Value const * const lV = pPIN.getValueByIndex(i);
				if (!lV)
					continue;
				pResult[i].mPropID = lV->getPropID();
				size_t lSize = 0;
				if (pSession) pSession->getURI(pResult[i].mPropID, NULL, lSize);
				if (lSize > 0)
				{
					pResult[i].mName = new char[1 + lSize];
					pResult[i].mName[lSize++] = 0;
					pSession->getURI(pResult[i].mPropID, pResult[i].mName, lSize);
				}
				else
				{
					pResult[i].mName = new char[32];
					sprintf(pResult[i].mName, "%u", pResult[i].mPropID);

					static bool sWarned = false;
					if (!sWarned && (NULL != pSession) && pResult[i].mPropID < PROP_SPEC_FIRST)
					{
						std::cerr << "sortProperties warning: PropertyID not registered!" << std::endl << std::flush;
						sWarned = true;
					}
				}
			}
			qsort(pResult, pNumberOfProperties, sizeof(PropertyNameAndID), &PropertyNameAndID::compare);
		}
	};

	template <class TContextOut>
	inline void Out<TContextOut>::properties(TContextOut & pCtx, IPIN const & pPIN)
	{
		unsigned i;
		unsigned const lNumberOfProperties = pPIN.getNumberOfProperties();
		if (pCtx.mOrderProps)
		{
			PropertyNameAndID * const lSorted = (PropertyNameAndID *)alloca(lNumberOfProperties * sizeof(PropertyNameAndID));
			PropertyNameAndID::sortProperties(pCtx.mSession, pPIN, lNumberOfProperties, lSorted);
			for (i = 0; i < lNumberOfProperties; i++)
			{
				Value const * const lV = pPIN.getValue(lSorted[i].mPropID);
				if (!lV)
				{
					std::cerr << "Out<TContextOut>::properties warning: NULL value for property " << lSorted[i].mPropID << std::endl;
					continue;
				}
				property(pCtx, *lV);
				delete [] lSorted[i].mName;
			}
		}
		else
		{
			for (i = 0; i < lNumberOfProperties; i++)
			{
				Value const * const lV = pPIN.getValueByIndex(i);
				property(pCtx, *lV);
			}
		}
	}

	template <class TContextOut>
	inline bool Out<TContextOut>::pin(TContextOut & pCtx, IPIN const & pPIN)
	{
		if (!TContextOut::TPrimitives::beginPIN(pCtx, pPIN))
			return false;
		properties(pCtx, pPIN);
		TContextOut::TPrimitives::endPIN(pCtx, pPIN);
		return true;
	}

	/**
	 * In implementation.
	 * WARNING: HANDLE WITH CARE...
	 */
	template <class TContextIn>
	inline void In<TContextIn>::valueContent(TContextIn & pCtx, uint8_t valtype, Value & pValue)
	{
		switch (valtype)
		{
			case Afy::VT_STRUCT:
			{
				if (pValue.length == 0)
					pValue.varray = NULL;
				else if (pCtx.mSession)
					pValue.varray = (Value *)pCtx.mSession->malloc(pValue.length * sizeof(Value));
				else
					pValue.varray = new Value[pValue.length];

				size_t i, lRealLength = pValue.length;
				for (i = 0; i < pValue.length; i++)
				{
					property(pCtx, const_cast<Value&>(pValue.varray[i]));
					if (Afy::VT_ERROR == pValue.varray[i].type && lRealLength > i) // Note: Robustness for #5599 and potential similar issues (e.g. tx isolation issues).
						lRealLength = i;
				}
				if (lRealLength < pValue.length)
					pValue.length = (uint32_t)lRealLength;
				break;
			}

			// Collections (both types are stored as VT_COLLECTION, hence the pValue.length-independent mechanics for reload).
			case VT_COLLECTION:
			{
				if (pValue.length == 0)
					pValue.varray = NULL;
				else if (pCtx.mSession)
					pValue.varray = (Value *)pCtx.mSession->malloc(pValue.length * sizeof(Value));
				else
					pValue.varray = new Value[pValue.length];

				size_t i, lRealLength = pValue.length;
				for (i = 0; i < pValue.length; i++)
				{
					value(pCtx, const_cast<Value&>(pValue.varray[i]));
					if (Afy::VT_ERROR == pValue.varray[i].type && lRealLength > i) // Note: Robustness for #5599 and potential similar issues (e.g. tx isolation issues).
						lRealLength = i;
				}
				if (lRealLength < pValue.length)
					pValue.length = (uint32_t)lRealLength;
				break;
			}
			case VT_RANGE:
			{
				if(pValue.length > 0)
				{
					if (pCtx.mSession)
						pValue.range = (Value *)pCtx.mSession->malloc(pValue.length * sizeof(Value));
					else
						pValue.range = new Value[pValue.length];

					size_t i;
					for (i = 0; i < pValue.length; i++)
						value(pCtx, pValue.range[i]);
				}
				else
					pValue.range = NULL;
				break;
			}

			// Variable-length.
			case Afy::VT_STREAM: assert(false);
			case VT_STRING: TContextIn::TPrimitives::inString(pCtx, pValue, (char *)0); break;
			case Afy::VT_BSTR: TContextIn::TPrimitives::inString(pCtx, pValue, (unsigned char *)0); break;
			case VT_VARREF:
			{
				int lRefN, lType;
				pCtx.is() >> lRefN; pValue.refV.refN = (unsigned char)lRefN;
				pCtx.is() >> lType; pValue.refV.type = (unsigned char)lType;
				PropertyID * lPropIDs;
				uint32_t lNum = 1;
					lPropIDs = &pValue.refV.id;
				uint32_t i;
				if (pCtx.getVersion() < ContextIn::kVVTPARAMFix1)
				{
					// Patch for error in older persistence format...
					if (lRefN < 1)
						lRefN = 1;
					for (i = 0; i < (uint32_t)lRefN; i++)
					{
						PropertyID lPropID;
						TContextIn::TPrimitives::inPropertyID(pCtx, lPropID);
						if (i < lNum)
							lPropIDs[i] = lPropID;
					}
				}
				else
				{
					// Normal code...
					for (i = 0; i < lNum; i++)
						TContextIn::TPrimitives::inPropertyID(pCtx, lPropIDs[i]);
				}
				break;
			}
			case VT_STMT: TContextIn::TPrimitives::inQuery(pCtx, pValue); break;
			case VT_EXPR: TContextIn::TPrimitives::inExpr(pCtx, pValue); break;

			// Fixed-length.
			case VT_ENUM: pCtx.is() >> pValue.i; break;
			case Afy::VT_INT: pCtx.is() >> pValue.i; break;
			case Afy::VT_UINT: pCtx.is() >> pValue.ui; break;
			case VT_INT64: pCtx.is() >> pValue.i64; break;
			case VT_UINT64: pCtx.is() >> pValue.ui64; break;
			case VT_FLOAT: pCtx.is() >> pValue.f; break;
			case VT_DOUBLE: pCtx.is() >> pValue.d; break;
			case Afy::VT_BOOL: pCtx.is() >> pValue.b; break;
			case VT_DATETIME: pCtx.is() >> pValue.ui64; break;
			case VT_INTERVAL: pCtx.is() >> pValue.i64; break;
			case VT_CURRENT: break;

			// References.
			case VT_REF: assert(false);
			case VT_REFID: TContextIn::TPrimitives::inRef(pCtx, pValue.id); break;
			case VT_REFPROP: assert(false);
			case VT_REFIDPROP: TContextIn::TPrimitives::inRefIDVal(pCtx,pValue); break;
			case VT_REFELT: assert(false);
			case VT_REFIDELT: TContextIn::TPrimitives::inRefIDELT(pCtx,pValue); break;
			case VT_IDENTITY: TContextIn::TPrimitives::inIID(pCtx, pValue.iid); break;
			case VT_URIID: TContextIn::TPrimitives::inPropertyID(pCtx, pValue.uid); break;

			// Delete.
			case Afy::VT_ERROR: break;

			// TODO
			case VT_EXPRTREE:
			default:
				assert(false);
				break;
		}

	}
	template <class TContextIn>
	inline void In<TContextIn>::value(TContextIn & pCtx, Value & pValue)
	{
		Value lTmp;
		TContextIn::TPrimitives::beginValue(pCtx, lTmp);
		if (!pCtx.is().good())
			{ pValue.setError(STORE_INVALID_URIID); assert(false); return; }
		pValue.length = lTmp.length;
		#if 0
			std::cout << "in: " << (int)lTmp.type << std::endl;
		#endif
		In<TContextIn>::valueContent(pCtx, lTmp.type,pValue);

		TContextIn::TPrimitives::endValue(pCtx, pValue, lTmp);
	}

	template <class TContextIn>
	inline void In<TContextIn>::property(TContextIn & pCtx, Value & pValue)
	{
		PropertyID lPropID;
		TContextIn::TPrimitives::beginProperty(pCtx, lPropID);
		TContextIn::TPrimitives::value(pCtx, pValue);
		TContextIn::TPrimitives::endProperty(pCtx, pValue.property, lPropID);
	}

	template <class TContextIn>
	inline bool In<TContextIn>::pin(TContextIn & pCtx, IPIN & pPIN, bool pOverwrite)
	{
		bool lTotalSuccess = true;
		unsigned i;
		unsigned lNumberOfProperties;
		uint32_t lPINFlags;
		TContextIn::TPrimitives::beginPIN(pCtx, lNumberOfProperties, lPINFlags, &pPIN);
		for (i = 0; i < lNumberOfProperties && pCtx.is().good(); i++)
		{
			Value lV;
			property(pCtx, lV);

			if (pOverwrite && PROP_SPEC_CREATED == lV.property && pPIN.getValue(lV.property))
				;
			else if (pOverwrite && PROP_SPEC_UPDATED == lV.property && pPIN.getValue(lV.property))
				;
			else if (Afy::VT_COLLECTION == lV.type && !lV.isNav())
			{
				// Review with Mark: Should I need to do this?
				size_t i;
				for (i = 0; i < lV.length; i++)
				{
					ElementID lEid = lV.varray[i].eid;
					const_cast<Value&>(lV.varray[i]).eid = STORE_LAST_ELEMENT;
					const_cast<Value&>(lV.varray[i]).property = lV.property;
					const_cast<Value&>(lV.varray[i]).op = OP_ADD;
					RC const lRC = pPIN.modify(&lV.varray[i], 1, MODE_FORCE_EIDS, &lEid);
					if (RC_OK != lRC)
						{ assert(false); lTotalSuccess = false; }
				}
			}
			else
			{
				RC lRC = pPIN.modify(&lV, 1, MODE_FORCE_EIDS, &lV.eid);
				if (RC_INVPARAM == lRC && (pPIN.getFlags()&PIN_PERSISTENT)!=0)
				{
					ElementID lEid = lV.eid;
					lV.eid = STORE_COLLECTION_ID; // Note: Using STORE_LAST_ELEMENT currently forces a collection form, even for 1 element...
					lRC = pPIN.modify(&lV, 1, MODE_FORCE_EIDS, &lEid);
				}
				if (RC_OK != lRC)
					{ assert(false); lTotalSuccess = false; }
			}

			TContextIn::TPrimitives::freeValue(lV, pCtx.mSession);
		}
		TContextIn::TPrimitives::endPIN(pCtx, &pPIN);
		return lTotalSuccess;
	}

	/**
	 * Services implementation.
	 */
	inline bool Services::clearPIN(IPIN & pPIN, PropertyID const * pExceptions, long pNumExceptions)
	{
		unsigned int const lNum = pPIN.getNumberOfProperties();
		Value * const lVDelete = (Value *)alloca(lNum * sizeof(Value));
		unsigned int iP, iPr;
		for (iP = 0, iPr = 0; iP < lNum; iP++)
		{
			Value const * const lV = pPIN.getValueByIndex(iP);
			bool lIsException = false;
			if (pExceptions)
			{
				long iE;
				for (iE = 0; iE < pNumExceptions && !lIsException; iE++)
					lIsException = (pExceptions[iE] == lV->property);
			}
			if (!lIsException)
				lVDelete[iPr++].setDelete(lV->property);
		}
		return (RC_OK == pPIN.modify(lVDelete, iPr));
	}

	inline uint64_t Services::evaluateLength(Value const & pValue, uint32_t pUnicodeCharSize)
	{
		switch (pValue.type)
		{
			case VT_COLLECTION:
			{
				if (!pValue.isNav())
					return pValue.length;
				if (!pValue.nav)
					return 0;
				if (!pValue.nav->navigate(GO_FIRST)) // Note: Workaround for #5599 (i.e. inconsistent collections sent in notifications); assumes that nobody calls evaluateLength while enumerating...
					{ assert(0 == pValue.nav->count() && "Unhealthy collection [bug #5599]"); return 0; }
				return pValue.nav->count();
			}
			case Afy::VT_STREAM:
			{
				if (!pValue.stream.is)
					return 0;
				return pValue.stream.is->length();
			}
			default: return pValue.length;
		}
	}

	template <class TTest, class TContent>
	inline bool Services::contains(Value const & pValue, TTest const & pTest, TContent const & pContent)
	{
		if (pTest(pValue, pContent))
			return true;
		switch (pValue.type)
		{
			case Afy::VT_COLLECTION:
				if (pValue.isNav())
				{
					if (pValue.nav) { Value const * lNext; for (lNext = pValue.nav->navigate(GO_FIRST); NULL != lNext; lNext = pValue.nav->navigate(GO_NEXT)) { if (contains(*lNext, pTest, pContent)) return true; } } return false;
				}
			case Afy::VT_STRUCT:
				{ size_t i; for (i = 0; i < pValue.length; i++) if (contains(pValue.varray[i], pTest, pContent)) return true; } return false;
			default: break;
		}
		return false;
	}

	template <class TTest, class TContent>
	inline bool Services::contains(IPIN const & pPIN, TTest const & pTest, TContent const & pContent)
	{
		unsigned i;
		unsigned const lNumberOfProperties = pPIN.getNumberOfProperties();
		for (i = 0; i < lNumberOfProperties; i++)
			if (contains(*pPIN.getValueByIndex(i), pTest, pContent))
				return true;
		return false;
	}

	inline void Services::cvtUnicode(void * pString, uint32_t pLenInC, uint32_t pDstCharSize, uint32_t pOrgCharSize)
	{
		assert(pString && pDstCharSize > 0 && pOrgCharSize > 0);
		if (0 == pLenInC || pDstCharSize == pOrgCharSize)
			return;
		char * const lBuf = (char *)pString;
		uint32_t lDst, lSrc;
		if (pDstCharSize > pOrgCharSize) // Expand...
		{
			uint32_t const lDeltaC = pDstCharSize - pOrgCharSize;
			for (lSrc = (pLenInC - 1) * pOrgCharSize, lDst = (pLenInC - 1) * pDstCharSize; lSrc > 0; lSrc -= pOrgCharSize, lDst -= pDstCharSize)
			{
				assert(lDst > 0 && lSrc != lDst);
				memcpy(&lBuf[lDst], &lBuf[lSrc], pOrgCharSize);
				memset(&lBuf[lDst + pOrgCharSize], 0, lDeltaC);
			}
			memset(&lBuf[pOrgCharSize], 0, lDeltaC);
		}
		else // Shrink...
		{
			uint32_t const lSrcLenInB = pLenInC * pOrgCharSize;
			for (lSrc = pOrgCharSize, lDst = pDstCharSize; lSrc < lSrcLenInB; lSrc += pOrgCharSize, lDst += pDstCharSize)
				memcpy(&lBuf[lDst], &lBuf[lSrc], pDstCharSize);
			memset(&lBuf[lDst], 0, lSrcLenInB - lDst);
		}
	}

	/**
	 * CollectionIterator implementation.
	 */
	inline Value const * CollectionIterator::beginAtIndex(unsigned long pIndex)
	{
		reset();
		Value const * lV;
		if (mCollection.type==Afy::VT_COLLECTION || mCollection.type==Afy::VT_STRUCT)
		{
			if (mCollection.type==Afy::VT_STRUCT || !mCollection.isNav())
			{
				for (mI = 0; mI < pIndex && mI < mCollection.length; mI++);
				mCurr = (mI < mCollection.length) ? mCollection.varray[mI].eid : 0;
				return (mI < mCollection.length) ? &mCollection.varray[mI] : NULL;
			}
			else
			{
				for (lV = mCollection.nav->navigate(GO_FIRST), mI = 0; mI < pIndex && lV; lV = mCollection.nav->navigate(GO_NEXT), mI++);
				mCurr = lV ? lV->eid : 0;
				return lV;
			}
		}
		if (0 == pIndex)
			{ mI = 0; mCurr = mCollection.eid; return &mCollection; }
		return NULL;
	}

	inline Value const * CollectionIterator::beginAtEid(Afy::ElementID pEid)
	{
		reset();
		Value const * const lV = findValue(mCollection, pEid, &mI);
		mCurr = lV ? lV->eid : 0;
		if (lV && (unsigned long)-1 == mI)
			mI = 0;
		return lV;
	}

	inline Value const * CollectionIterator::next()
	{
		if ((unsigned long)-1 == mI || 0 == mCurr)
			return NULL; // Iteration not started.
		Value const * lV;
		if (mCollection.type==Afy::VT_COLLECTION || mCollection.type==Afy::VT_STRUCT)
		{
			if (mCollection.type==Afy::VT_STRUCT || !mCollection.isNav())
			{
				mI++;
				if (mI < mCollection.length)
					{ mCurr = mCollection.varray[mI].eid; return &mCollection.varray[mI]; }
				return NULL;
			}
			else
			{
				lV = mCollection.nav->navigate(GO_NEXT);
				mCurr = lV ? lV->eid : 0;
				return lV;
			}
		}
		return NULL;
	}

	inline Value const * CollectionIterator::previous()
	{
		if ((unsigned long)-1 == mI || 0 == mCurr)
			return NULL; // Iteration not started.
		Value const * lV;
		if (mCollection.type==Afy::VT_COLLECTION || mCollection.type==Afy::VT_STRUCT)
		{
			if (mCollection.type==Afy::VT_STRUCT || !mCollection.isNav())
			{
				if (mI > 0)
					{--mI; mCurr = mCollection.varray[mI].eid; return &mCollection.varray[mI]; }
				return NULL;
			}
			else
			{
				lV = mCollection.nav->navigate(GO_PREVIOUS);
				mCurr = lV ? lV->eid : 0;
				return lV;
			}
		}
		return NULL;
	}

	inline void CollectionIterator::reset()
	{
		mI = (unsigned long)-1;
		mCurr = 0;
	}

	inline Afy::Value const * CollectionIterator::findValue(Afy::Value const & pCollection, Afy::ElementID pEid, unsigned long * pIt)
	{
		// Warning: Usage of this function can result in O(n.log(n)), or O(n^2) patterns
		//          (the latter case would be expected for small collections only).
		if (pIt)
			*pIt = (unsigned long)-1;
		unsigned long i;
		if (pCollection.type==Afy::VT_COLLECTION || pCollection.type==Afy::VT_STRUCT)
		{
			if (pCollection.type==Afy::VT_STRUCT || !pCollection.isNav())
			{
				if(pEid == STORE_FIRST_ELEMENT)
					i = 0;
				else if(pEid == STORE_LAST_ELEMENT)
					i = pCollection.length > 0 ? pCollection.length - 1 : 0;
				else
					for (i = 0; i < pCollection.length && pCollection.varray[i].eid != pEid; i++);
				if (pIt)
					*pIt = i;
				return (i < pCollection.length) ? &pCollection.varray[i] : NULL;
			} else
			{
				return pCollection.nav->navigate(Afy::GO_FINDBYID, pEid);
			}
		}
		return (pCollection.eid == pEid || STORE_FIRST_ELEMENT == pEid || STORE_LAST_ELEMENT == pEid) ? &pCollection : NULL;
	}

	/**
	 * PrimitivesOutRaw implementation.
	 * WARNING: HANDLE WITH CARE...
	 */
	inline void PrimitivesOutRaw::outCvtString(ContextOut & pCtx, wchar_t const * pString, uint32_t pLenInB)
	{
		assert(pString);
		if (0 == pLenInB)
			; // Note: Just to be on the safe side... the store can return bogus pointers for 0-length values.
		else if (sizeof(wchar_t) == pCtx.mUnicodeCharSize)
			pCtx.os().write((char *)pString, pLenInB);
		else
		{
			assert(sizeof(wchar_t) > pCtx.mUnicodeCharSize);
			char * const lBuf = (char *)alloca(pLenInB);
			memcpy(lBuf, pString, pLenInB);
			uint32_t const lLenInC = pLenInB / sizeof(wchar_t);
			Services::cvtUnicode((wchar_t *)lBuf, lLenInC, pCtx.mUnicodeCharSize, sizeof(wchar_t));
			pCtx.os().write((char *)lBuf, lLenInC * pCtx.mUnicodeCharSize);
		}
	}

	template <class T>
	inline void PrimitivesOutRaw::outCvtString(ContextOut & pCtx, T const * pString, uint32_t pLenInB)
	{
		assert(pString && 1 == sizeof(T));
		if (pLenInB)
			pCtx.os().write((char *)pString, pLenInB);
	}

	template <class T>
	inline void PrimitivesOutRaw::outString(ContextOut & pCtx, T const * pString, uint32_t pLenInB)
	{
		if (pString && pLenInB)
		{
			outCvtString(pCtx, pString, pLenInB);
			pCtx.os() << " ";
		}
	}

	template <class T>
	inline void PrimitivesOutRaw::outStream(ContextOut & pCtx, Afy::IStream & pStream, T *, uint64_t pLenInB)
	{
		// Note: If pLenInB is specified (i.e. != uint64_t(~0)), then we will truncate or fill to make sure
		//       we write exactly that amount of bytes; normally pLenInB should be == lTotalReadInB, but
		//       due to bugs like 7938, we must take this precaution...
		
		if(0 == pLenInB)
			return;
		T lBuf[0x1000];
		size_t lRead;
		uint64_t lTotalReadInB;
		if (sizeof(lBuf[0]) > pCtx.mUnicodeCharSize)
			pLenInB *= pCtx.mUnicodeCharSize;
		for (lRead = pStream.read(lBuf, 0x1000), lTotalReadInB = 0;
			0 != lRead && lTotalReadInB < pLenInB;
			lRead = pStream.read(lBuf, 0x1000))
		{
			lTotalReadInB += lRead;
			if (lTotalReadInB > pLenInB)
			{
				assert(false && "pStream returned more bytes than expected! (bug 7938?)");
				lRead -= (unsigned long)(lTotalReadInB - pLenInB);
				lTotalReadInB = pLenInB;
			}
			outCvtString(pCtx, lBuf, (uint32_t)lRead);
		}
		if (uint64_t(~0) != pLenInB && lTotalReadInB < pLenInB)
		{
			assert(false && "pStream returned less bytes than expected! (bug 7938?)");
			memset(lBuf, 0, sizeof(lBuf));
			do
			{
				lRead = (pLenInB - lTotalReadInB) > sizeof(lBuf) ? sizeof(lBuf) : (unsigned long)(pLenInB - lTotalReadInB);
				outCvtString(pCtx, lBuf, (uint32_t)lRead);
				lTotalReadInB += lRead;
			} while (lTotalReadInB < pLenInB);
		}
		pCtx.os() << " ";
	}

	inline void PrimitivesOutRaw::outQuery(ContextOut & pCtx, Value const & pValue)
	{
		char * const lSer = pValue.stmt->toString();
		uint32_t const lSerLen = uint32_t(strlen(lSer));
		pCtx.os() << lSerLen << " ";
		outString(pCtx, lSer, lSerLen);
		static bool sWarned = false;
		if (pCtx.mSession)
			pCtx.session().free(lSer);
		else if (!sWarned)
		{
			std::cerr << "PrimitivesOutRaw warning: Leak due to NULL session while serializing query!" << std::endl << std::flush;
			sWarned = true;
		}
	}

	inline void PrimitivesOutRaw::outExpr(ContextOut & pCtx, Value const & pValue)
	{
		char * const lSer = pValue.expr->toString();
		uint32_t const lSerLen = uint32_t(strlen(lSer));
		pCtx.os() << lSerLen << " ";
		outString(pCtx, lSer, lSerLen);
		static bool sWarned = false;
		if (pCtx.mSession)
			pCtx.session().free(lSer);
		else if (!sWarned)
		{
			std::cerr << "PrimitivesOutRaw warning: Leak due to NULL session while serializing expr!" << std::endl << std::flush;
			sWarned = true;
		}
	}

	inline void PrimitivesOutRaw::outCLSID(ContextOut & pCtx, DataEventID const & pCLSID)
	{
		// Review: Can only persist a +/- irrelevant id at the moment...
		pCtx.os() << pCLSID << " ";
	}

	inline void PrimitivesOutRaw::outClassSpec(ContextOutRaw & pCtx, SourceSpec const & pClassSpec)
	{
		// Review: Can only persist a +/- irrelevant id at the moment...
		pCtx.os() << pClassSpec.objectID << " " << pClassSpec.nParams << " ";
		unsigned i;
		for (i = 0; i < pClassSpec.nParams; i++)
			Out<ContextOutRaw>::value(pCtx, pClassSpec.params[i]);
	}

	inline void PrimitivesOutRaw::outDateTime(ContextOutRaw & pCtx, DateTime const & pDateTime)
	{
		pCtx.os() << pDateTime.year << " ";
		pCtx.os() << pDateTime.month << " ";
		pCtx.os() << pDateTime.dayOfWeek << " ";
		pCtx.os() << pDateTime.day << " ";
		pCtx.os() << pDateTime.hour << " ";
		pCtx.os() << pDateTime.minute << " ";
		pCtx.os() << pDateTime.second << " ";
		pCtx.os() << pDateTime.microseconds << " ";
	}

	inline void PrimitivesOutRaw::outPropertyID(ContextOut & pCtx, PropertyID const & pPropID)
	{
		// Review: It would be more efficient to persist a global table and only an index here...
		size_t lPropertyURISize = 0;
		if (pCtx.mSession) pCtx.session().getURI(pPropID, NULL, lPropertyURISize);
		pCtx.os() << (unsigned int)lPropertyURISize << " ";
		if (0 == lPropertyURISize)
		{
			pCtx.os() << pPropID << " "; // Just in case...
			static bool sWarned = false;
			if (!sWarned && (NULL != pCtx.mSession) && pPropID < PROP_SPEC_FIRST)
			{
				std::cerr << "PrimitivesOutRaw warning: PropertyID not registered!" << std::endl << std::flush;
				sWarned = true;
			}
		}
		else
		{
			char * const lPropertyURI = (char *)alloca(1 + lPropertyURISize);
			lPropertyURI[lPropertyURISize++] = 0;
			pCtx.session().getURI(pPropID, lPropertyURI, lPropertyURISize);
			pCtx.os().write(lPropertyURI, (int)lPropertyURISize); pCtx.os() << " ";
		}
	}

	inline void PrimitivesOutRaw::outIID(ContextOut & pCtx, IdentityID const & pIID)
	{
		// Review: It would be more efficient to persist a global table and only an index here...
		size_t const lIdentityNameSize = (STORE_OWNER == pIID || NULL == pCtx.mSession) ? 0 : pCtx.session().getIdentityName(pIID, NULL, 0);
		size_t const lIdentityKeySize = (STORE_OWNER == pIID || NULL == pCtx.mSession) ? 0 : pCtx.session().getCertificate(pIID, NULL, 0);
		pCtx.os() << (unsigned int)lIdentityNameSize << " ";
		pCtx.os() << (unsigned int)lIdentityKeySize << " ";
		if (0 == lIdentityNameSize)
		{
			pCtx.os() << pIID << " ";
			static bool sWarned = false;
			if (!sWarned && (NULL != pCtx.mSession && STORE_OWNER != pIID))
			{
				std::cerr << "PrimitivesOutRaw warning: IdentityID not registered!" << std::endl << std::flush;
				sWarned = true;
			}
		}
		else
		{
			char * const lIdentityName = (char *)alloca(1 + lIdentityNameSize);
			unsigned char * const lIdentityKey = (unsigned char *)alloca(1 + lIdentityKeySize);
			lIdentityName[lIdentityNameSize] = 0;
			lIdentityKey[lIdentityKeySize] = 0;
			pCtx.session().getIdentityName(pIID, lIdentityName, 1 + lIdentityNameSize);
			pCtx.session().getCertificate(pIID, lIdentityKey, 1 + lIdentityKeySize);
			pCtx.os().write(lIdentityName, (int)lIdentityNameSize); pCtx.os() << " ";
			pCtx.os().write((char *)lIdentityKey, (int)lIdentityKeySize); pCtx.os() << " ";
		}
	}

	inline void PrimitivesOutRaw::outRef(ContextOut & pCtx, PID const & pPID)
	{
		pCtx.os() << PSSER_MANIP::hex << pPID.pid << " " << PSSER_MANIP::dec;
		outIID(pCtx, pPID.ident);
	}

	inline void PrimitivesOutRaw::outRef(ContextOut & pCtx, PID const & pPID, PropertyID const & pPropID)
	{
		outRef(pCtx, pPID);
		outPropertyID(pCtx, pPropID);
	}

	inline void PrimitivesOutRaw::outRef(ContextOut & pCtx, PID const & pPID, PropertyID const & pPropID, ElementID const & pEid)
	{
		outRef(pCtx, pPID, pPropID);
		pCtx.os() << pEid << " ";
	}

	inline void PrimitivesOutRaw::beginValue(ContextOut & pCtx, Value const & pValue, uint64_t * pLen)
	{
		// Normalize all equivalent types, to simplify persistence
		// and enable usage of persistence for comparisons;
		// convert NULL pointers for VT_COLLECTION and VT_STREAM to VT_ERROR.
		int const lPersistedType = normalizeVT(pValue);
		uint64_t const lPersistedLength = Services::evaluateLength(pValue, pCtx.mUnicodeCharSize);
		if (pLen)
			*pLen = lPersistedLength;

		// Write header.
		pCtx.os() << lPersistedType << " ";
		pCtx.os() << (int)pValue.meta << " ";
		#if 0 // Note: Most likely never to be persisted in this context...
			pCtx.os() << (int)pValue.flags << " ";
		#endif
		pCtx.os() << (int)pValue.op << " ";
		pCtx.os() << lPersistedLength << " ";
		pCtx.os() << pValue.eid << " ";
	}

	inline ValueType PrimitivesOutRaw::normalizeVT(Value const & pValue)
	{
		switch (pValue.type)
		{
			case VT_COLLECTION: return !pValue.isNav() || pValue.nav ? Afy::VT_COLLECTION : Afy::VT_ERROR;
			case Afy::VT_STREAM: return pValue.stream.is ? pValue.stream.is->dataType() : Afy::VT_ERROR;
			case VT_EXPR: return pValue.expr ? VT_EXPR : Afy::VT_ERROR;
			case VT_STMT: return pValue.stmt ? VT_STMT : Afy::VT_ERROR;
			case VT_REF: return VT_REFID;
			case VT_REFPROP: return VT_REFIDPROP;
			case VT_REFELT: return VT_REFIDELT;
			default: return (ValueType)pValue.type;
		}
	}

	/**
	 * PrimitivesInRaw implementation.
	 * WARNING: HANDLE WITH CARE...
	 */
	inline uint32_t PrimitivesInRaw::inCvtString(ContextIn & pCtx, wchar_t *& pString, uint32_t pLenInB)
	{
		pCtx.is().get();
		if (0 == pLenInB || !pCtx.is().good())
			{ pString = NULL; return 0; }

		// Allocate a unicode string of length
		// f(what's in the actual stream, the running platform's wchar_t).
		// Review: Is there a better approach? Convert everything to utf-8 and keep a flag? Other?
		uint32_t const lLenInC = pLenInB / pCtx.mUnicodeCharSize;
		uint32_t const lMaxC = (sizeof(wchar_t) > pCtx.mUnicodeCharSize) ? sizeof(wchar_t) : pCtx.mUnicodeCharSize;
		uint32_t const lNewLenInB = lMaxC * lLenInC;
		assert(lNewLenInB >= pLenInB);

		char * lBuf;
		if (pCtx.mSession)
			lBuf = (char *)pCtx.mSession->malloc(lNewLenInB + lMaxC);
		else
			lBuf = new char [lNewLenInB + lMaxC];

		pCtx.is().read(lBuf, pLenInB);
		pString = (wchar_t *)lBuf;

		// Convert to the running platform's wchar_t (in-place).
		Services::cvtUnicode(pString, lLenInC, sizeof(wchar_t), pCtx.mUnicodeCharSize);
		memset(&lBuf[lNewLenInB], 0, lMaxC);
		return lLenInC;
	}

	template <class T>
	inline uint32_t PrimitivesInRaw::inCvtString(ContextIn & pCtx, T *& pString, uint32_t pLen)
	{
		assert(1 == sizeof(T));
		if (pCtx.mSession)
			pString = (T *)pCtx.mSession->malloc((1 + pLen) * sizeof(T));
		else
			pString = new T [1 + pLen];

		pCtx.is().get();
		if (pLen && pCtx.is().good())
			pCtx.is().read((char *)pString, pLen);
		pString[pLen] = 0;
		return pLen;
	}
	
	template <class T>
	inline void PrimitivesInRaw::inString(ContextIn & pCtx, Value & pValue, T *)
	{
		T * lBuf;
		uint32_t const lLenInC = inCvtString(pCtx, lBuf, pValue.length);
		pValue.set(lBuf, lLenInC);
	}
	
	inline void PrimitivesInRaw::inQuery(ContextIn & pCtx, Value & pValue)
	{
		uint32_t const lSerLen = pValue.length;
		pCtx.is() >> pValue.length;
		inString(pCtx, pValue, (char *)0);
		IStmt * const lQ = (pCtx.mSession && pCtx.is().good()) ? pCtx.session().createStmt(pValue.str) : NULL;
		freeArray(pValue.str, pCtx.mSession);
		pValue.length = lSerLen;
		pValue.set(lQ);
	}

	inline void PrimitivesInRaw::inExpr(ContextIn & pCtx, Value & pValue)
	{
		uint32_t const lSerLen = pValue.length;
		pCtx.is() >> pValue.length;
		inString(pCtx, pValue, (char *)0);
		IExpr * const lE = (pCtx.mSession && pCtx.is().good()) ? pCtx.session().createExpr(pValue.str) : NULL;
		freeArray(pValue.str, pCtx.mSession);
		pValue.length = lSerLen;
		pValue.set(lE);
	}

	inline void PrimitivesInRaw::inIID(ContextIn & pCtx, IdentityID & pIID)
	{
		// Review: It would be more efficient to persist a global table and only an index here...
		unsigned int lIdentityNameSize;
		unsigned int lIdentityKeySize;
		pCtx.is() >> lIdentityNameSize;
		pCtx.is() >> lIdentityKeySize;
		if (0 == lIdentityNameSize)
			pCtx.is() >> pIID;
		else if (pCtx.is().good())
		{
			char * const lIdentityName = (char *)alloca(1 + lIdentityNameSize);
			unsigned char * const lIdentityKey = (unsigned char *)alloca(1 + lIdentityKeySize);
			pCtx.is().get(); pCtx.is().read(lIdentityName, lIdentityNameSize);
			pCtx.is().get(); pCtx.is().read((char *)lIdentityKey, lIdentityKeySize);
			lIdentityName[lIdentityNameSize] = 0;
			lIdentityKey[lIdentityKeySize] = 0;
			pIID = pCtx.session().storeIdentity(lIdentityName, NULL, true, lIdentityKey, lIdentityKeySize);
		}
		else
			pIID = STORE_INVALID_IDENTITY;
	}

	inline void PrimitivesInRaw::inRef(ContextIn & pCtx, PID & pPID)
	{
		pCtx.is() >> PSSER_MANIP::hex >> pPID.pid >> PSSER_MANIP::dec;
		if (pCtx.overridesStoreIDs() && pCtx.getStoreIDOrg() == (pPID.pid & 0xffff000000000000ll))
		{
			pPID.pid &= 0x0000ffffffffffffll;
			pPID.pid |= pCtx.getStoreIDOverride();
		}
		inIID(pCtx, pPID.ident);
	}
	inline void PrimitivesInRaw::inRef(ContextIn & pCtx, PID & pPID, PropertyID & pPropID)
	{
		inRef(pCtx, pPID);
		inPropertyID(pCtx, pPropID);
	}

	inline void PrimitivesInRaw::inRef(ContextIn & pCtx, PID & pPID, PropertyID & pPropID, ElementID & pEid)
	{
		inRef(pCtx, pPID, pPropID);
		pCtx.is() >> pEid;
	}

	inline void PrimitivesInRaw::inRefIDVal(ContextIn & pCtx,Value & pValue)
	{
		Afy::RefVID * lRVID = pCtx.mSession ? (RefVID *)(pCtx.mSession->malloc(sizeof(RefVID))) : new RefVID;
		pValue.refId = lRVID;
		if (pCtx.getVersion() < ContextIn::kVVTREFIDVALFix1)
			inRef(pCtx, lRVID->id, lRVID->pid);
		else
			inRef(pCtx, lRVID->id, lRVID->pid, lRVID->eid);
	}
	inline void PrimitivesInRaw::inRefIDELT(ContextIn & pCtx,Value & pValue)
	{
		RefVID * lRVID = pCtx.mSession ? (RefVID *)(pCtx.mSession->malloc(sizeof(RefVID))) : new RefVID;
		pValue.refId = lRVID; 
		inRef(pCtx, lRVID->id, lRVID->pid, lRVID->eid); 
	}
	inline void PrimitivesInRaw::inCLSID(ContextIn & pCtx, DataEventID & pCLSID)
	{
		// Review: Can only persist a +/- irrelevant id at the moment...
		pCtx.is() >> pCLSID;
	}

	inline void PrimitivesInRaw::inClassSpec(ContextInRaw & pCtx, SourceSpec & pClassSpec)
	{
		// Review: Can only persist a +/- irrelevant id at the moment...
		pCtx.is() >> pClassSpec.objectID >> pClassSpec.nParams;
		if (pCtx.is().good())
		{
			if(pClassSpec.nParams > 0)
			{
				Value * lParams;
				if (pCtx.mSession)
					lParams = (Value *)pCtx.mSession->malloc(pClassSpec.nParams * sizeof(Value));
				else
					lParams = new Value[pClassSpec.nParams];

				pClassSpec.params = lParams;
				unsigned i;
				for (i = 0; i < pClassSpec.nParams; i++)
					In<ContextInRaw>::value(pCtx, lParams[i]);
			}
			else
				pClassSpec.params = NULL;
		}
		else
		{
			pClassSpec.nParams = 0;
			pClassSpec.params = NULL;
		}
	}

	inline void PrimitivesInRaw::inDateTime(ContextInRaw & pCtx, DateTime & pDateTime)
	{
		pCtx.is() >> pDateTime.year;
		pCtx.is() >> pDateTime.month;
		pCtx.is() >> pDateTime.dayOfWeek;
		pCtx.is() >> pDateTime.day;
		pCtx.is() >> pDateTime.hour;
		pCtx.is() >> pDateTime.minute;
		pCtx.is() >> pDateTime.second;
		pCtx.is() >> pDateTime.microseconds;
	}

	inline void PrimitivesInRaw::inPropertyID(ContextIn & pCtx, PropertyID & pPropID)
	{
		// Review: It would be more efficient to persist a global table and only an index here...
		unsigned int lPropertyURISize;
		pCtx.is() >> lPropertyURISize;
		if (0 == lPropertyURISize)
			pCtx.is() >> pPropID;
		else if (pCtx.is().good())
		{
			char * const lPropertyURI = (char *)alloca(1 + lPropertyURISize);
			pCtx.is().get();
			pCtx.is().read(lPropertyURI, lPropertyURISize);
			lPropertyURI[lPropertyURISize] = 0;

			URIMap lPM;
			lPM.URI = lPropertyURI;
			lPM.uid = STORE_INVALID_URIID;
			pCtx.session().mapURIs(1, &lPM);
			assert(STORE_INVALID_URIID != lPM.uid);
			pPropID = lPM.uid;
		}
		else
			pPropID = STORE_INVALID_URIID;
	}

	inline void PrimitivesInRaw::beginValue(ContextIn & pCtx, Value & pHeader)
	{
		int lType; pCtx.is() >> lType; pHeader.type = (uint8_t)lType;
		int lMeta; pCtx.is() >> lMeta; pHeader.meta = (uint8_t)lMeta;
		#if 0 // Note: Most likely never to be persisted in this context...
			int lFlags; pCtx.is() >> lFlags; pHeader.flags = (uint8_t)lFlags;
		#endif
		int lOp; pCtx.is() >> lOp; pHeader.setOp((ExprOp)lOp);
		uint64_t lPersistedLength; pCtx.is() >> lPersistedLength; pHeader.length = (uint32_t)lPersistedLength;
		ElementID lEid; pCtx.is() >> lEid;
		if (pCtx.overridesStoreIDs() && pCtx.getPrefixEIDOrg() == (lEid & 0xff000000))
		{
			lEid &= 0x00ffffff;
			lEid |= pCtx.getPrefixEIDOverride();
		}
		pHeader.eid = lEid;
	}

	inline void PrimitivesInRaw::endValue(ContextIn & pCtx, Value & pDst, Value const & pHeader)
	{
		if (pCtx.is().good())
		{
			pDst.eid = pHeader.eid;
			pDst.type = pHeader.type;
			pDst.meta = pHeader.meta;
			pDst.setOp((ExprOp)pHeader.op);
			// Note: Don't touch flags.
		}
		else
		{
			pDst.setError(STORE_INVALID_URIID);
			assert(false);
		}
	}

	inline void PrimitivesInRaw::freeValue(Value & pValue, ISession * pSession)
	{
		switch (pValue.type)
		{
			// Collections.
			case Afy::VT_COLLECTION: 
			{
				if (pValue.isNav())
				{
					if (pValue.nav) pValue.nav->destroy(); pValue.nav = NULL;
					break;
				}
			}
			case Afy::VT_STRUCT:
			{
				unsigned int i;
				for (i = 0; i < pValue.length && pValue.varray; i++)
					PrimitivesInRaw::freeValue(const_cast<Value&>(pValue.varray[i]), pSession);
				freeArray(pValue.varray, pSession); pValue.varray = NULL; pValue.length = 0;
				break;
			}
			case VT_RANGE:
			if(pValue.range)
			{
				PrimitivesInRaw::freeValue(const_cast<Value&>(pValue.range[0]), pSession);
				PrimitivesInRaw::freeValue(const_cast<Value&>(pValue.range[1]), pSession);
				freeArray(pValue.range, pSession); pValue.range = NULL;
				break;
			}

			// Streams.
			case Afy::VT_STREAM: if (pValue.stream.is) pValue.stream.is->destroy(); pValue.stream.is = NULL; break;

			// Variable-length.
			case VT_STRING: if (0 != pValue.length && NULL != pValue.str) { freeArray(pValue.str, pSession); } pValue.str = NULL; pValue.length = 0; break;
			case Afy::VT_BSTR: if (0 != pValue.length && NULL != pValue.bstr) { freeArray(pValue.bstr, pSession); } pValue.bstr = NULL; pValue.length = 0; break;
			case VT_STMT: break; // Review (XXX): What if not consumed?
			case VT_EXPR: break; // Review (XXX): What if not consumed?

			// Fixed-length.
			case VT_ENUM:
			case Afy::VT_INT:
			case Afy::VT_UINT:
			case VT_INT64:
			case VT_UINT64:
			case VT_FLOAT:
			case VT_DOUBLE:
			case Afy::VT_BOOL:
			case VT_DATETIME:
			case VT_INTERVAL:
			case VT_CURRENT:
				break;
			case VT_VARREF:
				break;

			// References.
			case VT_REF:
			case VT_REFID:
			case VT_REFPROP:
			case VT_REFELT:
				break;
			case VT_REFIDPROP:
			case VT_REFIDELT: 
				freePtr(pValue.refId, pSession); pValue.refId = NULL;
				break;
			case VT_IDENTITY:
			case VT_URIID:
				break;

			// Delete.
			case Afy::VT_ERROR: break;

			// TODO
			case VT_EXPRTREE:
			default:
				assert(false);
				break;
		}
	}

	inline void PrimitivesInRaw::freeValues(Value *pValue, size_t iNumValues, ISession * pSession)
	{
		size_t i;
		for (i = 0; i < iNumValues; i++) 
			PrimitivesInRaw::freeValue(pValue[i], pSession);
	}

	inline void PrimitivesInRaw::freeClassSpec(SourceSpec & pClassSpec, ISession * pSession)
	{
		freeValues(const_cast<Value*>(pClassSpec.params), pClassSpec.nParams, pSession);
		freeArray(pClassSpec.params, pSession);
		pClassSpec.params = NULL;
		pClassSpec.nParams = 0;
	}

#ifdef SERIALIZATION_FOR_IPC
	/*************************************************************************
	 * IPC Serialization:
	 * Serialization very close to the "Raw" one, but with optimizations for
	 * usage by IPC internal mechanics (e.g. no need for session object to
	 * translate ids into text form; serialize pointers instead of content
	 * when possible).  Also certain data types are needed nowhere else
	 * than here (e.g. URIMap, or any non-persistent structure
	 * passed as argument somewhere).
	 *************************************************************************/

	class PrimitivesOutIPC;
	class ContextOutIPC : public ContextOutRaw
	{
		public:
			typedef PrimitivesOutIPC TPrimitives;
			IPCHeapHeader const * const mHeapHeader;
			TClientAddresses const * const mClientAddresses;
			ContextOutIPC(PSSER_OSTREAM & pOs, IPCHeapHeader const * pHeapHeader, TClientAddresses const * pClientAddresses = NULL)
				: ContextOutRaw(pOs), mHeapHeader(pHeapHeader), mClientAddresses(pClientAddresses) {}
			inline bool isSharedMem(URIMap const * pPM, unsigned pNum) const;
			inline bool isSharedMem(SourceSpec const * pCS, unsigned pNum) const;
			inline bool isSharedMem(Value const * pV, unsigned pNum) const;
			bool isSharedMem(void const * pPtr) const { return mClientAddresses ? IPC::isSharedMem(pPtr, *mClientAddresses) : IPC::isSharedMem(pPtr, mHeapHeader); }
			void const * S2C(void const * pServerPtr) const { return mClientAddresses ? IPC::S2C(pServerPtr, mHeapHeader, *mClientAddresses) : pServerPtr; }
			void const * C2S(void const * pClientPtr) const { return mClientAddresses ? IPC::C2S(pClientPtr, mHeapHeader, *mClientAddresses) : pClientPtr; }
		private:
			ContextOutIPC & operator=(ContextOutIPC const &);
	};
	class PrimitivesInIPC;
	class ContextInIPC : public ContextInRaw
	{
		public:
			typedef PrimitivesInIPC TPrimitives;
			IPCHeapHeader const * const mHeapHeader;
			TClientAddresses const * mClientAddresses;
			ContextInIPC(PSSER_ISTREAM & pIs, IPCHeapHeader const * pHeapHeader, TClientAddresses const * pClientAddresses = NULL)
				: ContextInRaw(pIs), mHeapHeader(pHeapHeader), mClientAddresses(pClientAddresses) {}
			void setSession(ISession * pSession) { *((ISession **)&mSession) = pSession; }
			bool isSharedMem(void const * pPtr) const { return mClientAddresses ? IPC::isSharedMem(pPtr, *mClientAddresses) : IPC::isSharedMem(pPtr, mHeapHeader); }
			void const * S2C(void const * pServerPtr) const { return mClientAddresses ? IPC::S2C(pServerPtr, mHeapHeader, *mClientAddresses) : pServerPtr; }
			void const * C2S(void const * pClientPtr) const { return mClientAddresses ? IPC::C2S(pClientPtr, mHeapHeader, *mClientAddresses) : pClientPtr; }
		private:
			ContextInIPC & operator=(ContextInIPC const &);
	};

	typedef Out<ContextOutIPC> OutIPC;
	typedef In<ContextInIPC> InIPC;

	class PrimitivesOutIPC : public PrimitivesOutRaw
	{
		public:
			inline static void outPropertyMap(ContextOutIPC & pCtx, URIMap const & pPM);
			inline static void beginValue(ContextOutIPC & pCtx, Value const & pValue, uint64_t * pLen);
	};
	class PrimitivesInIPC : public PrimitivesInRaw
	{
		public:
			inline static void inPropertyMap(ContextIn & pCtx, URIMap & pPropertyMap);
			inline static void freePropertyMap(URIMap & pPM, ISession * pSession);
			inline static void freeValue(ContextInIPC & pCtx, Value & pValue, ISession * pSession);
	};

	inline bool ContextOutIPC::isSharedMem(URIMap const * pPM, unsigned pNum) const
	{
		if (NULL == pPM)
			return true;
		if (!isSharedMem((void *)pPM))
			return false;
		unsigned i;
		for (i = 0; i < pNum; i++)
			if (!isSharedMem(pPM[i].URI))
				return false;
		return true;
	}
	inline bool ContextOutIPC::isSharedMem(SourceSpec const * pCS, unsigned pNum) const
	{
		if (NULL == pCS)
			return true;
		if (!isSharedMem((void *)pCS))
			return false;
		unsigned i;
		for (i = 0; i < pNum; i++)
			if (!isSharedMem(pCS[i].params))
				return false;
		return true;
	}
	inline bool ContextOutIPC::isSharedMem(Value const * pV, unsigned pNum) const
	{
		if (NULL == pV)
			return true;
		if (!isSharedMem((void *)pV))
			return false;
		unsigned i;
		for (i = 0; i < pNum; i++)
			if ((Services::isPointerType(pV[i]) && !Services::isRefPtrType(pV[i])) && !isSharedMem(pV[i].str))
				return false;
		return true;
	}

	inline void PrimitivesOutIPC::outPropertyMap(ContextOutIPC & pCtx, URIMap const & pPM)
	{
		uint32_t const lURILen = uint32_t(pPM.URI ? strlen(pPM.URI) : 0);
		pCtx.os() << pPM.uid << " ";
		pCtx.os() << lURILen << " ";
		pCtx.os().write(pPM.URI, lURILen); pCtx.os() << " ";
	}
	inline void PrimitivesOutIPC::beginValue(ContextOutIPC & pCtx, Value const & pValue, uint64_t * pLen)
	{
		if (pCtx.isSharedMem(pValue.str))
		{
			int const lPersistedType = pValue.type; // Not "normalized", in this case...
			#ifdef WIN32
				uint64_t const lPersistedLength = Services::evaluateLength(pValue, pCtx.mUnicodeCharSize);
			#else
				uint64_t const lPersistedLength = Services::evaluateLength(pValue, sizeof(wchar_t)); // Note: See bug 18934.
			#endif
			if (pLen)
				*pLen = lPersistedLength;

			// Write header.
			pCtx.os() << lPersistedType << " ";
			pCtx.os() << (int)pValue.meta << " ";
			#if 0 // Note: Most likely never to be persisted in this context...
				pCtx.os() << (int)pValue.flags << " ";
			#endif
			pCtx.os() << (int)pValue.op << " ";
			pCtx.os() << lPersistedLength << " ";
			pCtx.os() << pValue.eid << " ";
		}
		else
			PrimitivesOutRaw::beginValue(pCtx, pValue, pLen);
	}
	inline void PrimitivesInIPC::inPropertyMap(ContextIn & pCtx, URIMap & pPM)
	{
		uint32_t lURILen;
		pPM.URI = NULL;
		pCtx.is() >> pPM.uid;
		pCtx.is() >> lURILen;
		if (lURILen && pCtx.is().good())
		{
			char * lURI;
			if (pCtx.mSession)
				lURI = (char *)pCtx.mSession->malloc(1 + lURILen);
			else
				lURI = new char[1 + lURILen];

			pPM.URI = lURI;
			pCtx.is().get();
			pCtx.is().read(lURI, lURILen);
			lURI[lURILen] = 0;
		}
	}
	inline void PrimitivesInIPC::freePropertyMap(URIMap & pPM, ISession * pSession)
	{
		if (pPM.URI)
			{ freeArray(pPM.URI, pSession); pPM.URI = NULL; }
	}
	inline void PrimitivesInIPC::freeValue(ContextInIPC & pCtx, Value & pValue, ISession * pSession)
	{
		if (Afy::VT_STREAM == pValue.type && pCtx.isSharedMem(pValue.stream.is))
			return;
		else if (Services::isRefPtrType(pValue) && pCtx.isSharedMem(pValue.pin))
			return;
		else if (Services::isPointerType(pValue) && pCtx.isSharedMem(pValue.str))
			return;
		PrimitivesInRaw::freeValue(pValue, pSession);
	}

	enum eIPCValueType { kIPCVTCopy = 0, kIPCVTPointer = 1, kIPCVTPointerToRef = 2, kIPCVTDefault = kIPCVTCopy };
	template<> inline void Out<ContextOutIPC>::value(ContextOutIPC & pCtx, Value const & pValue)
	{
		// Special handling for all pointer types...
		// Review: Could refine further for collections...
		if (Afy::VT_STREAM == pValue.type)
		{
			if (pCtx.isSharedMem(pValue.stream.is))
			{
				pCtx.os() << kIPCVTPointer << " ";
				ContextOutIPC::TPrimitives::beginValue(pCtx, pValue, NULL);
				pCtx.os() << pCtx.C2S(pValue.stream.is) << " ";
				ContextOutIPC::TPrimitives::endValue(pCtx, pValue);
			}
			else
				assert(false);
		}
		else if (Services::isRefPtrType(pValue) && pCtx.isSharedMem(pValue.pin))
		{
			pCtx.os() << kIPCVTPointerToRef << " ";
			ContextOutIPC::TPrimitives::beginValue(pCtx, pValue, NULL);
			if (VT_REF == pValue.type)
				{ pCtx.os() << pCtx.C2S(pValue.pin) << " "; }
			else if (VT_REFPROP == pValue.type)
				{ pCtx.os() << pCtx.C2S(pValue.ref.pin) << " "; ContextOutIPC::TPrimitives::outPropertyID(pCtx, pValue.ref.pid); }
			else if (VT_REFELT == pValue.type)
				{ pCtx.os() << pCtx.C2S(pValue.ref.pin) << " "; ContextOutIPC::TPrimitives::outPropertyID(pCtx, pValue.ref.pid); pCtx.os() << pValue.ref.eid << " "; }
			ContextOutIPC::TPrimitives::endValue(pCtx, pValue);
		}
		else if (Services::isPointerType(pValue) && pCtx.isSharedMem(pValue.str))
		{
			pCtx.os() << kIPCVTPointer << " ";
			ContextOutIPC::TPrimitives::beginValue(pCtx, pValue, NULL);
			pCtx.os() << pCtx.C2S(pValue.str) << " ";
			ContextOutIPC::TPrimitives::endValue(pCtx, pValue);
		}
		else
		{
			pCtx.os() << kIPCVTDefault << " ";
			Out<ContextOutRaw>::value(pCtx, pValue);
		}
	}
	template<> inline void In<ContextInIPC>::value(ContextInIPC & pCtx, Value & pValue)
	{
		int lIPCVT;
		pCtx.is() >> lIPCVT;
		if (kIPCVTPointer == lIPCVT)
		{
			memset(&pValue, 0, sizeof(Value)); // Quick solution to fully initialize pointer types containing more stuff, like VT_STREAM ('prefix')...
			Value lTmp;
			ContextInIPC::TPrimitives::beginValue(pCtx, lTmp);
			pValue.length = lTmp.length;
			void * lAddr;
			pCtx.is() >> lAddr;
			lAddr = (void *)pCtx.S2C(lAddr);
			if (Afy::VT_STREAM == lTmp.type)
				pValue.stream.is = (Afy::IStream *)lAddr;
			else
				pValue.str = (char *)lAddr;
			ContextInIPC::TPrimitives::endValue(pCtx, pValue, lTmp);
		}
		else if (kIPCVTPointerToRef == lIPCVT)
		{
			Value lTmp;
			ContextInIPC::TPrimitives::beginValue(pCtx, lTmp);
			assert(VT_REF == lTmp.type || VT_REFPROP == lTmp.type || VT_REFELT == lTmp.type);
			pValue.length = lTmp.length;
			void * lAddr;
			if (VT_REF == lTmp.type)
				{ pCtx.is() >> lAddr; pValue.pin = (IPIN *)pCtx.S2C(lAddr); }
			else if (VT_REFPROP == lTmp.type)
				{ pCtx.is() >> lAddr; pValue.ref.pin = (IPIN *)pCtx.S2C(lAddr); ContextInIPC::TPrimitives::inPropertyID(pCtx, pValue.ref.pid); }
			else if (VT_REFELT == lTmp.type)
				{ pCtx.is() >> lAddr; pValue.ref.pin = (IPIN *)pCtx.S2C(lAddr); ContextInIPC::TPrimitives::inPropertyID(pCtx, pValue.ref.pid); pCtx.is() >> pValue.ref.eid; }
			ContextInIPC::TPrimitives::endValue(pCtx, pValue, lTmp);
		}
		else
		{
			assert(kIPCVTDefault == lIPCVT);
			In<ContextInRaw>::value(pCtx, pValue);
		}
	}
#endif

	/*************************************************************************
	 * Comparisons Output:
	 * Serialization very close to the "Raw" one, but with just enough
	 * flexibility to produce identical output for equivalent pins
	 * (i.e. tolerate small discrepancies between almost identical pins;
	 * no need to deserialize).
	 *************************************************************************/
#ifndef SERIALIZATION_FOR_IPC

	#define PINHASH_VERSION_BEFORECACHES 0 // To recreate the <= m1.x hashes.
	#define PINHASH_VERSION_CHUNKS_1MB_16BYTEHASHES 1 // To recreate the m2 hashes before bug 9840 optimizations.
	#define PINHASH_VERSION_CHUNKS_256KB_4BYTEHASHES 2 // Optimizations for bug 9840.
	#define PINHASH_VERSION_LATEST PINHASH_VERSION_CHUNKS_256KB_4BYTEHASHES // Current format.

	#define PINHASH_HASHSIZE 16 // Using 128-bit hashes.
	#define PINHASH_CHUNKHASHSIZE 4 // Using only 32-bit hashes per chunk (n.b. changing this has implications in terms of the resulting hash...).
	#define PINHASH_THRESHOLD 0x20000 // Starting at 128Kb, we care about hashing perf of long strings.
	#define PINHASH_CHUNKSIZE 0x40000 // Stream chunking with 256Kb chunks (n.b. changing this has implications in terms of the resulting hash...).
	#define PINHASH_BLOCKSIZE 0x4000 // Scan the streams by 16Kb blocks (n.b. changing this has *no* implications in terms of the resulting hash).

	class IHashStreamFactory
	{
		public:
			virtual std::ostream * createStream() = 0;
			virtual bool hashStream(std::ostream *, unsigned char * pHash, size_t pHashSize) = 0;
			virtual void releaseStream(std::ostream *) = 0;
		public:
			virtual long getVersion() const { return PINHASH_VERSION_LATEST; }
			virtual size_t getChunkHashSizeInB() const { return PINHASH_VERSION_CHUNKS_1MB_16BYTEHASHES == getVersion() ? 16 : PINHASH_CHUNKHASHSIZE; }
			virtual size_t getChunkSizeInB() const { return PINHASH_VERSION_CHUNKS_1MB_16BYTEHASHES == getVersion() ? 0x100000 : PINHASH_CHUNKSIZE; }
	};

	class PrimitivesOutComparisons;
	class ContextOutComparisons : public ContextOutRaw
	{
		public:
			typedef PrimitivesOutComparisons TPrimitives;
			enum eFlags { kFExcludePropSpecDates = (1 << 0), kFExcludeEids = (1 << 1), };
			enum eCVersion { kCVFirst = 0, kCVChunkedStreams, kCVLatest = kCVChunkedStreams };
		protected:
			PropertyID const * mExceptions;
			long const mNumExceptions;
			long const mFlags;
			eCVersion mCVersion;
			IHashStreamFactory * mHashStreamFactory;
			std::ostream * mHashStream;
		public:
			ContextOutComparisons(PSSER_OSTREAM & pOs, ISession & pSession, long pFlags = 0, PropertyID const * pExceptions = NULL, long pNumExceptions = 0)
				: ContextOutRaw(pOs, pSession, true, true), mExceptions(pExceptions), mNumExceptions(pNumExceptions), mFlags(pFlags), mCVersion(kCVLatest), mHashStreamFactory(NULL), mHashStream(NULL) {}
			~ContextOutComparisons() { if (mHashStreamFactory && mHashStream) { mHashStreamFactory->releaseStream(mHashStream); mHashStream = NULL; } }
			long getFlags() const { return mFlags; }
			bool excludePropSpecDates() const { return 0 != (mFlags & kFExcludePropSpecDates); }
			bool excludeEids() const { return 0 != (mFlags & kFExcludeEids); }
			bool isException(PropertyID pPropID) const
			{
				if (excludePropSpecDates() && (PROP_SPEC_UPDATED == pPropID || PROP_SPEC_CREATED == pPropID))
					return true;
				long i;
				for (i = 0; i < mNumExceptions; i++)
					if (pPropID == mExceptions[i]) return true;
				return false;
			}
			PropertyID const * getExceptions() const { return mExceptions; }
			long getNumExceptions() const { return mNumExceptions; }
			eCVersion getCVersion() const { return mCVersion; }
			void setCVersion(eCVersion pCVersion) { mCVersion = pCVersion; }
			IHashStreamFactory * getHashStreamFactory() const { return mHashStreamFactory; }
			void setHashStreamFactory(IHashStreamFactory * pHashStreamFactory) { mHashStreamFactory = pHashStreamFactory; }
			std::ostream * getHashStream()
			{
				if (!mHashStreamFactory)
					return NULL;
				if (!mHashStream)
					mHashStream = mHashStreamFactory->createStream();
				return mHashStream;
			}
		private:
			ContextOutComparisons & operator=(ContextOutComparisons const &);
	};

	typedef Out<ContextOutComparisons> OutComparisons;

	class PrimitivesOutComparisons : public PrimitivesOutRaw
	{
		public:
			inline static void beginValue(ContextOutComparisons & pCtx, Value const & pValue, uint64_t * pLen);
			inline static unsigned long chunkStream(ISession & pSession, IHashStreamFactory & pHSFact, Value const & pValue, unsigned char *& pChunks, std::ostream * pStream = NULL, uint32_t * pFirstChunk = NULL, uint32_t * pLastChunk = NULL);
			inline static bool getChunksHash(IHashStreamFactory & pHSFact, unsigned long pNumChunks, unsigned char const * pChunks, unsigned char pHash[PINHASH_HASHSIZE], std::ostream * pStream = NULL);
	};

	inline void PrimitivesOutComparisons::beginValue(ContextOutComparisons & pCtx, Value const & pValue, uint64_t * pLen)
	{
		int const lPersistedType = normalizeVT(pValue);
		uint64_t lPersistedLength = Services::evaluateLength(pValue, pCtx.mUnicodeCharSize);
		if (VT_REFID == lPersistedType || VT_REFIDPROP == lPersistedType || VT_REFIDELT == lPersistedType)
			lPersistedLength = 1; // We don't care about the subtle struct size differences between gcc and vc...
		if (pLen)
			*pLen = lPersistedLength;
		pCtx.os() << lPersistedType << " ";
		pCtx.os() << lPersistedLength << " ";
		if (!pCtx.excludeEids())
			pCtx.os() << pValue.eid << " ";
	}

	inline unsigned long PrimitivesOutComparisons::chunkStream(ISession & pSession, IHashStreamFactory & pHSFact, Value const & pValue, unsigned char *& pChunks, std::ostream * pStream, uint32_t * pFirstChunk, uint32_t * pLastChunk)
	{
		// Note: lNumChunks and pChunks represent only the chunks for the requested range.
		unsigned long lNumChunks = 0;
		unsigned long const lFirstChunk = pFirstChunk ? (*pFirstChunk) : 0;
		pChunks = NULL;
		std::ostream * const lOs = pStream ? pStream : pHSFact.createStream();
		if (!lOs)
			{ assert(false && "No stream factory in pCtx!"); return lNumChunks; }
		uint64_t const lTotalLength = Services::evaluateLength(pValue, SERIALIZATION_NORMALIZED_UNICODE_CHARSIZE);
		#if 1
			// Note: In order to obtain a 1st chunk == a plain property, the property's header
			//       must be streamed in the first chunk; however, to not force a re-generation
			//       of the first chunk when there are more than 1 chunk (just to update the length
			//       in this header), we effectively encode here only the length of the first chunk.
			// Note: I decided to use STORE_COLLECTION_ID systematically, because chunking
			//       only applies to non-collections at the moment, and because the eid of single-element
			//       values is sometimes unpredictable; unfortunately, this is not a generalized
			//       convention at the moment...
			if (0 == lFirstChunk)
			{
				MvStoreSerialization::ContextOutComparisons lSerCtx(*lOs, pSession);
				MvStoreSerialization::PrimitivesOutComparisons::outPropertyID(lSerCtx, pValue.getPropID());
				int const lType = normalizeVT(pValue);
				(*lOs) << lType << " ";
				(*lOs) << (uint64_t)(lTotalLength > pHSFact.getChunkSizeInB() ? pHSFact.getChunkSizeInB() : lTotalLength) << " ";
				(*lOs) << STORE_COLLECTION_ID << " ";
			}
		#endif
		switch (pValue.type)
		{
			// Ignore collections for now; collections of large streams are incomplete, and not used.
			// If we need to support them later, then chunkStream should probably be invoked at
			// valueContent() level rather than property() level.
			case Afy::VT_STREAM:
			{
				char * const lBuf = (char *)pSession.malloc(PINHASH_BLOCKSIZE);
				if (lBuf)
				{
					size_t lRead, lTotalProcessed;
					Afy::IStream * lIs = pValue.stream.is;
					bool lCloned = false;
					if (RC_OK != lIs->reset())
						{ lIs = lIs->clone(); lCloned = true; }
					if (lFirstChunk > 0)
					{
						uint32_t iSkipChunk = 0;
						for (iSkipChunk = 0; iSkipChunk < *pFirstChunk; iSkipChunk++)
						{
							for (lTotalProcessed = 0, lRead = pValue.stream.is->read(lBuf, PINHASH_BLOCKSIZE); 0 != lRead && lTotalProcessed < pHSFact.getChunkSizeInB();)
							{
								lTotalProcessed += lRead;
								if (lTotalProcessed >= pHSFact.getChunkSizeInB())
									{ assert(pHSFact.getChunkSizeInB() == lTotalProcessed); break; }
								lRead = pValue.stream.is->read(lBuf, PINHASH_BLOCKSIZE);
							}
						}
					}
					do
					{
						for (lTotalProcessed = 0, lRead = pValue.stream.is->read(lBuf, PINHASH_BLOCKSIZE); 0 != lRead && lTotalProcessed < pHSFact.getChunkSizeInB();)
						{
							assert(pHSFact.getChunkSizeInB() - lTotalProcessed >= lRead);
							lOs->write(lBuf, lRead);
							lTotalProcessed += lRead;
							if (lTotalProcessed >= pHSFact.getChunkSizeInB())
								{ assert(pHSFact.getChunkSizeInB() == lTotalProcessed); break; }
							lRead = pValue.stream.is->read(lBuf, PINHASH_BLOCKSIZE);
						}
						#if 1 // Note: This code could be removed later on, when traces of m2 intermediate stores disappear...
							if (pHSFact.getVersion() == PINHASH_VERSION_CHUNKS_1MB_16BYTEHASHES && 0 == lRead) // Old dysfunctional idea, preserved for backward compatibility.
								(*lOs) << " ";
						#endif
						lNumChunks++;
						pChunks = (unsigned char *)realloc(pChunks, pHSFact.getChunkHashSizeInB() * lNumChunks);
						pHSFact.hashStream(lOs, &pChunks[pHSFact.getChunkHashSizeInB() * (lNumChunks - 1)], pHSFact.getChunkHashSizeInB()); 
					} while (lRead > 0 && (!pLastChunk || (lFirstChunk + lNumChunks <= *pLastChunk)));
					lCloned ? lIs->destroy() : (void)lIs->reset();
					pSession.free(lBuf);
				}
				break;
			}
			case Afy::VT_STRING: case Afy::VT_BSTR:
			{
				size_t iChunk;
				size_t const lLastAvailableChunk = (pValue.length - 1) / pHSFact.getChunkSizeInB();
				for (iChunk = lFirstChunk; iChunk <= lLastAvailableChunk && (!pLastChunk || iChunk <= *pLastChunk); iChunk++)
				{
					size_t lLength = pValue.length - (iChunk * pHSFact.getChunkSizeInB());
					if (!lLength)
						break;
					if (lLength > pHSFact.getChunkSizeInB())
						lLength = (unsigned long)pHSFact.getChunkSizeInB();
					lOs->write(&pValue.str[iChunk * pHSFact.getChunkSizeInB()], lLength);
					#if 1 // Note: This code could be removed later on, when traces of m2 intermediate stores disappear...
						if (pHSFact.getVersion() == PINHASH_VERSION_CHUNKS_1MB_16BYTEHASHES && iChunk == lLastAvailableChunk) // For compatibility with outProperty, which always appends a " " after a property.
							(*lOs) << " ";
					#endif
					lNumChunks++;
					pChunks = (unsigned char *)realloc(pChunks, pHSFact.getChunkHashSizeInB() * lNumChunks);
					pHSFact.hashStream(lOs, &pChunks[pHSFact.getChunkHashSizeInB() * (lNumChunks - 1)], pHSFact.getChunkHashSizeInB());
				}
				break;
			}
			default:
				assert(false && "Unexpected value type for chunkStream!");
				break;
		}
		if (!pStream)
			pHSFact.releaseStream(lOs);
		return lNumChunks;
	}

	inline bool PrimitivesOutComparisons::getChunksHash(IHashStreamFactory & pHSFact, unsigned long pNumChunks, unsigned char const * pChunks, unsigned char pHash[PINHASH_HASHSIZE], std::ostream * pStream)
	{
		if (0 == pNumChunks)
			return false;
		std::ostream * const lOs = pStream ? pStream : pHSFact.createStream();
		lOs->write((char *)pChunks, pNumChunks * pHSFact.getChunkHashSizeInB());
		pHSFact.hashStream(lOs, pHash, PINHASH_HASHSIZE);
		if (!pStream)
			pHSFact.releaseStream(lOs);
		return true;
	}

	template<> inline void Out<ContextOutComparisons>::property(ContextOutComparisons & pCtx, Value const & pValue)
	{
		if (pCtx.isException(pValue.property))
			return;

		// Normalize 1-element collections, since the store doesn't do it for us.
		// Note: We didn't use to do this until version kCVChunkedStreams,
		//       but results were unpredictable anyway so I don't see the point of
		//       attempting bw-compatibility here...
		// Note: According to Mark there's a bit of unpredictability in terms of the
		//       eid of these single-elements... for now I'm not trying to fix this,
		//       because I have actually not encountered obvious cases of this in real
		//       life, but we may need to do something about it.
		if (Services::isCollectionType(pValue) && 1 == Services::evaluateLength(pValue))
		{
			if (pValue.type!=Afy::VT_COLLECTION)
				assert(false && "Not a collection!");
			else if (pValue.isNav())
			{
				Value const * lNext = pValue.nav->navigate(GO_FIRST);
				if (lNext)
				{
					ContextOutComparisons::TPrimitives::beginProperty(pCtx, pValue.property);
					Out<ContextOutComparisons>::value(pCtx, *lNext);
					ContextOutComparisons::TPrimitives::endProperty(pCtx, pValue.property);
					return;
				}
			} else
			{
				ContextOutComparisons::TPrimitives::beginProperty(pCtx, pValue.property);
				Out<ContextOutComparisons>::value(pCtx, pValue.varray[0]);
				ContextOutComparisons::TPrimitives::endProperty(pCtx, pValue.property);
				return;
			}
		}

		// Chunk large streams, for performance reasons (see bug #7722).
		if (pCtx.getCVersion() >= ContextOutComparisons::kCVChunkedStreams &&
			pCtx.getHashStreamFactory() &&
			Services::evaluateLength(pValue, pCtx.mUnicodeCharSize) > PINHASH_THRESHOLD)
		{
			unsigned char lHash[PINHASH_HASHSIZE];
			unsigned char * lChunks = NULL;
			std::ostream * lOs = pCtx.getHashStream();
			unsigned long const lNumChunks = PrimitivesOutComparisons::chunkStream(*pCtx.mSession, *pCtx.getHashStreamFactory(), pValue, lChunks, lOs);
			if (lNumChunks)
			{
				PrimitivesOutComparisons::getChunksHash(*pCtx.getHashStreamFactory(), lNumChunks, lChunks, lHash, lOs);
				free(lChunks);
				pCtx.mOs.write((char *)lHash, PINHASH_HASHSIZE); // Note: No point putting another space after this...
				return;
			}
		}

		// Handle remaining cases with the standard serialization.
		outProperty<ContextOutComparisons>(pCtx, pValue);
	}

	/*************************************************************************
	 * Dbg Output:
	 * Can be seen as a "beautification" of the "Raw" serialization
	 * (with less constraints and no need to deserialize).
	 *************************************************************************/

	class PrimitivesOutDbg;
	class ContextOutDbg : public ContextOut
	{
		public:
			struct CollStackItem { long mLevel; long mPropId; bool mStruct; CollStackItem(long pLevel, long pPropId, bool pStruct) : mLevel(pLevel), mPropId(pPropId), mStruct(pStruct) {} };
			typedef std::vector<IPIN const *> TPinStack;
			typedef std::vector<CollStackItem> TCollStack;
			typedef void (*TCustomOutputFunc)(ContextOutDbg &, Value const &, void *);
			enum eFlags { kFRecurseRefs = (1 << 0), kFShowPropIds = (1 << 1), kFDefault = (kFRecurseRefs | kFShowPropIds), };
		public:
			TPinStack mPinStack;
			TCollStack mCollStack;
			size_t const mShowNumChars;
			TCustomOutputFunc mCustomOutputFunc;
			void * mCustomOutputArg;
			long const mFlags;
			long mLevel;
			bool mNestedPin;
		public:
			typedef PrimitivesOutDbg TPrimitives;
			ContextOutDbg(std::ostream & pOs, ISession * pSession, size_t pShowNumChars = 20, long pFlags = kFDefault) : ContextOut(pOs, pSession, false, NULL != pSession), mShowNumChars(pShowNumChars), mCustomOutputFunc(NULL), mCustomOutputArg(NULL), mFlags(pFlags) { clear(); }
			void clear() { mPinStack.clear(); mCollStack.clear(); mLevel = 0; mNestedPin = false; }
			bool recurseRefs() const { return 0 != (mFlags & kFRecurseRefs); }
			bool showPropIds() const { return 0 != (mFlags & kFShowPropIds); }
			void setCustomOutput(TCustomOutputFunc pFunc, void * pArg) { mCustomOutputFunc = pFunc; mCustomOutputArg = pArg; }
	};

	typedef Out<ContextOutDbg> OutDbg;

	class PrimitivesOutDbg
	{
		public:
			template <class T> inline static T mymin(T const & t1, T const & t2) { return (t1 <= t2) ? t1 : t2; } // @#!$
			inline static void outString(ContextOutDbg & pCtx, wchar_t const * pString, uint32_t pLenInB);
			inline static void outString(ContextOutDbg & pCtx, unsigned char const * pString, uint32_t pLenInB);
			template <class T> inline static void outString(ContextOutDbg & pCtx, T const * pString, uint32_t pLenInB) { if (pString) { std::basic_string<T> lString(pString, mymin(pCtx.mShowNumChars, size_t(pLenInB / sizeof(T)))); outString(pCtx, (unsigned char *)lString.c_str(), (uint32_t)lString.length() * sizeof(T)); } else pCtx.os() << "null"; }
			template <class T> inline static void outStream(ContextOutDbg & pCtx, Afy::IStream & pStream, T * pT, uint64_t pLenInB = uint64_t(~0));
			inline static void outQuery(ContextOutDbg & pCtx, Value const & pValue) { PrimitivesOutRaw::outQuery(pCtx, pValue); }
			inline static void outExpr(ContextOutDbg & pCtx, Value const & pValue) { PrimitivesOutRaw::outExpr(pCtx, pValue); }
			inline static void outIID(ContextOutDbg & pCtx, IdentityID const & pIID) { if (pCtx.mSession) PrimitivesOutRaw::outIID(pCtx, pIID); else { pCtx.os() << pIID << " "; } }
			inline static void outRef(ContextOutDbg & pCtx, PID const & pPID);
			inline static void outRef(ContextOutDbg & pCtx, PID const & pPID, PropertyID const & pPropID) { if (pCtx.mSession) PrimitivesOutRaw::outRef(pCtx, pPID, pPropID); else { outRef(pCtx, pPID); pCtx.os() << pPropID << " "; } }
			inline static void outRef(ContextOutDbg & pCtx, PID const & pPID, PropertyID const & pPropID, ElementID const & pEid) { if (pCtx.mSession) PrimitivesOutRaw::outRef(pCtx, pPID, pPropID, pEid); else { outRef(pCtx, pPID, pPropID); pCtx.os() << pEid << " "; } }
			inline static void outCLSID(ContextOutDbg & pCtx, DataEventID const & pCLSID) { PrimitivesOutRaw::outCLSID(pCtx, pCLSID); }
			inline static void outPropertyID(ContextOutDbg & pCtx, PropertyID const & pPropID);
			inline static void beginValue(ContextOutDbg & pCtx, Value const & pValue, uint64_t * pLen);
			inline static void endValue(ContextOutDbg & pCtx, Value const & pValue);
			inline static void beginProperty(ContextOutDbg &, PropertyID const &) {}
			inline static void endProperty(ContextOutDbg &, PropertyID const &) {}
			inline static bool beginPIN(ContextOutDbg & pCtx, IPIN const & pPIN);
			inline static void endPIN(ContextOutDbg & pCtx, IPIN const & pPIN);
		public:
			static ValueType normalizeVT(Value const & pValue) { return PrimitivesOutRaw::normalizeVT(pValue); }
			static std::ostream & outTab(ContextOutDbg & pCtx) { for (long i = 0; i < pCtx.mLevel; i++) pCtx.os() << "  "; return pCtx.os(); }
	};

	inline void PrimitivesOutDbg::outString(ContextOutDbg & pCtx, wchar_t const * pString, uint32_t pLenInB) 
	{
		// Convert wchar_t to char for debug output
		char * const lBuf = (char *)alloca(pLenInB);
		memcpy(lBuf, pString, pLenInB);
		uint32_t const lLenInC = pLenInB / sizeof(wchar_t);
		Services::cvtUnicode((wchar_t *)lBuf, lLenInC, sizeof(char), sizeof(wchar_t));
		outString(pCtx, lBuf, lLenInC); 
	}

	inline void PrimitivesOutDbg::outString(ContextOutDbg & pCtx, unsigned char const * pString, uint32_t pLenInB)
	{ 
		if (pString && pLenInB) 
		{ 
			std::basic_string<char> lString((char *)pString, mymin(uint32_t(pCtx.mShowNumChars), pLenInB)); 

			// Binary data (VT_BSTR) is being sent in here, so remove bad ascii characters.
			// Note: this should also take care of removing CR-LF, thus preserving all data on one line.
			char safechar = '.' ;
			for ( size_t i = 0 ; i < lString.length() ; i++ )
			{
				char c = lString[i] ;
				if (( c < ' ' ) || ( c > 126 ))
				{
					lString[i] = safechar ;
				}
			}

			PrimitivesOutRaw::outString(pCtx, lString.c_str(), (uint32_t)lString.length()); 
		}
		else pCtx.os() << "null"; 
	}

	template <class T>
	inline void PrimitivesOutDbg::outStream(ContextOutDbg & pCtx, Afy::IStream & pStream, T *, uint64_t)
	{
		static size_t const sBufSize = 0x1000;

		Afy::ValueType type = pStream.dataType();
		T lBuf[sBufSize];
		size_t lMaxRemaining = pCtx.mShowNumChars;
		size_t lRead = pStream.read(lBuf, mymin(sBufSize, lMaxRemaining));
		while (lRead)
		{
			lMaxRemaining -= lRead;
			if ( type == Afy::VT_BSTR )
			{
				// Avoid any bad ascii characters
				char safechar = '.' ;
				for ( size_t i = 0 ; i < lRead ; i++ )
				{
					T c = lBuf[i] ;
					if (( c < ' ' ) || ( c > 126 ))
					{
						lBuf[i] = safechar ;
					}
				}
			}

			pCtx.os().write((char *)lBuf, lRead);
			lRead = pStream.read(lBuf, mymin(sBufSize, lMaxRemaining));
		}
		pCtx.os() << " ";
	}

	inline void PrimitivesOutDbg::outRef(ContextOutDbg & pCtx, PID const & pPID)
	{
		if (pCtx.mSession && pCtx.recurseRefs())
		{
			IPIN * const lPIN = pCtx.session().getPIN(pPID);
			if (lPIN)
			{
				ContextOutDbg lCtx(pCtx);
				lCtx.mLevel++;
				pCtx.os() << std::endl;
				pCtx.mNestedPin = true;
				Out<ContextOutDbg>::pin(lCtx, *lPIN);
				lPIN->destroy();
				return;
			}
		}
		PrimitivesOutRaw::outRef(pCtx, pPID);
	}

	inline bool PrimitivesOutDbg::beginPIN(ContextOutDbg & pCtx, IPIN const & pPIN)
	{
		outTab(pCtx) << "PIN: ";
		if (pCtx.mSession)
			PrimitivesOutRaw::outRef(pCtx, pPIN.getPID());
		else
			pCtx.os() << std::hex << pPIN.getPID().pid << " " << std::dec << pPIN.getPID().ident;

		pCtx.os() << std::endl;
		if (pCtx.mPinStack.end() != std::find(pCtx.mPinStack.begin(), pCtx.mPinStack.end(), &pPIN))
			{ outTab(pCtx) << "  *cycle detected!*" << std::endl; return false; }
		ContextOutDbg::TPinStack::iterator i;
		for (i = pCtx.mPinStack.begin(); pCtx.mPinStack.end() != i; i++)
			if ((*i)->getPID() == pPIN.getPID())
				{ outTab(pCtx) << "  *cycle detected!*" << std::endl; return false; }

		pCtx.mPinStack.push_back(&pPIN);
		return true;
	}

	inline void PrimitivesOutDbg::endPIN(ContextOutDbg & pCtx, IPIN const &)
	{
		pCtx.mPinStack.pop_back();
	}

	inline void PrimitivesOutDbg::outPropertyID(ContextOutDbg & pCtx, PropertyID const & pPropID)
	{
		size_t lPropertyURISize = 0;
		if (pCtx.mSession) pCtx.mSession->getURI(pPropID, NULL, lPropertyURISize);
		char const * lPropertyURI = NULL;
		if (0 != lPropertyURISize)
		{
			lPropertyURI = (char const *)alloca(1 + lPropertyURISize);
			((char *)lPropertyURI)[lPropertyURISize++] = 0;
			pCtx.session().getURI(pPropID, (char *)lPropertyURI, lPropertyURISize);
		}
		else 
		{
			switch(pPropID)
			{
				case PROP_SPEC_PINID: lPropertyURI = "PROP_SPEC_PINID"; break;
				case PROP_SPEC_DOCUMENT: lPropertyURI = "PROP_SPEC_DOCUMENT"; break;
				case PROP_SPEC_PARENT: lPropertyURI = "PROP_SPEC_PARENT"; break;
				case PROP_SPEC_VALUE: lPropertyURI = "PROP_SPEC_VALUE"; break;
				case PROP_SPEC_CREATED: lPropertyURI = "PROP_SPEC_CREATED"; break;
				case PROP_SPEC_CREATEDBY: lPropertyURI = "PROP_SPEC_CREATEDBY"; break;
				case PROP_SPEC_UPDATED: lPropertyURI = "PROP_SPEC_UPDATED"; break;
				case PROP_SPEC_UPDATEDBY: lPropertyURI = "PROP_SPEC_UPDATEDBY"; break;
				case PROP_SPEC_ACL: lPropertyURI = "PROP_SPEC_ACL"; break;
				case PROP_SPEC_STAMP: lPropertyURI = "PROP_SPEC_STAMP"; break;
				default: lPropertyURI = "PROP_SPEC_<other>"; break;
			}
		}

		if (lPropertyURI)
			pCtx.os() << lPropertyURI << " ";

		if (lPropertyURI==NULL||pCtx.showPropIds())
			pCtx.os() << "(propid=" << pPropID << ") ";
	}

	inline void PrimitivesOutDbg::beginValue(ContextOutDbg & pCtx, Value const & pValue, uint64_t * pLen)
	{
		pCtx.mLevel++;
		outTab(pCtx);
		if (!pCtx.mCollStack.empty() && pCtx.mCollStack.back().mLevel == pCtx.mLevel - 1 && !pCtx.mCollStack.back().mStruct)
			pCtx.os() << "elm: ";
		else
			outPropertyID(pCtx, pValue.getPropID());

		pCtx.os() << "(op=" << (int)pValue.op << ") ";

		if ( pValue.eid==STORE_COLLECTION_ID )
			pCtx.os() << "(eid=STORE_COLLECTION_ID,len="; // REVIEW: Perhaps list no eid at all?
		else
			pCtx.os() << "(eid=" << std::hex << pValue.eid << ",len=";

		uint64_t const lPersistedLength = Services::evaluateLength(pValue, pCtx.mUnicodeCharSize);
		if (pLen)
			*pLen = lPersistedLength;
		pCtx.os() << std::dec << lPersistedLength << ") ";

		if (Afy::VT_COLLECTION == pValue.type || Afy::VT_STRUCT == pValue.type)
		{
			switch (pValue.type)
			{
				case Afy::VT_COLLECTION: pCtx.os() << (pValue.isNav() ? " VT_COLLECTION " : " VT_ARRAY "); break;
				case Afy::VT_STRUCT: pCtx.os() << " VT_STRUCT "; break;
				default: pCtx.os() << " <vt_unknown> "; break;
			}
			pCtx.mLevel++;
			pCtx.mCollStack.push_back(ContextOutDbg::CollStackItem(pCtx.mLevel, pValue.property, Afy::VT_STRUCT == pValue.type));
			pCtx.os() << std::endl;
		}
	}

	inline void PrimitivesOutDbg::endValue(ContextOutDbg & pCtx, Value const & pValue)
	{
		if (Afy::VT_COLLECTION == pValue.type || Afy::VT_STRUCT == pValue.type)
		{
			pCtx.mCollStack.pop_back();
			pCtx.mLevel--;
		}
		else if (!pCtx.mNestedPin)
			pCtx.os() << std::endl;
		pCtx.mNestedPin = false;
		pCtx.mLevel--;
	}

	template<> inline void Out<ContextOutDbg>::value(ContextOutDbg & pCtx, Value const & pValue)
	{
		if (pCtx.mCustomOutputFunc && !Services::isCollectionType(pValue))
		{
			PrimitivesOutDbg::beginValue(pCtx, pValue, NULL);
			if (STORE_INVALID_URIID == pValue.property && !pCtx.mCollStack.empty() && pCtx.mCollStack.back().mLevel == pCtx.mLevel - 1)
			{
				Value lVTmp;
				memcpy(&lVTmp, &pValue, sizeof(pValue));
				lVTmp.property = pCtx.mCollStack.back().mPropId;
				(*pCtx.mCustomOutputFunc)(pCtx, lVTmp, pCtx.mCustomOutputArg);
			}
			else
				(*pCtx.mCustomOutputFunc)(pCtx, pValue, pCtx.mCustomOutputArg);
			PrimitivesOutDbg::endValue(pCtx, pValue);
		}
		else
			outValue<ContextOutDbg>(pCtx, pValue);
	}

	/*************************************************************************
	 * XML Output
	 * (Used to be taken care of by XQuery; I only take care of output here;
	 * input is implemented in a service and relies on a 3rd-party SAX parser)
	 *************************************************************************/

	class PrimitivesOutXml;
	class ContextOutXml : public ContextOut
	{
// TODO: a pmap of "known" xml props (name, children etc.)
//       -> when retrieve these guys in rendered pin, shortcut normal logic
		public:
			struct CollStackItem { long mLevel; long mPropId; bool mStruct; CollStackItem(long pLevel, long pPropId, bool pStruct) : mLevel(pLevel), mPropId(pPropId), mStruct(pStruct) {} };
			typedef std::vector<IPIN const *> TPinStack;
			typedef std::vector<CollStackItem> TCollStack;
			enum eFlags { kFRecurseRefs = (1 << 0), kFShowPropIds = (1 << 1), kFDefault = (kFRecurseRefs | kFShowPropIds), };
		public:
			TPinStack mPinStack;
			TCollStack mCollStack;
			size_t const mShowNumChars;
			long const mFlags;
			long mLevel;
			bool mNestedPin;
		public:
			typedef PrimitivesOutXml TPrimitives;
			ContextOutXml(std::ostream & pOs, ISession * pSession, size_t pShowNumChars = 20, long pFlags = kFDefault) : ContextOut(pOs, pSession, false, NULL != pSession), mShowNumChars(pShowNumChars), mFlags(pFlags) { clear(); }
			void clear() { mPinStack.clear(); mCollStack.clear(); mLevel = 0; mNestedPin = false; }
			bool recurseRefs() const { return 0 != (mFlags & kFRecurseRefs); }
			bool showPropIds() const { return 0 != (mFlags & kFShowPropIds); }
	};

	typedef Out<ContextOutXml> OutXml;

	class PrimitivesOutXml
	{
		public:
			template <class T> inline static T mymin(T const & t1, T const & t2) { return (t1 <= t2) ? t1 : t2; } // @#!$
			inline static void outString(ContextOutXml & pCtx, wchar_t const * pString, uint32_t pLenInB);
			inline static void outString(ContextOutXml & pCtx, unsigned char const * pString, uint32_t pLenInB);
			template <class T> inline static void outString(ContextOutXml & pCtx, T const * pString, uint32_t pLenInB) { if (pString) { std::basic_string<T> lString(pString, mymin(pCtx.mShowNumChars, size_t(pLenInB / sizeof(T)))); outString(pCtx, (unsigned char *)lString.c_str(), (uint32_t)lString.length() * sizeof(T)); } else pCtx.os() << "null"; }
			template <class T> inline static void outStream(ContextOutXml & pCtx, Afy::IStream & pStream, T * pT, uint64_t pLenInB = uint64_t(~0));
			inline static void outQuery(ContextOutXml & pCtx, Value const & pValue) { PrimitivesOutRaw::outQuery(pCtx, pValue); }
			inline static void outExpr(ContextOutXml & pCtx, Value const & pValue) { PrimitivesOutRaw::outExpr(pCtx, pValue); }
			inline static void outIID(ContextOutXml & pCtx, IdentityID const & pIID) { if (pCtx.mSession) PrimitivesOutRaw::outIID(pCtx, pIID); else { pCtx.os() << pIID << " "; } }
			inline static void outRef(ContextOutXml & pCtx, PID const & pPID);
			inline static void outRef(ContextOutXml & pCtx, PID const & pPID, PropertyID const & pPropID) { if (pCtx.mSession) PrimitivesOutRaw::outRef(pCtx, pPID, pPropID); else { outRef(pCtx, pPID); pCtx.os() << pPropID << " "; } }
			inline static void outRef(ContextOutXml & pCtx, PID const & pPID, PropertyID const & pPropID, ElementID const & pEid) { if (pCtx.mSession) PrimitivesOutRaw::outRef(pCtx, pPID, pPropID, pEid); else { outRef(pCtx, pPID, pPropID); pCtx.os() << pEid << " "; } }
			inline static void outCLSID(ContextOutXml & pCtx, DataEventID const & pCLSID) { PrimitivesOutRaw::outCLSID(pCtx, pCLSID); }
			inline static void outPropertyID(ContextOutXml & pCtx, PropertyID const & pPropID);
			inline static void beginValue(ContextOutXml & pCtx, Value const & pValue, uint64_t * pLen);
			inline static void endValue(ContextOutXml & pCtx, Value const & pValue);
			inline static void beginProperty(ContextOutXml &, PropertyID const &) {}
			inline static void endProperty(ContextOutXml &, PropertyID const &) {}
			inline static bool beginPIN(ContextOutXml & pCtx, IPIN const & pPIN);
			inline static void endPIN(ContextOutXml & pCtx, IPIN const & pPIN);
		public:
			static ValueType normalizeVT(Value const & pValue) { return PrimitivesOutRaw::normalizeVT(pValue); }
			static std::ostream & outTab(ContextOutXml & pCtx) { for (long i = 0; i < pCtx.mLevel; i++) pCtx.os() << "  "; return pCtx.os(); }
	};

	inline void PrimitivesOutXml::outString(ContextOutXml & pCtx, wchar_t const * pString, uint32_t pLenInB) 
	{
		// Convert wchar_t to char for debug output
		char * const lBuf = (char *)alloca(pLenInB);
		memcpy(lBuf, pString, pLenInB);
		uint32_t const lLenInC = pLenInB / sizeof(wchar_t);
		Services::cvtUnicode((wchar_t *)lBuf, lLenInC, sizeof(char), sizeof(wchar_t));
		outString(pCtx, lBuf, lLenInC); 
	}

	inline void PrimitivesOutXml::outString(ContextOutXml & pCtx, unsigned char const * pString, uint32_t pLenInB)
	{ 
		if (pString && pLenInB) 
		{ 
			std::basic_string<char> lString((char *)pString, mymin(uint32_t(pCtx.mShowNumChars), pLenInB)); 

			// Binary data (VT_BSTR) is being sent in here, so remove bad ascii characters.
			// Note: this should also take care of removing CR-LF, thus preserving all data on one line.
			char safechar = '.' ;
			for ( size_t i = 0 ; i < lString.length() ; i++ )
			{
				char c = lString[i] ;
				if (( c < ' ' ) || ( c > 126 ))
				{
					lString[i] = safechar ;
				}
			}

			PrimitivesOutRaw::outString(pCtx, lString.c_str(), (uint32_t)lString.length()); 
		}
		else pCtx.os() << "null"; 
	}

	template <class T>
	inline void PrimitivesOutXml::outStream(ContextOutXml & pCtx, Afy::IStream & pStream, T *, uint64_t)
	{
		static size_t const sBufSize = 0x1000;

		Afy::ValueType type = pStream.dataType();
		T lBuf[sBufSize];
		size_t lMaxRemaining = pCtx.mShowNumChars;
		size_t lRead = pStream.read(lBuf, mymin(sBufSize, lMaxRemaining));
		while (lRead)
		{
			lMaxRemaining -= lRead;
			if ( type == Afy::VT_BSTR )
			{
				// Avoid any bad ascii characters
				char safechar = '.' ;
				for ( size_t i = 0 ; i < lRead ; i++ )
				{
					T c = lBuf[i] ;
					if (( c < ' ' ) || ( c > 126 ))
					{
						lBuf[i] = safechar ;
					}
				}
			}

			pCtx.os().write((char *)lBuf, lRead);
			lRead = pStream.read(lBuf, mymin(sBufSize, lMaxRemaining));
		}
		pCtx.os() << " ";
	}

	inline void PrimitivesOutXml::outRef(ContextOutXml & pCtx, PID const & pPID)
	{
		if (pCtx.mSession && pCtx.recurseRefs())
		{
			IPIN * const lPIN = pCtx.session().getPIN(pPID);
			if (lPIN)
			{
				ContextOutXml lCtx(pCtx);
				lCtx.mLevel++;
				pCtx.os() << std::endl;
				pCtx.mNestedPin = true;
				Out<ContextOutXml>::pin(lCtx, *lPIN);
				lPIN->destroy();
				return;
			}
		}
		PrimitivesOutRaw::outRef(pCtx, pPID);
	}

	inline bool PrimitivesOutXml::beginPIN(ContextOutXml & pCtx, IPIN const & pPIN)
	{
		// TODO: lookup node/name in pin
		
		outTab(pCtx) << "<afy:PIN afy:PID='";
		if (pCtx.mSession)
			PrimitivesOutRaw::outRef(pCtx, pPIN.getPID());
		else
			pCtx.os() << std::hex << pPIN.getPID().pid << " " << std::dec << pPIN.getPID().ident;
		pCtx.os() << "'";

		pCtx.os() << std::endl;
		if (pCtx.mPinStack.end() != std::find(pCtx.mPinStack.begin(), pCtx.mPinStack.end(), &pPIN))
			{ outTab(pCtx) << " afy:cycle=true />" << std::endl; return false; }
		ContextOutXml::TPinStack::iterator i;
		for (i = pCtx.mPinStack.begin(); pCtx.mPinStack.end() != i; i++)
			if ((*i)->getPID() == pPIN.getPID())
				{ outTab(pCtx) << " afy:cycle=true />" << std::endl; return false; }

		pCtx.mPinStack.push_back(&pPIN);
		return true;
	}

	inline void PrimitivesOutXml::endPIN(ContextOutXml & pCtx, IPIN const &)
	{
		outTab(pCtx) << "</afy:PIN>" << std::endl;
		pCtx.mPinStack.pop_back();
	}

	inline void PrimitivesOutXml::outPropertyID(ContextOutXml & pCtx, PropertyID const & pPropID)
	{
		size_t lPropertyURISize = 0;
		if (pCtx.mSession) pCtx.mSession->getURI(pPropID, NULL, lPropertyURISize);
		char const * lPropertyURI = NULL;
		if (0 != lPropertyURISize)
		{
			lPropertyURI = (char const *)alloca(1 + lPropertyURISize);
			((char *)lPropertyURI)[lPropertyURISize++] = 0;
			pCtx.session().getURI(pPropID, (char *)lPropertyURI, lPropertyURISize);
		}
		else 
		{
			switch(pPropID)
			{
				case PROP_SPEC_PINID: lPropertyURI = "afy:pinID"; break;
				case PROP_SPEC_DOCUMENT: lPropertyURI = "afy:document"; break;
				case PROP_SPEC_PARENT: lPropertyURI = "afy:parent"; break;
				case PROP_SPEC_VALUE: lPropertyURI = "afy:value"; break;
				case PROP_SPEC_CREATED: lPropertyURI = "afy:created"; break;
				case PROP_SPEC_CREATEDBY: lPropertyURI = "afy:createdBy"; break;
				case PROP_SPEC_UPDATED: lPropertyURI = "afy:updated"; break;
				case PROP_SPEC_UPDATEDBY: lPropertyURI = "afy:updatedBy"; break;
				case PROP_SPEC_ACL: lPropertyURI = "afy:ACL"; break;
				case PROP_SPEC_STAMP: lPropertyURI = "afy:stamp"; break;
				case PROP_SPEC_OBJID: lPropertyURI = "afy:objectID"; break;
				case PROP_SPEC_PREDICATE: lPropertyURI = "afy:predicate"; break;
				case PROP_SPEC_COUNT: lPropertyURI = "afy:count"; break;
				case PROP_SPEC_SPECIALIZATION: lPropertyURI = "afy:subclasses"; break;
				case PROP_SPEC_ABSTRACTION: lPropertyURI = "afy:superclasses"; break;
				case PROP_SPEC_INDEX_INFO: lPropertyURI = "afy:indexInfo"; break;
				case PROP_SPEC_PROPERTIES: lPropertyURI = "afy:properties"; break;
				case PROP_SPEC_ONENTER: lPropertyURI = "afy:onEnter"; break;
				case PROP_SPEC_ONUPDATE: lPropertyURI = "afy:onUpdate"; break;
				case PROP_SPEC_ONLEAVE: lPropertyURI = "afy:onLeave"; break;
				default: lPropertyURI = "afy:unknown"; break;
			}
		}

		if (lPropertyURI)
			pCtx.os() << lPropertyURI;
	}

	inline void PrimitivesOutXml::beginValue(ContextOutXml & pCtx, Value const & pValue, uint64_t * pLen)
	{
		pCtx.mLevel++;
		outTab(pCtx);
		if (!pCtx.mCollStack.empty() && pCtx.mCollStack.back().mLevel == pCtx.mLevel - 1 && !pCtx.mCollStack.back().mStruct)
			pCtx.os() << "<afy:element>";
		else
			{ pCtx.os() << "<"; outPropertyID(pCtx, pValue.getPropID()); pCtx.os() << ">"; }

		uint64_t const lPersistedLength = Services::evaluateLength(pValue, pCtx.mUnicodeCharSize);
		if (pLen)
			*pLen = lPersistedLength;

		if (Afy::VT_COLLECTION == pValue.type || Afy::VT_STRUCT == pValue.type)
		{
			pCtx.mLevel++;
			pCtx.mCollStack.push_back(ContextOutXml::CollStackItem(pCtx.mLevel, pValue.property, Afy::VT_STRUCT == pValue.type));
			pCtx.os() << std::endl;
		}
	}

	inline void PrimitivesOutXml::endValue(ContextOutXml & pCtx, Value const & pValue)
	{
		bool const lContentWasDeeper = (!pCtx.mCollStack.empty() && pCtx.mCollStack.back().mLevel > pCtx.mLevel);
		bool const lIsElm = (!pCtx.mCollStack.empty() && pCtx.mCollStack.back().mLevel == pCtx.mLevel - 1 && !pCtx.mCollStack.back().mStruct);
		if (Afy::VT_COLLECTION == pValue.type || Afy::VT_STRUCT == pValue.type)
		{
			pCtx.mCollStack.pop_back();
			pCtx.mLevel--;
		}

		if (lContentWasDeeper)
			outTab(pCtx);
		if (lIsElm)
			pCtx.os() << "</afy:element>" << std::endl;
		else
			{ pCtx.os() << "</"; outPropertyID(pCtx, pValue.getPropID()); pCtx.os() << ">" << std::endl; }

		pCtx.mNestedPin = false;
		pCtx.mLevel--;
	}
#endif
};

#endif
