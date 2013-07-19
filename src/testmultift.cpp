/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"
#include "teststream.h"
#include <sys/stat.h>
#if defined(WIN32)
	#include <direct.h>		
#endif

using namespace std;

typedef std::map<PropertyID, std::string> TSearchInfo;
class UTF8Buffer
{
	#define PINTO_ROOT "PINTO"
#ifdef WIN32
	#define LOCATION_TO_ROOT "\\store\\tests\\textdata\\"
#else		
	#define LOCATION_TO_ROOT "/vstore/tests/textdata/"
#endif
#define  WHOLE_FILE        "UTF-8-demo.txt"
#define  PART_IN_RUS       "UTF-8-rus.txt"
#define  PART_IN_RUS1      "UTF-8-rus1.txt"
#define  PART_IN_RUS2      "UTF-8-rus2.txt"
#define  PART_IN_GREEK     "UTF-8-greek.txt"
#define  PART_IN_ETHIOPIAN "UTF-8-ethiopian.txt"
#define  PART_IN_GEORGIAN  "UTF-8-georgian.txt"
#define  PART_IN_THAI      "UTF-8-thai.txt"
public:
	char *buf;

	UTF8Buffer(const char * pFileName)
	{
		struct stat lResults;
		char * lPrefix= getenv(PINTO_ROOT);
		string lFullName;
		if(lPrefix) 
		{
			lFullName = lPrefix;
			lFullName += (char *)LOCATION_TO_ROOT;
		}
		else
		{
			char cwd[_MAX_PATH];
			lFullName = getcwd(cwd, _MAX_PATH);
			char *lSlash = NULL;
			#ifdef WIN32
				lSlash = "\\";
			#else
				lSlash = "//";
			#endif
			lFullName.append(lSlash);
			lFullName.append("textdata");
			lFullName.append(lSlash);
		}		
		lFullName += pFileName;
					
		if (stat(lFullName.c_str(), &lResults) == 0)
		{
			// The size of the file in bytes is in results.st_size
		   buf=(char*)malloc(lResults.st_size+1);
		}
		else
		{
			cerr << "Failed to get size of " << pFileName << " !" << endl; buf=NULL;
		}
		ifstream  fs(lFullName.c_str(), ios::in | ios::binary);
		if(!fs)
		{
			cerr << "Failed to open " << pFileName << " !" << endl; 
		}
		else
		{
			fs.read(buf, lResults.st_size); buf[lResults.st_size]=0;
			fs.close();
		}
	}
	~UTF8Buffer(){free(buf);};
public:
	char * getBuffer() { if(buf) return buf; return 0;}
};

class MultiFTSearchContext
{
	ITest*		mTest ;
	ISession *	mSession ;
	const int mNumProps ;
	PropertyID	*mPropIDs ;	
	int mCurrentPropIndex;	
	PID mPID;
	static int  mPropCnt; 
	PropertyID *mSearchProps;
	int mNumSearchProps;
public:
	static bool sUseStream ;
	MultiFTSearchContext(ITest* inTest, ISession * inSession, const int pNumProps = 2, bool pCreatePIN = false)
		: mTest(inTest)
		, mSession(inSession)
		, mNumProps(pNumProps)		
	{
		mPropIDs = new PropertyID[pNumProps];
		int i = 0;
		for(i = 0; i < pNumProps; i++)
		{
			char lPropName[64];
			sprintf(lPropName, "MultiFTSearchContext_%d_%d", mPropCnt++,MVTApp::Suite().mSeed);
			mPropIDs[i] = MVTUtil::getProp(mSession,lPropName);
		}
		mCurrentPropIndex = 0;
		mPID.pid = STORE_INVALID_PID; mPID.ident = STORE_OWNER;
		if(pCreatePIN) mPID = createPINAndCommit();		
		mSearchProps = NULL;
		mNumSearchProps = 0;
	}	

	PID createPINAndCommit()
	{
		PID lPID = {STORE_INVALID_PID, STORE_OWNER};
		CREATEPIN(mSession, lPID, NULL, 0);
		return lPID;
	}	

