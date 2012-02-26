/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "serialization.h"
#include "mvauto.h"
#include <fstream>
using namespace std;

// Test full text searching of long stream-based strings that contain some known
// random word.  Includes coverage of collections of these streams

// Warning: this depends on random strings so there is a small chance
// that it will generate the same word more than once and report a bogus failure

// TODO: Also try HUGE Collection 256+ elements

#define STRESS 0 // Really bash on the feature by repeating core test many times from many threads
			     // (which will bloat the store and really take a long time)

#define STRESS_COUNT 10
#define STRESS_THREAD_COUNT 10

// Publish this test.
class TestFTStreams : public ITest
{
		AfyKernel::StoreCtx *mStoreCtx;
	public:
		TEST_DECLARE(TestFTStreams);
		virtual char const * getName() const { return "testftstreams"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test for FT on streams"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testFTStreams(ISession *session);
		static THREAD_SIGNATURE testFTStreamsThread(void * pTI);
		void testFTStreamsInThreadImpl();
		void testExpectedFT(ISession *session, PropertyID prop, const char *keyword, const char* msg) ; 
		int getFTMatches(ISession *session, PropertyID prop, const char *keyword, vector<PID>& outMatches);
		void purgeTestPins( ISession* session, PropertyID prop ) ;
};
class testStream : public AfyDB::IStream
{
	// testStream builds random streams of characters with a specific 
	// search string embedded somewhere in the characters.  The challenge is for the FT search to find the
	// search string

