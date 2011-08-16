/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"
#include <iomanip>

#ifdef WIN32
#include <Psapi.h>
#pragma comment (lib, "Psapi.lib" )
#endif

using namespace std;

// Test of Mvstore performance when dealing with large doc data.
//
// Work in progress!
//
// Current syntax:

// tests testdocload g 1000 -nbuf=3000   // Add 1000 pins (you should erase your store file first)
// tests testdocload gq 10000 -nbuf=5000 // Add 10000 pins and then run query test (you should erase your store file first)
// tests testdocload q 0 // Run the queries in the existing store
// tests testdocload d 0 // Delete all document pins
// 
// tests testdocloadsql g 1000 // Add 1000 records to MySQL
// etc.

//#define MYSQL_COMPARE // Enable if you have MySQL installed 

// MVStore and MySQL flags
//#define NO_FTINDEX // For comparison (queries obviously won't work)
//#define DELAYED_INDEX // Build the index after the data is added (recommended approach for MySQL)
#define CNT_QUERY 150 // Number of querys to perform, should be a multiple of query words in g_QueryVals


// MVSTORE ONLY
//#define NBUFFERS 3000  // USE -nbuf argument to specify now 
#define	PAGESIZE 0x8000 // WARNING: As set in app.cpp, not currently set by this test
#define CNT_PIN_COMMIT_SIZE 1000 // Number of PINs to commit in each batch.  Set to 1 to use straightfoward ISession::createPIN()
#define PROFILE_IO 0

// Strings from the source text.
// Each word is also injected into at least one record so all queries should return
// at least one item.
// (Tip: you can try "sea" if the MySQL database is set to index short words, see below for comments)
const char * g_QueryVals[] = { "flash", "house", "father", "daylight", "river", 
							   "plead", "friend", "food", "monkey", "bridge", 
							   "patriotic", "considerable", "acknowledgment", "beach", "white" } ;

static double GetMemoryUsage( bool inPeak = false )
{
#ifdef WIN32
	// REVIEW: This information is not valid if running in IPC mode.
	// We would need to look at the mvstore.exe memory usage rather than
	// test usage.
	PROCESS_MEMORY_COUNTERS meminfo ;
	meminfo.cb = sizeof( PROCESS_MEMORY_COUNTERS ) ;
	::GetProcessMemoryInfo(GetCurrentProcess(),&meminfo, sizeof( PROCESS_MEMORY_COUNTERS )) ;
	if ( inPeak )
		return meminfo.WorkingSetSize / 1048576.0 ; 
	else
		return meminfo.PeakWorkingSetSize / 1048576.0 ; 
#else
	// TODO - find linux equivalent
	return 1 ;
#endif
}

class TextGenerator
{
	// Class to read english text and make it available as random content
	// it doesn't talk to the store itself
public:
	TextGenerator()
		: mAddedQueryTerms(false)
	{
		// TODO: make this relative or an argument

		// This file came from internet and is Charles Dickon's Great Expections 
		// (1 meg of english text, no copywrite).
		// Another possible source for good text would be any newsgroup content, but
		// that would need more preprocessing to turn into meaningful text.

		#if TEST_ADAPTED_FOR_SEARCHTEST01_PROTO
			mSourceFile = "W:\\developer\\maxwind\\searchtest\\dataandrew\\great_expectations.txt" ;
		#else
			#ifdef WIN32
				// Assuming tests.exe running within Debug or Release directory
				mSourceFile = "../textdata/great_expectations.txt" ;
			#else
				// Assuming running directly in tests directory
				mSourceFile = "textdata/great_expectations.txt" ;
			#endif
		#endif
	}

	bool ReadParagraphs()
	{
		// Read source file into paragraphs

		// This code is dependent on the specific file format
		// of the test file, which has line breaks at fixed line width,
		// so it is the presence of a empty line that signifies the
		// end of a paragraph.
		// There are about 4000 of them, up to 2500 characters each

		FILE * lfSrc = fopen( mSourceFile.c_str(), "r" ) ;
		if ( !lfSrc ) 
		{
			std::cerr << "Cannot open text source file: " << mSourceFile.c_str() << endl ;
			return false;
		}

		char buf[128] ;

		std::string strParagraph ;


		size_t cntMaxLen = 0 ;

		// Todo - port to linux 
		while( fgets( buf, 128, lfSrc ) != NULL )
		{
			size_t len = strlen( buf ) ;
			buf[len-1] = ' ' ; // Remove \n

			//??
			//if ( len >= 2 && buf[len-2] == "\r" ) buf[len-2] = "\0" ;

			if ( len == 1 ) 
			{
				if ( cntMaxLen < strParagraph.size() )
				{
					cntMaxLen = strParagraph.size() ;
				}

				mSourceText.push_back( strParagraph ) ;
				strParagraph.erase() ;
			}
			else
			{
				strParagraph += buf ;
			}
		}

		fclose( lfSrc ) ; lfSrc = NULL ;
		return true ;
	}

