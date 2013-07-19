/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

///////////////////////////////////////////////////////////////////////////////
// construct, and setup a driver stack from an init string 
///////////////////////////////////////////////////////////////////////////////

#ifndef _stackbuilder_h_
#define _stackbuilder_h_

#ifndef WIN32
#include <dlfcn.h>
#endif

/*
init_string = '{driver_name[,param_name[:|=]value]}[{...}]'

stack is build bottom up

driver_name = [ddl_name:]driver
*/

///////////////////////////////////////////////////////////////////////////////
// integer support
///////////////////////////////////////////////////////////////////////////////

/*
Can parse integer numbers, and '0x00' values into a int to pass directly to the driver
this requires the following method on the IStoreIO interface.

virtual RC setParam(const char *in_key, int in_value, bool in_broadcast)

without this the numbers will be passed as strings.

warning with int support off 3ghixyz will be passed as a string to the driver
with support on this will pass 3, and generate an error on the 'g'
*/

//#define SUPPORT_INT_METHOD

namespace BuildStack
{

#define MAX_TOKEN 1024

///////////////////////////////////////////////////////////////////////////////
// simple string stream
///////////////////////////////////////////////////////////////////////////////

class Stream
{
public:
	Stream( const char* in_string )
	: m_string( in_string )
	, m_pnt( in_string )
	{
	}
	int getChar( void )
	{		
		if ( eof() )
			return 0;
		return *m_pnt++;
	}
	void putBack( void )
	{
		if ( m_string != m_pnt )
				--m_pnt;
	}
	bool eof( void )
	{
		return *m_pnt?false:true;
	}
	int offset( void )
	{
		return (int)(m_pnt - m_string);
	}
protected:
	const char* m_string;
	const char* m_pnt;
};

///////////////////////////////////////////////////////////////////////////////
// crude tokenizer 
///////////////////////////////////////////////////////////////////////////////

class Tokenizer
{
public:
	enum TokenTypes {
		TK_STRING = 128,
		TK_INT,
		TK_SYMBOL
	};
	Tokenizer( Stream* in_stream )
	: m_stream( in_stream )
	{
		nextToken();
	}

	bool is( int in_char )
	{
		if ( m_token == in_char )
		{
			if ( m_token == TK_STRING || m_token == TK_SYMBOL )
				strcpy( m_buffer, m_data );
			nextToken();
			return true;
		}
		return false;
	}
	bool isData()
	{
		if ( m_token == ':' || m_token == '=' )
		{
			int c = m_stream->getChar();
			if ( c == '\'' || c == '"' )
				string( c );
			else
				data( c );
			strcpy( m_buffer, m_data );
			nextToken();
			return true;
		}
		return false;
	}
	void eatSpace( void )
	{
		while ( isspace( m_stream->getChar() ));
		m_stream->putBack();
	}
	void string( int in_end )
	{
		char* p = m_data;
		int c;
		while ( p < m_data+MAX_TOKEN-1 && ( c = m_stream->getChar() ) != in_end )
		{
			*p++ = c;
		}
		*p = 0;
	}
	void symbol( int in_char )
	{
		char* p = m_data;
		*p++ = in_char;
		int c;
		while ( p < m_data+MAX_TOKEN-1 && isalnum( c = m_stream->getChar() ))
		{
			*p++ = c;
		}
		m_stream->putBack();
		*p = 0;
	}

#ifdef SUPPORT_INT_METHOD

	int hex( void )
	{
		int l_value = 0;
		while ( !m_stream->eof() )
		{
			char l_c = m_stream->getChar();

			if ( l_c >= '0' && l_c <= '9' )
				l_value = (l_value<<4) + (l_c - '0');
			else if ( l_c >= 'a' && l_c <= 'f' )
				l_value = (l_value<<4) + (l_c - 'a' + 10);
			else if ( l_c >= 'A' && l_c <= 'F' )
				l_value = (l_value<<4) + (l_c - 'A' + 10);
			else
			{
				m_stream->putBack();
				break;
			}
		}
		return l_value;
	}

	int number( char in_first )
	{
		int l_value = in_first - '0';
		int c = m_stream->getChar();

        if ( !l_value && c == 'x' )
			return hex();

		m_stream->putBack();
		
		while ( !m_stream->eof() )
		{
			c = m_stream->getChar();

			if ( c < '0' || c > '9' )
			{
				m_stream->putBack();
				break;
			}

			l_value = (l_value*10) + (c - '0');
		}
		return l_value;
	}

#else
	void data( int in_char )
	{
		char* p = m_data;
		*p++ = in_char;
		int c;
		while ( p < m_data+MAX_TOKEN-1 && ( c = m_stream->getChar() ) != ',' && c != '}')
		{
			*p++ = c;
		}
		m_stream->putBack();
		*p = 0;
	}
#endif

