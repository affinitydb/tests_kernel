/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

// Basic coverage of the Full Text searching support
// Other tests already cover Full Text Searching, but often for more specific scenarios, for example
// streams, multi-threads etc.  This test is more of an introduction to the simple usage and confirm
// what sort of word matching really is performed.

// TODO: as implemented cover Stemming, other locals etc

#include "app.h"
#include "mvauto.h"
#include "teststream.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

#define VERBOSE 0

#define TEST_FTQ_AS_STRING_9084 0 // Note (maxw, Dec2010): I disabled this and logged new bug #128.
//The following structure is used for UTF8 encoded patterns presentaions...
struct utf8Buf{
	char *buf; 
	
	utf8Buf(const char * fName)
	{
#ifdef WIN32
#define LOCATION_TO_DATA "..\\textdata\\"
#else
#define LOCATION_TO_DATA "../textdata/"
#endif
/* 
 * It is expected that the files, mentioned below are located within 
 * <test directory>/textdata
*/
#define  WHOLE_FILE        "UTF-8-demo.txt"
#define  PART_IN_RUS       "UTF-8-rus.txt"
#define  PART_IN_RUS1      "UTF-8-rus1.txt"
#define  PART_IN_RUS2      "UTF-8-rus2.txt"
#define  PART_IN_GREEK     "UTF-8-greek.txt"
#define  PART_IN_ETHIOPIAN "UTF-8-ethiopian.txt"
#define  PART_IN_GEORGIAN  "UTF-8-georgian.txt"
#define  PART_IN_THAI      "UTF-8-thai.txt"
		struct stat results;
		string lFullName(LOCATION_TO_DATA);
		lFullName += fName;

		if (stat(lFullName.c_str(), &results) == 0){
			// The size of the file in bytes is in results.st_size
			buf=(char*)malloc(results.st_size+1);
		}else{
			cerr << "Failed to get size of " << fName << " !" << endl; buf=NULL;
		}
		ifstream  fs(lFullName.c_str(),ios::in | ios::binary);
		if(!fs){
			cerr << "Failed to open " << fName << " !" << endl; 
		}else{
			fs.read(buf, results.st_size); buf[results.st_size]=0;
			fs.close();
		}
	}
	~utf8Buf(){free(buf);};
};

class FTSearchContext
{
public:
	// Helper to make each FT scenario simple and isolated from other
	// content of the store
	
	FTSearchContext(ITest* inTest, ISession * inSession)
		: mSession(inSession)
		, mTest(inTest)
	{
		// Sequential property names to avoid risk of collision during a single test
		char URIName[64];
		sprintf(URIName,"FTSearchContext_%d_%d",mPropCnt++,MVTApp::Suite().mSeed);
		mProp=MVTUtil::getProp(mSession,URIName);
	}

	PID createTextPin(const char * inProp1, bool bRemoveStopWords = true )
	{
		if (sUseStream)
			return createStreamTextPin(inProp1,bRemoveStopWords);

		Value vl ; 
		vl.set( inProp1 ) ; 
		vl.property = mProp ;

		if (bRemoveStopWords)
		{
			// Means that any common english words will be excluded from the index
			// vl.meta = META_PROP_STOPWORDS ; // it's done always now
		}
		IPIN *pin;
		TVRC_R(mSession->createPIN(&vl, 1, &pin, MODE_PERSISTENT|MODE_COPY_VALUES),mTest) ;
		return pin->getPID() ;
	}

