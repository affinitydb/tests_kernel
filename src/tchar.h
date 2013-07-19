/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

/*
 * based on log4cxx
 * 
 * Includes the SmartA2W and SmartW2A classes for easy UTF8/UTF16 conversion on all platforms.
 *
 * NOTE: Although TCHAR is defined here it is normally better to use either char or wchar_t.
 * And when calling windows APIS it is better to use the "W" version directly rather than
 * assuming that TCHAR and compiler settings will do the right thing.
 */
 
#ifndef _MVCORE_UTILS_TCHAR_H
#define _MVCORE_UTILS_TCHAR_H

#include "types.h"
#include <string>
#include <iostream>
#include <sstream>
#include <cwchar>

#if defined( WIN32 )
#define snprintf _snprintf
#endif

#ifdef MUST_UNDEF_T
#undef _T
#endif

#ifdef WIN32
#ifndef USES_CONVERSION
	#include <malloc.h>
	#define USES_CONVERSION void * _dst = _alloca(1024);
#endif
#else
	#include <alloca.h>
	#include <strings.h>
	#define USES_CONVERSION void * _dst = alloca(1024);
#endif

#ifndef W2A
#define W2A(src) Convert::unicodeToAnsi((char *)_dst, src)
#endif

#ifndef A2W
#define A2W(src) Convert::ansiToUnicode((wchar_t *)_dst, src)
#endif

#ifdef UNICODE
	#include <wctype.h>

#ifndef _T
	#define _T(x) L ## x
#endif

#ifndef _TCHAR_DEFINED
	typedef char TCHAR;
#endif
	#define totupper towupper
	#define totlower towlower
	#define tcout std::wcout
	#define tcerr std::wcerr
#ifdef WIN32
	#define tstrncasecmp _wcsnicmp
#else
	#define tstrncasecmp wcsncasecmp
#endif // WIN32

#ifndef T2A
	#define T2A(src) W2A(src)
#endif

#ifndef T2W
	#define T2W(src) src
#endif

#ifndef A2T
	#define A2T(src) A2W(src)
#endif

#ifndef W2T
	#define W2T(src) src
#endif

	#define ttol(s) wcstol(s, 0, 10)
	#define itot _itow
	#define tcscmp wcscmp
#else // Not UNICODE
	#include <ctype.h>

#ifndef _T
	#define _T(x) x
#endif

	typedef char TCHAR;
	#define totupper toupper
	#define totlower tolower
	#define tcout std::cout
	#define tcerr std::cerr
#ifdef WIN32
	#define tstrncasecmp _strnicmp
#else
	#define tstrncasecmp strncasecmp
#endif // WIN32

#ifndef T2A
	#define T2A(src) src
#endif

#ifndef T2W
	#define T2W(src) A2W(src)
#endif

#ifndef A2T
	#define A2T(src) src
#endif

#ifndef W2T
	#define W2T(src) W2A(src)
#endif

	#define ttol atol
	#define itot itoa
	#define tcscmp strcmp
#endif // UNICODE

#define _MinInc  size_t(512)
#define _MaxInc size_t(100 * 1024)

#ifndef WIN32
	#define stricmp  strcasecmp
#endif

template<typename T>
void ReplaceChar(T* pcSource,T chSearch, T chReplace)
{
	while( *pcSource != '\0') 
	{
		if( *pcSource == chSearch)
		{
			*pcSource = chReplace;
		}
		pcSource++;
	}
}

#ifndef WIN32
char* strlwr(char* pcSource);
#endif

#ifdef USE_T
#if defined(UNICODE) && defined(WIN32) 
	#define SmartA2T SmartA2W
	#define SmartT2A SmartW2A	
#else
	#define SmartA2T SmartC2C 
	#define SmartT2A SmartC2C
#endif
#endif

//This is to enable use of this smart object in linux, this 
//will allow creation of temporary SmartX2Y objects
class SmartC2C
{
private:
	char* m_pData;

public:
	SmartC2C(const char* pData)
	{
		m_pData = const_cast<char*>(pData);
	}

	operator char*()
	{
		return m_pData;
	}

	operator const char*()
	{
		return m_pData;
	}

};

	
#ifdef WIN32

//Note:: This objects needs to be created on stack
//If its created on heap then delete needs to be called
class SmartA2W
{
private:
	wchar_t* m_pData;

public:
	SmartA2W(const char* pData)
	{
		size_t size = strlen(pData)+ 1;
		m_pData = new wchar_t[size];
		MultiByteToWideChar(CP_UTF8,0,pData,-1,m_pData,(int)size);
	}
	
	~SmartA2W()
	{
		delete[] m_pData;
	}

	operator wchar_t*()
	{
		return m_pData;
	}

	operator const wchar_t*()
	{
		return (const wchar_t*)m_pData;
	}

	// For consistency with std::string
	const wchar_t * c_str() { return m_pData; }
};

class SmartW2A
{
private:
	char* m_pData;

public:
	SmartW2A(const wchar_t* pData)
	{
		size_t size=0;
		size = WideCharToMultiByte(CP_UTF8,0,pData,-1,m_pData,0,NULL,NULL);
		if( size > 0)
		{
			m_pData = new char[size+1];
			WideCharToMultiByte(CP_UTF8,0,pData,-1,m_pData,(int)size,NULL,NULL);
		}
		else
		{
			m_pData="";
		}
	}
	
	~SmartW2A()
	{
		delete[] m_pData;
	}

	operator char*()
	{
		return m_pData;
	}

	operator const char*()
	{
		return (const char*)m_pData;
	}

	// For consistency with std::string
	const char * c_str() { return m_pData; }
};

#else

class SmartA2W
{
private:
	wchar_t* m_pData;

