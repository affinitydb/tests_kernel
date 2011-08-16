/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _LOCALBUILD_H
#define _LOCALBUILD_H

	#include "iotracer.h"
	#include "ioprofiler.h"
	#include "iofailer.h"


	#include "stackbuilder.h"

namespace BuildStack
{

class LocalBuilder : public StackBuilder
{
public:
	LocalBuilder( Tokenizer* in_tokenizer )
	: StackBuilder( in_tokenizer )
	{
	}
	static IStoreIO* createDefaultIO()
	{
#ifdef STORE_DYNAMIC_LINK
		// For use by tests and other utilities that use the dynamic link mechanism
		typedef IStoreIO* (*TgetMvStoreIO)();
		#ifdef WIN32
			HMODULE lMvstore = ::LoadLibrary("mvstore.dll");
			if (!lMvstore)
				return NULL;
			TgetMvStoreIO lGetIO = (TgetMvStoreIO)::GetProcAddress(lMvstore, "getStoreIO");
			IStoreIO* lResult = lGetIO ? (*lGetIO)() : NULL;
			::FreeLibrary(lMvstore);
			return lResult;
		#else
			void * lMvstore = dlopen("libmvstore.so", RTLD_LAZY);
			if (!lMvstore)
				return NULL;
			TgetMvStoreIO lGetIO = (TgetMvStoreIO)dlsym(lMvstore, "getStoreIO");
			IStoreIO* lResult = lGetIO ? (*lGetIO)() : NULL;
			dlclose(lMvstore);
			return lResult;
		#endif
#else
		// For use by code which links directly to mvstore
		// e.g. mvstore server, store dr...
		return getStoreIO();
#endif
	}

	IStoreIO* fromStatic( const char* in_name, IStoreIO* in_stack )
	{
		if ( !strcmp( in_name, "stdio" ))
		{
			return createDefaultIO();
		}
		else if ( !strcmp( in_name, "iotracer" ) )
		{
			return new IOTracer(in_stack);
		}
		else if ( !strcmp( in_name, "ioprofiler" ) )
		{
			return new IOProfiler(in_stack);
		}
		else if ( !strcmp( in_name, "iofailer" ) )
		{
			return new IOFailer(in_stack);
		}
		return 0;
	}
};
};
#endif
