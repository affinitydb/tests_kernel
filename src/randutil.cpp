/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "randutil.h"
#include "tests.h"

template <class TRandGen>
uint64_t _randomDateTime(ISession *pSession, bool pAllowFuture)
{
	TIMESTAMP ui64;
	DateTime lDT;
	static const int lsStartYear = 1995;
	int lsNumYears = 11; // REVIEW this stops in 2006
	if (pAllowFuture)
		lsNumYears = 22;  
	lDT.year = TRandGen::getRange(lsStartYear, lsStartYear + lsNumYears);
	lDT.month = TRandGen::getRange(1, 12);
	lDT.day = TRandGen::getRange(1, 28); // to be exact range could be based on the month
	lDT.dayOfWeek = 0; 
	lDT.microseconds = 0;
	lDT.hour = TRandGen::getRange(1, 23);
	lDT.minute = TRandGen::getRange(1, 59);
	lDT.second = TRandGen::getRange(1, 59);
	pSession->convDateTime(lDT,ui64);
	return ui64;
}

template <class TRandGen>
Tstring _randomGetString2(int pMinLen, int pMaxLen, bool pWords, bool pKeepCase)
{
	Tstring str;
	if (pMaxLen <= pMinLen)
		TRandGen::getString(str, pMinLen, 0, pWords, pKeepCase);
	else
		TRandGen::getString(str, pMinLen, pMaxLen - pMinLen, pWords, pKeepCase);
	return str;
}

template <class TRandGen>
Tstring & _randomGetString(Tstring & pS, int pMin, int pExtra, bool pWords, bool pKeepCase)
{
	// NOTE:
	//   The implementation used to use push_back to append character by character;
	//   on windows+dbg, even with 'reserve', this was painfully slow for large
	//   strings.
	pS.clear();
	long const lLength = TRandGen::getRange(pMin, pMin + pExtra);
	char * const lS = new char[lLength + 1];
	lS[lLength] = 0;
	long lNextSpace = TRandGen::getRange(5, 25);
	long i, j;
	for (i = 0, j = 0; i < lLength; i++, j++)
	{
		char lC;
		if (pWords && j > lNextSpace)
		{
			lC = ' ';
			lNextSpace = TRandGen::getRange(5, 25);
			j = 0;
		}
		else if (!pKeepCase || TRandGen::getBool())
			lC = 'a' + (char)TRandGen::getRange(0, 25);
		else
			lC = 'A' + (char)TRandGen::getRange(0, 25);
		lS[i] = lC;
	}
	pS += lS;
	delete [] lS;
	return pS;
}

template <class TRandGen>
Wstring & _randomGetWString(Wstring & pS, int pMin, int pExtra, bool pWords, bool pKeepCase)
{
	pS.clear();
	long const lLength = TRandGen::getRange(pMin, pMin + pExtra);
	wchar_t * const lS = new wchar_t[lLength + 1];
	lS[lLength] = 0;
	long lNextSpace = TRandGen::getRange(5, 25);
	long i, j;
	for (i = 0, j = 0; i < lLength; i++, j++)
	{
		wchar_t lC;
		if (pWords && j > lNextSpace)
		{
			lC = ' ';
			lNextSpace = TRandGen::getRange(5, 25);
			j = 0;
		}
		else if (!pKeepCase || TRandGen::getBool())
			lC = 'a' + (char)TRandGen::getRange(0, 25);
		else
			// This is a bit dangerous for testing as it produces characters
			// from sets that may not be supported on current machine
			lC = 0x7700 + (wchar_t)TRandGen::getRange(0, 256);
		lS[i] = lC;
	}
	pS += lS;
	delete [] lS;
	return pS;
}

int MVTRand::getRange(int pMin, int pMax)
{
	if (pMin == pMax)
		return pMin;
	if (pMax >= RAND_MAX)
	{
		// Note (maxw):
		//   On some platforms RAND_MAX is MAX_INT, but on other platforms
		//   it's much smaller.  The code below is just a quick patch for the latter case.
		//   We could attempt something better (e.g. some computation based on
		//   multiple evaluations of rand()), or we could use a different random
		//   generator.  I preferred a simple patch at this point, for historical/continuity
		//   reasons (and because this issue/usage is very infrequent).
		return (int)getDoubleRange((double)pMin, (double)pMax);
	}

	// Tests widely use random numbers
	// This makes it easy for picking number randomly in a range
	// pMin and pMax values are included.

	//For example: randInRange( 1, 100 ) -> random number from 1 to 100
	//             randInRange( 0, 1 ) -> random true or false

	int normalized = 1 + pMax - pMin;
	assert(normalized > 0 || normalized < RAND_MAX);

	int randval; // Will get a well distributed value from 0 to normalized -1 
	const int bucket_size = RAND_MAX / normalized;
	do 
	{
		randval = rand() / bucket_size;
	}
	while (randval >= normalized);

	return pMin + randval;
}

