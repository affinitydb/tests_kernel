/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef __MVCORE_UTIL__
#define __MVCORE_UTIL__

#include <cstdlib> 
#include <string> 
#include <sstream>
#include <cctype>

#ifdef WIN32
#include <Windows.h>
#endif

#include "types.h"

namespace mvcore
{
	class utilities
	{
	public:

		static bool putenvvar (const char *pcKey, const char *pcValue)
		{
#ifdef WIN32

			return (SetEnvironmentVariableA (pcKey, pcValue)) ? true : false;
#else
			return (setenv (pcKey, pcValue, 1) == 0) ? true : false;

#endif
		}

		static std::string getenvvar(const char *pKey)
		{
			std::string retVal = "";
#ifdef WIN32
			DWORD ldword = GetEnvironmentVariableA(pKey,NULL,0);
			if(ldword)
			{
				char *pval = new char[ldword];
				DWORD lldword = GetEnvironmentVariableA(pKey,pval,ldword);
				pval[lldword < ldword ? lldword : ldword - 1] = '\0';
				retVal = pval;
				delete[] pval;
			}
#else
			char *pval = getenv(pKey);
			retVal = pval ? pval : "";
#endif
			return retVal;
		};

        // method to convert between string and numeric values
        template <typename T>
        static std::string itoa(T i)
        {
            std::ostringstream os;
            os << i;
            return os.str();
        }

        template <typename T>
        static T atoi(const std::string& a)
        {
            T i;
            std::istringstream os(a);
            os >> i;
            return i;
        }

		// For assistance creating meaningful error logs
		// Handles most common errors, less common are printed as original error code
		
		// Prints errno values on linux and winerror.h values
		// on windows
		static std::string err2str(int error);

		// Print errno as a string (normally linux but possible on
		// windows also)
		static std::string errno2str(int error);

#ifdef WIN32
		// Print winerror.h error
		// e.g. GetLastError()
		// When bUserFriendly is false it returns the technical error code as a string,
		// e.g. ERROR_FILE_NOT_FOUND
		// Otherwise it formats the string into 
		static std::string winerr2str(int error, bool bUserFriendly = false);
#endif

        /** CaseLessString class to provide case less string comparison
        **/
        struct CaseLessChar
        {
            bool operator()(char c1, char c2) const
            { return std::tolower(c1) < std::tolower(c2); }
        };

        struct CaseEqualChar
        { 
            bool operator()(char c1, char c2) const 
            { return std::tolower(c1) == std::tolower(c2); } 
        };

        template <typename Op>
        struct CaseOpString
        {
            bool operator()(const std::string & s1, const std::string & s2) const
            {
                return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), Op()); 
            }
        };

        typedef CaseOpString<CaseLessChar> CaseLessString;
        typedef CaseOpString<CaseEqualChar> CaseEqualString;

#if 0
        static bool isLan(const char* pcIP);
        static bool isLan(uint32_t uiIP);
        static bool isLocal(const char* pcIP);
        static bool isLocal(uint32_t uiIP);
        static bool isInternal(const char* pcIP);
        static bool isInternal(uint32_t uiIP);
#endif

        // TODO: remove this API from here, should be used only by appfwk
        static bool isPooledService();
        
        template< class Container>
        struct StaticInitializer
        {
            typedef Container container_type;

            template< typename Initializer>
            StaticInitializer( Initializer initializer = Initializer() )
            {
                initializer( mContainer);
            }

            container_type mContainer;
	    };

        //////////////////////////////////////////////////
        // OnScopeExit
        // Usage:
        // mvcore::utilities::OnScopeExit scope(mvcore::utilities::makeOnScopeExit( &f) );

        class OnScopeExitBase {};

        template <typename F>
        class OnScopeExitImpl : public OnScopeExitBase
        {
        public:
            OnScopeExitImpl( F f) : _f(f)
            {}

            ~OnScopeExitImpl()
            {
                _f();
            }

        private:
            F _f;
        };

        template <typename F>
        static inline OnScopeExitImpl<F> makeOnScopeExit(F f)
        {
            return OnScopeExitImpl<F>(f);
        }

        typedef const OnScopeExitBase& OnScopeExit;

        template<typename T>
        static void suppress_unused_variable_warning(const T&) {}

	}; //utilities
}

#endif