	unsigned long search(
		size_t pLineNo,
		char* inSearch, 
		unsigned long inExpected, 
		unsigned int flags = 0  /* for example MODE_ALL_WORDS*/)
	{
		PropertyID props[1] ; int cnt = 1 ;
		props[0] = mProp ;

		TV_R(inSearch!=NULL&&strlen(inSearch)>0,mTest);

		uint64_t cntResults = 0 ;
		CmvautoPtr<IStmt> ftQ(mSession->createStmt()) ;
		unsigned char v = ftQ->addVariable() ;

		string strFlags;
		if ( flags & QFT_FILTER_SW ) strFlags.append( "QFT_FILTER_SW ");
		if ( flags & MODE_ALL_WORDS ) strFlags.append( "MODE_ALL_WORDS ");

		// For testing convenience two different flag args jammed into one
		// but if any value overlap happens in future this will have to split
		unsigned int flagsToAddConditionFT = 0;
		if (flags&QFT_FILTER_SW)
		{
			flagsToAddConditionFT |= QFT_FILTER_SW;
			flags &= ~QFT_FILTER_SW ;
		}

		// NOTE: This isn't a "Full scan" query even though no class is specified
		TVRC_R( ftQ->addConditionFT( v, 
					inSearch, 
					flagsToAddConditionFT /*WARNING THIS IS NOT THE PLACE FOR MODE_ALL_WORDS*/, 
					props, 
					cnt ), mTest ) ;

		// Verify conversion from string back to query
#if TEST_FTQ_AS_STRING_9084
		char * strQ = ftQ->toString() ;
		CmvautoPtr<IStmt> ftQ2(mSession->createStmt(strQ));
		if ( ftQ2.IsValid() )
		{
			char * strQ2 = ftQ2->toString() ;

			if ( mTest->isVerbose() )
			{
				mTest->getLogger().out() << "Q1" << endl << strQ << endl << "Q2" << endl << strQ2 << endl ;
			}
			mSession->free(strQ2);

			ftQ.Attach(ftQ2.Detach());
		}
		else
		{
			TV_R(!"Failure to convert FT query from string",mTest) ;
		}
		mSession->free(strQ);
#endif

		TVRC_R( ftQ->count( cntResults, NULL, 0, ~0, flags ), mTest );
		ICursor* lC = NULL;
		ftQ->execute(&lC,NULL,0,~0,0,flags);
		CmvautoPtr<ICursor> res(lC);
		IPIN * pin ;
		unsigned long cntCheck = 0 ;
		while( pin = res->next() )
		{
			cntCheck++ ;
			pin->destroy() ;
		}
		TV_R(cntCheck == inExpected, mTest) ;
		TV_R(cntCheck == cntResults, mTest) ;

		if (mTest->isVerbose())
			printResults( inSearch, flags ) ;

		if ( cntCheck != inExpected || cntResults != inExpected )
		{
			if ( cntCheck == 0 )
			{
				mTest->getLogger().out() << "No matches for \"" << inSearch << "\" " << strFlags << " Prop: " << mProp ;
				mTest->getLogger().out() << " (line " << pLineNo << ")" << endl ;
			}
			else
			{
				mTest->getLogger().out() << strFlags << " Expected " << inExpected << " got " << cntCheck << " Prop: " << mProp;
				mTest->getLogger().out() << " (line " << pLineNo << ")" << endl ;
				printResults( inSearch, flags );
			}
			
			// Full scan print properties and show what is there
			mTest->getLogger().out() << "-------------Dump of strings on Properties--------------" << endl;
			CmvautoPtr<IStmt> qFullScan(mSession->createStmt());
			TVRC_R(qFullScan->setPropCondition(qFullScan->addVariable(),&mProp,1),mTest);
			ICursor* lC = NULL;
			qFullScan->execute(&lC);
			CmvautoPtr<ICursor> rFullScan(lC);
			IPIN* pin;
			while(NULL!=(pin=rFullScan->next()))
			{
				mTest->getLogger().out() << "Pin: " << hex << pin->getPID().pid << dec ;
				const Value * indexedStr = pin->getValue(mProp); TV_R(indexedStr!=NULL,mTest);
				if ( indexedStr->type==VT_STRING)
					mTest->getLogger().out() << " " << indexedStr->str << endl;
				else if ( indexedStr->type==VT_STREAM )
					mTest->getLogger().out() << " (stream)" << endl;
				else
					TV_R(false,mTest);
				pin->destroy();
			}
		}

		return (unsigned long)cntResults ;
	}

	void printResults( char* inSearch, unsigned int flags )
	{
		CmvautoPtr<IStmt> ftQ(mSession->createStmt());
		unsigned char v = ftQ->addVariable() ;
		TVRC_R( ftQ->setConditionFT( v, inSearch, 0, &mProp, 1 ), mTest ) ;
		ICursor* lC = NULL;
		ftQ->execute(&lC,NULL,0,~0,0,flags);
		CmvautoPtr<ICursor> res(lC);
		IPIN * pin ;
		mTest->getLogger().out() << "Search for \"" << inSearch << "\"" << endl << "----------------------" << endl ;
		while( pin = res->next() )
		{
			const Value * v = pin->getValue(mProp);
			mTest->getLogger().out()<<std::hex<<pin->getPID().pid<<std::dec<<" ";
			MVTApp::output(*v,mTest->getLogger().out(),mSession);

			pin->destroy() ;
		}
		mTest->getLogger().out() << "------------------" << endl ; 
	}

	PropertyID getProp() const { return mProp; }

	static bool sUseStream ;

protected:

	PID createStreamTextPin(const char * inProp1, bool bRemoveStopWords = true )
	{
		// Different code in the kernel for stream based strings
		// BUT strings < 0xFF length are turned back to string format
		// so we need to force it.

		TestStream * ts = new TestStream((int)(40000)*sizeof(char), 
					NULL,
					VT_STRING ) ;
		
		string forceContent = " "; forceContent+=inProp1; forceContent += " ";
		
		// Insert the real words part way through the dummy content
		memcpy(ts->getBuffer()/*+32*/,forceContent.c_str(),forceContent.size()*sizeof(char));

		Value vl ; 
		vl.set(MVTApp::wrapClientStream(mSession, ts)) ; 
		vl.property = mProp ;

		if (bRemoveStopWords)
		{
			// Means that any common english words will be excluded from the index
			// vl.meta = META_PROP_STOPWORDS ; // it's done always now
		}
		IPIN *pin;
		TVRC_R(mSession->createPIN(&vl, 1, &pin, MODE_PERSISTENT|MODE_COPY_VALUES),mTest) ;
		ts->destroy() ;
		return pin->getPID() ;
	}

protected:
	PropertyID	mProp ;
	ISession *	mSession ;
	ITest*		mTest ;
	static int  mPropCnt; // Extra precaution to avoid duplicate properties
} ;

bool FTSearchContext::sUseStream = false ;
int FTSearchContext::mPropCnt = 0;

