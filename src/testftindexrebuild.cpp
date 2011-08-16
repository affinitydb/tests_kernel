/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h"
#include "mvauto.h"

class TestFTIndexRebuild : public ITest
{
		static const int sNumProps = 10;
		PropertyID mPropIds[sNumProps];
		std::string lfilePath;
	public:
		TEST_DECLARE(TestFTIndexRebuild);
		virtual char const * getName() const { return "testftindexrebuild"; }
		virtual char const * getHelp() const { return ""; }
		
		virtual char const * getDescription() const { return "Functionality test for rebuilding of FTIndex"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "test fails currently"; return false; }
		virtual void destroy() { delete this; }	
		virtual int execute();
	protected:
		void testFTRebuild(ISession *session);
		void verifyFTIndex(ISession *session);
		void verifyMiscOps(ISession *session);
		bool verify2(ISession *session,char * search, long expcount);
};
TEST_IMPLEMENT(TestFTIndexRebuild, TestLogger::kDStdOut);

int TestFTIndexRebuild::execute()
{	
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		ISession *session = MVTApp::startSession();		
		MVTApp::mapStaticProperty(session,"TestFTIndexRebuild.prop1",mPropIds[0]);
		MVTApp::mapStaticProperty(session,"TestFTIndexRebuild.prop2",mPropIds[1]);
		verifyMiscOps(session);
		testFTRebuild(session);
		session->terminate();
		MVTApp::stopStore();
	}
	return lSuccess?0:1;
}
void TestFTIndexRebuild::testFTRebuild(ISession *session)
{
	lfilePath = "../textdata/great_expectations.txt";
	CFileStream *lfilestream = new CFileStream(lfilePath,false);
	IStream *stream = MVTApp::wrapClientStream(session,lfilestream);
	PID pid;
	TVERIFYRC(session->createPIN(pid,NULL,0));

	Value val[1];
	IPIN *pin = session->getPIN(pid);
	val[0].set(stream);val[0].setPropID(mPropIds[0]);

	TVERIFYRC(pin->modify(val,1));
	mLogger.out() << std::endl << "Verification pre-rebuild..." << std::endl;
	verifyFTIndex(session);
	TVERIFYRC(session->rebuildIndexFT());
	mLogger.out() << std::endl << "Verification post-rebuild..." << std::endl;
	verifyFTIndex(session);
}
void TestFTIndexRebuild::verifyFTIndex(ISession *session)
{
	static char const * const search[] =
	{
		"Pirrip","Gargery","mother","fancies","grave","marsh",
		"memorable","Bartholomew","scattered","shivers","briars",
		"tombstone","roasted","pecooliar","gibbet","apron","improbable",
		"elixir","garret","blubbered","penknife","Pumblechook","scornful",
		"fireside","Sundays","Mooncalfs","Havisham","unpromoted","triumphantly",
		"discernible","countenance","politely omitting","forcible argumentation",
		"muffins","to express","something unwonted","bearing his poverty","unacquainted",
		"prosperous","where Wemmick","Estella","Richmond","natural","barrack way",
		"thankfulness","tranquil",
	};
	size_t i;
	for (i=0; i<sizeof(search) / sizeof(search[0]); i++){
		IStmt *query = session->createStmt();
		unsigned char var = query->addVariable();
		query->setConditionFT(var,search[i],MODE_ALL_WORDS);
		uint64_t cnt = 0;
		query->count(cnt);
		query->destroy();
		if (cnt == 0){
			std::cout<<"FT Returned Zero results for search "<<search[i]<<std::endl;
			TVERIFY(false);
		}
	}
}
void TestFTIndexRebuild::verifyMiscOps(ISession *session)
{
	Value val[4];
	//case 1: META_PROP_NOFTINDEX should be honored
	IPIN *pin = session->createUncommittedPIN();
	val[0].set("Wollongabba");val[0].setPropID(mPropIds[1]);val[0].meta = META_PROP_NOFTINDEX;
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	pin->destroy();
	TVERIFYRC(session->rebuildIndexFT());
	TVERIFY(verify2(session,"Wollongabba",0));

    //case 2: META_PROP_STOPWORDS should be honored
	pin = session->createUncommittedPIN();
	val[0].set("brisbane is somewhere");val[0].setPropID(mPropIds[1]);val[0].meta = META_PROP_STOPWORDS;
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	pin->destroy();
	TVERIFYRC(session->rebuildIndexFT());
	TVERIFY(verify2(session,"somewhere",0));

    //case 3: PIN_NOINDEX should be honored
	pin = session->createUncommittedPIN();
	val[0].set("adeleide is somewhere");val[0].setPropID(mPropIds[1]);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1,PIN_NO_INDEX));
	pin->destroy();
	TVERIFYRC(session->rebuildIndexFT());
	TVERIFY(verify2(session,"adeleide",0));
	//case 4: Extended char set tests
}
bool TestFTIndexRebuild::verify2(ISession *session,char * search, long expcount)
{
	IStmt *query = session->createStmt();
	unsigned char var = query->addVariable();
	uint64_t cnt =0;
	query->addConditionFT(var,search,MODE_ALL_WORDS|QFT_FILTER_SW);
	query->count(cnt);
	if (cnt != uint64_t(expcount))
	{
		query->destroy();
		return false;
	}
	query->destroy();
	return true;
}
