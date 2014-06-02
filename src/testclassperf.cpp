/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

// Publish this test.
class TestClassPerf : public ITest
{
	public:
		RC mRCFinal;
		TEST_DECLARE(TestClassPerf);
		virtual char const * getName() const { return "testclassperf"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test for classes perf"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "More of a perf test..."; return false; }
		virtual bool includeInBashTest(char const *& pReason) const { pReason = "Need to restart the store for true pass..."; return false; }
		virtual bool isLongRunningTest() const { return true; }
		virtual bool includeInPerfTest() const { return true; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void mapProps(ISession *session);
		void createPINS(ISession *session);
		bool testclassperf(ISession *session, int pWhich);
	protected:
		URIMap pm[2];
		DataEventID cls;
};
#define lAllClassNotifs (CLASS_NOTIFY_JOIN | CLASS_NOTIFY_LEAVE | CLASS_NOTIFY_CHANGE | CLASS_NOTIFY_DELETE | CLASS_NOTIFY_NEW)
TEST_IMPLEMENT(TestClassPerf, TestLogger::kDStdOut);

int TestClassPerf::execute()
{
	mRCFinal = RC_FALSE;
	
	if (MVTApp::startStore())
	{
		mRCFinal = RC_OK;
		ISession * const session = MVTApp::startSession();
		mapProps(session);
		cls = STORE_INVALID_CLASSID;
		//if (!testclassperf(session, 3))
		{
			// First time test has been run
			createPINS(session);
			if (!testclassperf(session, 3))
			{
				std::cout << "Unexpected: didn't retrieve pins created in first pass!" << std::endl;
				mRCFinal = RC_FALSE ;
			}
		}
		session->terminate();
		MVTApp::stopStore();
	}
	if (MVTApp::startStore())
	{
		// Perform just the 
		ISession * const session = MVTApp::startSession();
		mapProps(session);
		if (!testclassperf(session, 1))
		{
			std::cout << "Unexpected: didn't retrieve pins created in first pass!" << std::endl;
			mRCFinal = RC_FALSE ;
		}

		session->terminate();
		MVTApp::stopStore();
	}
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		mapProps(session);
		if (!testclassperf(session, 2))
		{
			std::cout << "Unexpected: didn't retrieve pins created in first pass!" << std::endl;
			mRCFinal = RC_FALSE ;
		}
		session->terminate();
		MVTApp::stopStore();
	}
 	return mRCFinal;
}
void TestClassPerf::mapProps(ISession *session)
{
	memset(pm,0,2*sizeof(URIMap));
	pm[0].URI = "TestClassPerf.Prop1";
	pm[1].URI = "TestClassPerf.Prop2";
	if (RC_OK != session->mapURIs(2,pm))
		std::cout << "Error: failed in mapURIs!" << std::endl;

	// cls won't be valid first time this test is run, but will be fixed later
	session->getDataEventID("TestClassPerf.classperf",cls);
	session->enableClassNotifications(cls,lAllClassNotifs);
}
void TestClassPerf::createPINS(ISession *session)
{
	Value val[2];
	Value args[2];
	PropertyID pids[1];

	// Create the class query, which look for "class pin" as the value of property "TestClassPerf.Prop1"
	IStmt *classquery = session->createStmt();
	unsigned var = classquery->addVariable();
	Tstring str;

	pids[0]=pm[0].uid;
	args[0].setVarRef(0,*pids);
	args[1].set("class pin");
	IExprNode *classopexistexpr = session->expr(OP_EQ,2,args);
	classquery->addCondition(var,classopexistexpr);

	if (RC_OK != defineClass(session,"TestClassPerf.classperf",classquery,&cls))
		std::cout << "Error: failed in defineClass" << std::endl;
	if (RC_OK != session->enableClassNotifications(cls,lAllClassNotifs))
		std::cout << "Error: failed in getDataEventID" << std::endl;
	classquery->destroy();
	classopexistexpr->destroy();

	// Create a huge number of pins, which are committed in batches.  
	// 1% of them will have "class pin" as a property, and hence belong to "classperf"

	std::vector<IPIN *> lUncommitted;
	static int const sNumPins = 100000;
	int i;
	IBatch *lBatch=session->createBatch();
	TVERIFY(lBatch!=NULL);  	
	for (i=0;i<sNumPins;i++){
		if (0 == i % 100)
			mLogger.out() << ".";

		//create pin for class.
		if (((double)100.0 * rand() / RAND_MAX) <= 1.0)
		{
			val[0].set("class pin");val[0].setPropID(pm[0].uid);
			TVERIFYRC(lBatch->createPIN(val,1,MODE_COPY_VALUES));
		}
		//create ordinary pin.
		else
		{
			MVTRand::getString(str,10,0,true);
			// Note: avoid lengthy ft-indexing...
			val[0].set((unsigned char *)str.c_str(), (unsigned)str.length());val[0].setPropID(pm[0].uid);
			val[1].set((unsigned char *)str.c_str(), (unsigned)str.length());val[1].setPropID(pm[1].uid);
			TVERIFYRC(lBatch->createPIN(val,2,MODE_COPY_VALUES));
		}
	}
	std::cout << std::endl;
	if (RC_OK != lBatch->process())
			std::cout << "Error: failed committing pin!" << std::endl;		
}
bool TestClassPerf::testclassperf(ISession *session, int pWhich)
{
	// You can run either or both queries
	bool bRunClassQ = 0 != ( pWhich & 1 ) ;
	bool bRunFullScanQ = 0 != ( pWhich & 2 ) ;

	std::set<uint64_t> lFound;  // Contains accumulated pids found in Class Query
	Value val[2];
	ICursor *result;
	clock_t lBef, lAft;

	if (bRunClassQ)
	{
		// Case 1 : class query
		IStmt * const query = session->createStmt();
		SourceSpec cs;
		cs.objectID = cls;
		cs.nParams = 0;
		cs.params = NULL;
		query->addVariable(&cs, 1);
		{
			lBef = getTimeInMs();
			TVERIFYRC(query->execute(&result));
			lAft = getTimeInMs();
		}
		std::cout << (bRunFullScanQ ? "testclass perf: class query (followed by full scan): " : "testclass perf: class query (alone)): ") << lAft-lBef << "ms" << std::endl;
		if (result)
		{
			IPIN *next;
			for (next = result->next(); next; next = result->next())
			{
				lFound.insert(next->getPID().pid);
				next->destroy();
			}
			result->destroy();
		}
		query->destroy();

		std::cout << "Class Query found : " << (unsigned int)lFound.size() << " pins" << std::endl;
	}
	
	int lCount = 0;
	if (bRunFullScanQ)
	{
		//fullscan query
		PropertyID pids1[1];
		IStmt * const fsquery = session->createStmt();
		unsigned const var2 = fsquery->addVariable();
		pids1[0] = pm[0].uid;
		val[0].setVarRef(0,*pids1);
		val[1].set("class pin");
		IExprNode *expr = session->expr(OP_EQ,2,val);
		fsquery->addCondition(var2,expr);
		{
			lBef = getTimeInMs();
			TVERIFYRC(fsquery->execute(&result));
			lAft = getTimeInMs();
		}
		expr->destroy();
		std::cout << (bRunClassQ ? "testclass perf: fullscan query (preceded by class query): " : "testclass perf: fullscan query (alone): " ) << (lAft-lBef) << "ms" << std::endl;
		if (result)
		{
			IPIN *next;
			for (next = result->next(); next; next = result->next())
			{
				// If ran both queries make sure that the results match
				if ((bRunClassQ) && lFound.end() == lFound.find(next->getPID().pid))
				{
					std::cout << "Error: pin " << std::hex << next->getPID().pid << " unexpected!" << std::endl;
					mRCFinal = RC_FALSE ;
				}
				next->destroy();
				lCount++;
			}
			if ((bRunClassQ) && lCount != (int)lFound.size())
			{
				std::cout << "Error: did not find the same number of pins (" << (int)lFound.size() << " vs " << lCount << ")" << std::endl;
				mRCFinal = RC_FALSE ;
			}
			result->destroy();
		}
		else if (bRunClassQ)
		{
			std::cout << "Error: did not get results for full scan!" << std::endl;
			mRCFinal = RC_FALSE ;
		}

		std::cout << "Full Scan Query found : " << lCount << " pins" << std::endl;

		fsquery->destroy();
	}

	if ( bRunClassQ ) 
		return !lFound.empty();
	else 
		return (lCount > 0);
}
