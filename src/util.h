/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#ifndef __UTIL_H
#define __UTIL_H

#include "tests.h"
#include <map>

namespace utf {
/*
 * utf namespace contains implementation for UTF8 and UNICODE support, 
 * coppied from the PiStroKernel. 
 * From conversation with Mark and Andrew, the code for tests should not contain
 * and include headers from the kernel itself. (Even if it does right now, that 
 * will be fixed soon.)
 * Consequently, we have to maintain the copy of kernel implementation for UTF8
 * and UNICODE support. That what 'utf' namespace is about. 
*/
class UTF8
{
	const static byte	slen[256];
	const static byte	sec[64][2];
	const static byte	mrk[7];
	const static ushort	upperrng[];
	const static ulong	nupperrng;
	const static ushort	uppersgl[];
	const static ulong	nuppersgl;
	const static ushort	lowerrng[];
	const static ulong	nlowerrng;
	const static ushort	lowersgl[];
	const static ulong	nlowersgl;
	const static ushort	otherrng[];
	const static ulong	notherrng;
	const static ushort	othersgl[];
	const static ulong	nothersgl;
	const static ushort	spacerng[];
	const static ulong	nspacerng;
	static bool	test(wchar_t wch,const ushort *p,ulong n,const ushort *sgl,ulong nsgl) {
		const ushort *q; ulong k;
		while (n>1) {k=n>>1; q=p+k*3; if (wch>=*q) {p=q; n-=k;} else n=k;}
		if (n>0 && wch>=p[0] && wch<=p[1]) return true;
		p=sgl; n=nsgl;
		while (n>1) {k=n>>1; q=p+k*2; if (wch>=*q) {p=q; n-=k;} else n=k;}
		return n>0 && wch==p[0];
	}
public:
	static int	len(byte ch) {return slen[ch];}
	static int	ulen(ulong ch) {return ch>0x10FFFF || (ch&0xFFFE)==0xFFFE || ch>=0xD800 && ch<=0xDFFF ? 0 : ch<0x80 ? 1 : ch<0x800 ? 2 : ch<0x10000 ? 3 : 4;}
	static ulong decode(ulong ch,const byte *&s,size_t xl) {
		ulong len=slen[ch]; if (len==1) return ch;
		if (len==0 || len>xl+1) return ~0u;
		byte c=*s++; ulong i=ch&0x3F;
		if (c<sec[i][0] || c>sec[i][1]) return ~0u;
		for (ch&=0x3F>>--len;;) {
			ch = ch<<6|(c&0x3F);
			if (--len==0) return (ch&0xFFFE)!=0xFFFE?ch:~0u;
			if (((c=*s++)&0xC0)!=0x80) return ~0u;
		}
	}
	static int encode(byte *s,ulong ch) {
		if (ch<0x80) {*s=(byte)ch; return 1;} int len=ulen(ch);
		for (int i=len; --i>=1; ch>>=6) s[i]=byte(ch&0x3F|0x80);
		s[0]=byte(ch|mrk[len]); return len;
	}
	static bool iswdigit(wchar_t wch) {
		return wch>='0' && wch<='9';			// ???
	}
	static bool iswlower(wchar_t wch) {
		return test(wch,lowerrng,nlowerrng,lowersgl,nlowersgl);
	}
	static bool iswupper(wchar_t wch,ulong& res) {
		const ushort *p=upperrng,*q; ulong n=nupperrng,k;
		while (n>1) {k=n>>1; q=p+k*3; if (wch>=*q) {p=q; n-=k;} else n=k;}
		if (n>0 && wch>=p[0] && wch<=p[1]) {res=wchar_t(wch+p[2]-500); return true;}
		p=uppersgl; n=nuppersgl;
		while (n>1) {k=n>>1; q=p+k*2; if (wch>=*q) {p=q; n-=k;} else n=k;}
		if (n>0 && wch==p[0]) {res=wchar_t(wch+p[1]-500); return true;}
		return false;
	}
	static bool iswlalpha(wchar_t wch) {
		if (test(wch,lowerrng,nlowerrng,lowersgl,nlowersgl)) return true;
		const ushort *p=otherrng,*q; ulong n=notherrng,k;
		while (n>1) {k=n>>1; q=p+k*2; if (wch>=*q) {p=q; n-=k;} else n=k;}
		if (n>0 && wch>=p[0] && wch<=p[1]) return true;
		p=othersgl; n=nothersgl;
		while (n>1) {k=n>>1; q=p+k; if (wch>=*q) {p=q; n-=k;} else n=k;}
		return n>0 && wch==p[0];
	}
	static bool iswalnum(wchar_t wch) {
		return iswdigit(wch) || iswlalpha(wch) || test(wch,upperrng,nupperrng,uppersgl,nuppersgl);
	}
	static bool iswspace(wchar_t wch) {
		const ushort *p=spacerng,*q; ulong n=nspacerng,k;
		while (n>1) {k=n>>1; q=p+k*2; if (wch>=*q) {p=q; n-=k;} else n=k;}
		return n>0 && wch>=p[0] && wch<=p[1];
	}
	static wchar_t towlower(wchar_t wch) {
		const ushort *p=upperrng,*q; ulong n=nupperrng,k;
		while (n>1) {k=n>>1; q=p+k*3; if (wch>=*q) {p=q; n-=k;} else n=k;}
		if (n>0 && wch>=p[0] && wch<=p[1]) return wchar_t(wch+p[2]-500);
		p=uppersgl; n=nuppersgl;
		while (n>1) {k=n>>1; q=p+k*2; if (wch>=*q) {p=q; n-=k;} else n=k;}
		return n>0 && wch==p[0] ? wchar_t(wch+p[1]-500) : wch;
	}
};
}

