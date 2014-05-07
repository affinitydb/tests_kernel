#include "app.h"
using namespace std;

#include "mvauto.h"

/*
try to repro an error in mvEngine: 1. create a multi-column class family :  create table t1(id int not null,  \
			id2 int not null, index i1(id,id2)) engine=MVEngine;  2. insert a tuple into the table: insert into t1(id,id2) values(10,20); \
			3. see whether commitPINs goes wrong.\
			The class in this test case is defined on the following query:\
			SELECT * \
			WHERE PROP1 IN :0 \
				AND PROP2 IN :1 \
				AND PROP3 IN :2"
*/

// Publish this test.
class TestMultiColumnIndex1 : public ITest
{
	public:
		TEST_DECLARE(TestMultiColumnIndex1);
		virtual char const * getName() const { return "testmulticolumnindex1"; }
		virtual char const * getHelp() const { return "please always add -newstore in the arguments"; }
		virtual char const * getDescription() const { return "error on inserting tuple into a table with multi-column index"; }

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
TEST_IMPLEMENT(TestMultiColumnIndex1, TestLogger::kDStdOut);

static char *propNames[] = {"TestMultiColumnIndex1.PROP1","TestMultiColumnIndex1.PROP2","TestMultiColumnIndex1.PROP3","TestMultiColumnIndex1.PROP4","TestMultiColumnIndex1.PROP5",
	"TestMultiColumnIndex1.PROP6","TestMultiColumnIndex1.PROP7","TestMultiColumnIndex1.PROP8","TestMultiColumnIndex1.PROP9","TestMultiColumnIndex1.PROP10"};

int TestMultiColumnIndex1::execute()
{
	doTest();
	return RC_OK;
}

void TestMultiColumnIndex1::doTest()
{
	//int pass = 1;
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	mSession = MVTApp::startSession();
	char *class1 = "TestMultiColumnIndex1.CLASS1";
	ClassID clsid = STORE_INVALID_CLASSID;
	if (mSession->getClassID(class1,clsid) != RC_OK)
	{
		//first time run this test...		
		Afy::PropertyID props[10];
		Afy::URIMap pmaps[10];
		
		for (int i = 0; i < 10; i++)
		{
			pmaps[i].URI = propNames[i];
			pmaps[i].uid = STORE_INVALID_URIID;
		}
		AfyRC::RC rc;
		mSession->mapURIs(10, pmaps);
		for (int i = 0; i < 10; i++)
			props[i] = pmaps[i].uid;

		IStmt *qry; 
		qry = mSession->createStmt();
		QVarID var = qry->addVariable();
		for (int i = 0; i < 3; i++)
		{
			Value val[2];
			val[0].setVarRef(0,pmaps[i].uid); val[1].setParam(i);
			IExprNode *exprt = mSession->expr(Afy::OP_IN,2,val);
			qry->addCondition(var,exprt);
			exprt->destroy();
		}
		rc = defineClass(mSession,class1,qry,&clsid);
		printf("defined query: %s\n",qry->toString());
		TVERIFY(rc==RC_OK);
		qry->destroy();

		Value vals[10];
		for (int i = 0; i < 3; i++)
		{
			Value &va = vals[i];
			va.set((int)i);
			va.setOp(OP_SET);
			va.setPropID(pmaps[i].uid); 
		}
		rc = mSession->createPIN(vals,3,NULL,MODE_PERSISTENT|MODE_COPY_VALUES);
		TVERIFY(rc == RC_OK);
	}
	 
	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
	//pass++;
	//if (pass <= 3) goto start1;
}