	void nextToken( void )
	{
		eatSpace();
		int c = m_stream->getChar();
		if ( c == '\'' || c == '\"' )
		{
			string( c );
			m_token = TK_STRING;
			return;
		}
		else if ( isalpha( c ))
		{
			symbol( c );
			m_token = TK_SYMBOL;
			return;
		}
/*		else if ( isdigit( c ))
		{
#ifdef SUPPORT_INT_METHOD
			m_value = number( c );
			m_token = TK_INT;
#else
			numsymbol( c );
			m_token = TK_SYMBOL;
#endif
			return;
		} */
		m_token = c;
	}
	void copyData( char* out_data )
	{
		strcpy( out_data, m_buffer );
	}
	int value( void )
	{
		return m_value;
	}
	int offset( void )
	{
		return m_stream->offset();
	}
	bool eof( void )
	{
		return m_stream->eof();
	}

protected:
	Stream* m_stream;
	int m_token;
	int m_value;
	char m_data[MAX_TOKEN];
	char m_buffer[MAX_TOKEN];
};

///////////////////////////////////////////////////////////////////////////////
// main builder oject, with error support 
///////////////////////////////////////////////////////////////////////////////

class StackBuilder
{
public:
	enum RC {
		RC_OK,
		RC_SYNTAX,
		RC_DRIVER,
		RC_PARAM,
	};

	StackBuilder( Tokenizer* in_tokenizer )
	: m_stack( 0 )
	, m_tokenizer( in_tokenizer )
	{
	}

	RC parsePair( void )
	{
		if ( !m_tokenizer->is( Tokenizer::TK_SYMBOL ))
		{
			error( "expecting key" );
			return RC_SYNTAX;
		}
		m_tokenizer->copyData( m_symbol );
		if ( !m_tokenizer->isData() )
		{
			error( "not a valid format for data" );
			return RC_SYNTAX;
		}
		m_tokenizer->copyData( m_value );
		m_isstring = true;
		return RC_OK;
	}

#ifdef WIN32
#define _callconv __cdecl
#define _defineLibName( _name, _dllname ) char _name[256];sprintf( _name, "%s.dll", _dllname )
#define _loadLibrary( _mod, _dllname ) HMODULE _mod = LoadLibrary( _dllname )
#define _unloadLibrary( _mod )
#define _getProcAddress GetProcAddress
#else
#define _callconv
#define _defineLibName( _name, _dllname ) char _name[256];sprintf( _name, "lib%s.so", _dllname )
#define _loadLibrary( _mod, _dllname ) void* _mod = dlopen( _dllname, RTLD_LAZY)
#define _unloadLibrary( _mod )
#define _getProcAddress dlsym
#endif

	IStoreIO* fromDll( const char* in_dllname, const char* in_name, IStoreIO* in_stack )
	{
		typedef IStoreIO* ( _callconv *FactoryMethod )( const char* in_name, IStoreIO* in_stack );

		_defineLibName( l_dllname, in_dllname );
		_loadLibrary( l_mod, l_dllname );
		if ( !l_mod ) return 0;

		FactoryMethod l_factory;
		l_factory = (FactoryMethod)_getProcAddress( l_mod, "getDriver" );
		if ( !l_factory )
		{
			_unloadLibrary( l_mod );		
			return 0;
		}

		return (l_factory)( in_name, in_stack );
	}

	virtual IStoreIO* fromStatic( const char* in_name, IStoreIO* in_stack ) = 0;

	void error( const char* in_error )
	{
		printf( "ERROR stackbuilder : %s, offset %d\n", in_error, m_tokenizer->offset() ); // TEMP
	}

	void warn( const char* in_error )
	{
		printf( "WARNING: stackbuilder : %s, offset %d \n", in_error, m_tokenizer->offset() );
	}

	RC parseNode( void )
	{
		if ( !m_tokenizer->is( Tokenizer::TK_SYMBOL ))
		{
			error( "expecting node type" );
			return RC_SYNTAX;
		}
		m_tokenizer->copyData( m_symbol );
		if ( m_tokenizer->is( ':' ) && m_tokenizer->is( Tokenizer::TK_SYMBOL ))
			m_tokenizer->copyData( m_value );
		else
			strcpy( m_value, m_symbol );

		IStoreIO* l_driver = fromDll( m_symbol, m_value, m_stack );
		if ( !l_driver )
		{
			l_driver = fromStatic( m_value, m_stack );
			if ( !l_driver )
			{
				error( "can't instantiate driver" );
				return RC_DRIVER;
			}
		}

		while ( m_tokenizer->is( ',' ) )
		{
			RC rc = parsePair();
			if ( rc != RC_OK )
				return rc;

			if ( m_isstring )
			{
				if ( l_driver->setParam( m_symbol, m_value, false ) != AfyRC::RC_OK )
				{
					error( "driver reported bad param" );
					return RC_PARAM;
				}
			}
#ifdef SUPPORT_INT_METHOD
			else
			{
				if ( l_driver->setParam( m_symbol, m_int, false ) != RC_OK )
				{
					error( "driver reported bad param" );
					return RC_PARAM;
				}
			}
#else
			else
			{
				error( "bad paramater" );
				return RC_PARAM;
			}
#endif
		}

		m_stack = l_driver;

		if ( !m_tokenizer->is( '}' ))
		{
			error( "expecting end of node '}'");
			return RC_SYNTAX;
		}
		return RC_OK;
	}

	RC parseStack( void )
	{
		while ( !m_tokenizer->eof() )
		{
			if ( m_tokenizer->is( '{' ))
			{
				RC l_rc = parseNode();
				if (  l_rc != RC_OK )
					return l_rc;
			}
			else
			{
				error( "expecting node list" );
				return RC_SYNTAX;
			}
		}
		return RC_OK;
	}

	IStoreIO* getStack( void )
	{
		return m_stack;
	}

	protected:
		IStoreIO *m_stack;
		Tokenizer* m_tokenizer;
		char m_symbol[MAX_TOKEN];
		bool m_isstring;
		int m_int;
		char m_value[MAX_TOKEN];
};
};

#endif