	void setPropsForSearch(PropertyID *pProps, const int pNumProps)
	{
		TV_R(!pProps || pNumProps != 0, mTest);
		if(mSearchProps) delete[] mSearchProps;
		mSearchProps = new PropertyID[pNumProps];
		int i = 0;
		for(i = 0; i < pNumProps; i++)
			mSearchProps[i] = pProps[i];
		mNumSearchProps = pNumProps;
	}

	PropertyID addTextProperty(PID pPID, const char * pValue, bool pRemoveStopWords = true, PropertyID pPropID = STORE_INVALID_URIID )
	{
		TV_R(pPropID != STORE_INVALID_URIID || mCurrentPropIndex != mNumProps, mTest);
		if(pPID.pid == 0 || pPID.pid == STORE_INVALID_PID)
			pPID = mPID;

		if (sUseStream)
			return addStreamTextProperty(pPID, pValue, pRemoveStopWords, pPropID);

		PropertyID lPropID;
		if(pPropID == STORE_INVALID_URIID) lPropID = mPropIDs[mCurrentPropIndex++];
		else lPropID = pPropID;

		Value lV ; 
		SETVALUE(lV, lPropID, pValue, OP_SET);
		lV.meta = META_PROP_FTINDEX;
		//if (pRemoveStopWords) lV.meta = META_PROP_STOPWORDS ;	
		
		IPIN *lPIN = mSession->getPIN(pPID);;
		TVRC_R(lPIN->modify(&lV, 1), mTest);
		if(lPIN) lPIN->destroy();
		return  lPropID;
	}

	unsigned long search(char *pCommonSearchKey, unsigned long pExpResults, unsigned int *pFlags = NULL)
	{
		TV_R(mSearchProps !=NULL && mNumSearchProps != 0, mTest);
		TSearchInfo lInfo;
		int i = 0;
		for(i = 0; i < mNumSearchProps; i++)
			lInfo.insert(std::map<PropertyID, std::string>::value_type(mSearchProps[i], pCommonSearchKey));
		return search(lInfo, pExpResults, pFlags);
	}

	unsigned long search(PropertyID pPropID1, char *pSearchKey1, PropertyID pPropID2, char *pSearchKey2, unsigned long pExpResults, unsigned int *pFlags = NULL)
	{
		TSearchInfo lInfo;
		lInfo.insert(std::map<PropertyID, std::string>::value_type(pPropID1, pSearchKey1));
		lInfo.insert(std::map<PropertyID, std::string>::value_type(pPropID2, pSearchKey1));
		return search(lInfo, pExpResults, pFlags);
	}
	
	unsigned long search(PropertyID *pPropID, int pNumProps, char *pCommonSearchKey, unsigned long pExpResults, unsigned int *pFlags = NULL)
	{
		TV_R(pCommonSearchKey != NULL || pNumProps != 0, mTest);
		TSearchInfo lInfo;
		int i = 0;
		for(i = 0; i < pNumProps; i++)
			lInfo.insert(std::map<PropertyID, std::string>::value_type(pPropID[i], pCommonSearchKey));
		return search(lInfo, pExpResults, pFlags);
	}

	unsigned long search(TSearchInfo pSearchInfo, unsigned long pExpResults, unsigned int *pFlags = NULL)
	{
		unsigned int lFlags = 0;
		TV_R(!pSearchInfo.empty(), mTest);

		uint64_t cntResults = 0 ;
		CmvautoPtr<IStmt> ftQ(mSession->createStmt()) ;	
		unsigned char lVar = ftQ->addVariable() ;

		TSearchInfo::const_iterator lIter = pSearchInfo.begin();
		int i = 0;
		for(; lIter != pSearchInfo.end(); lIter++, i++)
		{
			//string strFlags;
			//if ( pFlags[i] & QFT_FILTER_SW ) strFlags.append( "QFT_FILTER_SW ");
			//if ( pFlags[i] & MODE_ALL_WORDS ) strFlags.append( "MODE_ALL_WORDS ");
			PropertyID lPropID = lIter->first;
			Tstring lSearchKey = lIter->second;
			
			unsigned int flagsToAddConditionFT = 0;
			if (pFlags && pFlags[i])
			{
				lFlags |= pFlags[i];
				if (pFlags[i] & QFT_FILTER_SW)
				{
					flagsToAddConditionFT |= QFT_FILTER_SW;
					lFlags &= ~QFT_FILTER_SW ;
				}
			}			
			TVRC_R( ftQ->addConditionFT( lVar, lSearchKey.c_str(), flagsToAddConditionFT, &lPropID, 1), mTest);
		}		
		
		TVRC_R( ftQ->count( cntResults, NULL, 0, ~0, lFlags ), mTest );	
		ICursor* lC = NULL;
		ftQ->execute(&lC, NULL, 0, ~0, 0, lFlags);
		CmvautoPtr<ICursor> res(lC);
		PID lPID ;
		unsigned long cntCheck = 0 ;
		while( RC_OK == res->next(lPID) ) 
		{
			if (mTest->isVerbose())
				MVTApp::output(lPID, std::cout, mSession);
			cntCheck++ ;			
		}
		
		TV_R(cntCheck == pExpResults, mTest) ;
		TV_R(cntCheck == cntResults, mTest) ;

		return (unsigned long)cntResults ;
	}

