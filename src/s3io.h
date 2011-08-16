/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the S3IO_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// S3IO_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.

#ifndef _interface_s3io_h
#define _interface_s3io_h

#ifdef WIN32
#ifdef S3IO_EXPORTS
#define S3IO_API __declspec(dllexport)
#else
#define S3IO_API __declspec(dllimport)
#endif
#else
#include <dlfcn.h>
#define S3IO_API
#endif

///////////////////////////////////////////////////////////////////////////////
// Forward class references
///////////////////////////////////////////////////////////////////////////////

class IStoreIO;
class IConfigurationMap;

///////////////////////////////////////////////////////////////////////////////
// Dll interface
///////////////////////////////////////////////////////////////////////////////

extern "C" {
	// store interface to s3io driver
	S3IO_API IStoreIO* getDriver( const char* in_name, IStoreIO* in_localdevice );

	// s3tool interface
	S3IO_API int cleanKeys( IConfigurationMap* in_config );

	// Configuration interface
	S3IO_API IConfigurationMap* newConfig( void );
	S3IO_API void setConfig( IConfigurationMap* in_config, const char* in_name, const char* in_value );
	S3IO_API const char* getConfig( IConfigurationMap* in_config, const char* in_name );
	S3IO_API void deleteConfig( IConfigurationMap* in_config );

	S3IO_API int listVersions( IConfigurationMap* in_config );
	S3IO_API int uploadStore( IConfigurationMap* in_config );
	S3IO_API int compactStore( IConfigurationMap* in_config );
	S3IO_API int mapStore( IConfigurationMap* in_config );
};

///////////////////////////////////////////////////////////////////////////////
// Simple/cross platform on demand loading of dynamic link library
///////////////////////////////////////////////////////////////////////////////

namespace S3IO
{
#ifdef WIN32
typedef HMODULE MHandle;
#define _loadLibrary LoadLibrary
#define _defineLibName( _name, _dllname ) char _name[256];sprintf( _name, "%s.dll", _dllname )
#define _getProcAddress GetProcAddress
#else
typedef void* MHandle;
#define _loadLibrary( _name ) dlopen( _name, RTLD_LAZY)
#define _defineLibName( _name, _dllname ) char _name[256];sprintf( _name, "lib%s.so", _dllname )
#define _getProcAddress dlsym
#endif

class Library
{
public:
	Library( const char* in_library )
	: m_library( in_library )
	, m_ref( 0 )
	{		
	}
	~Library( )
	{
	}
	MHandle load( void )
	{
		if ( m_ref )
		{
			++m_ref;		
			return m_mhandle;
		}
		_defineLibName( l_libraryname, m_library );
		m_mhandle = _loadLibrary( l_libraryname );
		if ( m_mhandle )
		{
			++m_ref;
		}
		return m_mhandle;
	}
	void release( void )
	{
		if ( m_ref )
		{
			--m_ref;
			if ( !m_ref )
			{
			 // ?? delete something	
			}
		}
	}
	const char *name( void ) const
	{
		return m_library;
	}
protected:
	const char* m_library;
	int m_ref;
	MHandle m_mhandle;
};

template <class T>
class Link
{
public:
	Link( Library& in_library, const char* in_name )
	: m_library( in_library )
	, m_name( in_name  )
	, m_method( 0 )
	{
	}
	~Link()
	{
		m_library.release();
	}
	T method( const char* in_file, int in_line )
	{
		if ( m_method ) return m_method;
		MHandle l_handle = m_library.load();
		if ( l_handle )
			m_method = (T)_getProcAddress( m_library.load(), m_name );
		
		if ( !m_method )
		{
			printf( "%s(%d) : error : failed to call %s::%s\n", in_file, in_line, m_library.name(), m_name );
			exit( -1 );
		}
		return m_method;
	}
protected:
	Library& m_library;
	const char* m_name;
	T m_method;
};

#undef _loadLibrary
#undef _defineLibName
#undef _getProcAddress

};

#define DynamicLoad( _library ) S3IO::Library s_##_library( #_library )
#define DynamicLink( _return, _library, _method, _param ) S3IO::Link < _return (*) _param > s_##_method( s_##_library, #_method )
#define DynamicCall( _name ) (s_##_name.method( __FILE__, __LINE__ ))

/*
// define the library to load
DynamicLoad( s3io );

// define the return type, library, and method to call, with call function signiture
DynamicLink( bool, s3io, cleanKeys, ( const char* in_bucket, const char* in_key, const char* in_user, const char* in_secret, const char* in_host ));

// call the method, at call time the library is loaded, the entry point resolved, and the method called.
return DynamicCall( cleanKeys )( m_bucket, m_key, m_user, m_secret, m_host );
*/



#endif // _interface_s3io_h