float MVTRand::getFloatRange(float a, float b) { return ((b-a)*((float)rand()/RAND_MAX))+a; }
double MVTRand::getDoubleRange(double a, double b) { return ((b-a)*((double)rand()/RAND_MAX))+a; }
bool MVTRand::getBool() { return 0 != getRange(0,1); }
uint64_t MVTRand::getDateTime(ISession *pSession, bool pAllowFuture) { return _randomDateTime<MVTRand>(pSession, pAllowFuture); }
Tstring MVTRand::getString2(int pMinLen, int pMaxLen, bool pWords, bool pKeepCase) { return _randomGetString2<MVTRand>(pMinLen, pMaxLen, pWords, pKeepCase); }
Tstring & MVTRand::getString(Tstring & pS, int pMin, int pExtra, bool pWords, bool pKeepCase) { return _randomGetString<MVTRand>(pS, pMin, pExtra, pWords, pKeepCase); }
Wstring & MVTRand::getString(Wstring & pS, int pMin, int pExtra, bool pWords, bool pKeepCase) { return _randomGetWString<MVTRand>(pS, pMin, pExtra, pWords, pKeepCase); }

class TestgenDeterministicRandomGenerator
{
	// Note (maxw, Dec2010):
	//   This simple random generator was taken from public domain
	//   descriptions of work from George Marsaglia.  It was chosen
	//   for its availability, simplicity, and ease of implementation on any
	//   platform/language.
	protected:
		mutable uint32_t mS1, mS2;
		static MVTestsPortability::Tls sDRG;
	public:
		TestgenDeterministicRandomGenerator() { setSeed(); }
		void setSeed(uint32_t pS1 = 179424673, uint32_t pS2 = 222222227) { assert(pS1 && pS2); mS1 = pS1; mS2 = pS2; }
		uint32_t getRandUI() const
		{ 
			mS1 = 36969 * (mS1 & 65535) + (mS1 >> 16);
			mS2 = 18000 * (mS2 & 65535) + (mS2 >> 16);
			uint32_t const lRet = (mS1 << 16) + (mS2 & 65535);
			// printf("next rand: %lu\n", lRet);
			return lRet;
		}
		double getRandD() const
		{
			return (getRandUI() + 1.0) / 4294967298.0;
		}
	public:
		static TestgenDeterministicRandomGenerator * getGen() { if (!sDRG.get()) { sDRG.set(new TestgenDeterministicRandomGenerator()); } return (TestgenDeterministicRandomGenerator *)sDRG.get(); }
		static uint32_t randUI() { return getGen()->getRandUI(); }
		static double randD() { return getGen()->getRandD(); }
};
MVTestsPortability::Tls TestgenDeterministicRandomGenerator::sDRG;

int MVTRand2::getRange(int pMin, int pMax)
{
	if (pMin == pMax)
		return pMin;
	int const lR = int(double(pMin) + TestgenDeterministicRandomGenerator::randD() * double(pMax - pMin));
	assert(lR >= pMin && lR <= pMax);
	return lR;
}

float MVTRand2::getFloatRange(float pMin, float pMax) { return pMin + (float)TestgenDeterministicRandomGenerator::randD() * (pMax - pMin); }
double MVTRand2::getDoubleRange(double pMin, double pMax) { return pMin + TestgenDeterministicRandomGenerator::randD() * (pMax - pMin); }
bool MVTRand2::getBool() { return TestgenDeterministicRandomGenerator::randD() > 0.5; }
uint64_t MVTRand2::getDateTime(ISession *pSession, bool pAllowFuture) { return _randomDateTime<MVTRand2>(pSession, pAllowFuture); }
Tstring MVTRand2::getString2(int pMinLen, int pMaxLen, bool pWords, bool pKeepCase) { return _randomGetString2<MVTRand2>(pMinLen, pMaxLen, pWords, pKeepCase); }
Tstring & MVTRand2::getString(Tstring & pS, int pMin, int pExtra, bool pWords, bool pKeepCase) { return _randomGetString<MVTRand2>(pS, pMin, pExtra, pWords, pKeepCase); }
Wstring & MVTRand2::getString(Wstring & pS, int pMin, int pExtra, bool pWords, bool pKeepCase) { return _randomGetWString<MVTRand2>(pS, pMin, pExtra, pWords, pKeepCase); }