	protected:
		size_t const mLength;
		ValueType const mVT;
		char const mStartChar;
		size_t mSeek;
		Tstring srchString;
		size_t rndm;
	public:
		testStream(size_t pLength, char pStartChar = 'a', ValueType pVT = VT_STRING
			,Tstring strSrch = "Search String",size_t rndnum = 0 /*offset*/) : mLength(pLength), mVT(pVT)
			, mStartChar(pStartChar), mSeek(0),srchString(strSrch),rndm(rndnum){}
		virtual ValueType dataType() const { return mVT; }
		virtual	uint64_t length() const { return 0; }
		virtual size_t read(void * buf, size_t maxLength) 
		{
			size_t const lLength = MvStoreSerialization::PrimitivesOutDbg::mymin(mLength - mSeek, maxLength); 

			if ( lLength == 0 ) return 0 ;

			for (size_t i = 0; i < lLength; i++) 
				((char*)buf)[i] = getCharAt(mSeek + i, mStartChar);

			// Algorithm below assumes read is only called once (it could be fixed
			// if this is found not to be the case)

			if ( rndm < maxLength ) 
			{
				//insert my search string at the offset rndm in the stream
				//(i.e. 0 for begining).
				Tstring tmpString = srchString;

				tmpString.insert(0," ");
				tmpString += " " ;

				// If this fails we are about to do a buffer overrun because we need to insert the
				// string too near the end of the buffer.  This is just a unlikely bug in the test
				assert( rndm + tmpString.length() < maxLength ) ; 

				// memcpy instread of string copy so that the string terminator isn't copied
				memcpy((char*)buf+rndm,tmpString.c_str(), tmpString.length());
			}
			else
			{	
				// Need to add it in a later call to read
				rndm -= maxLength ;
			}

			mSeek += lLength; // Position for the next read calls
			return lLength; 
		}
		virtual size_t readChunk(uint64_t pSeek, void * buf, size_t maxLength) 
		{ 
			mSeek = (unsigned long)pSeek; 
			return read(buf, maxLength); 
		}
		virtual	IStream * clone() const 
		{ 
			return new testStream(mLength,mStartChar,mVT,srchString,rndm); 
		}
		virtual	RC reset() { mSeek = 0; return RC_OK; }
		virtual void destroy() { delete this; }
	public:
		static char getCharAt(size_t pIndex, char pStartChar = 'a') 
		{ 
			if ( 100*rand()/RAND_MAX > 95 )
			{
				// Break the string into words about 20 characters long 
				return ' ' ;
			}
			return pStartChar + (char)(pIndex % 26); 
		}
};
TEST_IMPLEMENT(TestFTStreams, TestLogger::kDStdOut);

int TestFTStreams::execute()
{
	if (MVTApp::startStore())
	{
		mStoreCtx = MVTApp::getStoreCtx();
#if STRESS
		// REVIEW: the multi-threading support her is fairly general
		// and might be something that could be generalized into the base class
		HTHREAD lThreads[STRESS_THREAD_COUNT];
		for ( int i = 0 ; i < STRESS_THREAD_COUNT ; i++ )
		{
			createThread(TestFTStreams::testFTStreamsThread, this, lThreads[i]);
			MVTestsPortability::threadSleep( 25 ) ; 
		}
		MVTestsPortability::threadsWaitFor(STRESS_THREAD_COUNT, lThreads);
#else
		// Regular test, suitable for smoke test
		ISession * const session = MVTApp::startSession();
		testFTStreams(session);
		session->terminate();
#endif
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Failed to open store"); }

	return RC_OK  ;
}


THREAD_SIGNATURE TestFTStreams::testFTStreamsThread(void * pTI)
{
	TestFTStreams * pTest = (TestFTStreams*)pTI ;
	pTest->testFTStreamsInThreadImpl() ;
	return 0 ;
}

void TestFTStreams::testFTStreamsInThreadImpl()
{
	ISession * const threadSession = MVTApp::startSession(mStoreCtx);
	unsigned int const lSeed = (unsigned int)getTimeInMs() ;
	srand(lSeed) ;

	for ( int i = 0 ; i < STRESS_COUNT ; i++ )
	{
		long lBef, lAft;			
		lBef = getTimeInMs();
		testFTStreams(threadSession);
		lAft = getTimeInMs() ;

		// Serialize logger output
		mOutputLock.lock() ;
		// Show signs of progress
		// REVIEW: It is interesting to see how the speed starts to degrade as
		// the store bloats
		mLogger.out() << i << " complete in " << lAft - lBef << "(ms)" << endl ; 
		mOutputLock.unlock() ;
	}
	threadSession->terminate() ;
}


void TestFTStreams::testFTStreams(ISession *session)
{
	const bool bWords = false ; // No words in the search string, because FT search matches with each and short random words can appear anywhere

	Value val[5];
	PID pid;
	unsigned long streamlen = 20 + rand() % 40;
	Tstring serstr; // Random Search String
	MVTRand::getString(serstr,5,0,bWords);
	streamlen+=6;
#define	NUM_PROPS 10
	PropertyID lPropIDs[NUM_PROPS];
	MVTApp::mapURIs(session,"TestFTStreams.testFTStreams",NUM_PROPS,lPropIDs);

	//case 1: Short Stream and search string at the begining.(create)
	val[0].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97/*start character*/,VT_STRING,serstr,0)));
	val[0].setPropID(lPropIDs[0]);
	TVERIFYRC(session->createPIN(pid,val,1,0));
	testExpectedFT( session, lPropIDs[0], serstr.c_str(),"case 1: Short Stream and search string at the begining.(create) failed");

	//case 2: Short stream collection and search string at the begining (create).
	streamlen = 40 + rand() % 60;
	MVTRand::getString(serstr,7,0,bWords);
	streamlen+=8;
	val[0].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,0)));
	val[0].setPropID(lPropIDs[1]);
	val[0].op = OP_ADD; val[0].eid = STORE_LAST_ELEMENT;

	streamlen = 10 + rand()% 20;
	MVTRand::getString(serstr,4,0,bWords);
	streamlen+=6;
	val[1].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,0)));
	val[1].setPropID(lPropIDs[1]);
	val[1].op = OP_ADD; val[1].eid = STORE_LAST_ELEMENT;

	streamlen = 10 + rand()% 30;
	MVTRand::getString(serstr,9,0,bWords);
	streamlen+=10;
	val[2].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,0)));
	val[2].setPropID(lPropIDs[1]);
	val[2].op = OP_ADD; val[2].eid = STORE_LAST_ELEMENT;
	TVERIFYRC(session->createPIN(pid,val,3,0));
	testExpectedFT( session, lPropIDs[1], serstr.c_str(),"case 2: Short stream collection and search string at the begining (create) failed");
	
	//case 3: large stream with search string somewhere in between
	streamlen = 10000 + rand() % 20000;
	MVTRand::getString(serstr,35,0,bWords); 
	streamlen+=36;
	val[0].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,5612)));
	val[0].setPropID(lPropIDs[1]);
	TVERIFYRC(session->createPIN(pid,val,1,0));

	testExpectedFT( session, lPropIDs[1], serstr.c_str(),"case 3: large stream with search string somewhere in between (create) failed");

	//case 4: big stream collection (25000,30000,15000 fails: investigate)
	//Assertion failed: ulong(sht)==pin->stamp, file c:\software\eclipse\workspace\pinto\mvstore\commitpins.cpp, line 208
	streamlen = 35000 + rand()%30000;
	MVTRand::getString(serstr,16,0,bWords);
	streamlen+=17;
	val[0].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,9534)));
	val[0].setPropID(lPropIDs[2]);
	val[0].op = OP_ADD; val[0].eid = STORE_FIRST_ELEMENT;
	
	streamlen  = 40000 + rand() % 30000;
	MVTRand::getString(serstr,20,0,bWords);
	streamlen+=21;
	val[1].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,12567)));
	val[1].setPropID(lPropIDs[2]);
	val[1].op = OP_ADD; val[1].eid = STORE_FIRST_ELEMENT;
	
	streamlen = 25000 + rand() % 30000;
	MVTRand::getString(serstr,6,0,bWords);
	streamlen+=7;
	val[2].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,666)));
	val[2].setPropID(lPropIDs[2]);
	val[2].op = OP_ADD; val[2].eid = STORE_LAST_ELEMENT;
	TVERIFYRC(session->createPIN(pid,val,3,0));

	testExpectedFT( session, lPropIDs[2], serstr.c_str(),"case 4: big stream collection (create) failed");
	
	//case 5: Modify pin with a short stream
	streamlen = 40 + rand()% 100;
	MVTRand::getString(serstr,6,0,bWords);
	streamlen+=7;
	val[0].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,0)));
	val[0].setPropID(lPropIDs[3]);
	TVERIFYRC(session->createPIN(pid,val,1,0));
	
	streamlen = 40 + rand() % 200;
	MVTRand::getString(serstr,4,0,bWords);
	streamlen+=5;
	val[0].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,0)));
	val[0].setPropID(lPropIDs[3]);
	TVERIFYRC(session->modifyPIN(pid,val,1,0));

	testExpectedFT( session, lPropIDs[3], serstr.c_str(),"case 5: Modify pin with a short stream failed");
	
	//case 6: collection modify of short stream
	streamlen = 60 + rand() % 40;
	streamlen+=7;
	val[0].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING," junk ",0)));
	val[0].setPropID(lPropIDs[5]);
	streamlen = 50 + rand() % 70;
	MVTRand::getString(serstr,9,0,bWords);
	streamlen+=10;
	val[1].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,0)));
	val[1].setPropID(lPropIDs[5]);
	TVERIFYRC(session->createPIN(pid,val,2,0));


	IPIN *pin = session->getPIN(pid);
	streamlen = 40 + rand() % 60;
	MVTRand::getString(serstr,5,0,bWords);
	streamlen+=6;
	val[0].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,0)));
	val[0].setPropID(lPropIDs[5]);
	streamlen = 100 + rand() % 100;
	streamlen+=5;
	val[1].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,"junk",0)));
	val[1].setPropID(lPropIDs[6]);

	pin->modify(val,2,0);

	if ( MVTApp::bVerbose )
		MVTApp::output(*pin,mLogger.out(),session);

	testExpectedFT( session, lPropIDs[6], serstr.c_str(),"case 6: Modify pin with a short stream collection failed");
	pin->destroy();

	//case 7: modifypin with a long stream
	val[0].set("this will converted to streams");val[0].setPropID(lPropIDs[7]);
	val[1].set(123445);val[1].setPropID(lPropIDs[8]);
	val[2].setURL("http://www.f1.com");val[2].setPropID(lPropIDs[9]);
	TVERIFYRC(session->createPIN(pid,val,3));
	
	pin = session->getPIN(pid);
	streamlen = 900000 + rand() % 30000;
	MVTRand::getString(serstr,8,0,bWords);
	streamlen+=9;
	val[0].set(MVTApp::wrapClientStream(session, new testStream(streamlen,97,VT_STRING,serstr,0)));
	val[0].setPropID(lPropIDs[7]);
	TVERIFYRC(session->modifyPIN(pid,val,1,0));
	testExpectedFT( session, lPropIDs[7], serstr.c_str(),"case 7: Modify pin with a long stream failed");
	pin->destroy();

	MVTApp::sReporter.enable(false); // Hide full scan query noise
	for ( int i = 0 ; i < NUM_PROPS ; i++ )
		purgeTestPins( session, lPropIDs[i] ) ; // Avoid future iterations failing
	MVTApp::sReporter.enable(true); 
}