	void GenerateFakeEmail( std::string & outMsg )
	{
		if ( !mAddedQueryTerms )
		{
			// Special case first first record for test purposes
			mAddedQueryTerms = true ;
			size_t cntItems = sizeof(g_QueryVals)/sizeof(g_QueryVals[0]) ;
			outMsg = "These words will be part of future queries: " ;
			for ( size_t t = 0 ; t < cntItems ; t++ )
			{
				outMsg += g_QueryVals[t] ;
				outMsg += ", " ;
			}
			outMsg += "so they are included in some of the text" ;
			return ;
		}

		// total number of potential paragraphs
		unsigned int cntParagraphs = (unsigned int)mSourceText.size() ; 

		outMsg.erase() ;

		// Basic simulation of mail threads, where
		// the message body of previous messages are included
		// over and over again
		// Assume that 80% of emails are responding to thread
		bool lIsThread = ( MVTRand::getRange( 1, 100 ) < 80 ) ;

		if ( lIsThread )
		{
			// Append paragraph to last message
			unsigned int paragraph = MVTRand::getRange( 0, cntParagraphs - 1 ) ;
			outMsg = mPreviousMsg + mSourceText[paragraph] + "\n\n" ;
		}
		else
		{
			// Start new message

			// Pick a jumble of up to 10 paragraphs for the email
			unsigned int lcntParagaphsInEmail = MVTRand::getRange( 1, 10 ) ;
			for ( size_t k = 0 ; k < lcntParagaphsInEmail ; k++ )
			{
				unsigned int paragraph = MVTRand::getRange( 0, cntParagraphs - 1 ) ;
				outMsg += mSourceText[paragraph] + "\n\n" ;
			}
		}

		mPreviousMsg = outMsg ;
	}

	std::string mSourceFile ;
	std::string mPreviousMsg ; // For email thread generation
	std::vector<std::string> mSourceText ; // Large amounts of text
	bool mAddedQueryTerms ;
} ;

class BatchEmailPINHelper
{
	// Convenient helper to create PINs in large batchs
	// Currently only supports including a single string property on each PIN

public:
	BatchEmailPINHelper(ISession* inSession, PropertyID inPropID, unsigned int inBatchSize, ITest* inTest, uint8_t inMeta)
	{
		mPos = 0 ;
		mSession = inSession ;
		mMsgs = (char**) malloc( sizeof(char*) * inBatchSize ) ;
		mPropID = inPropID ;
		mTtlTime = 0 ;
		mBatchSize = inBatchSize ;
		mTest = inTest ;
		mMeta = inMeta ;

		// Header for the detailed time results
		mTest->getLogger().out() << "Commit Time/pin (ms),Working set (MB), %Pool" << endl ;
	}

	~BatchEmailPINHelper()
	{
		// Assume no partially completed batch is left,
		// otherwise we need to add code to commit a partial batch
		assert( mPos == 0 ) ;
		free( mMsgs ) ; mMsgs = NULL ;
	}

	void AddMsg(const char * inMsg)
	{
		// REVIEW: the times to allocate in the store is not currently
		// timed

		// Copy string text into store memory
		mMsgs[mPos] = (char*) mSession->alloc( strlen(inMsg) + 1 ) ;  // Will be destroy later by IPin->destroy() call
		memcpy( mMsgs[mPos], inMsg, strlen(inMsg) + 1 ) ;

		if ( mPos == int( mBatchSize - 1 )  )
		{
			CommitBatch() ;
			mPos = 0 ;
		}
		else
		{
			mPos++ ;
		}
	}