	~MultiFTSearchContext() { if(mPropIDs) delete[] mPropIDs; if(mSearchProps) delete[] mSearchProps;}	
protected:

	PropertyID addStreamTextProperty(PID pPID, const char * pValue, bool pRemoveStopWords = true, PropertyID pPropID = STORE_INVALID_URIID)
	{
		TestStream * lStream = new TestStream((int)(40000)*sizeof(char), NULL, VT_STRING ) ;
		
		string forceContent = " "; forceContent+=pValue; forceContent += " ";
		
		memcpy(lStream->getBuffer(), forceContent.c_str(), forceContent.size()*sizeof(char));
		
		PropertyID lPropID;
		if(pPropID == STORE_INVALID_URIID) lPropID = mPropIDs[mCurrentPropIndex++];
		else lPropID = pPropID;
		Value lV ; 
		lV.set(MVTApp::wrapClientStream(mSession, lStream)) ; 
		lV.property = lPropID ;
		lV.meta = META_PROP_FTINDEX;
		if (pRemoveStopWords)
		{
			// Means that any common english words will be excluded from the index
//			lV.meta = META_PROP_STOPWORDS ;	
		}
		IPIN *lPIN = mSession->getPIN(pPID);;
		TVRC_R(lPIN->modify(&lV, 1), mTest);
		if(lPIN) lPIN->destroy();

		lStream->destroy() ;

		return lPropID;
	}
} ;

bool MultiFTSearchContext::sUseStream = false ;
int MultiFTSearchContext::mPropCnt = 0;

