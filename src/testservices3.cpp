#include "app.h"
#include "mvauto.h"
using namespace std;

class TestServices3 : public ITest , public MVTApp
{
	public:
		TEST_DECLARE(TestServices3);
		virtual char const * getName() const { return "TestServices3"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "a fake custom listener"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Not ready yet..."; return false; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void doCase1();
		void doCase2();
		void doCase3_receiver();
		void doCase3_sender();
		void doCase4_chatting();
	private:
		ISession * mSession;
};
TEST_IMPLEMENT(TestServices3, TestLogger::kDStdOut);

#define TESTSERVICES3_NAME AFFINITY_SERVICE_PREFIX "TestServices3"

class TestServices3_service : public IService
{
	protected:
		friend class TestServiceListener;
 		class TestServiceListener : public IListener
		{
			protected:
				TestServices3_service & mService; // Back-reference to the service.
				HTHREAD mThread;
				long volatile mStop;
				const URIID id;
			public:
				TestServiceListener(TestServices3_service & pService,URIID i)
					: mService(pService)
					, mThread(NULL)
					, mStop(0)
					, id(i)
				{
					std::cout << "Created a TestServiceListener " << this << std::endl;
					createThread(sThreadProc, this, mThread);
				}
				virtual ~TestServiceListener() {}
				virtual	IService *getService() const {return &mService;}
				virtual	URIID getID() const {return id;}
				virtual	RC create(IServiceCtx *ctx,uint32_t& dscr,IService::Processor *&ret) {ret=NULL; return RC_INVOP;}
				virtual RC stop(bool fSuspend=false)
				{
					// Review: fSuspend ?
					InterlockedIncrement(&mStop);
					MVTestsPortability::threadsWaitFor(1, &mThread);
					delete this; // Review: this->~TestServiceListener(); mgr.listeners.dealloc(this); ?
					return RC_OK;
				}
			protected:
				static THREAD_SIGNATURE sThreadProc(void *pThis){((TestServiceListener *)pThis)->threadProc(); return 0;}
				void threadProc()
				{
					ISession * lSession = MVTApp::startSession(mService.mCtx);
					if (NULL == lSession)
						return;
					char lStmt[2048];
					int iP = 0;
					while (!mStop)
					{
						sprintf(lStmt, "INSERT %s=%d", mService.mURIs[1].URI, iP);
						if (RC_OK != mService.mTest.execStmt(lSession, lStmt))
							std::cout << "Error while INSERTing PIN" << std::endl;
						MVTestsPortability::threadSleep(MVTRand::getRange(100, 2000));
					}
					lSession->terminate();
				}
		};
	protected:
		TestServices3 & mTest;
		IAffinity * mCtx;
		const URIMap * mURIs;
	public:
		TestServices3_service(TestServices3 & pTest, IAffinity *pCtx, URIMap *pURIs) : mTest(pTest), mCtx(pCtx), mURIs(pURIs) {}
		virtual ~TestServices3_service() {}
		virtual RC listen(ISession *ses,URIID id,const Value *params,unsigned nParams,const Value *srvParams,unsigned nSrvparams,unsigned mode,IListener *&ret)
		{
			ret = new TestServiceListener(*this,id); // Review: any obligation with respect to allocation model?
			return RC_OK;
		}
};

int TestServices3::execute()
{
	if (!MVTApp::startStore())
		{ TVERIFY2(0, "Could not start store, bailing out completely"); return -1; }
	IAffinity * lCtx = MVTApp::getStoreCtx();
	ISession * lSession = MVTApp::startSession();

	Tstring lURIrad; MVTRand::getString(lURIrad, 10, 10, false, true);
	Tstring lURIstrs[4];
	URIMap lURIs[4];
	size_t i;
	for (i=0; i< sizeof(lURIs) / sizeof(lURIs[0]); i++) { lURIstrs[i] = Tstring("testservices3") + lURIrad; lURIstrs[i].push_back('1' + i); lURIs[i].URI = lURIstrs[i].c_str(); lURIs[i].uid = STORE_INVALID_URIID; }
	TVERIFYRC(lSession->mapURIs(sizeof(lURIs) / sizeof(lURIs[0]), lURIs));

	TestServices3_service * lTService = new(lCtx) TestServices3_service(*this, lCtx, lURIs);
	TVERIFY(NULL != lTService);
	TVERIFYRC(lCtx->registerService(TESTSERVICES3_NAME, lTService));

	char lStmt[2048];
	sprintf(lStmt, "CREATE CLASS %s AS SELECT * WHERE EXISTS(%s) SET afy:onEnter=${UPDATE @self SET %s=CURRENT_TIMESTAMP}", lURIs[0].URI, lURIs[1].URI, lURIs[2].URI);
	std::cout << lStmt << std::endl;
	TVERIFYRC(execStmt(lSession, lStmt));
	sprintf(lStmt, "CREATE LISTENER %s ON 'whatever' AS {.srv:TestServices3}", lURIs[3].URI);
	TVERIFYRC(execStmt(lSession, lStmt));

	mLogger.out() << "Waiting for 10 seconds..." << std::endl;
	MVTestsPortability::threadSleep(1000 * 10);
	mLogger.out() << "  finished waiting." << std::endl;

	sprintf(lStmt, "SELECT * FROM %s", lURIs[0].URI);
	uint64_t lC1 = MVTApp::countStmt(lSession, lStmt);
	mLogger.out() << "The fake listener produced " << lC1 << " pins/events" << std::endl;
	TVERIFY(lC1 > 0);
	sprintf(lStmt, "SELECT * WHERE EXISTS(%s)", lURIs[2].URI);
	TVERIFY(MVTApp::countStmt(lSession, lStmt) == lC1);

	lSession->terminate(); 
	MVTApp::stopStore();
	return RC_OK;
}