	void CommitBatch()
	{
#if PROFILE_IO
		//Interface to change for IStoreIO
		MVTApp::sReporter.IOProfile_reset() ;
#endif
		long lStart = getTimeInMs() ;
	
		// Time to commit
		IPIN** newPINs = (IPIN**) malloc(mBatchSize * sizeof(IPIN*)) ;
		size_t i  ;
		for ( i = 0 ; i < mBatchSize ; i++ )
		{	
			Value * mMsgWrapper = (Value*) mSession->alloc( sizeof( Value ) ) ;
			mMsgWrapper->set( mMsgs[i] ) ; mMsgWrapper->property = mPropID ;		
			mMsgWrapper->meta = mMeta ; 

			newPINs[i] = mSession->createUncommittedPIN( mMsgWrapper,1 ) ; 
		}

		unsigned int mode = 0 ; /*MODE_SYNC_FTINDEX doesn't currently make a difference*/
		if (RC_OK != mSession->commitPINs( newPINs, mBatchSize, mode)) { assert(false); }

		for ( i = 0 ; i < mBatchSize ; i++ )
		{
			newPINs[i]->destroy() ;   // This also frees the string and Value that we have allocated
		}
		free( newPINs ) ;

		long lEndTime = getTimeInMs() ;
		long lCommitTime = lEndTime - lStart ;

		if ( lCommitTime < 0 )
		{
			// REVIEW: Used to happen during long tests.  I think it is fixed but leave check because
			// overall avg result is invalid if this happens
			mTest->getLogger().out() << "\t\t\tWARNING BOGUS TIMING VALUE" << endl ; 
		}

		// Show per-pin avg commit time
		mTest->getLogger().out() << std::setprecision(3) << fixed << 1.0 * lCommitTime / mBatchSize ;
#ifdef WIN32
		PROCESS_MEMORY_COUNTERS meminfo ;
		meminfo.cb = sizeof( PROCESS_MEMORY_COUNTERS ) ;
		::GetProcessMemoryInfo(GetCurrentProcess(),&meminfo, sizeof( PROCESS_MEMORY_COUNTERS )) ;

		double lRamInMegs = ::GetMemoryUsage(false) ; 
		mTest->getLogger().out() << "\t" << std::setprecision(1) << fixed <<  lRamInMegs 
								 << "\t" << 100.0*1048576.0*lRamInMegs/(PAGESIZE*MVTApp::getNBuffers()) ;
#endif
		mTest->getLogger().out() << endl ; 

#if PROFILE_IO
		// INTERFACE TO CHANGE
		MVTApp::sReporter.IOProfile_report( mTest->getLogger().out() ) ;
#endif

		if ( lCommitTime > 0 )
		{
			mTtlTime += lCommitTime ;
		}
	}

	long getTtlTime() const { return mTtlTime ; }

private:
	char ** mMsgs ;				// Array pointing to strings in Store memory as they are accumulated before commit
	int mPos ;					// Track accumulation of PIN data for the batch 
	ISession * mSession ;
	PropertyID mPropID ;		// Property to create on each new PIN
	long mTtlTime ;				// Counting time spent inside the store API calls
	unsigned int mBatchSize ;	// Number of pins per commit
	ITest * mTest ;				// Parent test object
	uint8_t mMeta ;
} ;

class TestDocLoadBase : public ITest
{
	// Base class for shared code between Mvstore and MySQL implementations
		
		int execute();
		virtual char const * getDescription() const { return "load test for pidoc style data.  Args opts - (g)enerate,(q)uery,(m)odify, cntPins (for generate, e.g. 10000)"; }
	protected:
		virtual void doTest(bool inQuery,bool inDelete) = 0 ;
		void doQuery() ;
		virtual void deleteAll() = 0 ;
		virtual void generateData() = 0 ;
		virtual unsigned long ftQueryWord( const char * inStr ) = 0 ;

		unsigned int mCntRecords ; // Number of pins or database rows to generate
};

int TestDocLoadBase::execute()
{
	string lopts; bool pparsing(true);
	
	if(!mpArgs->get_param("opts",lopts)){
		mLogger.out() << "Problem with --opts parameter initialization!" << endl;
		pparsing = false;
	}else
	{
	  cout << "Got opts=" << lopts << endl;
	}
	
	if(!mpArgs->get_param("cntpins",mCntRecords)){
		mLogger.out() << "Problem with --cntpins parameter initialization!" << endl;
		pparsing = false;
	}else
	{
	  cout << "Got cntpins=" << mCntRecords << endl;
	}
	
	if(!pparsing){
	  mLogger.out() << "Parameter initialization problems! " << endl; 
	  mLogger.out() << "Expects:  ./tests testdocload --opts={g}{q}{m} --cntpins={int}  " << endl; 
			
	  return 1;
	}
	
	if ( "g" == lopts)
	{
		if ( mCntRecords == 0 )
		{
			mLogger.out() << "Invalid arg  {--cntpins= } number of pins, try 10000" << endl ; return RC_FALSE ; 
		}
	}
	else
	{
		mCntRecords = 0 ;
	}

	bool lDoQuery = ( lopts == "q" );
	bool lDelete  = ( lopts == "d" );
	doTest( lDoQuery, lDelete ) ;

	return RC_OK  ;
}