class TestMultiFT : public ITest
{
	public:
		static const int mNumProps = 6;
		PropertyID mPropIds[mNumProps];
		static const int mNumPINs = 200;	
		ISession *mSession;
	public:
		TEST_DECLARE(TestMultiFT);
		virtual char const * getName() const { return "testmultift"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Tests ANDing of multiple FT conditions in query"; }

		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void insertToSearchMap(TSearchInfo &lInfo, PropertyID pPropID, Tstring pSearchKey, bool pClean = false);
		void doTests() ;

		void testSimpleSearch() ;
		void testSimpleSearch2();
		void testSimpleSearch3();
		void testUTF8();
		void testStopWords();
		
};

TEST_IMPLEMENT(TestMultiFT, TestLogger::kDStdOut);

int	TestMultiFT::execute()
{	
	if (MVTApp::startStore())
	{
		mSession =	MVTApp::startSession();
		
		MultiFTSearchContext::sUseStream = false ;
		doTests();
		
		MultiFTSearchContext::sUseStream = true ;
		doTests();		

		mSession->terminate();
		MVTApp::stopStore();
	}
	else
		TVERIFY(false && "Failed to open Store");

	return RC_OK;
}
void TestMultiFT::doTests()
{
	// Basic tests
	testSimpleSearch();
	testSimpleSearch2();
	testSimpleSearch3();
	
	testUTF8();
	testStopWords();
	
}

void TestMultiFT::testSimpleSearch()
{
	// Searching a specific single property in the store
	MultiFTSearchContext lCtx( this, mSession, 2) ;
	static const bool bFilterStopWords = true ;	
	TSearchInfo lInfo;
	unsigned int lFlags[2] = {0, 0};

	PID lPID1 = lCtx.createPINAndCommit();	
	PID lPID2 = lCtx.createPINAndCommit();	
	
	PropertyID lProp1 = lCtx.addTextProperty(lPID1, "EMC began in 1979.\n  The founders were Richard (Dick) Egan and Roger Marino, the E and M in the company's name.", bFilterStopWords);
	PropertyID lProp2 = lCtx.addTextProperty(lPID1, "EMC did not adopt the EMC notation to refer to Einstein's famous equation, E=mc2", bFilterStopWords);
	
	lCtx.addTextProperty(lPID2, "richard.... The first C stands for the .... Roger... third partner who left before the formation of the company and the second C stands for the Corporation", bFilterStopWords, lProp1);
	lCtx.addTextProperty(lPID2, "Originally a manufacturer of memory boards, EMC expanded beyond memory to disk drives", bFilterStopWords, lProp2);
	
	// both PINs have prop1 and prop2 search
	insertToSearchMap(lInfo, lProp1, "company", true);
	insertToSearchMap(lInfo, lProp2, "EMC");	
	lCtx.search(lInfo, 2);

	insertToSearchMap(lInfo, lProp1, "richard", true);
	insertToSearchMap(lInfo, lProp2, "EMC");	
	lCtx.search(lInfo, 2);

	// PIN1 and PIN2, prop1 contains "company". Expected Results = 0
	insertToSearchMap(lInfo, lProp1, "company", true/*clear map*/); 
	insertToSearchMap(lInfo, lProp2, "junk");		
	lCtx.search(lInfo, 0);	

	// PIN1 prop1 contains "founder" and PIN2 prop2 contains "memory". Expected Results = 0
	insertToSearchMap(lInfo, lProp1, "founder", true);
	insertToSearchMap(lInfo, lProp2, "memory");		
	lCtx.search(lInfo, 0);	

	// Valid PIN1 prop1 and prop2 search
	insertToSearchMap(lInfo, lProp1, "1979", true);
	insertToSearchMap(lInfo, lProp2, "notation");	
	lCtx.search(lInfo, 1);

	// Valid PIN1 has prop1 and prop2 search but PIN2 has only prop1 search
	insertToSearchMap(lInfo, lProp1, "company", true);
	insertToSearchMap(lInfo, lProp2, "notation");	
	lCtx.search(lInfo, 1);	

	// WITH MODE_ALL_WORDS
	lFlags[0] = MODE_ALL_WORDS;
	insertToSearchMap(lInfo, lProp1, "Richard Roger", true);
	insertToSearchMap(lInfo, lProp2, "einstein equation");	
	lCtx.search(lInfo, 1, lFlags);	

}

void TestMultiFT::testSimpleSearch2()
{
	// Searching a specific single property in the store
	MultiFTSearchContext lCtx( this, mSession, 2) ;
	static const bool bFilterStopWords = true ;	
	TSearchInfo lInfo;
	unsigned int lFlags[2] = {0, 0};

	PID lPID1 = lCtx.createPINAndCommit();	
	PID lPID2 = lCtx.createPINAndCommit();	
	
	PropertyID lProp1 = lCtx.addTextProperty(lPID1, "EMC began in 1979.\n  The founders were Richard (Dick) Egan and Roger Marino, the E and M in the company's name.", bFilterStopWords);
	PropertyID lProp2 = lCtx.addTextProperty(lPID1, "EMC did not adopt the EMC2 notation to refer to Einstein's famous equation, E=mc2", bFilterStopWords);
	
	lCtx.addTextProperty(lPID2, "richard.... The first C stands for the .... Roger... third partner who left before the formation of the company and the second C stands for the Corporation", bFilterStopWords, lProp1);
	lCtx.addTextProperty(lPID2, "Originally a manufacturer of memory boards, EMC expanded beyond memory to disk drives", bFilterStopWords, lProp2);
	
	// both PINs have prop1 and prop2 search
	insertToSearchMap(lInfo, lProp1, "company", true);
	insertToSearchMap(lInfo, lProp2, "EMC");	
	lCtx.search(lInfo, 2);

	insertToSearchMap(lInfo, lProp1, "richard", true);
	insertToSearchMap(lInfo, lProp2, "EMC");	
	lCtx.search(lInfo, 2);

	// PIN1 and PIN2, prop1 contains "company". Expected Results = 0
	insertToSearchMap(lInfo, lProp1, "company", true/*clear map*/); 
	insertToSearchMap(lInfo, lProp2, "junk");		
	lCtx.search(lInfo, 0);	

	// PIN1 prop1 contains "founder" and PIN2 prop2 contains "memory". Expected Results = 0
	insertToSearchMap(lInfo, lProp1, "founder", true);
	insertToSearchMap(lInfo, lProp2, "memory");		
	lCtx.search(lInfo, 0);	

	// Valid PIN1 prop1 and prop2 search
	insertToSearchMap(lInfo, lProp1, "1979", true);
	insertToSearchMap(lInfo, lProp2, "notation");	
	lCtx.search(lInfo, 1);

	// Valid PIN1 has prop1 and prop2 search but PIN2 has only prop1 search
	insertToSearchMap(lInfo, lProp1, "company", true);
	insertToSearchMap(lInfo, lProp2, "notation");	
	lCtx.search(lInfo, 1);

	// WITH MODE_ALL_WORDS
	lFlags[0] = MODE_ALL_WORDS;
	insertToSearchMap(lInfo, lProp1, "Richard Roger", true);
	insertToSearchMap(lInfo, lProp2, "einstein equation");	
	lCtx.search(lInfo, 1, lFlags);	

}

void TestMultiFT::testSimpleSearch3()
{
	// Searching a specific single property in the store
	MultiFTSearchContext lCtx( this, mSession, 2) ;
	static const bool bFilterStopWords = true ;	
	TSearchInfo lInfo;
	unsigned int lFlags[2] = {0, 0};

	int lNumType1PINs = 1, lNumType2PINs = 0;
	PID lPID1 = lCtx.createPINAndCommit();
	
	PropertyID lProp1 = lCtx.addTextProperty(lPID1, "EMC began in 1979.\n  The founders were Richard (Dick) Egan and Roger Marino, the E and M in the company's name.", bFilterStopWords);
	PropertyID lProp2 = lCtx.addTextProperty(lPID1, "EMC did not adopt the EMC2 notation to refer to Einstein's famous equation, E=mc2", bFilterStopWords);
	
	const static int sNumPINs = 20;
	int i = 0;
	for(i = 1; i < sNumPINs; i++)
	{
		PID lPID = lCtx.createPINAndCommit();
		if(MVTRand::getRange(10, 100) > 50)
		{
			lNumType1PINs++;
			if(MVTRand::getBool())
			{
				lCtx.addTextProperty(lPID, "EMC began in 1979.\n  The founders were Richard (Dick) Egan and Roger Marino, the E and M in the company's name.", bFilterStopWords, lProp1);
				lCtx.addTextProperty(lPID, "EMC did not adopt the EMC2 notation to refer to Einstein's famous equation, E=mc2", bFilterStopWords, lProp2);				
			}
			else
			{
				lCtx.addTextProperty(lPID, "EMC began in 1979.\n  The founders were Richard (Dick) Egan and Roger Marino, the E and M in the company's name.", bFilterStopWords, lProp1);
				lCtx.addTextProperty(lPID, "EMC did not adopt the EMC2 notation to refer to Einstein's famous equation, E=mc2", bFilterStopWords, lProp2);
			}			
		}
		else
		{
			lNumType2PINs++;
			if(MVTRand::getBool())
			{
				lCtx.addTextProperty(lPID, "richard.... The first C stands for the .... Roger... third partner who left before the formation of the company and the second C stands for the Corporation", bFilterStopWords, lProp1);
				lCtx.addTextProperty(lPID, "Originally a manufacturer of memory boards, EMC expanded beyond memory to disk drives", bFilterStopWords, lProp2);
			}
			else
			{
				lCtx.addTextProperty(lPID, "richard.... The first C stands for the .... Roger... third partner who left before the formation of the company and the second C stands for the Corporation", bFilterStopWords, lProp1);
				lCtx.addTextProperty(lPID, "Originally a manufacturer of memory boards, EMC expanded beyond memory to disk drives", bFilterStopWords, lProp2);
			}	
		}
	}

	// both PINs have prop1 and prop2 search
	insertToSearchMap(lInfo, lProp1, "company", true);
	insertToSearchMap(lInfo, lProp2, "EMC");	
	lCtx.search(lInfo, sNumPINs);

	insertToSearchMap(lInfo, lProp1, "richard", true);
	insertToSearchMap(lInfo, lProp2, "EMC");	
	lCtx.search(lInfo, sNumPINs);

	// PIN1 and PIN2, prop1 contains "company". Expected Results = 0
	insertToSearchMap(lInfo, lProp1, "company", true/*clear map*/); 
	insertToSearchMap(lInfo, lProp2, "junk");		
	lCtx.search(lInfo, 0);	

	// PIN1 prop1 contains "founder" and PIN2 prop2 contains "memory". Expected Results = 0
	insertToSearchMap(lInfo, lProp1, "founder", true);
	insertToSearchMap(lInfo, lProp2, "memory");		
	lCtx.search(lInfo, 0);	

	// Valid PIN1 prop1 and prop2 search
	insertToSearchMap(lInfo, lProp1, "1979", true);
	insertToSearchMap(lInfo, lProp2, "notation");	
	lCtx.search(lInfo, lNumType1PINs);

	// Valid PIN1 has prop1 and prop2 search but PIN2 has only prop1 search
	insertToSearchMap(lInfo, lProp1, "company", true);
	insertToSearchMap(lInfo, lProp2, "notation");	
	lCtx.search(lInfo, lNumType1PINs);

	// WITH MODE_ALL_WORDS
	lFlags[0] = MODE_ALL_WORDS;
	insertToSearchMap(lInfo, lProp1, "Richard Roger", true);
	insertToSearchMap(lInfo, lProp2, "einstein equation");	
	lCtx.search(lInfo, lNumType1PINs, lFlags);	

}
void TestMultiFT::testUTF8()
{
	UTF8Buffer lFile(WHOLE_FILE); 
	if(!lFile.buf)
	{
		mLogger.out() << "Warning: No testUTF8()" << endl; 
		return;
	}

	// Searching a specific single property in the store
	MultiFTSearchContext lCtx( this, mSession ) ;
	static const bool bFilterStopWords = true ;
	unsigned int lFlags[2] = {MODE_ALL_WORDS, MODE_ALL_WORDS};
		
	mLogger.out() << "Running series of simple UTF8 tests..." << endl;

	PID lPID1 = lCtx.createPINAndCommit();
	PropertyID lPropID1 = lCtx.addTextProperty(lPID1, lFile.buf, bFilterStopWords);
	PropertyID lPropID2 = lCtx.addTextProperty(lPID1, lFile.buf, bFilterStopWords);
	
	PropertyID lProps[2] = {lPropID1, lPropID2};
	lCtx.setPropsForSearch(lProps, 2);
	//Step1. Searching for some titles, which are in English within the whole documents... 
	{
		mLogger.out() << "Searching for English titles..." << endl;
		lCtx.search( "Markus Kuhn", 1);
		lCtx.search( "mARKUS kUHN", 1);
		lCtx.search( "Mathematics and sciences", 1);
		lCtx.search( "mATHEMATICS SCIENCES", 1, lFlags/*MODE_ALL_WORDS*/);
		lCtx.search( "Linguistics and dictionaries", 1);
		lCtx.search( "Georgian", 1);
		lCtx.search( "Russian", 1);
		lCtx.search( "rUSSIAN", 1);
		lCtx.search( "Ethiopian", 1);
		lCtx.search( "eTHIOPIAN", 1);
		lCtx.search( "Runes", 1);
		lCtx.search( "Braille", 1);
	}

	//Step2. Searching for some patterns, which are not in English within the whole documents...
	{
		mLogger.out() << "Searching for patterns in Russian..." << endl;
		UTF8Buffer patternRus(PART_IN_RUS);
		if(!patternRus.buf)
		{
			mLogger.out() << "Warning: No this part of testUTF8()" << endl; 
		}
		else	
			lCtx.search(patternRus.buf,1, lFlags);
	  
		//The following 2 searches are initialezed with capital characters...
		UTF8Buffer patternRus1(PART_IN_RUS1);
		if(!patternRus1.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}else
			lCtx.search(patternRus1.buf,1, lFlags);
		
		UTF8Buffer patternRus2(PART_IN_RUS2);
		if(!patternRus2.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}else
			lCtx.search(patternRus2.buf,1, lFlags);
	}

	//Step3. Searching for some patterns, which are not in English within the whole documents...
	{
		mLogger.out() << "Searching for patterns in Greek..." << endl;
		UTF8Buffer patternGreek(PART_IN_GREEK);
		if(!patternGreek.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}else
			lCtx.search(patternGreek.buf,1);
	}

	//Step4. Searching for some patterns, which are not in English within the whole documents...
	{
		mLogger.out() << "Searching for patterns in Georgian ..." << endl;
		UTF8Buffer patternGeorgian(PART_IN_GEORGIAN);
		if(!patternGeorgian.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}else
			lCtx.search(patternGeorgian.buf,1);
	}
	
		//Step5. Searching for some patterns, which are not in English within the whole documents...
	{
		//The check for Ethiopian is disabled for now since the code for UTF8 escape sequences is not 
		//supported. Quoting Mark: "... One day later... may be..."  
#if 0
		mLogger.out() << "Searching for patterns in Ethiopian ..." << endl;
		UTF8Buffer patternethiopian(PART_IN_ETHIOPIAN);
		if(!patternethiopian.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}else
			lCtx.search(patternethiopian.buf,1);
#endif
	}

		//Step6. Searching for some patterns, which are not in English within the whole documents...
	{
		mLogger.out() << "Searching for patterns in THAI ..." << endl;
		UTF8Buffer patternthai(PART_IN_THAI);
		if(!patternthai.buf){
			mLogger.out() << "Warning: No this part of testUTF8()" << endl;
		}else
			lCtx.search(patternthai.buf,1);
	}

}
void TestMultiFT::testStopWords()
{
	// Demonstrate how META_PROP_STOPWORDS will remove common
	// words, so they cannot be found by FT search
	
	MultiFTSearchContext ctxtFilterStop( this, mSession, 3 ) ;
	MultiFTSearchContext ctxtWithStop( this, mSession, 3 ) ;
	unsigned int lFlags[2] = {MODE_ALL_WORDS, MODE_ALL_WORDS};
		
	PropertyID lProps[3] = {STORE_INVALID_URIID, STORE_INVALID_URIID, STORE_INVALID_URIID};
	PropertyID lProps1[3] = {STORE_INVALID_URIID, STORE_INVALID_URIID, STORE_INVALID_URIID};
	//lCtx.setPropsForSearch(lProps, 2);

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
		PID lPID1 = ctxtFilterStop.createPINAndCommit();
		lProps[0] = ctxtFilterStop.addTextProperty( lPID1, testStrs[pos], true /* META_PROP_STOPWORDS */, lProps[0]) ;
		lProps[1] = ctxtFilterStop.addTextProperty( lPID1, testStrs[pos], false, lProps[1]) ;
		lProps[2] = ctxtFilterStop.addTextProperty( lPID1, testStrs[pos], true/* META_PROP_STOPWORDS */, lProps[2]) ;
		
		PID lPID2 = ctxtWithStop.createPINAndCommit();
		lProps1[0] = ctxtWithStop.addTextProperty( lPID2, testStrs[pos], false, lProps1[0]) ;
		lProps1[1] = ctxtWithStop.addTextProperty( lPID2, testStrs[pos], true/* META_PROP_STOPWORDS */, lProps1[1]) ;
		lProps1[2] = ctxtWithStop.addTextProperty( lPID2, testStrs[pos], false, lProps1[2]) ;
		pos++ ;
	}