class MVTUtil
{
public:
	// Helper methods to help write tests
	// These are all static and stateless
	//
	// Many of these methods began life on MVTApp object
	//
	// If any "type" of methods become too numerous then the
	// can be split into separate classes.  E.g. MVTRand
	static int executeProcess(char const * pExe, char const * pArgs, clock_t * outTimeTaken = NULL, TIMESTAMP * outTS = NULL, bool bLowPriority = false, bool bVerbose=false, bool bIgnoreSignal = false) ;

	static void mapURIs(ISession *pSession, const char * pPropName, int pNumProps, PropertyID *pPropIDs, const char *base=NULL);
	static void mapURIs(ISession *pSession, const char * pPropName, int pNumProps, URIMap *pPropMaps, const char *base=NULL);
	static void mapStaticProperty(ISession *pSession, const char * pPropName, URIMap &pPropMap, const char *base=NULL);
	static void mapStaticProperty(ISession *pSession, const char * pPropName, PropertyID &pPropID, const char *base=NULL);
	static int countPinsFullScan(ISession * pSession) ;

	// Query Helpers
	static int countPins(ICursor *result,ISession *session) ;
	static size_t getCollectionLength(Value const & pV);
	static bool findDuplicatePins( IStmt * lQ, std::ostream & log) ;
	static bool checkQueryPIDs( 
					IStmt * inQ, 
					int cntExpected, const PID * inExpected,
					std::ostream & errorDetails = cout, 
					bool bVerbose = false,	
					unsigned int inCntParams = 0,
					const Value * inParams = NULL) ;

	static PropertyID getProp(ISession* inS, const char* inName) ;
	static PropertyID getPropRand(ISession *pSession, const char *pPropName) ;
	static DataEventID getClass(ISession* inS, const char* inClass,uint32_t classnotify=0/*optional*/) ;

	/*! \brief Generate a new class.  
	
	Resulting class has a random number in it to ensure 
	it does not already exist in the store.  Recommended way to avoid tests failing
    when run more than once, especially more than once with the same seed.

	\param inS Session to use
	\param inPrefix Prefix to use at the beginning of the class name (it should identify the test)
	\param inQ Query that defines the class
	\param outname Optional string to receive the generated class name
	\param class-related notification flags, see ISession::setNotification()
	\return INVALID_CLASSID if unable to create a unique class
	*/
	static DataEventID createUniqueClass(ISession* inS, const char* inPrefix, IStmt* inQ, std::string * outname=NULL,uint32_t classnotify=0);

	static char * myToUTF8(wchar_t const * pStr, size_t pLen, uint32_t & pBogus);
	static char *toUTF8(const wchar_t *ustr,uint32_t ilen,uint32_t& olen);
	static wchar_t *toUNICODE(const char *str,uint32_t ilen,uint32_t& olen);
	
	static char * toLowerUTF8(const char *str,size_t ilen,uint32_t& olen,char *extBuf);
    static char * toUTF8(const wchar_t *ustr,size_t ilen,uint32_t& olen, char *str, bool fToLower);
    static wchar_t * toUNICODE(const char *str,size_t ilen,uint32_t& olen,wchar_t *ustr);
 
	
	static void getCurrentTime(Tstring & pTime);

	static bool equal(IPIN const & p1, IPIN const & p2, ISession & pSession, bool pIgnoreEids = false);
	static bool equal(Value const & pVal1, Value const & pVal2, ISession & pSession, bool pIgnoreEids = false);

	static void output(Value const & pV, std::ostream & pOs, ISession * pSession = NULL);
	static void output(IPIN const & pPIN, std::ostream & pOs, ISession * pSession = NULL);
	static void output(const PID & pid, std::ostream & pOs, ISession * pSession);
	static void output(const IStoreNotification::NotificationEvent & event, std::ostream & pOs, ISession * pSession = NULL);
	static void outputTab(std::ostream & pOs, int pLevel) { for (int i = 0; i < pLevel; i++) pOs << "  "; }

	// register/unregister pins for a test scope only
	static void registerTestPINs(std::vector<PID> &pTestPINs, PID * pPIDs, const int pNumPIDs = 1);
	static void registerTestPINs(std::vector<IPIN *> &pTestPINs, IPIN **pPINs, const int pNumPINs = 1);
	static void unregisterTestPINs(std::vector<IPIN *> &pTestPINs, ISession *pSession);
	static void unregisterTestPINs(std::vector<PID> &pTestPIDs, ISession *pSession);

	static void ensureDir( const char * inDir ) ;
	static void backupStoreFiles(const char * inStoreDirNoTrailingSlash = NULL, const char * inDestDirNoTrailingSlash = NULL ); 

	//Mostly obsolete version, remove and replace with MVTApp::deleteStore, or MVTUtil::deleteStore
	static bool deleteStoreFiles(const char * inDirNoTrailingSlash = NULL ); 

	// Low level version, most callers use MVTApp::deleteStore
	static bool deleteStore(const char * inIOInit, const char * inStoreDir, const char * inLogDir = NULL, bool bArchiveLogs = false) ;

	static bool removeReplicationFiles(const char * inPathWithSlash);

	// Generate i/o config string with any multi-store placeholders substituted
	static string completeMultiStoreIOInitString(const string & strConfig, int storeIndex);
	static RC getHostname(char* hostname);
};
#endif