void TestDocLoadBase::doQuery()
{
	mLogger.out() << endl << "Starting FT Queries..." << endl ;

	size_t cntItems = sizeof(g_QueryVals)/sizeof(g_QueryVals[0]) ;
	size_t lWordIndex = 0 ;
	size_t lCntTotalMatches = 0 ;

	vector<unsigned long> lWordMatches(cntItems) ;
	size_t k ;
	for ( k = 0 ; k < cntItems ; k++ ) lWordMatches[k] = 0 ;

	long lStartTime = getTimeInMs() ;

	for ( size_t t = 0 ; t < CNT_QUERY ; t++ )
	{
		unsigned long lCntMatches = ftQueryWord( g_QueryVals[lWordIndex] );

		// We ensured that each query string was added to at least one of the records
		// so we expect at least one match
		TVERIFY2( lCntMatches > 0, g_QueryVals[lWordIndex] ) ;
		lWordMatches[lWordIndex] = lWordMatches[lWordIndex] + lCntMatches ;
		lCntTotalMatches += lCntMatches ;

		// We have fewer search words than total number of queries we perform
		lWordIndex++ ; if ( lWordIndex == cntItems ) lWordIndex = 0 ;
	}
	long lEndTime = getTimeInMs() ;

	mLogger.out() << "Execution of " << CNT_QUERY << " queries found " << (unsigned int) lCntTotalMatches << " records" << endl ;
	mLogger.out() << "Execution time: " << lEndTime - lStartTime << " ms.  Avg: " << ((double)(lEndTime - lStartTime))/CNT_QUERY<< endl ;

	for (k=0; k< cntItems ;k++ )
	{
		mLogger.out() << "\t" << g_QueryVals[k] << " : " << lWordMatches[k] << " matches" << endl ;
	}
}

class TestDocLoad : public TestDocLoadBase
{
	// Mvstore Doc load test
	public:
		TEST_DECLARE(TestDocLoad);
		virtual char const * getName() const { return "testdocload"; }
		virtual char const * getHelp() const { return ""; }
		