	PropertyID lFilterPropList1[2] = {lProps[0], lProps[1]}; 
	PropertyID lFilterPropList2[2] = {lProps[0], lProps[2]};

	PropertyID lNoFilterPropList1[2] = {lProps1[0], lProps1[1]}; 
	PropertyID lNoFilterPropList2[2] = {lProps1[0], lProps1[2]};

	// Prop0 and Prop2 have META_PROP_STOPWORDS flag but Prop1 doesn't
	ctxtFilterStop.search( lFilterPropList1, 2, "of", 0) ; 
	ctxtFilterStop.search( lFilterPropList2, 2, "of", 0) ;	
	
	// Prop1 has META_PROP_STOPWORDS flag but Prop0 and Prop2 dont
	ctxtWithStop.search( lNoFilterPropList1, 2, "of", 0) ; 
	ctxtWithStop.search( lNoFilterPropList2, 2, "of", 1) ;

	// Full match
	ctxtFilterStop.search( lFilterPropList1, 2, "newly", 1) ; ctxtFilterStop.search( lFilterPropList2, 2, "newly", 1) ;
	ctxtWithStop.search( lNoFilterPropList1, 2, "newly", 1) ; ctxtWithStop.search( lNoFilterPropList2, 2, "newly", 1) ;	

	// Matches against newly but not new
	ctxtFilterStop.search( lFilterPropList1, 2, "new", 1) ; ctxtFilterStop.search( lFilterPropList2, 2, "new", 1) ;
	
