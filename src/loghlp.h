/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _mvstoreex_loghlp_h
#define _mvstoreex_loghlp_h

// Includes.
#include "mvstoreexports.h"
#include <commons/mvcore/sync.h>
#include <time.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

/**
 * Tracing/debugging helpers.
 */
#define DBGBUFS_MAXCONCURRENT 32
class MVSTOREEX_DLL DbgOutputHlp
{
	protected:
		typedef std::basic_string<char> Tstring;
		static Tstring sStrings[DBGBUFS_MAXCONCURRENT];
		static long volatile sNextString;
	public:
		static Tstring & getString();
		static char const * outputTime(std::ostream & pOs);
};
class DbgOutput_Console
{
	public:
		std::ostream & out() { return std::cout; }
};
class DbgOutput_Nil
{
	public:
		std::basic_ostringstream<char> mNullOutput;
		DbgOutput_Nil() { /*mNullOutput.rdbuf()->freeze(1);*/ }
		std::ostream & out() { return mNullOutput; }
	private:
		DbgOutput_Nil(DbgOutput_Nil const &);
		DbgOutput_Nil & operator=(DbgOutput_Nil const &);
};
class DbgOutput_File
{
	protected:
		std::basic_ofstream<char> mOut;
	public:
		DbgOutput_File(char const * pFileName) : mOut(pFileName, std::ios::out | std::ios::app) {}
		~DbgOutput_File() { mOut.flush(); }
		std::ostream & out() { return mOut; }
	private:
		DbgOutput_File(DbgOutput_File const &);
		DbgOutput_File & operator=(DbgOutput_File const &);
};
#ifndef DBG_PRINT
	#define DBG_PRINT(statement) {statement;}
#endif
#ifndef DBG_PRINT2
	#define DBG_PRINT2(statement) statement;
#endif
#define DBGMVSTORE(var) DbgOutput_Console var /*DbgOutput_File var("jmvstore_log.txt")*/
#define DBGSTAMP(var) DbgOutputHlp::outputTime(var.out())

#endif