void TestFTStreams::purgeTestPins( ISession* session, PropertyID prop )
{
	CmvautoPtr<IStmt> lq(session->createStmt(STMT_DELETE));
	lq->setPropCondition(lq->addVariable(),&prop,1);
	TVERIFYRC(lq->execute(NULL,0,0,~0,0,MODE_PURGE));
}

void TestFTStreams::testExpectedFT(ISession *session, PropertyID prop, const char *keyword, const char* msg)
{
	vector<PID> matches ;
	int cnt = getFTMatches(session, prop, keyword, matches) ;

	if ( cnt != 1 )
	{
		TVERIFY2( 0, msg ) ;

		// Print diagnostics
		mLogger.out() << cnt << " matches for \"" << keyword << "\" (expected 1)" << endl ; 
	}
}

int TestFTStreams::getFTMatches(ISession *session, PropertyID prop, const char *keyword, vector<PID>& outMatches )
{
	// Return the number of FT matches for the given keyword
	IStmt *query;
	uint64_t count;
	
	query = session->createStmt();
	unsigned var = query->addVariable();

	TVERIFYRC(query->setPropCondition(var,&prop,1));

	query->setConditionFT(var,keyword);
	query->count(count);

	// TODO: we could also actually search the matches ourselves manually
	// to confirm whether the string is really present in the match.
	// But this is lower priority unless this test actually starts failing

	ICursor * r = NULL;
	TVERIFYRC(query->execute(&r)) ;
	while(1)
	{
		IPIN * pin = r->next() ;
		if ( pin != NULL )
		{
			outMatches.push_back( pin->getPID() ) ;

			if ( MVTApp::bVerbose )
			{
				mLogger.out() << "Match for " << keyword  << endl ;
				// This doesn't print the entire huge stream (which is a bit of a blessing!)
				// but not fully useful if trying to figure out where the match occurred
				MVTApp::output( *pin, mLogger.out(), session ) ;
			}

			pin->destroy() ;
		}
		else
		{
			break ;
		}
	}

	query->destroy();
	
	return (int)count;
}
