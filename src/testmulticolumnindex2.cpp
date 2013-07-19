#include "app.h"
using namespace std;

#include "mvauto.h"

/*

Repro:

1. define such a class family:
	select * where propa in :0(BSTR) and propb in :1(BSTR)
2. commit a PIN with both properties non-null, say ("99999.99999", "-99999.99999")
3. retrieve from the class family with the above values being the parameters. Specifically:
	Range 1 = ["99999.99999", "99999.99999"] 
	Range 2 = ["-99999.99999", "-99999.99999"]
4. Result set is EMPTY

Remark:

The following part in idxscan.cpp will be executed:

			else if (key!=NULL) {
				if (!tp->findKey(*key,index)) {
					if (index>=tp->info.nSearchKeys) fAdv=true;
					else if (!checkBounds(tp,true)) {pb.release(); return RC_EOF;}
				} else if (skip==NULL && (state&SCAN_EXCLUDE_START)!=0 && ++index>=tp->info.nSearchKeys) fAdv=true;

- tp->findKey() will return false. context: the prefix of current page is "1199999.9999912-99999.99999" and search key is exactly the prefix
- checkBounds() will return false. context: the finish search key is exactly equal to the prefix of the page, and scan order is forward
*/

// Publish this test.
class TestMultiColumnIndex2 : public ITest
{
	public:
		TEST_DECLARE(TestMultiColumnIndex2);
		virtual char const * getName() const { return "TestMultiColumnIndex2"; }
		virtual char const * getHelp() const { return "please always add -newstore in the arguments"; }
		virtual char const * getDescription() const { return "error on retrieving a PIN with multi-column index"; }

		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Simple test..."; return true; }
		virtual bool isLongRunningTest()const { return false; }
		virtual bool includeInMultiStoreTests() const { return false; }

		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void doTest();

	private:
		ISession * mSession;
};
TEST_IMPLEMENT(TestMultiColumnIndex2, TestLogger::kDStdOut);

int TestMultiColumnIndex2::execute()
{
	doTest();
	return RC_OK;
}

void TestMultiColumnIndex2::doTest()
{
	//int pass = 1;
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	mSession = MVTApp::startSession();
	//char *class1 = "TestMultiColumnIndex2.CLASS1";
	ClassID clsid = STORE_INVALID_CLASSID, clsid2 = STORE_INVALID_CLASSID;

	URIMap pmaps[20];
	MVTApp::mapURIs(mSession,"TestMultiColumnIndex2",3,pmaps);
	URIID ids[20];
	ids[0] = pmaps[0].uid; ids[1] = pmaps[1].uid; ids[2] = pmaps[2].uid;

	IStmt *query = mSession->createStmt("select * where $0 IN :0(BSTR) and $1 IN :1(BSTR) and $2 IN :2(BSTR)",ids,3);
	
	this->defineClass(mSession,"TestMultiColumnIndex2.class",query,&clsid);

	query->destroy();

	query = mSession->createStmt("select * where $0 IN :0(BSTR)",ids,1);
	
	this->defineClass(mSession,"TestMultiColumnIndex2.class2",query,&clsid2);

	query->destroy();

	Value va[3];

	va[0].set((unsigned char*)"99999.99999",11); va[0].property = ids[0]; 
	va[1].set((unsigned char*)"-99999.99999",12); va[1].property = ids[1]; 
	va[2].set((unsigned char*)"-77777.77777",12); va[2].property = ids[2]; 

	PID pid;
	mSession->createPINAndCommit(pid,va,3);

	SourceSpec spec;

	Value rng[10];

	rng[0] = va[0]; rng[1] = va[0]; rng[6].setRange(&rng[0]);
	rng[2] = va[1]; rng[3] = va[1]; rng[7].setRange(&rng[2]);
	rng[4] = va[2]; rng[5] = va[2]; rng[8].setRange(&rng[4]);

	spec.objectID = clsid; spec.params = &rng[6]; spec.nParams = 3;

	query = mSession->createStmt();

	query->addVariable(&spec,1);

	ICursor *res = NULL;
	TVERIFYRC(query->execute(&res));

	IPIN *pin = res->next(); 

	TVERIFY(pin && pin->getPID() == pid);

	if (pin) pin->destroy();

	res->destroy(); 
	spec.objectID = clsid2; spec.nParams = 1;

	query->destroy();
	query = mSession->createStmt();
	query->addVariable(&spec,1,NULL);
	query->execute(&res);
	pin = res->next(); 

	TVERIFY(pin && pin->getPID() == pid);
	if (pin) pin->destroy();

	res->destroy(); 
	query->destroy();

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
	//pass++;
	//if (pass <= 3) goto start1;
}