	ctxtWithStop.search( lNoFilterPropList1, 2, "new", 1) ; 
	ctxtWithStop.search( lNoFilterPropList2, 2, "new", 2) ; // should return 2 PINs for "new" in prop0 and prop2 without META_PROP_STOPWORDS as well

	// Phrase implications (see bug 9732)

	// With stop words filtered "the" from Peter the Great is not indexed
	// Because matching is for any word both "Peter the Great" and "Great old Peter" will match
	ctxtFilterStop.search( lFilterPropList1, 2, "Peter the Great", 2) ; ctxtFilterStop.search( lFilterPropList2, 2, "Peter the Great", 2) ;
	ctxtWithStop.search( lNoFilterPropList1, 2, "Peter the Great", 2) ; 

	// Without stop words the "the" can lead to uninteresting matches, for example 
	// this will match with "Even the example of yours"
	ctxtWithStop.search( lNoFilterPropList2, 2, "Peter the Great", 3) ; 

	// Search with MODE_ALL_WORDS
	ctxtFilterStop.search( lFilterPropList1, 2, "Peter the Great", 0, lFlags) ; 
	ctxtFilterStop.search( lFilterPropList2, 2, "Peter the Great", 0, lFlags) ;
	ctxtWithStop.search( lNoFilterPropList1, 2, "Peter the Great", 0, lFlags ) ;
	