// Publish this test.
class TestFTBasic : public ITest
{
	public:
		TEST_DECLARE(TestFTBasic);
		virtual char const * getName() const { return "testftbasic"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Basic FT coverage"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "known failure..."; return false;}
		virtual bool isPerformingFullScanQueries() const { return true; }
	
		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void doTests() ;

		void testSinglePropSearch() ;
		void testMultiPropSearch() ;
		void testStemmingBehavior() ;
		void testStopWords() ;
		void testStopWords2() ;
		void testStopWords3() ;
		void testStopWords4() ;
		void testStopWordsPrefix();
		void testStopWordsPrefix2();
		void testApostrophe() ;
		void testEmailIndexing();
		void testWordNumberMix() ;
		void testLuisVocab() ;
		void testFTStats();
		void testSingleChar();
		void showHexOfMBStr(const char* inStr);
		void testUTF8();

		PID create2PropPin( const char * inProp1, const char * inProp2 = NULL, bool bStripStopWords = true ) ;
		unsigned long search2PropPin( char* inSearch, bool bProp1 = true, bool bProp2 = false, unsigned int flags = 0 ) ;

	private:
		ISession * mSession ;
		PropertyID mProp1 ;
		PropertyID mProp2 ;

		bool m14806Repro;
};
TEST_IMPLEMENT(TestFTBasic, TestLogger::kDStdOut);

int TestFTBasic::execute()
{
	m14806Repro = (get_argc() >=3 && 0 == strcmp(get_argv(2),"14806")) ;
	bool bWiki = (get_argc() >=3 && 0 == strcmp(get_argv(2),"wiki")) ;

	if (m14806Repro)
	{
		MVTApp::deleteStore();
	}

	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	// Creates unique properties so that the exact number of FT matches can be confirmed
	MVTApp::mapURIs(mSession,"FTBasic.prop1",1,&mProp1) ;
	MVTApp::mapURIs(mSession,"FTBasic.prop2",1,&mProp2) ;
	if (bWiki)
	{
		testFTStats();
	}
	else
	{
		// Regular scenario
		FTSearchContext::sUseStream = false ;
		doTests();

		// Repeat all tests again, but use stream format instead
		// This excercises different tokenization code inside the kernel
		mLogger.out() << "...repeating test with VT_STREAM..." << endl;
		FTSearchContext::sUseStream = true ;

		// Reset these props for second pass
		MVTApp::mapURIs(mSession,"FTBasic.prop1",1,&mProp1) ;
		MVTApp::mapURIs(mSession,"FTBasic.prop2",1,&mProp2) ;

		doTests();
	}

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
	return RC_OK;
}

void TestFTBasic::testUTF8()
{
	utf8Buf pattern1(WHOLE_FILE); 
	if(!pattern1.buf){
		mLogger.out() << "Warning: No testUTF8()" << endl; return;
	}
	// Searching a specific single property in the store
	FTSearchContext ctxt( this, mSession ) ;
	static const bool bFilterStopWords = true ;
	 
	mLogger.out() << "Running series of simple UTF8 tests..." << endl;

	ctxt.createTextPin( pattern1.buf, bFilterStopWords ) ; //Now I have a pin withe the whole UTF8 demo text...

	//Step1. Searching for some titles, which are in English within the whole documents... 
	{
		mLogger.out() << "Searching for English titles..." << endl;
		
		ctxt.search( __LINE__, "Markus Kuhn", 1);
		ctxt.search( __LINE__, "mARKUS kUHN", 1);
		ctxt.search( __LINE__, "Mathematics and sciences", 1, 0 /*MODE_ALL_WORDS*/);
		ctxt.search( __LINE__, "mATHEMATICS SCIENCES", 1, MODE_ALL_WORDS);
		ctxt.search( __LINE__, "Linguistics and dictionaries", 1);
		ctxt.search( __LINE__, "Georgian", 1);
		ctxt.search( __LINE__, "Russian", 1);
		ctxt.search( __LINE__, "rUSSIAN", 1);
		ctxt.search( __LINE__, "Ethiopian", 1);
		ctxt.search( __LINE__, "eTHIOPIAN", 1);
		ctxt.search( __LINE__, "Runes", 1);
		ctxt.search( __LINE__, "Braille", 1);
	}
    
	//Step2. Searching for some patterns, which are not in English within the whole documents...
	{
		mLogger.out() << "Searching for patterns in Russian..." << endl;
		utf8Buf patternRus(PART_IN_RUS);
		if(!patternRus.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl; 
		}
		else
			ctxt.search( __LINE__,patternRus.buf,1, MODE_ALL_WORDS);

		//The following 2 searches are initialezed with capital characters...
		utf8Buf patternRus1(PART_IN_RUS1);
		if(!patternRus1.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}
		else
			ctxt.search( __LINE__,patternRus1.buf,1, MODE_ALL_WORDS);

		utf8Buf patternRus2(PART_IN_RUS2);
		if(!patternRus2.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}
		else
			ctxt.search( __LINE__,patternRus2.buf,1, MODE_ALL_WORDS);
	}

	//Step3. Searching for some patterns, which are not in English within the whole documents...
	{
		mLogger.out() << "Searching for patterns in Greek..." << endl;
		utf8Buf patternGreek(PART_IN_GREEK);
		if(!patternGreek.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}else
			ctxt.search( __LINE__,patternGreek.buf,1);
	}

	//Step4. Searching for some patterns, which are not in English within the whole documents...
	{
		mLogger.out() << "Searching for patterns in Georgian ..." << endl;
		utf8Buf patternGeorgian(PART_IN_GEORGIAN);
		if(!patternGeorgian.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}else
			ctxt.search( __LINE__,patternGeorgian.buf,1);
	}

	//Step5. Searching for some patterns, which are not in English within the whole documents...
	{
		//The check for Ethiopian is disabled for now since the code for UTF8 escape sequences is not 
		//supported. Quoting Mark: "... One day later... may be..."  
		#if 0
			mLogger.out() << "Searching for patterns in Ethiopian ..." << endl;
			utf8Buf patternethiopian(PART_IN_ETHIOPIAN);
			if(!patternethiopian.buf){
				mLogger.out() << "Warning: No this part of testUTF8()" << endl;
			}else
				ctxt.search( __LINE__,patternethiopian.buf,1);
		#endif
	}

	//Step6. Searching for some patterns, which are not in English within the whole documents...
	{
		mLogger.out() << "Searching for patterns in THAI ..." << endl;
		utf8Buf patternthai(PART_IN_THAI);
		if(!patternthai.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}else
			ctxt.search( __LINE__,patternthai.buf,1);
	}
}

void TestFTBasic::doTests()
{
	testSinglePropSearch() ;
	testStemmingBehavior() ;
	testStopWords() ;
	testStopWords2() ;
	testStopWords3() ;

	if (m14806Repro)
	{
		for ( int i = 0 ; i < 1000 ; i++ )
		{
			testStopWordsPrefix();
		}
	}
	else 
		testStopWordsPrefix();

	testStopWordsPrefix2();
	testApostrophe();
	testEmailIndexing() ;
	testMultiPropSearch() ;
	testWordNumberMix() ;
	testLuisVocab();
	testFTStats();
	testSingleChar();

	testUTF8();
}

void TestFTBasic::testSinglePropSearch()
{
	// Searching a specific single property in the store
	FTSearchContext ctxt( this, mSession ) ;
	static const bool bFilterStopWords = true ;

	ctxt.createTextPin( "Lots of interesting info\nabout stuff that you should know", bFilterStopWords ) ;
	ctxt.createTextPin( "\tNotes from last years meeting\t1. Meeting minutes\t2. Lots of talk. \"blah, blah, blah\"\t3. End of meeting\n", bFilterStopWords ) ;
	ctxt.createTextPin( "Event 1000\nEvent 1001\nEvent 1002\nEvent 1003\nWordNumberCombo789", bFilterStopWords ) ;
	ctxt.createTextPin( "File stored at the root directory, because very important\nIt must be easy to find", bFilterStopWords ) ;
	ctxt.createTextPin( "words another words words words", bFilterStopWords ) ;

	// REVIEW: Brief overview of FT searching capabilities, based on the 
	// file content set in PopulateStore.  FT searching is tested in more detail
	// elsewhere

	ctxt.search( __LINE__, "lots", 2) ;// case insensitive
	ctxt.search( __LINE__, "Lots", 2) ;
	ctxt.search( __LINE__, "Notes Meeting", 1) ;// Words don't have to be consecutive

	// substring matching
	ctxt.search( __LINE__, "directory", 1) ;	// Will it match with directory,
	ctxt.search( __LINE__, "director", 1) ;	// Will match with directory,	
	ctxt.search( __LINE__, "ectory", 0 ) ;	// Matching only looks at beginning of string
													
	ctxt.search( __LINE__, "files", 0) ; // The word "file" is in the string.  
						// REVIEW: Would stemming feature, when implemented, match this?

	// Numbers
	ctxt.search( __LINE__, "1000", 1) ;
	ctxt.search( __LINE__, "100", 1) ;			// Matches with "1000" etc
	ctxt.search( __LINE__, "WordNumberCombo789", 1) ;
	ctxt.search( __LINE__, "789", 0) ;			// Probably ok that nothing found
	ctxt.search( __LINE__, "WordNumberCombo", 1) ;

	// These words appear in 4 different pins, but no pin as all 4 words
	// This where MODE_ALL_WORDS is important

	ctxt.search( __LINE__, "File Event years interesting", 4, 0 ) ;
	ctxt.search( __LINE__, "File Event years interesting", 0, MODE_ALL_WORDS) ; // All appear in some but not all

	ctxt.search( __LINE__, "Directory IMPORTANT", 1, 0) ;
	ctxt.search( __LINE__, "apple banana", 0, MODE_ALL_WORDS ) ; // Appear in none
}

void TestFTBasic::testSingleChar()
{
	FTSearchContext ctxt( this, mSession ) ;
	ctxt.createTextPin( "1 2 3", false /* META_PROP_STOPWORDS */) ;
	ctxt.createTextPin( "a b c", false) ;

	// Single char no longer indexed (#14713)
	ctxt.search( __LINE__, "1", 0) ;	
	ctxt.search( __LINE__, "2", 0) ;	
	ctxt.search( __LINE__, "a", 0) ;	

	// length 2 is ok and indexed
	ctxt.createTextPin( "11", false) ;
	ctxt.createTextPin( "aa", false) ;
	ctxt.search( __LINE__, "aa", 1) ;	
	ctxt.search( __LINE__, "11", 1) ;	
	ctxt.search( __LINE__, "a", 1) ;	  // "a" search matches "aa"
	ctxt.search( __LINE__, "1", 1) ;	// "1" search matches "11"

	// A few more "incremental" search examples
	ctxt.createTextPin( "andrew", false) ;
	ctxt.createTextPin( "ansector", false) ;
	ctxt.search( __LINE__, "a", 3) ;	 // matches aa, andrew, ansector
	ctxt.search( __LINE__, "an", 2) ;	 // matches	andrew, ansector
}

void TestFTBasic::testWordNumberMix()
{
	// Sanity check to make sure that Band name like "U2" works properly

	FTSearchContext ctxt( this, mSession ) ;
	ctxt.createTextPin( "U2", true /* META_PROP_STOPWORDS */) ;
	ctxt.search( __LINE__, "U2", 1) ;	
	ctxt.search( __LINE__, "u2", 1) ;	

	ctxt.createTextPin( "us3", true /* META_PROP_STOPWORDS */) ;
	ctxt.search( __LINE__, "us3", 1) ;	
	
	ctxt.createTextPin( "25point", true /* META_PROP_STOPWORDS */) ;
	ctxt.createTextPin( "6lucent", true /* META_PROP_STOPWORDS */) ;

	ctxt.search( __LINE__, "25point", 1,MODE_ALL_WORDS) ;  
	ctxt.search( __LINE__, "25 point", 1,MODE_ALL_WORDS) ;// fails due to bug 23126
	ctxt.search( __LINE__, "25", 1,MODE_ALL_WORDS) ;  
	ctxt.search( __LINE__, "point", 1,MODE_ALL_WORDS) ;   // fails due to bug 23126
	ctxt.search( __LINE__, "25moint", 0,MODE_ALL_WORDS) ; // fails due to bug 23126
	ctxt.search( __LINE__, "6lucent", 1,MODE_ALL_WORDS) ; //fails due to bug 23126
	ctxt.search( __LINE__, "lucent", 1,MODE_ALL_WORDS) ; //fails due to bug 23126
	
	ctxt.createTextPin( "lottery67", true /* META_PROP_STOPWORDS */) ;
	ctxt.search( __LINE__, "lottery67", 1) ;
}

void TestFTBasic::testStopWords()
{
	// Demonstrate how META_PROP_STOPWORDS will remove common
	// words, so they cannot be found by FT search
	FTSearchContext ctxtFilterStop( this, mSession ) ;
	FTSearchContext ctxtWithStop( this, mSession ) ;

	// Based on ftproceng, these are full of stop words
	const char * testStrs[] = { "new the example of yours",   
							  "no thanx", 
							  "newly",
							  "Peter the Great",
							  "Great old Peter",
							  "" } ;

	size_t pos = 0 ;
	while ( strlen( testStrs[pos] ) > 0 )
	{
		ctxtFilterStop.createTextPin( testStrs[pos], true /* META_PROP_STOPWORDS */) ;
		ctxtWithStop.createTextPin( testStrs[pos], false ) ;
		pos++ ;
	}

	ctxtFilterStop.search( __LINE__, "of", 0) ;	ctxtWithStop.search( __LINE__, "of", 1) ;
	
	// Full match
	ctxtFilterStop.search( __LINE__, "newly",1 ) ; ctxtWithStop.search( __LINE__, "newly",1 ) ; 

	// Matches against newly but not new
	ctxtFilterStop.search( __LINE__, "new", 1) ;	ctxtWithStop.search( __LINE__, "new",2 ) ; 
	ctxtFilterStop.search( __LINE__, "ne", 1) ; ctxtWithStop.search( __LINE__, "ne",2 ) ; 

	// Phrase implications (see bug 9732)

	// With stop words filtered "the" from Peter the Great is not indexed
	// Because matching is for any word both "Peter the Great" and "Great old Peter" will match
	ctxtFilterStop.search( __LINE__, "Peter the Great", 2 /*expected number of matches*/) ;

	// Without stop words the "the" can lead to uninteresting matches, for example 
	// this will match with "Even the example of yours"
	ctxtWithStop.search( __LINE__, "Peter the Great",3 ) ;

	// REVIEW: Doing an "exact" match is where trouble occurs.  There are NO matches
	// because "the" is present in the query but not in the index.
	// Perhaps the input phrase should also have stop words stripped?
	ctxtFilterStop.search( __LINE__, "Peter the Great", 0, MODE_ALL_WORDS) ; 

	// Without stop words removed we get the "correct" results
	ctxtWithStop.search( __LINE__, "Peter the Great",1, MODE_ALL_WORDS ) ;

	// Solution is to filter the StopWords even on the input (e.g.
	// as if we only search for Peter Great.  It is inaccurate because
	// it also returns "Great old Peter"
	ctxtFilterStop.search( __LINE__, "Peter the Great", 2, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 

	//This is the extreme case, but not very interesting for real app
	ctxtFilterStop.search( __LINE__, "the", 0 ) ;  ctxtWithStop.search( __LINE__, "the", 2 ) ; 

	ctxtFilterStop.search( __LINE__, "the no of yours", 0) ; ctxtWithStop.search( __LINE__, "the no of yours",3 ) ; 
}

void TestFTBasic::testStopWords2()
{
	// Bug 4460 scenario
	FTSearchContext ctxtFilterStop( this, mSession ) ;
	FTSearchContext ctxtWithStop( this, mSession ) ;

	ctxtFilterStop.createTextPin( "India", true /* META_PROP_STOPWORDS */) ;
	ctxtWithStop.createTextPin( "India", false ) ;

	// "in" is a stop word, but should always match against india
	ctxtFilterStop.search( __LINE__, "in", 1 ) ;				  ctxtWithStop.search( __LINE__, "in", 1 ) ; 
	ctxtFilterStop.search( __LINE__, "in", 1, QFT_FILTER_SW ) ; ctxtWithStop.search( __LINE__, "in", 1, QFT_FILTER_SW ) ; 
	ctxtFilterStop.search( __LINE__, "in", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; ctxtWithStop.search( __LINE__, "in", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 

	// Another scenario mentioned in 4460. Because "-" is a word delimiter this indexes to abc and 123 as separate words
	ctxtFilterStop.createTextPin( "abc-123", true) ; ctxtWithStop.createTextPin( "abc-123", false ) ;
	ctxtFilterStop.search( __LINE__, "abc", 1 ) ;  ctxtWithStop.search( __LINE__, "abc", 1 ) ; 
	ctxtFilterStop.search( __LINE__, "ab", 1 ) ;  ctxtWithStop.search( __LINE__, "ab", 1 ) ; 
	ctxtFilterStop.search( __LINE__, "123", 1 ) ;  ctxtWithStop.search( __LINE__, "123", 1 ) ; 
}

void TestFTBasic::testStopWords3()
{
	//7496 scenario - a problem with stop words filtered on the input phrase
	FTSearchContext ctxtFilterStop( this, mSession ) ;
	FTSearchContext ctxtWithStop( this, mSession ) ;

	// These are ALL stop words
	ctxtFilterStop.createTextPin( "did either about had both who", true /* META_PROP_STOPWORDS */) ;
	ctxtWithStop.createTextPin( "did either about had both who", false ) ;

	// For ctxtFilterStop nothing is in the index, so all substrings will fail.
	// But for ctxtWithStop the searchs should succeed, even though the search strings are stop words
	ctxtFilterStop.search( __LINE__, "did ", 0 ) ;							ctxtWithStop.search( __LINE__, "did ", 1 ) ; 
	ctxtFilterStop.search( __LINE__, "did", 0 ) ;								ctxtWithStop.search( __LINE__, "did", 1 ) ;
	ctxtFilterStop.search( __LINE__, "did either about had both who", 0 ) ;   ctxtWithStop.search( __LINE__, "did either about had both who", 1 ) ;
}

void TestFTBasic::testStopWordsPrefix()
{
	// Bug 14806 scenario
	FTSearchContext ctxtFilterStop( this, mSession ) ;
	FTSearchContext ctxtWithStop( this, mSession ) ;

	ctxtFilterStop.createTextPin( "the nearest town in India", true /* META_PROP_STOPWORDS */) ; // expect "nearest town India" indexed
	ctxtWithStop.createTextPin( "the nearest town in India", false ) ;

	ctxtFilterStop.createTextPin( "in", true /* META_PROP_STOPWORDS */) ; // Only contains stop word, so NOTHING indexed
	ctxtWithStop.createTextPin( "in", false ) ;

	// "in" is a stop word, but should always match against india
	ctxtFilterStop.search( __LINE__, "in", 1 ) ;				  ctxtWithStop.search( __LINE__, "in", 2 ) ; 
	ctxtFilterStop.search( __LINE__, "in", 1, QFT_FILTER_SW ) ; ctxtWithStop.search( __LINE__, "in", 2, QFT_FILTER_SW ) ; 
	ctxtFilterStop.search( __LINE__, "in", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; ctxtWithStop.search( __LINE__, "in", 2, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtFilterStop.search( __LINE__, "in", 1, MODE_ALL_WORDS ) ; ctxtWithStop.search( __LINE__, "in", 2, MODE_ALL_WORDS ) ; 

	// Still matching against "india"
	ctxtFilterStop.search( __LINE__, "in nearest", 1, MODE_ALL_WORDS ) ; ctxtWithStop.search( __LINE__, "in nearest", 1, MODE_ALL_WORDS ) ; 

	// With MODE_ALL_WORDS we can get into trouble because "in" has not been indexed
	ctxtFilterStop.search( __LINE__, "the nearest town in", 0, MODE_ALL_WORDS ) ; 
	ctxtWithStop.search( __LINE__, "the nearest town in", 1, MODE_ALL_WORDS ) ; 

	// But QFT_FILTER_SW helps
	// These 4 are failing
	ctxtFilterStop.search( __LINE__, "the nearest town in", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtWithStop.search( __LINE__, "the nearest town in", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtFilterStop.search( __LINE__, "the nearest town in in", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtWithStop.search( __LINE__, "the nearest town in in", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 

	// More variations to narrow down 14806
	ctxtFilterStop.search( __LINE__, "the nearest town ", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtWithStop.search( __LINE__, "the nearest town ", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtFilterStop.search( __LINE__, "the nearest", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtWithStop.search( __LINE__, "the nearest", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtFilterStop.search( __LINE__, "nearest", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtWithStop.search( __LINE__, "nearest", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 

	// Jumbled words no problem - this is not phrase matching
	ctxtFilterStop.search( __LINE__, "in the nearest town", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; ctxtWithStop.search( __LINE__, "in the nearest town", 1, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 

	// edge-case QFT_FILTER_SW can't help if "the" is not in the index at all
	ctxtFilterStop.search( __LINE__, "the", 0, 0 ) ; ctxtWithStop.search( __LINE__, "the", 1, 0 ) ; 
	ctxtFilterStop.search( __LINE__, "the", 0, QFT_FILTER_SW ) ; ctxtWithStop.search( __LINE__, "the", 1, QFT_FILTER_SW ) ; 
}

void TestFTBasic::testStopWordsPrefix2()
{
	// Another 14806 scenario
	FTSearchContext ctxtFilterStop( this, mSession ) ;

	// these are extensions of the stop words "about", "do", "ever"
	ctxtFilterStop.createTextPin( "aboutimus doit everitus", true /* META_PROP_STOPWORDS */) ; 
	ctxtFilterStop.createTextPin( "doit everitus aboutimus", true /* META_PROP_STOPWORDS */) ; 

	ctxtFilterStop.search( __LINE__, "aboutimus doit everitus", 2, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtFilterStop.search( __LINE__, "aboutimus doit ever", 2, MODE_ALL_WORDS | QFT_FILTER_SW ) ; 
	ctxtFilterStop.search( __LINE__, "aboutimus do ever", 2, MODE_ALL_WORDS | QFT_FILTER_SW ) ;		// "do" would be removed from search, but "ever" for prefix match
	ctxtFilterStop.search( __LINE__, "about do ever", 2, MODE_ALL_WORDS | QFT_FILTER_SW ) ;			// "about", "do" would be removed
	ctxtFilterStop.search( __LINE__, "about do in ever", 2, MODE_ALL_WORDS | QFT_FILTER_SW ) ;		// "about", "do", "in" would be removed
	ctxtFilterStop.search( __LINE__, "about do ever in", 0, MODE_ALL_WORDS | QFT_FILTER_SW ) ;        // all stop words, no prefix match for "in"
	ctxtFilterStop.search( __LINE__, "about aboutimus do doit everitus", 2, MODE_ALL_WORDS | QFT_FILTER_SW ) ;    // "about", "do" would be removed
}

void TestFTBasic::testApostrophe()
{
	/*
	//expected ft contents: (from sdrcl ftstats=1)
		<w str="ain't" cnt="1" recs="1"/>
        <w str="she's" cnt="1" recs="1"/>
        <w str="they'd" cnt="1" recs="1"/>	
	*/

	FTSearchContext ctxtFilterStop( this, mSession ) ;
	FTSearchContext ctxtWithStop( this, mSession ) ;
	FTSearchContext ctxtFilterStop1( this, mSession ) ;
	FTSearchContext ctxtWithStop1( this, mSession ) ;

	// These are stop words with apostrophe
	ctxtFilterStop.createTextPin( "ain't she's they'd", true /* META_PROP_STOPWORDS */) ;
	ctxtWithStop.createTextPin( "ain't she's they'd", false ) ;

	ctxtFilterStop1.createTextPin( "your's flight number was", true /* For Bug # 20568 */) ;
	ctxtWithStop1.createTextPin( "your's flight number was", false ) ;

	ctxtFilterStop.search( __LINE__, "ain't", 0 ) ;					ctxtWithStop.search( __LINE__, "ain't", 1 ) ;
	ctxtFilterStop.search( __LINE__, "she's", 0 ) ;					ctxtWithStop.search( __LINE__, "she's", 1 ) ; 

	ctxtFilterStop1.search( __LINE__, "your's flight number", 1,MODE_ALL_WORDS ) ;
	ctxtFilterStop1.search( __LINE__, "your's flight number was", 0,MODE_ALL_WORDS ) ;
	ctxtWithStop1.search( __LINE__, "your's flight number was", 1,MODE_ALL_WORDS ) ;
}

void TestFTBasic::testEmailIndexing()
{
	// Searching a specific single property in the store
	FTSearchContext ctxt( this, mSession ) ;
	static const bool bFilterStopWords = true ;
	ctxt.createTextPin( "george@pilocation.tv", bFilterStopWords ) ;

	// See how an email address would be tokenized
	ctxt.search( __LINE__, "george", 1) ;
	ctxt.search( __LINE__, "george@pilocation.tv", 1) ;
	ctxt.search( __LINE__, "pilocation.tv", 1) ;
	ctxt.search( __LINE__, "pilocation", 1) ;
	ctxt.search( __LINE__, "location", 0) ;
	ctxt.search( __LINE__, "tv", 1) ;
}

void TestFTBasic::testStemmingBehavior()
{
	// 
	// See #8699
	//
	FTSearchContext ctxt( this, mSession ) ;
	ctxt.createTextPin( "Good portability for everyone", false /* filter stop words */) ;
	ctxt.createTextPin( "He is able but lacking in every ability", false ) ;
	ctxt.createTextPin( "Several good looking goats", false ) ;

	ctxt.search( __LINE__, "goat", 1 ) ;
	ctxt.search( __LINE__, "goats", 1 ) ;

	ctxt.search( __LINE__, "portability", 1 ) ;
	ctxt.search( __LINE__, "ability", 1 ) ;

	ctxt.search( __LINE__, "look", 1 ) ;
	ctxt.search( __LINE__, "looki", 1 ) ;
	ctxt.search( __LINE__, "lookin", 1 ) ;
	ctxt.search( __LINE__, "looking", 1 ) ;
	ctxt.search( __LINE__, "looker", 0 ) ;

	ctxt.search( __LINE__, "several", 1 ) ;
	ctxt.search( __LINE__, "severa", 1 ) ;
	ctxt.search( __LINE__, "every", 2 ) ;
	ctxt.search( __LINE__, "everyo", 1 ) ;
	ctxt.search( __LINE__, "everyone", 1 ) ;

	// 
	// TODO: Multi-word prefix matching
	//
}

void TestFTBasic::testLuisVocab()
{
	FTSearchContext ctxt( this, mSession ) ;

	bool bStopWords = false; 
	ctxt.createTextPin( "are you sure the word list works?\ni can't fuking believe that, after scanning 15K emails, there is not a SINGLE fuck in my inbox...", bStopWords );
	ctxt.createTextPin( "Fix the Fucken bug!", bStopWords );
	ctxt.createTextPin( "Stop fucking around", bStopWords );
	ctxt.createTextPin( "What the FUCK are you talking about?", bStopWords );

	ctxt.search( __LINE__, "fuck", 4 /*expected matches*/) ;
	ctxt.search( __LINE__, "FUCK", 4 ) ;	
}

void TestFTBasic::showHexOfMBStr(const char* inStr)
{
	// of course debugger normally does this
	// but linux is another beast....
	
	mLogger.out() << "Hex version of " << inStr << ":" << endl; 

	for ( int i = 0 ; inStr[i] != 0 ; i++ )
	{
		mLogger.out() << std::hex << (unsigned int)(unsigned char)inStr[i] << " " ;
	}
	mLogger.out() << endl;
}

void TestFTBasic::testFTStats()
{
	// scenario from https://wiki.vmware.com/ow.asp?StoreDoctorFTStats
	//PID file1 = create2PropPin( "One One One One One" /*prop1*/, NULL, false ) ; // Duplicate case also interesting
	PID file1 = create2PropPin( "One" /*prop1*/, NULL, false ) ;
	PID file2 = create2PropPin( "One", NULL, false ) ;
	PID file3 = create2PropPin( NULL, "Two Three", false ) ;
}

void TestFTBasic::testMultiPropSearch()
{
	// Searching a specific single property in the store

	PID file1 = create2PropPin( "Funk Jazz Rock" /*prop1*/, "Classical DeathMetal" /*prop2*/ ) ;
	PID file2 = create2PropPin( "Funk", "Funk" ) ;
	PID file3 = create2PropPin( "Ambient", "Country" ) ;

	TVERIFY( 2 == search2PropPin( "Funk", true /*search prop1*/, false /*search prop2*/) ) ;
	TVERIFY( 1 == search2PropPin( "Funk", false, true ) ) ;
	TVERIFY( 2 == search2PropPin( "Funk", true, true ) ) ;

	TVERIFY( 2 == search2PropPin( "Funk Jazz", true, true ) ) ; // file2 doesn't have Jazz but still matches
	TVERIFY( 1 == search2PropPin( "Funk Jazz", true, true, MODE_ALL_WORDS ) ) ; // only file1 has both words in same property

	// IMPORTANT DETAIL:
	TVERIFY( 1 == search2PropPin( "Funk Classical", true, true, MODE_ALL_WORDS ) ) ; // file1 has both words but in different properties
}

PID TestFTBasic::create2PropPin( const char * inProp1, const char * inProp2, bool bStripStopWords )
{
	Value vls[2] ; int cnt = 0 ;
	if ( inProp1 ) 
	{ 
		vls[cnt].set( inProp1 ) ; 
		vls[cnt].property = mProp1 ;

		// Means that any common english words will be excluded from the index
//		if ( bStripStopWords)
//			vls[cnt].meta = META_PROP_STOPWORDS ;	// TODO: can also cover when not specified
		cnt++ ;
	}
	if ( inProp2 ) 
	{ 
		vls[cnt].set( inProp2 ) ; 
		vls[cnt].property = mProp2 ;
//		if ( bStripStopWords)
//			vls[cnt].meta = META_PROP_STOPWORDS ;
		cnt++ ;
	}

	IPIN *pin;
	mSession->createPIN(vls, cnt, &pin, MODE_PERSISTENT|MODE_COPY_VALUES) ;
	return pin->getPID() ;
}

unsigned long TestFTBasic::search2PropPin( char* inSearch, bool bProp1, bool bProp2, unsigned int flags )
{
	PropertyID props[2] ; int cnt = 0 ;
	if ( bProp1 ) { props[cnt] = mProp1 ; cnt++ ; }
	if ( bProp2 ) { props[cnt] = mProp2 ; cnt++ ; }

	uint64_t cntResults = 0 ;
	IStmt * ftQ = mSession->createStmt() ;
	unsigned char v = ftQ->addVariable() ;

	// NOTE: This isn't a "Full scan" query even though no class is specified
	TVERIFYRC( ftQ->setConditionFT( v, inSearch, 0 /*WARNING THIS IS NOT THE PLACE FOR MODE_ALL_WORDS*/, 
												   props, cnt ) ) ;
	TVERIFYRC( ftQ->count( cntResults, NULL, 0, ~0, flags ) );

	ICursor* lC = NULL;
	TVERIFYRC(ftQ->execute(&lC,NULL,0,~0,0,flags));
	CmvautoPtr<ICursor> res(lC);
	IPIN * pin ;
	unsigned long cntCheck = 0 ;

#if VERBOSE
	mLogger.out() << "Search for \"" << inSearch << "\"" << endl << "----------------------" << endl ;

	while( pin = res->next() )
	{
		if ( bProp1 ) { mLogger.out() << pin->getValue( mProp1 )->str ; }
		if ( bProp2 ) { mLogger.out() << pin->getValue( mProp2 )->str ; }
		mLogger.out() << endl ;
		cntCheck++ ;
		pin->destroy() ;
	}

	mLogger.out() << "------------------" << endl ; 
#else
	while( pin = res->next() )
	{
		cntCheck++ ;
		pin->destroy() ;
	}
#endif
	TVERIFY(cntCheck == cntResults ) ;
	ftQ->destroy() ;
	return (unsigned long)cntResults ;
}
