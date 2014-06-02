#include "app.h"
using namespace std;

#include "mvauto.h"

/*

Repro:

1. define 3 props, a,b,c
2. commit 600+ pins, each of which has random integer values for the three properties
3. clone the above 600+ pins for 100 times, such that each pin roughly has 100+ duplicates and in total there are 60000+ pins in store
4. define classes with queries "select where a in :0", "select where b in :0", "select where c in :0"
5. during defining the classes, errors occur

*/

// Publish this test.
class TestIndexRollback : public ITest
{
	public:
		TEST_DECLARE(TestIndexRollback);
		virtual char const * getName() const { return "TestIndexRollback"; }
		virtual char const * getHelp() const { return "please always add -newstore in the arguments"; }
		virtual char const * getDescription() const { return "error on creating class families for a lot of PINs"; }

		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Simple test..."; return true; }
		virtual bool isLongRunningTest()const {return false;}		
		virtual bool includeInMultiStoreTests() const { return false; }

		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void doTest();

	private:
		ISession * mSession;
};
TEST_IMPLEMENT(TestIndexRollback, TestLogger::kDStdOut);

int TestIndexRollback::execute()
{
	doTest();
	return RC_OK;
}

void TestIndexRollback::doTest()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	mSession = MVTApp::startSession();
	char *class1 = "TestIndexRollback.CLASS1";
	DataEventID clsid = STORE_INVALID_CLASSID;

	URIMap pmaps[20];
	MVTApp::mapURIs(mSession,"TestIndexRollback",3,pmaps);
	URIID ids[20];
	ids[0] = pmaps[0].uid; ids[1] = pmaps[1].uid; ids[2] = pmaps[2].uid;

	IStmt *query;
	query = mSession->createStmt("select where exists($0) AND exists($1) AND exists($2)",ids,3);
	defineClass(mSession,class1,query,&clsid);
	query->destroy();

	IPIN *pins[650];

	for (int i = 0; i < 6; i++)
	{
		Value va[3];
		va[0].set(int((double)MVTRand::getRange(1,RAND_MAX-2) / RAND_MAX * 999999)); va[0].property = ids[0];
		va[1].set(int((double)MVTRand::getRange(1,RAND_MAX-2) / RAND_MAX * 999999)); va[1].property = ids[1];
		va[2].set(int((double)MVTRand::getRange(1,RAND_MAX-2) / RAND_MAX * 999999)); va[2].property = ids[2];
		TVERIFYRC(mSession->createPIN(va,3,&pins[i],MODE_COPY_VALUES)); // Note (maxw): If MODE_COPY_VALUES is not specified, mvstore assumes that va is allocated via ISession::alloc.
	}
	IPIN *cpins[650];
	for (int i = 0; i < 60000/6+1; i++)
	{
		for (int j = 0; j < 6; j++)
			cpins[j] = pins[j]->clone(NULL,0,MODE_PERSISTENT);
		for (int j = 0; j < 6; j++)
			cpins[j]->destroy();
		std::cout << "." << flush;
	}
	for (int j = 0; j < 6; j++)
	{
		pins[j]->destroy();
		pins[j] = NULL;
	}
	
	query = mSession->createStmt();
	SourceSpec spec;
	spec.objectID = clsid; spec.nParams = 0; spec.params = NULL;

	Tstring st;
	std::cout<<"\n starting classification" << endl;
	for (int i = 0; i < 3; i++)
	{
		query = mSession->createStmt("select where $0 in :0",&ids[i],1);
		Value va[2];
		va[0].set(query); va[0].property = PROP_SPEC_PREDICATE; va[0].meta = META_PROP_INDEXED;
		MVTRand::getString(st,10,0,true);
		va[1].set(st.c_str()); va[1].property = PROP_SPEC_OBJID;
		mSession->createPIN(va,2,NULL,MODE_PERSISTENT|MODE_COPY_VALUES);
		std::cout << char('$'+i) << flush;
	}
	std::cout << "\n starting deletion" << endl;
	query = mSession->createStmt(STMT_DELETE);
	spec.objectID = clsid; spec.nParams = 0; spec.params = NULL;
	query->addVariable(&spec,1);
	query->execute(NULL,NULL,0,~0U,0,MODE_PURGE);
	query->destroy();

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
}
