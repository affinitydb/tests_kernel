/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

// Publish this test.
class TestSpillover : public ITest
{
	public:
		RC mRCFinal;
		TEST_DECLARE(TestSpillover);
		virtual char const * getName() const { return "testtranrollover"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test for transactions / nested tx spilling over log files"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void tranSpillover(ISession *session);
		void tranBIGPins(ISession *session);
		void countPINS (ISession *session);
};
TEST_IMPLEMENT(TestSpillover, TestLogger::kDStdOut);

// Implement this test.
int TestSpillover::execute()
{
	mRCFinal = RC_OK;
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		tranSpillover(session);
		tranBIGPins(session);
		session->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }	

 	return mRCFinal;
}
void TestSpillover::tranSpillover(ISession *session)
{
	Tstring str;
	unsigned nLoops = 5000;
	PropertyID lPropIDs[4]; MVTApp::mapURIs(session,"TestTranRollover.testSpillover",4,lPropIDs);
	Value pvs[4];
	PID pid;
	
	//create an obscene amount of pins -- hell raiser to the rollover
	for (unsigned i=0; i<nLoops;i++) {
		if (i % 100 == 0)
			mLogger.out() << "." << std::flush;
		MVTRand::getString(str,10,500,true);
		SETVALUE(pvs[0], lPropIDs[0], str.c_str(), OP_SET);
		SETVALUE(pvs[1], lPropIDs[1], str.c_str(), OP_SET);
		SETVALUE(pvs[2], lPropIDs[2], str.c_str(), OP_SET);
		SETVALUE(pvs[3], lPropIDs[3], str.c_str(), OP_SET);
		TVERIFYRC(session->createPINAndCommit(pid,pvs,4));
	}
	mLogger.out() << std::endl;
	//loop to spill over across log files.
	//case 1: simple rollback.
#if 0
	session->startTransaction(); 
		for (unsigned i=0; i<nLoops;i++) {
			if (i % 100 == 0)
				mLogger.out() << "." << std::flush;
			MVTRand::getString(str,10,1024,true);
			pvs[0].set(0,str.c_str());
			pvs[1].set(1,str.c_str());
			pvs[2].set(3,str.c_str());
			pvs[3].set(4,str.c_str());
			session->createPINAndCommit(pid,pvs,4);
		}
		mLogger.out() << std::endl;
	session->rollback();
#else
	/*case 2: second variation -- (RC_ALREADYEXISTS)shutdown goin to infinite cos
	of lSlottab being int and fid being byte and lSlotTab value being > 255 
	Happens in the FileMgr::closeAll()
	*/
	session->startTransaction(); 
		for (unsigned i=0; i<nLoops;i++) {
			if (i % 100 == 0)
				mLogger.out() << "." << std::flush;
			MVTRand::getString(str,10,500,true);
			SETVALUE(pvs[0], lPropIDs[0], str.c_str(), OP_SET);
			SETVALUE(pvs[1], lPropIDs[1], str.c_str(), OP_SET);
			SETVALUE(pvs[2], lPropIDs[2], str.c_str(), OP_SET);
			SETVALUE(pvs[3], lPropIDs[3], str.c_str(), OP_SET);
			session->createPINAndCommit(pid,pvs,4);
		}
		mLogger.out() << std::endl;
	session->rollback();
#endif

	//todo:: add test for big pins and collections.
	//add case for partial rollback in nested transactions.
}

void TestSpillover::tranBIGPins(ISession *session)
{
	//Case 3: transaction rollover with nested tx and partial rollback
	URIMap pm[1000];
	char *p,buf[1060];
	PID pid;
	Value pvs[1000];
	strcpy(buf,"this will take it beyond the limit");
	RC rc;
	string str, lRand;
	unsigned i;
	MVTRand::getString(lRand,10,10,false,false);

	for (i =0; i<200; i++) {
		//check why > 200 is not working.
		char buff[100];
		sprintf(buff,"TestBigPins.My Prop Name.%s.%d",lRand.c_str(), i);
		pm[i].uid=0;
		pm[i].URI = buff;
		rc = session->mapURIs(1,&pm[i]);
	}
	/*pvs[0].set(1,"jai jai shiv shankar");
	rc = session->createPINAndCommit(pid,pvs,1);*/
	//if the above is present then no issues with recovery.
	for (int j=0;j<200;j++) {
		p=buf+strlen(buf);
		*p++=rand()%26+((rand()&1)!=0?'a':'A');
		*p='\0';
		pvs[j].set(buf); pvs[j].setPropID(pm[j].uid);
	}
	session->startTransaction();
	rc = session->createPINAndCommit(pid,pvs,1);
	rc = session->rollback();
	countPINS(session);
}

void TestSpillover::countPINS(ISession *session)
{
	int count=0;
	
	IStmt *query = session->createStmt();
	unsigned const char lVar = query->addVariable();
	
	Value args[2];
	args[0].set("foo");
	args[1].set("foo");

	IExprTree *expr = session->expr(OP_EQ,2,args);
	query->addCondition(lVar,expr);
	ICursor *result = NULL;
       TVERIFYRC(query->execute(&result));
	for (IPIN *pin; (pin=result->next())!=NULL; ){
		count++;
		pin->destroy();
	}
	result->destroy();
	expr->destroy();
	query->destroy();
	cout<<"Number of pins "<<count<<endl;
}
