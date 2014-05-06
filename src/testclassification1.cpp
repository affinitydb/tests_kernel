#include "app.h"
using namespace std;

#include "mvauto.h"

/*
1. create a class, and commit a few PINs belonging to the class. \
2. shutdown store.  3. Restart store. 4. commit a few more PINs belonging to the class. 5. see whether classification goes wrong
Note(maxw): very similar to the very old testcustom1
*/

// Publish this test.
class TestClassification1 : public ITest
{
	public:
		TEST_DECLARE(TestClassification1);
		virtual char const * getName() const { return "testClassification1"; }
		virtual char const * getHelp() const { return "please always add -newstore in the arguments"; }
		virtual char const * getDescription() const { return "inserted tuple missing from the table in mvEngine"; }

		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Simple test..."; return true; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual bool isLongRunningTest()const {return false;}		
		virtual bool includeInMultiStoreTests() const { return false; }

		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void doTest();

	private:
		ISession * mSession;
};
TEST_IMPLEMENT(TestClassification1, TestLogger::kDStdOut);

int TestClassification1::execute()
{
	doTest();
	return RC_OK;
}

void TestClassification1::doTest()
{
	int pass = 1;
start1:
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	mSession = MVTApp::startSession();
	char *class1 = "TestClassification1.CLASS1";
	ClassID clsid = STORE_INVALID_CLASSID;
	if (mSession->getClassID(class1,clsid) != RC_OK)
	{
		//first time run this test...
		
		Afy::PropertyID prop;
		Afy::URIMap pmap;
	  
		pmap.URI = "TestClassification1.PROP1";
		pmap.uid = STORE_INVALID_URIID;
		mSession->mapURIs(1, &pmap);
		prop = pmap.uid;

		IStmt *qry; 
		uint64_t cnt;
		qry = mSession->createStmt();
		qry->addVariable();
		qry->count(cnt);
		printf("first pass: full scan retrieved %llu\n", cnt);
		qry->destroy();

		qry = mSession->createStmt();
		QVarID var = qry->addVariable();
		qry->setPropCondition(var,&prop,1);
	
		defineClass(mSession,class1,qry,&clsid);
		IBatch *arr = mSession->createBatch();
		TVERIFY(arr!=NULL);
		for (int i = 0; i < 10; i++)
		{
			Value va;
			va.set((int)i);
			va.setOp(OP_SET);
			va.setPropID(prop); 
			TVERIFYRC(arr->createPIN(&va,1,MODE_COPY_VALUES)); // Note (maxw): If MODE_COPY_VALUES is not specified, mvstore assumes that va is allocated via IBatch::createValues().
		}
		TVERIFYRC(arr->process());

		qry->destroy();
		qry = mSession->createStmt();
		SourceSpec spec;
		spec.objectID = clsid; spec.params=NULL; spec.nParams = 0;
		qry->addVariable(&spec,1);
		qry->count(cnt);
		printf("first pass: 10 pins commited in CLASS1, and %llu pins retrieved\n", cnt);
		TVERIFY(cnt==10);
		qry->destroy();

		qry = mSession->createStmt();
		qry->addVariable(&spec,1);
		Value va; va.set((int)99); va.setOp(OP_SET);va.setPropID(pmap.uid); 
		TVERIFYRC(mSession->createPIN(&va,1, NULL, MODE_COPY_VALUES|MODE_PERSISTENT));

		qry->count(cnt);
		printf("first pass: 11 pins commited in CLASS1, and %llu pins retrieved\n", cnt);
		TVERIFY(cnt==11);
		qry->destroy();
	}
	else if (pass == 2)
	{
		IStmt *qry = mSession->createStmt();
		SourceSpec spec;
		spec.objectID = clsid; spec.params=NULL; spec.nParams = 0;
		qry->addVariable(&spec,1);
		uint64_t cnt;
		qry->count(cnt);
		printf("second pass: 11 pins commited in CLASS1, and %llu pins retrieved\n", cnt);
		TVERIFY(cnt==11);
		qry->destroy();

		Value va;

		PropertyID prop;
		URIMap pmap;

		pmap.URI ="TestClassification1.PROP1";
		pmap.uid = STORE_INVALID_URIID;
		mSession->mapURIs(1,&pmap);
		va.set((int)99); va.setOp(OP_SET);va.setPropID(pmap.uid); 
		TVERIFYRC(mSession->createPIN(&va,1,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));

		qry = mSession->createStmt();
		qry->addVariable(&spec,1);
		qry->count(cnt);
		printf("second pass: commit 12 pins in CLASS1 and %llu pins are retrieved\n",cnt);
		TVERIFY(cnt==12);
		qry->destroy();

		qry = mSession->createStmt();
		qry->addVariable();
		qry->count(cnt);
		printf("second pass: full scan retrieved %llu\n", cnt);
		qry->destroy();

		qry = mSession->createStmt();
		pmap.URI = "TestClassification1.PROP2";
		pmap.uid = STORE_INVALID_URIID;
		mSession->mapURIs(1,&pmap);
		prop = pmap.uid;
		qry->setPropCondition(qry->addVariable(),&prop,1);
		defineClass(mSession,"TestClassification1.CLASS2",qry,&clsid);
		va.set((double) 77); va.setOp(OP_SET); va.setPropID(prop);
		TVERIFYRC(mSession->createPIN(&va,1,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));
		uint64_t cnt2;
		qry->count(cnt2);
		qry->destroy();

		qry = mSession->createStmt();
		spec.objectID = clsid;
		qry->addVariable(&spec,1);
		qry->count(cnt);
		qry->destroy();

		printf("second pass: commited to CLASS2 %llu, retrieved %llu\n", cnt2, cnt);
		TVERIFY(cnt2==cnt);
	}
	else
	{
		IStmt *qry = mSession->createStmt();
		SourceSpec spec;
		spec.objectID = clsid; spec.params=NULL; spec.nParams = 0;
	
		uint64_t cnt;
		Value va;

		PropertyID prop;
		URIMap pmap;

		qry = mSession->createStmt();
		pmap.URI = "TestClassification1.PROP2";
		pmap.uid = STORE_INVALID_URIID;
		mSession->mapURIs(1,&pmap);
		prop = pmap.uid;

		mSession->getClassID("TestClassification1.CLASS2",clsid);
		spec.objectID = clsid;

		qry->setPropCondition(qry->addVariable(),&prop,1);

		va.set((double) 97); va.setOp(OP_SET); va.setPropID(prop);
		TVERIFYRC(mSession->createPIN(&va,1,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));
		uint64_t cnt2;
		qry->count(cnt2);
		qry->destroy();

		qry = mSession->createStmt();
		spec.objectID = clsid;
		qry->addVariable(&spec,1);
		qry->count(cnt);
		qry->destroy();

		printf("second pass: commited to CLASS2 2, retrieved by class query %llu\n", cnt);
		TVERIFY(2==cnt);

		printf("second pass: commited to CLASS2 2, retrieved by full scan query %llu\n", cnt2);
		TVERIFY(2==cnt2);
	}

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
	pass++;
	if (pass <= 3) goto start1;
}
