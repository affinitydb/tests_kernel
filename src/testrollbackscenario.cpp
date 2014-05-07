/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h"

#define NTHREAD 10
#define NPINS 20
#define NSESSION 9

#define CRITICAL_SECTION(expr) lck->lock(); {expr} lck->unlock();
#define TRY_LOCK(lock) 	while(!lock->trylock()) {MVTestsPortability::threadSleep(5);}

class testrollbackscenario;

typedef struct info
{
	unsigned int mSeed;
	testrollbackscenario *thisPtr;
}ThreadInfo;

class testrollbackscenario : public ITest
{
	protected:
		static const unsigned int nProps = 10;
		PropertyID mPropIDs[nProps];
		Afy::IAffinity *mCtx;
		ISession *mSession[NSESSION];
		ClassID clsid,familyid;
		MVTestsPortability::Mutex *lck;
		MVTestsPortability::Event *evnt;
		unsigned ses_count;
		bool mStarted,finish;
	public:
		TEST_DECLARE(testrollbackscenario);
		virtual char const * getName() const { return "testrollbackscenario"; }
		virtual char const * getHelp() const { return "run the test"; }		
		virtual char const * getDescription() const { return "nested transaction scenario with rollback thread"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "pins with large data set,time consuming"; return false; }
		virtual bool isLongRunningTest()const {return true;}
		virtual bool includeInMultiStoreTests() const { return true; }
		virtual void destroy() { delete this; }		
		virtual int execute();
		void commit_pins();
		void trans_rollback();
		static THREAD_SIGNATURE threadCommitPIN(void * pParam)
		{ 
			unsigned int seed = ((ThreadInfo *)pParam)->mSeed;
			srand(seed);
			(((ThreadInfo *)pParam)->thisPtr)->commit_pins();	
			return 0;
		}
		static THREAD_SIGNATURE threadRollBack(void *pParam)
		{ 
			((testrollbackscenario *)pParam)->trans_rollback(); 
			return 0;
		}
};

TEST_IMPLEMENT(testrollbackscenario, TestLogger::kDStdOut);

void testrollbackscenario::trans_rollback()
{
	while(1)
	{
        TRY_LOCK(lck);
		while( (!mStarted) && (ses_count >=NSESSION) ) evnt->wait(*lck,0); 
	
		//CRITICAL_SECTION(
		ISession* lSession = NULL;
		unsigned int idx = rand()%ses_count; //mLogger.out()<<"Idx::"<<idx<<endl;
		lSession = mSession[idx];
	
		TVERIFYRC(lSession->attachToCurrentThread());
		mLogger.out()<<"Going for rollback..\n";
		TVERIFYRC(lSession->rollback());
		TVERIFYRC(lSession->detachFromCurrentThread());
		//)
		lck->unlock();
		//going to sleep.
		MVTestsPortability::threadSleep(30);
		if(finish) break;
	}
}
void testrollbackscenario::commit_pins()
{
	TRY_LOCK(lck);
	ISession *lSession = mSession[ses_count++] = MVTApp::startSession(mCtx);
	if(lSession == NULL) {
		mLogger.out()<<"Failed to create session\n";
		return;
	}
	lck->unlock();

	Tstring str;
	mLogger.out()<<"Creating pins..\n";
	TVERIFYRC(lSession->startTransaction());
	for(int i=0;i<NPINS;i++)
	{
		TRY_LOCK(lck);	
		//mLogger.out()<<"Attaching session\n";
		TVERIFYRC(lSession->attachToCurrentThread());
		
		int pCnt = (MVTRand::getRange(1,10)%nProps)+1;
		Value *lV = (Value *)lSession->malloc(pCnt * sizeof(Value));

		//mLogger.out()<<"Start transaction\n";
		TVERIFYRC(lSession->startTransaction());
		
		//Stream
		if(pCnt > 0) {
			SETVALUE(lV[0], mPropIDs[0], MVTApp::wrapClientStream(lSession, new TestStringStream(30000)), OP_SET);
		}

		//String
		if(pCnt > 1) {
			str = MVTRand::getString2(30000,30000);
			Tstring sstr = str.substr(0, 100);
			SETVALUE(lV[1],mPropIDs[1],sstr.c_str(),OP_SET); // Note: mPropIDs[1] is indexed by a family, so don't abuse...
			if (pCnt > 2) {
				SETVALUE(lV[2],mPropIDs[2],str.c_str(),OP_SET); // Note: but it's ok to test FT...
			}
		}

		//DateTime
		if(pCnt > 3) {
			lV[3].setDateTime(MVTRand::getDateTime(lSession,true));
			lV[3].setPropID(mPropIDs[3]);
		}

		//rest are of int/uint/uint64
		for(int j=4;j<pCnt;j++) {
			SETVALUE(lV[j],mPropIDs[j],MVTRand::getRange(10,20000),OP_SET);
		}

		//Commit half of the pins without indexing.
		if(i < NPINS/2)
			TVERIFYRC(lSession->createPIN(lV,pCnt,NULL,PIN_HIDDEN|MODE_PERSISTENT|MODE_COPY_VALUES));
		else
			TVERIFYRC(lSession->createPIN(lV,pCnt,NULL,MODE_PERSISTENT|MODE_COPY_VALUES));

		TVERIFYRC(lSession->commit());
		lSession->free(lV);
		mLogger.out()<<"C";
		TVERIFYRC(lSession->detachFromCurrentThread());
	
		if(i == 0){mStarted = true; evnt->signalAll();}
		lck->unlock();
		MVTestsPortability::threadSleep(10);
	}

	TRY_LOCK(lck);
	TVERIFYRC(lSession->attachToCurrentThread());
	lSession->commit();
	mLogger.out()<<"\nDone.\n";
	TVERIFYRC(lSession->detachFromCurrentThread());
	lck->unlock();
}

