/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#define NUMBEROFPINS 100000
using namespace std;

class TestFTIndexPage : public ITest
{
	public:
		RC mRCFinal;
		PropertyID propid[1];
		std::vector<Tstring> mString;
		TEST_DECLARE(TestFTIndexPage);
		virtual	char const * getName() const { return "testftindexpage"; }
		virtual	char const * getHelp() const { return ""; }
		virtual	char const * getDescription() const	{ return "test for ftindexing of lots of pins"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual bool isLongRunningTest()const {return true;}		
		virtual	int	execute();
		virtual	void destroy() { delete	this; }
	protected:
		void populateStore(ISession *session);
		void testFTIndexPage(ISession *session);
};
TEST_IMPLEMENT(TestFTIndexPage, TestLogger::kDStdOut);

int	TestFTIndexPage::execute()
{
	mRCFinal = RC_FALSE;
	if (MVTApp::startStore())
	{
		mRCFinal = RC_OK;
		Value args[2];		
		uint64_t rescount =0;

		ISession * const session = MVTApp::startSession();
		Tstring lStr;
		MVTRand::getString(lStr,10,10,false,false);
		mString.push_back(lStr);
		IStmt *query =	session->createStmt();
		unsigned var = query->addVariable();
		
		// query for control pin, if exists do not populate store.
		// Note: fullscan...
		URIMap lData;
		lData.URI = "TestFTIndexPage.prop"; lData.uid = STORE_INVALID_PROPID;
		session->mapURIs(1, &lData);
		propid[0] = lData.uid;	
		
		args[0].setVarRef(0,*propid);
		args[1].set("testftindexpagepin");		
		IExprTree *expr = session->expr(OP_EQ,2,args);
		query->addCondition(var,expr);
		query->count(rescount);
		
		if (0 == rescount){
			populateStore(session);
		}
		else
		{
			mLogger.out() << "Control PIN found. Continuing with query" << std::endl;
		}
		expr->destroy();
		query->destroy();
		testFTIndexPage(session);

		session->terminate();
		MVTApp::stopStore();
	}
		
	return mRCFinal;
}

void TestFTIndexPage::testFTIndexPage(ISession *session)
{
	uint64_t rescount;
	IStmt *query =	session->createStmt();
	unsigned var = query->addVariable();
	query->setConditionFT(var,"goldflake");
	mLogger.out() << "Counting... ";
	query->count(rescount);
	mLogger.out() << "Done." << std::endl;
	
	if (NUMBEROFPINS != rescount)
		mRCFinal = RC_FALSE;
}

void TestFTIndexPage::populateStore(ISession *session)
{
	Value pvs[2];
	string titleStr;
	PID	pid;
	//create a control pin to allow this test run multiple times w.o failure.
	pvs[0].set("testftindexpagepin");pvs[0].setPropID(propid[0]);
	TVERIFYRC(session->createPIN(pid,pvs,1));

	URIMap pm[1];
	/*
	int npm = 1;
	memset(pm,0,npm*sizeof(URIMap));
	pm[0].URI="TestFTIndexPage.title";
	session->mapURIs(sizeof(pm)/sizeof(pm[0]),pm);
	*/
	MVTApp::mapURIs(session,"TestFTIndexPage.prop",1,pm);

	mLogger.out() << "Populating" << std::endl;
	for(int	i =0; i<NUMBEROFPINS;i++)
	{
		if (0 == i % 100)
			mLogger.out() << ".";

		IPIN *pin = session->createUncommittedPIN();
		titleStr = "" ;
		char buff[7];
		titleStr +=	"goldflake kings";
		sprintf(buff,"%d",i);
		titleStr +=	buff;

		//create the pins here.
		pvs[0].set(titleStr.c_str());pvs[0].setPropID(pm[0].uid);
		pin->modify(pvs,1);
		session->commitPINs(&pin,1);
		pin->destroy();
	}
	mLogger.out() << std::endl;
}