	void cleanup()
	{
		if (m_pData != NULL) {
			delete[] m_pData;
			m_pData = NULL;
		}
	}


public:

SmartA2W(const char* pData)
	: m_pData(NULL)
	{
	  
		// determine size required
		size_t size = mbstowcs(NULL, pData, 0);
		if ( size == (size_t) -1) {
			// failed;
			m_pData = NULL;
			return;
		}
		// allocate
		m_pData = new wchar_t[size + 1];

		// convert
		int result = mbstowcs(m_pData, pData, size);
		m_pData[size] = '\0';

		if (result == -1) {
			// failed
			cleanup();
			return;
		}
	}


	~SmartA2W()
	{
		cleanup();
	}

	operator wchar_t*()
	{
		return m_pData;
	}

	operator const wchar_t*()
	{
		return (const wchar_t*)m_pData;
	}

	const wchar_t * c_str() { return m_pData; }
};

class SmartW2A
{
private:
	char* m_pData;

	void cleanup()
	{
		if (m_pData != NULL) {
			delete[] m_pData;
			m_pData = NULL;
		}
	}

public:

SmartW2A(const wchar_t* pData)
	: m_pData(NULL)
	{
		// Determine size required
		size_t size = wcstombs(NULL, pData, 0);
		if (size == ((size_t) -1)) {
			// failed
			cleanup();
			return;
		}
	
		// Allocate
		m_pData = new char[size + 1];

		// convert
		int result = wcstombs(m_pData, pData, size);
		m_pData[size] = '\0';

		if (result == -1) {
			cleanup();
			return;
		}
	}

	~SmartW2A()
	{
		cleanup();
	}

	operator char*()
	{
		return m_pData;
	}

	operator const char*()
	{
		return (const char*)m_pData;
	}

	const char * c_str() { return m_pData; }
};


#endif


#ifdef POSIX
#ifdef USE_T

typedef TCHAR *  LPSTR;
typedef TCHAR *  LPTSTR;
typedef const TCHAR * LPCTSTR;
typedef const TCHAR * LPCSTR;

#define _tcscmp stricmp
#define _tcsicmp stricmp
#define _tcsncpy strncpy
#define _tcsdup strdup
#define _tcsrchr strrchr
#define _tcslen strlen
#define _tcscpy strcpy
#define _tprintf printf
#define _putts puts
#define _totlower tolower
#define _sntprintf snprintf
#define _tcslwr strlwr
#define _tfopen fopen
#define _istalpha isalpha
#define _istalpha isalpha
#define _istdigit isdigit
#define _totupper toupper
#define _tcscat strcat
#define _tcsncat strncat
#define _tcsstr strstr
#define _trename(x,y) rename(x,y)
#define _wtoi(x) atoi(W2CA(x))
#define _wtoi_NOTNULL(x) atoi(W2CA_NOTNULL(x))
typedef unsigned short USHORT;
typedef const wchar_t * LPCWSTR; 
typedef wchar_t * LPWSTR;
typedef wchar_t WCHAR;

#ifdef USES_CONVERSION
#undef USES_CONVERSION
#define USES_CONVERSION int _convert;LPCWSTR _lpw;LPCSTR _lpa;
#endif

#undef A2W
#define A2W(lpa)(\
  ((_lpa = lpa) == NULL) ? NULL : (\
      _convert = (strlen(_lpa) + 1), _lpw = 0,\
	  A2WHELPER((LPWSTR) alloca(_convert * sizeof(WCHAR)), _lpa, _convert)))

#undef W2A
#define W2A(lpw) (\
	((_lpw = lpw) == NULL) ? NULL : (\
		_convert = (wcslen(_lpw)+1), _lpa = 0,\
		W2AHELPER((LPSTR) alloca(_convert), _lpw, _convert)))


// plm --- #16099 We wished to add a version of this conversion macro that
// would not return NULL in the event that the source string was NULL.
// (In the places we copy strings, a NULL return from this macro would
// cause a GPF anyway). This change was originally motived by helpful gcc warnings.
// To minimize risk and changes, we've created a new chain of macros,
// all named something_NOTNULL. The difference is the "" in the true branch
// of the conditional operator.

#undef W2A_NOTNULL
#define W2A_NOTNULL(lpw) (\
	((_lpw = lpw) == NULL) ? "" : (\
		_convert = (wcslen(_lpw)+1), _lpa = 0,\
		W2AHELPER((LPSTR) alloca(_convert), _lpw, _convert)))

#define A2CW(lpa) ((LPCWSTR)A2W(lpa))

#define W2CA(lpw) ((LPCSTR)W2A(lpw))

#define W2CA_NOTNULL(lpw) ((LPCSTR)W2A_NOTNULL(lpw))

LPWSTR mbstowcs2( LPWSTR lpw, LPCSTR lpa, int length );
LPSTR wcstombs2( LPSTR lpa, LPCWSTR lpw, int length );

#define A2WHELPER(x,lpa,len) mbstowcs2(x,lpa,len)
#define W2AHELPER(x,lpw,len) wcstombs2(x,lpw,len)

#define T2CA(x) x
#define T2CW(x) A2CW(x)
#define A2CT(x) x
#define W2CT(x) W2CA(x)
#define W2CT_NOTNULL(x) W2CA_NOTNULL(x)

#endif
#endif

// windows
#ifndef W2CT_NOTNULL
#define W2CT_NOTNULL(x) W2CT(x)
#endif

#ifndef W2CA_NOTNULL
#define W2CA_NOTNULL(x) W2CA(x)
#endif

#ifndef _wtoi_NOTNULL
#define _wtoi_NOTNULL(x) _wtoi(x)
#endif

#endif //_MVCORE_UTILS_TCHAR_H