		virtual void destroy() { delete this; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Performance test that requires input arguments"; return false; }
	protected:
		void generateData() ;
		virtual unsigned long ftQueryWord( const char * inStr ) ;
		void doTest(bool inQuery,bool inDelete) ;
		void deleteAll() ;
		uint8_t getMetaFlags();
		void indexExistingStringProp( PropertyID inProp );
		unsigned int mBatchSize ; // How many pins per commit
		ISession * mSession ;
};
TEST_IMPLEMENT(TestDocLoad, TestLogger::kDStdOut);

void TestDocLoad::doTest(bool inQuery,bool inDelete)
{
	mBatchSize = CNT_PIN_COMMIT_SIZE ;		

	if (MVTApp::startStore())
	{
		// Regular test, suitable for smoke test
		mSession = MVTApp::startSession();

		if ( mCntRecords > 0 )
		{
			generateData() ;
		}

		if ( inQuery )
		{
#ifndef NO_FTINDEX
			doQuery() ;
#else
			mLogger.out() << "No index generated - cannot query" << endl ;
#endif
		}

		if ( inDelete )
		{
			deleteAll() ;
		}

		// Record maximum RAM usage
		mLogger.out() << "Maximum Working Set (MB): " << std::setprecision(1) << fixed << ::GetMemoryUsage(true) 
			<< "\tStatic Pool Size (MB): " << (PAGESIZE*MVTApp::getNBuffers())/1048576.0 << endl;

		mSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
}

uint8_t TestDocLoad::getMetaFlags()
{
#if defined NO_FTINDEX
	return META_PROP_NOFTINDEX ; // To turn off FT indexing for this property
#elif defined DELAYED_INDEX
	return META_PROP_NOFTINDEX ; 
#else
	return META_PROP_STOPWORDS ; // Make sure stop words aren't indexed
#endif
}

void TestDocLoad::generateData()
{
	PropertyID emailBody = MVTApp::getProp( mSession,"TestDocLoad.emailbody" ) ;

	TextGenerator tg ;
	TVERIFY(tg.ReadParagraphs()) ;

	BatchEmailPINHelper lBatchHelp( mSession, emailBody, mBatchSize, this, getMetaFlags() ) ;

	unsigned int cntTtlSize = 0 ;
	unsigned int cntMaxSize = 0 ;

	std::string lMsgBody ;

	for ( size_t i = 0 ; i < mCntRecords ; i++ )
	{
		tg.GenerateFakeEmail( lMsgBody ) ;

		if ( cntMaxSize < lMsgBody.size() ) 
			cntMaxSize = (unsigned int)lMsgBody.size() ;
		cntTtlSize +=(unsigned int) lMsgBody.size() ;

		#if TEST_ADAPTED_FOR_SEARCHTEST01_PROTO
			ofstream lOs("W:\\developer\\maxwind\\searchtest\\dataandrew\\gemod.txt", std::ios::app);
			lOs << "@@newpin@@" << std::endl;
			lOs << lMsgBody.c_str() << std::endl;
		#else
			if ( mBatchSize == 1)
			{
				Value msg ; msg.set( lMsgBody.c_str() ) ; msg.property = emailBody ;
				msg.meta = getMetaFlags() ;
				PID newpid ;
				TVERIFYRC(mSession->createPIN( newpid, &msg, 1 )) ;
			}
			else
			{
				lBatchHelp.AddMsg( lMsgBody.c_str() ) ;
			}
		#endif
	}

	mLogger.out() << "Generated " << mCntRecords 
					<< " PINs.  Email body total: " << (int)cntTtlSize 
					<< ", average: " << cntTtlSize/mCntRecords 
					<< ", max: " << cntMaxSize << endl ;

	if ( mBatchSize != 1)
	{
		mLogger.out() << "Time to commit pins: " << lBatchHelp.getTtlTime() 
					<< " Average " << (double)lBatchHelp.getTtlTime()/mCntRecords << endl ; 
	}

#ifdef DELAYED_INDEX
	indexExistingStringProp( emailBody ) ;
#endif
}

void TestDocLoad::deleteAll()
{
	PropertyID emailBody = MVTApp::getProp( mSession,"TestDocLoad.emailbody" ) ;

	CmvautoPtr<IStmt> lQ(mSession->createStmt()) ;
	TVERIFYRC(lQ->setPropCondition(lQ->addVariable(),&emailBody,1)) ;

	static const int lBatchSize = 100 ;
	int lPos = 0 ;
	IPIN* lDeletePins[lBatchSize] ;

	ICursor* lC = NULL;
	TVERIFYRC(lQ->execute(&lC));
	CmvautoPtr<ICursor> lR(lC);

	unsigned int lMode = MODE_PURGE ;

	while(true)
	{		
		lDeletePins[lPos] = lR->next() ;

		if ( lDeletePins[lPos] == NULL )
		{
			// Remaining items
			if ( lPos > 0 )
			{
				TVERIFYRC(mSession->deletePINs( lDeletePins, lPos, lMode));
			}
			break ;
		}

		lPos++ ;

		if ( lPos == lBatchSize )
		{
			TVERIFYRC(mSession->deletePINs( lDeletePins, lPos, lMode ) );
			lPos = 0 ;
		}
	}

	// Just to test algorithm above
	uint64_t cnt = 0 ;
	TVERIFYRC(lQ->count( cnt )) ;
	TVERIFY( cnt == 0 ) ;
}

void TestDocLoad::indexExistingStringProp( PropertyID inProp )
{
	// Pins were added quickly with indexing turned off for the property
	// Now as a second step mark the strings for index.
	// This could be a valid simulation for real application use - import all the
	// raw data as fast as possible, then extract the text and index it as a longer background process

	// Real code would probably create a new property with just the
	// searchable text content, based on a original property that included
	// all the email headers and other data.
	// But in this simple test we reuse the existing string
	// and change its indexing settings (to avoid doubling the store size)

	mLogger.out() << endl << "Second pass... enabling index" << endl ;

	long lStart = getTimeInMs() ;

	IStmt * lQ = mSession->createStmt() ;
	lQ->setPropCondition(lQ->addVariable(),&inProp,1) ;
	ICursor * lR = NULL;
	TVERIFYRC(lQ->execute(&lR));

	IPIN* pin = NULL ;
	while( NULL != ( pin = lR->next() ) )
	{
		const Value * emailBodyText = pin->getValue( inProp ) ;

		// All existing pins should have this flag set, unless test has been
		// run with an existing store
		if ( 0 != (emailBodyText->meta & META_PROP_NOFTINDEX) )
		{
			// REVIEW: Unless there is some clever trick to change
			// the flag, I found that it is necessary to make a copy of the string,
			// remove the property, then reapply with correct flag.  Otherwise
			// the store remembers that the property was not indexed.  (To see this
			// comment out the calls to DeleteOp)
			Value updatedEmailBodyText ;

			std::string strCopy = emailBodyText->str ; 
			updatedEmailBodyText.set(&(strCopy[0])) ;

			updatedEmailBodyText.meta = META_PROP_STOPWORDS ;
			updatedEmailBodyText.property = inProp ; 

			Value lDeleteOp ; lDeleteOp.setDelete(inProp) ;
			TVERIFYRC( pin->modify( &lDeleteOp, 1 ) );

			// Reapply
			TVERIFYRC( pin->modify( &updatedEmailBodyText, 1 ) );
		}
		pin->destroy() ;
	}

	lR->destroy() ;
	lQ->destroy() ;
	
	long lEnd = getTimeInMs() ;
	mLogger.out() << "Additional time " << lEnd-lStart << " (avg per pin:" << (1.0*(lEnd-lStart))/mCntRecords << ")" << endl ;
}

unsigned long TestDocLoad::ftQueryWord( const char * inStr )
{
	PropertyID emailBody = MVTApp::getProp( mSession,"TestDocLoad.emailbody" ) ;

	IStmt* lQ = mSession->createStmt() ;
	TVERIFYRC(lQ->setConditionFT( lQ->addVariable(), inStr, 0, &emailBody, 1 )) ;

	uint64_t cntResults = 0;

#if 0
	lQ->count( cntResults ) ;
#else
	// Actual retrieve the PINs rather than just the count to be more realistic
	// However for some words this may retrieve huge % of all the PINs
	ICursor * lR = NULL;
	TVERIFYRC(lQ->execute(&lR));

	/*int returncnt = 10 ;*/

	for ( IPIN* lP = NULL ; /*returncnt>0 &&*/ NULL != ( lP = lR->next() ) ; )
	{
		/*const char * pMsg = lP->getValue( emailBody )->str ;*/
		lP->destroy() ;
		cntResults++ ; 
		/*returncnt-- ; */
	}

	lR->destroy() ;
#endif
	lQ->destroy() ;

	return (unsigned long)cntResults ;
}

#ifdef MYSQL_COMPARE
struct st_mysql ;
class TestDocLoadSQL : public TestDocLoadBase
{
	// MySQL equivalent to TestDocLoad
	public:
		TEST_DECLARE(TestDocLoadSQL);
		virtual char const * getName() const { return "TestDocLoadSQL"; }
		virtual char const * getHelp() const { return ""; }
		virtual void destroy() { delete this; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Performance test that requires input arguments"; return false; }
	protected:
		void doTest(bool inQuery,bool inDelete) ;
		void generateData() ;
		unsigned long ftQueryWord( const char * inStr ) ;
		void deleteAll() { assert(!"Not implemented") ;} 

		st_mysql * mSQLState ; /* MYSQL*/
		char mQBuffer[0x10000];
};
TEST_IMPLEMENT(TestDocLoadSQL, TestLogger::kDStdOut);

// Change this lib path if it is not correct for your installation of MySQL
#pragma comment (lib, "C:\\Progra~1\\MySQL\\MySQL Server 5.0\\lib\\opt\\libmysql.lib")
#include <winsock.h>
#include <my_global.h>
#define HAVE_STRTOULL // To avoid warning in these nasty mysql headers
#include <m_string.h> // strmov
#include <mysql.h>
#undef bool // SQL header files try to set bool=BOOL

// Macro to run SQL query and report error message returned
#define TVQUERY( SqlState, QString ) TVERIFY2( 0==::mysql_query((SqlState), (QString)), (const char*)(SqlState)->net.buff )

void TestDocLoadSQL::doTest(bool inQuery,bool inDelete)
{
	if ((mSQLState = ::mysql_init((MYSQL*)0)) && 
		::mysql_real_connect(mSQLState, NULL, NULL, NULL, NULL, MYSQL_PORT, NULL, 0))
	{
		if (::mysql_select_db(mSQLState, "test") < 0)
		{
			TVERIFY2(0,"Can't select the database!") ;
			::mysql_close(mSQLState);
			return ;
		}
	}
	
	if ( mCntRecords > 0 )
	{
		generateData() ;
	}

	if ( inQuery )
	{
		doQuery() ;
	}

	::mysql_close(mSQLState);	
}

void TestDocLoadSQL::generateData() 
{
	TextGenerator tg ;
	tg.ReadParagraphs() ;

	unsigned int cntTtlSize = 0 ;
	unsigned int cntMaxSize = 0 ;

	mLogger.out() << "mysql: preparing table" << std::endl;
	strcpy(mQBuffer, "DROP TABLE IF EXISTS docload;");
	TVQUERY( mSQLState, mQBuffer ) ;

#ifdef NO_FTINDEX
	// Syntax with no Full text index - InnoDB provides transaction support
	strcpy(mQBuffer, "CREATE TABLE docload("	
		"emailbody TEXT)  ENGINE = InnoDB;") ;
#else
	#ifdef DELAYED_INDEX
		strcpy(mQBuffer, "CREATE TABLE docload("	
			"emailbody TEXT)  ENGINE = MYISAM;") ;
	#else
		// Syntax with index
		// Notice that MyISAM needs to be specified 
		strcpy(mQBuffer, "CREATE TABLE docload("	
			"emailbody TEXT,"
			"FULLTEXT (emailbody)) ENGINE = MYISAM;") ;
	#endif
#endif

	TVQUERY( mSQLState, mQBuffer ) ;

	std::string lMsgBody ;

	// For the moment timing entire operation, although only part of this time is inside SQL
	long lStart = getTimeInMs() ;

	for ( size_t i = 0 ; i < mCntRecords ; i++ )
	{
		tg.GenerateFakeEmail( lMsgBody ) ;

		// Escaped string in theory could be twice as long as src
		assert( lMsgBody.size() < ( 0x10000 / 2 - 100 ) ) ;

		char * end = ::strmov( mQBuffer, "INSERT INTO docload (emailbody) VALUES ('" ) ;

		// Characters like ' need to be cleaned up in order to be put in an sql statement
		end += ::mysql_real_escape_string(mSQLState, end, lMsgBody.c_str(), (unsigned long)lMsgBody.size()) ;

		end = strmov( end, "');" ) ;
		
		TVQUERY( mSQLState, mQBuffer ) ;

		if ( cntMaxSize < lMsgBody.size() ) 
			cntMaxSize = (unsigned int)lMsgBody.size() ;
		cntTtlSize +=(unsigned int) lMsgBody.size() ;

	}

#ifdef DELAYED_INDEX
#ifndef NO_FTINDEX
	// MySQL documentation recommends building index after data import
	mLogger.out() << "Data import took " << getTimeInMs() - lStart << endl ;
	mLogger.out() << "Now adding index..." << endl ;

	strcpy(mQBuffer, "CREATE FULLTEXT INDEX bodyindex ON docload(emailbody) ;" ) ;
	TVQUERY( mSQLState, mQBuffer ) ;
#endif
#endif

	long lEndTime = getTimeInMs() ;

	mLogger.out() << "Generated " << mCntRecords 
					<< " rows.  Email body total: " << (int)cntTtlSize 
					<< ", average: " << cntTtlSize/mCntRecords 
					<< ", max: " << cntMaxSize << endl ;
	mLogger.out() << "Time to commit : " << lEndTime - lStart
				<< " Average " << (double)(lEndTime - lStart)/mCntRecords << endl ; 
}


unsigned long TestDocLoadSQL::ftQueryWord( const char * inStr )
{
	/*
	TIP: I tried to search for "day" but hit this MySQL feature.
	"MySQL does not index any words less than or equal to 3 characters in length, 
	nor does it index any words that appear in more than 50% of the rows. 
	This means that if your table contains 2 or less rows, a search on a FULLTEXT index will never return anything."
	This also implies that you may get failures if trying really small datasets or extremely common words

	To change the minimum word size from default of 4 add this line: 
		ft_min_word_len=3
	in the 	[mysqld] section of the my.ini file and restart the service.  
	
	The 50% limit can also be configured.
	*/

#if 0
	// This full text search does relevancy sorting and will also prune the results
	// from paragraphs that contain many more important words
	sprintf(mQBuffer, 
			"SELECT emailbody FROM docload WHERE MATCH (emailbody) AGAINST ('%s');", 
			 inStr);
#else
	// Raw, unsorted query more similar to the mvstore FT query
	sprintf(mQBuffer, 
			"SELECT emailbody FROM docload WHERE MATCH (emailbody) AGAINST ('+%s' IN BOOLEAN MODE);", 
			 inStr);
#endif

	TVQUERY( mSQLState, mQBuffer ) ;
	
	long lCnt = 0 ;

	MYSQL_RES * lRes = ::mysql_store_result(mSQLState);
	my_ulonglong cntMatches = mysql_num_rows(lRes) ;


	if (isVerbose())
	{
		mLogger.out() << "Query matches for " <<  inStr << " : " << cntMatches << endl ;

		// It would be too verbose to print every record			
		MYSQL_ROW lRow = ::mysql_fetch_row(lRes) ;
		const char * msg = lRow[0] ;
		mLogger.out() << "\n\n-----------------FIRST QUERY MATCH (" <<  inStr ;
		mLogger.out() << ")-------------\n" << msg << endl ; 
	}

	::mysql_free_result(lRes);

	return (unsigned long) cntMatches ;
}

#endif

class TestDocLoadText : public TestDocLoadBase
{
	// Generate text files with message content rather than use mvstore 
	public:
		TEST_DECLARE(TestDocLoadText);
		virtual char const * getName() const { return "testdocloadtext"; }
		virtual char const * getHelp() const { return ""; }
		virtual void destroy() { delete this; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Performance test that requires input arguments"; return false; }
	protected:
		void doTest(bool inQuery,bool inDelete) ;
		void generateData() ;
		void deleteAll() { assert(!"Not implemented") ;} 
		unsigned long ftQueryWord( const char * inStr ) ;
};
TEST_IMPLEMENT(TestDocLoadText, TestLogger::kDStdOut);

void TestDocLoadText::doTest(bool inQuery,bool inDelete)
{
	if ( mCntRecords > 0 )
	{
		generateData() ;
	}

	if ( inQuery )
	{
		doQuery() ;
	}
}

void TestDocLoadText::generateData() 
{
	TextGenerator tg ;
	tg.ReadParagraphs() ;

	unsigned int cntTtlSize = 0 ;
	unsigned int cntMaxSize = 0 ;

	//TODO: ERASE EXISTING DATA HERE

#ifdef WIN32
    // TODO take argument 
	std::string lMsgPath = ".\\" ; 
#else
	std::string lMsgPath = "./" ; 
#endif    

	std::string lMsgBody ;

	// For the moment timing entire operation, although only part of this time is inside SQL
	long lStart = getTimeInMs() ;

	for ( unsigned int i = 0 ; i < mCntRecords ; i++ )
	{
		tg.GenerateFakeEmail( lMsgBody ) ;

		char filename[128] ;
		sprintf( filename, "%smsg%u.txt", lMsgPath.c_str(), i ) ;
		FILE * lFile = fopen( filename, "w" ) ;
		if ( lFile )
		{
			fwrite( lMsgBody.c_str(), lMsgBody.size(), 1, lFile ) ;
			fclose( lFile ) ;
		}
		else
		{
			mLogger.out() << "Could not write to " << filename << endl ;
		}
			
		if ( cntMaxSize < lMsgBody.size() ) 
			cntMaxSize = (unsigned int)lMsgBody.size() ;
		cntTtlSize +=(unsigned int) lMsgBody.size() ;

	}

	long lEndTime = getTimeInMs() ;

	mLogger.out() << "Generated " << mCntRecords 
					<< " files.  Email body total: " << (int)cntTtlSize 
					<< ", average: " << cntTtlSize/mCntRecords 
					<< ", max: " << cntMaxSize << endl ;
	mLogger.out() << "Time to commit : " << lEndTime - lStart
				<< " Average " << (double)(lEndTime - lStart)/mCntRecords << endl ; 
}

unsigned long TestDocLoadText::ftQueryWord( const char * inStr )
{
	// This could, in theory, scan through the files which would
	// be very slow and not interesting.
	return (unsigned long) 0 ;
}
