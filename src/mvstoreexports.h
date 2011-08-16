/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _MVSTOREXEXPORTS_h
#define _MVSTOREXEXPORTS_h

#ifdef WIN32
	#ifdef MVSTOREEX_PROJECT
		#define MVSTOREEX_DLL __declspec(dllexport)
	#else
		#define MVSTOREEX_DLL __declspec(dllimport)
	#endif
#else
	#define MVSTOREEX_DLL
#endif

#endif