int testrollbackscenario::execute()
{
	if(!MVTApp::startStore()) {
		mLogger.out()<<"Failed to start store\n";
		return -1;
	}
	
	//init task
	mCtx = MVTApp::getStoreCtx();
	ses_count = 0;
	mStarted = false; finish = false;

	ISession *lSession = MVTApp::startSession(mCtx);
	if(NULL == lSession) {
		mLogger.out()<<"Failed to create session\n";
		return -2;
	}

	lck = new MVTestsPortability::Mutex();
	evnt = new MVTestsPortability::Event();

	MVTApp::mapURIs(lSession,"testrollbackscenario",nProps,mPropIDs);

	//Create a class.
	{
		IStmt *query = lSession->createStmt();
		unsigned char v = query->addVariable();
	
		query->setPropCondition(v,&(mPropIDs[0]),1);

		if( RC_NOTFOUND == lSession->getClassID("testrollbackscenario.basic.class",clsid))
		{
			mLogger.out()<<"Defining basic class..\n";
			TVERIFYRC(defineClass(lSession,"testrollbackscenario.basic.class",query,&clsid));
			mLogger.out()<<"Done.\n";
		}
		query->destroy();
	}

	//Create a family
	{
		IStmt *query = lSession->createStmt();
		unsigned char v = query->addVariable();
		Value args[2];
		
		args[0].setVarRef(0,(mPropIDs[1]));
		args[1].setParam(0);
		IExprNode *expr = lSession->expr(OP_BEGINS,2,args,CASE_INSENSITIVE_OP);
		
		query->addCondition(v,expr);

		if( RC_NOTFOUND == lSession->getClassID("testrollbackscenario.basic.family",familyid))
		{
			mLogger.out()<<"Defining basic family..\n";
			TVERIFYRC(defineClass(lSession,"testrollbackscenario.basic.family",query,&familyid));
			mLogger.out()<<"Done.\n";
		}
		query->destroy();
	}

	HTHREAD lThread[NTHREAD];
	for(int i=0;i<NTHREAD - 1;i++) {
		ThreadInfo *tCtx = new ThreadInfo();
		tCtx->mSeed = MVTRand::getRange(1,30000);
		tCtx->thisPtr = this;
		createThread(threadCommitPIN,tCtx,lThread[i]);
	}

	createThread(threadRollBack,this,lThread[NTHREAD - 1]);

	MVTestsPortability::threadsWaitFor(NTHREAD - 1,lThread);
	
	finish = true;
	MVTestsPortability::threadsWaitFor(1,&(lThread[NTHREAD-1]));

	//clean up
	for(int i=0;i<NSESSION;i++)
	{
		mSession[i]->attachToCurrentThread();
		mSession[i]->terminate();
	}
	delete lck; delete evnt;

	MVTApp::stopStore();
	return 0;
}
