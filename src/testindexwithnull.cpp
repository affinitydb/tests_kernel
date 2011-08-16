#include "app.h"
using namespace std;

#include "mvauto.h"

/*

Repo:
A. create class family X select where propA IN :0 (ORD_NULLS_BEFORE)
B. insert pin propA=20
C. select from X([20,20])
D. returned empty result set

REMARK:
single column index can not have null keys, but kernel still put null as a key internally

		if (fF) {
			const SearchKey *key=skip!=NULL?skip:start; if (findPage(key)!=RC_OK) return RC_EOF;
			const TreePageMgr::TreePage *tp=(const TreePageMgr::TreePage*)pb->getPageBuf(); //assert(key==NULL||tp->cmpKey(*key)==0);
			if (tp->info.nSearchKeys==0) fAdv=true;
			else if (key!=NULL) {
				if (!tp->findKey(*key,index)) {
					if (index>=tp->info.nSearchKeys) fAdv=true;
					else if (!checkBounds(tp,true)) {pb.release(); return RC_EOF;}
				} else if (skip==NULL && (state&SCAN_EXCLUDE_START)!=0 && ++index>=tp->info.nSearchKeys) fAdv=true;
			}
When the key is found, index = 0 and total number of keys in the page is 1. The state has flag SCAN_EXCLUDE_START, thus fAdv becomes true.

*/

// Publish this test.
class TestIndexWithNull : public ITest
{
	public:
		TEST_DECLARE(TestIndexWithNull);
		virtual char const * getName() const { return "TestIndexWithNull"; }
		virtual char const * getHelp() const { return "please always add -newstore in the arguments"; }
		virtual char const * getDescription() const { return "incorrect null handling in index"; }

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
TEST_IMPLEMENT(TestIndexWithNull, TestLogger::kDStdOut);

int TestIndexWithNull::execute()
{
	doTest();
	return RC_OK;
}

void TestIndexWithNull::doTest()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	mSession = MVTApp::startSession();
	URIMap pmaps[20];
	MVTApp::mapURIs(mSession,"TestIndexWithNull",10,pmaps);
	URIID ids[20];
	for (int i = 0; i < 10; i++)
		ids[i] = pmaps[i].uid;

	{
		IStmt *qry = mSession->createStmt();
		Value va[5];
		va[0].setVarRef(0,ids[0]);
		va[1].setParam(0);
		IExprTree *expr = mSession->expr(OP_IN,2,va);
		QVarID qid = qry->addVariable(NULL,0,expr);
		expr->destroy();

		va[0].setVarRef(0,ids[1]);
		va[1].setParam(1,VT_INT,ORD_NULLS_BEFORE);
		expr = mSession->expr(OP_IN,2,va);
		qry->addCondition(qid,expr);
		expr->destroy();

		va[0].set("TestIndexWithNull.class1"); va[0].property = PROP_SPEC_URI;
		va[1].set(qry); va[1].property = PROP_SPEC_PREDICATE;
		PID pid;
		mSession->createPIN(pid,va,2);

		std::cout << qry->toString() << std::endl;
		
		qry->destroy();
	
		va[0].set(20); va[1].set(30);
		va[0].property = ids[0]; va[1].property = ids[1];
		mSession->createPIN(pid,va,1);

		va[0].set(30); va[1].set(30);
		va[0].property = ids[0]; va[1].property = ids[1];
		mSession->createPIN(pid,va,2);

		qry = mSession->createStmt("select where $0 in :0 ( INT, NULLS FIRST )",&ids[1],1);

		va[0].set("TestIndexWithNull.class2"); va[0].property = PROP_SPEC_URI;
		va[1].set(qry); va[1].property = PROP_SPEC_PREDICATE;

		mSession->createPIN(pid,va,2);

		std::cout << qry->toString() << std::endl;
		
		qry->destroy();

		qry = mSession->createStmt();
		ClassSpec spec;
		mSession->getClassID("TestIndexWithNull.class1",spec.classID);
		spec.nParams = 0; spec.params = NULL;
		qry->addVariable(&spec,1);
		uint64_t cnt;
		qry->count(cnt);
		qry->destroy();
		TVERIFY(cnt == 2);
		qry = mSession->createStmt();
		mSession->getClassID("TestIndexWithNull.class2",spec.classID);
		va[1].set(30); va[2].set(30); va[0].setRange(&va[1]);
		spec.nParams = 1; spec.params = va;
		qry->addVariable(&spec,1);
		qry->count(cnt);
		qry->destroy();
		TVERIFY(cnt == 1);
	}

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
}
