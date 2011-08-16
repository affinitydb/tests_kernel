/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _RANDUTIL_H
#define _RANDUTIL_H
#include "tests.h"
#include <string>

// Collection of code for helping with random content generation
// (Code migrated from app.cpp and individual tests for ease of use)

class MVTRand
{
public:
	// Get a number in the inclusive range pMin to pMax.
	// e.g getRange(1,100) returns a number between 1 and 100
	static int getRange(int pMin, int pMax);

	static float getFloatRange(float pMin, float pMax);

	static double getDoubleRange(double pMin, double pMax);

	// Random true or false
	static bool getBool();

	// Generate a random time
	static uint64_t getDateTime(ISession *pSession, bool pAllowFuture = false);

	// Generate a random string (copy on return, min-max signature)
	static Tstring getString2(
		int pMinLen,
		int pMaxLen = -1,  // if -1 then all strings exactly pMinLen long
		bool pWords = true, // Whether to allow spaces
		bool pKeepCase = true); // If false string is forced to lowercase

	// Generate a random string (by reference, min-extra signature)
	// Note: pExtra represents the maximum number of 'extra' characters (total string length will be between pMin and pMin + pExtra)
	// Note: the wide char version produces non-ascii content
	static Tstring & getString(Tstring & pS, int pMin = 1, int pExtra = MAX_PARAMETER_SIZE, bool pWords = true, bool pKeepCase = true);
	static Wstring & getString(Wstring & pS, int pMin = 1, int pExtra = MAX_PARAMETER_SIZE, bool pWords = true, bool pKeepCase = true);
} ;

class MVTRand2
{
public:
	// Similar interface as MVTRand, except that here we control
	// the implementation; this is used by testgen, to predict
	// random values (e.g. to generate comparison snapshots from
	// other languages, without any c code linkage requirement).
	static int getRange(int pMin, int pMax);
	static float getFloatRange(float pMin, float pMax);
	static double getDoubleRange(double pMin, double pMax);
	static bool getBool();
	static uint64_t getDateTime(ISession *pSession, bool pAllowFuture = false);
	static Tstring getString2(int pMinLen, int pMaxLen = -1, bool pWords = true, bool pKeepCase = true);
	static Tstring & getString(Tstring & pS, int pMin = 1, int pExtra = MAX_PARAMETER_SIZE, bool pWords = true, bool pKeepCase = true);
	static Wstring & getString(Wstring & pS, int pMin = 1, int pExtra = MAX_PARAMETER_SIZE, bool pWords = true, bool pKeepCase = true);
} ;

class RandStrPool
{
public:
	// Helper for the common case where we want want to
	// work with a strings that are picked randomly out of
	// a fixed sized pool.
	// 
	// For example there might be one hundred categories 
	// in an app, or five different tags.
	//
	// This class supports both generated random strings and strings
	// from a provided list.
	
	RandStrPool()
	{	
	}

	RandStrPool( size_t inPoolSize, int minStr, int maxStr, bool bWords = false )
	{
		Init( inPoolSize, minStr, maxStr,bWords ) ;
	}

	void Init(size_t inPoolSize, int minStr, int maxStr, bool bWords = false)
	{
		// Randomly generated pool
		mPool.resize( inPoolSize ) ;
		for ( size_t i = 0 ; i < inPoolSize ; i++ )
		{
			string strRand = MVTRand::getString2( minStr, maxStr, bWords ) ;
			mPool[i] = strRand ;
		}
	}

	void Init( const char * inPool[] )
	{
		// Pool based on list of string (generated elsewhere)
		//
		// inPool must be terminated with an empty string

		int pos = 0 ;
		while( strlen(inPool[pos])>0)
		{
			mPool.push_back( inPool[pos] ) ;
			pos++ ;
		}
	}

	string getStr()
	{
		// Get a random string out of the pool
		if ( mPool.empty() ) { assert(!"Empty pool") ; return "" ; } 
		return mPool[MVTRand::getRange( 0, (int)mPool.size()-1 )] ;
	}

	string &getStrRef()
	{
		// Get a random string out of the pool
		static string empty;
		if ( mPool.empty() ) { assert(!"Empty pool") ; return empty ; } 
		return mPool[MVTRand::getRange( 0, (int)mPool.size()-1 )] ;
	}

	void getPoolSelection( 
			vector<string> & out, 
			int inclusionChance /* 1-99 */, 
			bool bAllowEmpty = true )
	{
		// Pick an assortment of items out of the pool
		//
		// No item is added more than one.
		//
		// If bAllowEmpty is true then the returned array 
		// might be completely empty, otherwise at least 
		// one element will be picked
		//
		// Pass inclusionChange=100 if you want every single element in the pool

		out.clear() ;
		if ( mPool.empty() ) { assert(!"Empty pool") ; return ; } 

		for ( size_t i = 0 ; i < mPool.size() ; i++ )
		{
			if ( MVTRand::getRange(0,99) < inclusionChance )
			{
				out.push_back( mPool[i] ) ;
			}
		}

		if ( !bAllowEmpty && out.empty() )
		{
			// Some apps won't expect a property to ever be completely missing
			out.push_back( getStr() ) ;
		}
	}

public:
	// Left public in case of enumeration
	vector<string> mPool ;
} ;

#endif
