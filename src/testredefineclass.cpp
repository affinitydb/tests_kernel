#include "app.h"
using namespace std;

#include "mvauto.h"

/*
The in-mem class object man

*/

// Publish this test.
class TestRedefineClass : public ITest
{
	public:
		TEST_DECLARE(TestRedefineClass);
		virtual char const * getName() const { return "TestRedefineClass"; }
		virtual char const * getHelp() const { return "please always add -newstore in the arguments"; }
		virtual char const * getDescription() const { return "testing redefining classes"; }

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
TEST_IMPLEMENT(TestRedefineClass, TestLogger::kDStdOut);

int TestRedefineClass::execute()
{
	doTest();
	return RC_OK;
}

void TestRedefineClass::doTest()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	mSession = MVTApp::startSession();
	URIMap pmaps[20];
	MVTApp::mapURIs(mSession,"TestRedefineClass",10,pmaps);
	URIID ids[20];
	for (int i = 0; i < 10; i++)
		ids[i] = pmaps[i].uid;

	{
		Value va[3];
		DataEventID clsid;
		PID pid1;
		IPIN *pin1;
		uint64_t cnt;
		IStmt *qry;

		va[0].set(20); va[1].set(20); va[2].setRange(va); //search range [20,20] for property1


		mSession->startTransaction(); //create c1 and c1.index
			const char *classStr= "CREATE CLASS \"TestRedefineClass.class1\" AS SELECT * WHERE EXISTS($0) AND EXISTS($1)"; 
			TVERIFYRC(mSession->execute(classStr,strlen(classStr),NULL,ids,2));
			const char *indexStr = "CREATE CLASS \"TestRedefineClass.class1.index\" AS SELECT * FROM \"TestRedefineClass.class1\" WHERE $0 IN :0(INT) AND $1 IN :1(INT)";
			TVERIFYRC(mSession->execute(indexStr,strlen(indexStr),NULL,ids,2));
		mSession->commit(true);

		mSession->startTransaction(); //search the class before insertion, and finally rollback the trans
			qry = mSession->createStmt("SELECT FROM \"TestRedefineClass.class1.index\"");
			qry->count(cnt,&va[2],1);
			assert(cnt==0); //before insertion, should be 0

			const char *insertStr = "INSERT $0=20,$1=20;";
			TVERIFYRC(mSession->execute(insertStr,strlen(insertStr),NULL,ids,2));

			qry->count(cnt,&va[2],1);
			assert(cnt==1); //after insertion should be 1
			qry->destroy();
		mSession->rollback(true); //we rollback here!


		mSession->startTransaction(); //drop c1.index, then delete all pins belonging to c1, then drop c1
			mSession->getDataEventID("TestRedefineClass.class1.index",clsid);
			mSession->getDataEventInfo(clsid,pin1);
			pid1 = pin1->getPID(); pin1->destroy();
			mSession->deletePINs(&pid1,1,MODE_PURGE); //drop c1.index

			qry = mSession->createStmt("DELETE FROM \"TestRedefineClass.class1\"");
			qry->execute(NULL,NULL,0U,~0U,0U,MODE_PURGE);
			qry->destroy(); //deleting all PINS

			mSession->getDataEventID("TestRedefineClass.class1",clsid);
			mSession->getDataEventInfo(clsid,pin1);
			pid1 = pin1->getPID(); pin1->destroy();
			mSession->deletePINs(&pid1,1,MODE_PURGE); //drop c1
		mSession->commit(true); 

		//redefining c1, and then define c1.index2
		mSession->startTransaction();
			classStr= "CREATE CLASS \"TestRedefineClass.class1\" AS SELECT * WHERE EXISTS($0) OR EXISTS($1)"; 
			TVERIFYRC(mSession->execute(classStr,strlen(classStr),NULL,ids,2));
			indexStr = "CREATE CLASS \"TestRedefineClass.class1.index2\" AS SELECT * FROM \"TestRedefineClass.class1\" WHERE $0 IN :0(INT,NULLS FIRST) AND $1 IN :1(INT, NULLS FIRST)";
			TVERIFYRC(mSession->execute(indexStr,strlen(indexStr),NULL,ids,2));
		mSession->commit(true);

		mSession->startTransaction(); //insertion
			qry = mSession->createStmt("SELECT FROM \"TestRedefineClass.class1.index2\"");
			qry->count(cnt,&va[2],1);
			assert(cnt == 0);//before insertion
	
			insertStr = "INSERT $0=20,$1=20;";
			TVERIFYRC(mSession->execute(insertStr,strlen(insertStr),NULL,ids,2));

			qry->count(cnt,&va[2],1);
			assert(cnt == 1);
			qry->destroy(); //after insertion
		mSession->commit(true);

		//a crash here... create table t1 (c1 int not null, c2 int null, index i1 (c1), index i2(c2));
		//drop i1, drop i2, drop t1, then re-create them all and drop again
		for (int iter = 0; iter < 10; iter++)
		{
			mSession->startTransaction();
			classStr= "CREATE CLASS \"TestRedefineClass.class3\" AS SELECT * WHERE EXISTS($0)"; 
			TVERIFYRC(mSession->execute(classStr,strlen(classStr),NULL,&ids[7],1));
			indexStr = "CREATE CLASS \"TestRedefineClass.class3.index1\" AS SELECT * FROM \"TestRedefineClass.class3\" WHERE $0 IN :0(INT)";
			TVERIFYRC(mSession->execute(indexStr,strlen(indexStr),NULL,&ids[7],1));
			indexStr = "CREATE CLASS \"TestRedefineClass.class3.index2\" AS SELECT * FROM \"TestRedefineClass.class3\" WHERE $0 IN :0(INT,NULLS FIRST)";
			TVERIFYRC(mSession->execute(indexStr,strlen(indexStr),NULL,&ids[8],1));
			mSession->commit(true);

			insertStr = "INSERT $0=20,$1=20;";
			TVERIFYRC(mSession->execute(insertStr,strlen(insertStr),NULL,&ids[7],2));

			mSession->getDataEventID("TestRedefineClass.class3.index1",clsid);
			mSession->getDataEventInfo(clsid,pin1);
			pid1 = pin1->getPID(); pin1->destroy();
			mSession->deletePINs(&pid1,1,MODE_PURGE); //drop i1

			mSession->getDataEventID("TestRedefineClass.class3.index2",clsid);
			mSession->getDataEventInfo(clsid,pin1);
			pid1 = pin1->getPID(); pin1->destroy();
			mSession->deletePINs(&pid1,1,MODE_PURGE); //drop i2


			qry = mSession->createStmt("DELETE FROM \"TestRedefineClass.class3\"");
			qry->execute(NULL,NULL,0U,~0U,0U,MODE_PURGE);
			qry->destroy(); //deleting all PINS

			mSession->getDataEventID("TestRedefineClass.class3",clsid);
			mSession->getDataEventInfo(clsid,pin1);
			pid1 = pin1->getPID(); pin1->destroy();
			mSession->deletePINs(&pid1,1,MODE_PURGE); //drop c1
		}


		//another crash here... create table t1 (c1 int, c2 int, c3 int);  create, insert, and drop...
		for (int iter = 0; iter < 10; iter++) 
		{
			classStr= "CREATE CLASS \"TestRedefineClass.class2\" AS SELECT * WHERE EXISTS($0) OR EXISTS($1) OR EXISTS($2)"; 
			TVERIFYRC(mSession->execute(classStr,strlen(classStr),NULL,&ids[4],3));

			for (int i = 0; i < 100; i++) //insert 100 PINs to this class
			{
				insertStr = "INSERT $0=20,$1=20,$2=20;";
				TVERIFYRC(mSession->execute(insertStr,strlen(insertStr),NULL,&ids[4],3));
			}

			qry = mSession->createStmt("DELETE FROM \"TestRedefineClass.class2\"");
			qry->execute(NULL,NULL,0U,~0U,0U,MODE_PURGE);
			qry->destroy(); //deleting all PINS

			mSession->getDataEventID("TestRedefineClass.class2",clsid);
			mSession->getDataEventInfo(clsid,pin1);
			pid1 = pin1->getPID(); pin1->destroy();
			mSession->deletePINs(&pid1,1,MODE_PURGE); //drop c1
		}	

	}

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
}
