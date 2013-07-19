#include "app.h"
using namespace std;

#include "mvauto.h"
#include <set>
#include <algorithm>
#include <vector>
/*
-newstore -pagesize=4096 -seed=36344

a lot of pins with the same keys, force kernel to spawn and do batch inserts on spawned pages during classification
found corrupted indexes (duplicate pids in the index)
*/

// Publish this test.
class TestClassCreation : public ITest
{
	public:
		TEST_DECLARE(TestClassCreation);
		virtual char const * getName() const { return "TestClassCreation"; }
		virtual char const * getHelp() const { return "please always add -newstore in the arguments"; }
		virtual char const * getDescription() const { return "error on creating class families for a lot of PINs"; }

		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Simple test..."; return false; }
		virtual bool isLongRunningTest()const {return true;}		
		virtual bool includeInMultiStoreTests() const { return false; }

		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void doTest();

	private:
		ISession * mSession;
};
TEST_IMPLEMENT(TestClassCreation, TestLogger::kDStdOut);

int TestClassCreation::execute()
{
	doTest();
	return RC_OK;
}

void TestClassCreation::doTest()
{
	//int pass = 1;
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	mSession = MVTApp::startSession();
	//char *class1 = "TestClassCreation.CLASS1";
	ClassID clsid = STORE_INVALID_CLASSID;

	URIMap pmaps[20];
	MVTApp::mapURIs(mSession,"TestClassCreation",10,pmaps);
	URIID ids[20];
	for (int i = 0; i < 10; i++)
		ids[i] = pmaps[i].uid;

	IStmt *query;
	IPIN *pins[650];
	std::set<PID> *pins_set = new std::set<PID>();

	for (int i = 0; i < 3; i++)
	{
		Value va[6];
		std::string lS = MVTRand::getString2(7, -1, false);
		va[0].set(int((double)MVTRand::getRange(1,RAND_MAX-2) / RAND_MAX * 999999)); va[0].property = ids[0];
		va[1].set(lS.c_str()); va[1].property = ids[1];
		va[2].set(int((double)MVTRand::getRange(1,RAND_MAX-2) / RAND_MAX * 999999)); va[2].property = ids[2];
		va[3].set(lS.c_str()); va[3].property = ids[3];
		va[4].set(int((double)MVTRand::getRange(1,RAND_MAX-2) / RAND_MAX * 999999)); va[4].property = ids[4];
		pins[i] = mSession->createPIN(va,5,MODE_COPY_VALUES);
	}
	IPIN *cpins[650];
	int counter1 = 0;
	for (int i = 0; i < 2000000/3+1; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			counter1++;
			cpins[j] = pins[j]->clone();
		}
		mSession->commitPINs(cpins,3);
		PID pid = cpins[0]->getPID();
		for (int j = 0; j < 3; j++)
		{
			pins_set->insert(cpins[j]->getPID());
			cpins[j]->destroy();
		}
		if (i % 10000==0) std::cout << "." << flush;
	}

	SourceSpec spec;
	Tstring st;
	IPIN *class_pins[10];
	ClassID clsids[10];
	std::cout<<"\n starting classification" << endl;
	for (int i = 0; i < 6; i++)
	{
		if (i < 3) query = mSession->createStmt("select where $0 in :0 and $1 in :1 and $2 in :2",&ids[i],3);
		else query = mSession->createStmt("select where exists($0) and exists($1) and exists($2)",&ids[i-3],3);
		Value va[2]; MVTRand::getString(st,10,0,true);
		va[0].set(st.c_str()); va[0].property = PROP_SPEC_OBJID;
		va[1].set(query); va[1].property = PROP_SPEC_PREDICATE; va[1].meta = META_PROP_INDEXED;
		class_pins[i] = mSession->createPIN(va,2,MODE_COPY_VALUES);
		query->destroy();
	}
	mSession->commitPINs(class_pins,6);
	clsid = class_pins[0]->getValue(PROP_SPEC_OBJID)->uid;
	std::set<PID> *pid_set = new std::set<PID>();
	std::vector<PID> *vecs = new std::vector<PID>[6];
	std::vector<PID> *miss_mvecs = new std::vector<PID>[6];
	for (int i = 0; i < 6; i++)
	{
		int counter2 = 0;
		clsids[i] = class_pins[i]->getValue(PROP_SPEC_OBJID)->uid;
		clsid = clsids[i];
		pid_set->clear();
		spec.objectID = clsid; spec.nParams = 0; spec.params = NULL;
		query=  mSession->createStmt();
		query->addVariable(&spec,1,NULL);
		ICursor *res = NULL;
		query->execute(&res);
		PID pid;
		vecs[i].clear();
		while (res->next(pid) == RC_OK)
		{
			counter2++;
			std::set<PID>::iterator iter = pid_set->find(pid);
			TVERIFY(iter == pid_set->end());
			if (iter != pid_set->end())
			{
				std::cout << "In " << i << "-th class, " << counter2 << "-th PIN, ID: " << pid.pid << std::endl;
				vecs[i].push_back(pid);
			}
			pid_set->insert(pid);
		}
		TVERIFY(counter1 == counter2);
		std::set<PID>::iterator it = pins_set->begin();
		while (it != pins_set->end())
		{
			if (pid_set->find(*it) == pid_set->end())
			{
				miss_mvecs[i].push_back(*it);
			}
			it++;
		}
		res->destroy();
		query->destroy();
		class_pins[i]->destroy();
	}
	delete pid_set;
	delete pins_set;
	for (int i = 0; i < 6; i++)
	{
		if (vecs[i].size() > 0)
		{
			std::cout << "Class " << i << " has duplicate pin entries: " << endl;
			std::vector<PID>::iterator iter = vecs[i].begin();
			while (iter != vecs[i].end())
			{
				std::cout << std::hex << iter->pid << " , ";
				iter++;
			}
			std::cout << endl;
			std::cout << "Class " << i << " misses pin entries: " << endl;
			iter = miss_mvecs[i].begin();
			while (iter != miss_mvecs[i].end())
			{
				std::cout << std::hex << iter->pid << " , ";
				iter++;
			}
			std::cout << endl;
		} 
	}
	delete [] vecs;
	delete [] miss_mvecs;
	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
	//pass++;
	//if (pass <= 3) goto start1;
}