	// Without stop words removed we get the "correct" results
	// This should return 1 PIN as prop0 and prop2 dont have META_PROP_STOPWORDS
	ctxtFilterStop.search( lNoFilterPropList2, 2, "Peter the Great", 1, lFlags) ; 

	// Solution is to filter the StopWords even on the input (e.g.
	// as if we only search for Peter Great.  It is inaccurate because
	// it also returns "Great old Peter"
	
	// Only for prop0 which is marked with META_PROP_STOPWORDS
	lFlags[0] = MODE_ALL_WORDS|QFT_FILTER_SW;
	ctxtFilterStop.search( lFilterPropList1, 2, "Peter the Great", 1, lFlags) ; 

	// This should return 0 because prop2 is marked for META_PROP_STOPWORDS 	
	ctxtFilterStop.search( lFilterPropList2, 2, "Peter the Great", 0, lFlags) ;	
	// Now make both props run with QFT_FILTER_SW
	lFlags[0] = MODE_ALL_WORDS|QFT_FILTER_SW;
	lFlags[1] = QFT_FILTER_SW;
	ctxtFilterStop.search( lFilterPropList2, 2, "Peter the Great", 2, lFlags) ;

	// only prop1 has META_PROP_STOPWORDS
	ctxtWithStop.search( lNoFilterPropList1, 2, "Peter the Great", 2, lFlags ) ;
}

void TestMultiFT::insertToSearchMap(TSearchInfo &lInfo, PropertyID pPropID, Tstring pSearchKey, bool pClean)
{
	if(pClean) lInfo.clear();
	lInfo.insert(std::map<PropertyID, std::string>::value_type(pPropID, pSearchKey));
}
