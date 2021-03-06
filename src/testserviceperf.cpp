#include "app.h"
#include "tests.h"
using namespace std;
#include "mvauto.h"


#define NUM_PROPS 4
#define NUM_PINS 30000 // presently 'selectComm' works fine with 30, but freezes with 300 (bug #422-c9).
#define MAX_STR_LEN 200
#define MAX_COL_SIZE 100
#define MAX_MAP_SIZE 10

class TestServicePerf : public ITest, public MVTApp
{
    public:
        TEST_DECLARE(TestServicePerf);
        virtual char const * getName() const { return "TestServicePerf"; }
        virtual char const * getHelp() const { return ""; }
        virtual char const * getDescription() const { return "testing the performance of services"; }
        virtual bool includeInSmokeTest(char const *& pReason) const {return false; }
        virtual bool isLongRunningTest()const {return false;}
        virtual bool includeInMultiStoreTests() const { return false; }
        virtual int execute();
        virtual void destroy() { delete this; }
    protected:
        void populate();
        void doTest();
        void selectComm();
        void longStreamComm();
        void multipleListenerComm();
        static THREAD_SIGNATURE threadProc(void * pInfo);
        URIID ids[NUM_PROPS];       
    private:
        ISession * mSession;
        typedef std::vector<PID> TPID;
};
TEST_IMPLEMENT(TestServicePerf, TestLogger::kDStdOut);

int TestServicePerf::execute()
{
    doTest();
    
    return RC_OK;
}

void TestServicePerf::doTest()
{
    bool bStarted = MVTApp::startStore() ;
    if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }
    mSession = MVTApp::startSession();

    selectComm();
    longStreamComm();
    multipleListenerComm();

    mSession->terminate(); 
    MVTApp::stopStore();
}

void TestServicePerf::populate(){
    int i;
    
    /* map property URIs */
    URIMap pmaps[NUM_PROPS];
    MVTApp::mapURIs(mSession, "TestServicePerf.prop", NUM_PROPS, pmaps);
    for (i = 0; i < NUM_PROPS; i++)
        ids[i] = pmaps[i].uid;

    static const ValueType types[4] = {VT_INT, VT_DOUBLE, VT_STRING,/* VT_MAP,*/VT_COLLECTION};

	IBatch *batch=NULL;

    for (i = 0; i < NUM_PINS; i++) {

        if ((i % 10) == 0)
            mLogger.out() << "." << std::flush;

		if (batch==NULL) {
			batch=mSession->createBatch();
			TVERIFY(batch!=NULL);
		}
        
        // create pin with random number of properties, with random value 
        int nProps = MVTRand::getRange(1, NUM_PROPS);
		Value *values = batch->createValues(nProps);
		TVERIFY(values!=NULL);

        for (int j = 0; j < nProps; j++) {
            ValueType type = types[j];
            switch(type)
            {
                case VT_INT: 
                    {
                        SETVALUE(values[j], ids[j], i, OP_SET);
                        break;
                    }
                case VT_DOUBLE: 
                    {
                        SETVALUE(values[j], ids[j], MVTRand::getDoubleRange(-1000, 1000), OP_SET);
                        break;
                    }                
                case VT_STRING:
                    {
                        std::string str = MVTRand::getString2(1, MAX_STR_LEN, false);
						char *p=(char*)batch->malloc(str.size()+1);
						TVERIFY(p!=NULL);
						strcpy(p,str.c_str());
                        SETVALUE(values[j], ids[j], p, OP_SET);
                        break;
                    }
                #if 0 // comment out due to kernel bug of modify VT_MAP
                case VT_MAP:
                    {
                        // random map size, sequecial keys, random strings
                        int nElt = MVTRand::getRange(1, MAX_MAP_SIZE);
                        vals = (MapElt *)mSession->malloc(sizeof(MapElt)*nElt);
                        vector<string> strings(nElt);
                        for (int c=0; c<nElt; c++) {
                            vals[c].key.set(c);
                            MVTRand::getString(strings[c], 1, MAX_STR_LEN, false);
                            vals[c].val.set(strings[c].c_str());
                        }
                        TVERIFYRC(mSession->createMap(vals, nElt, map));
                        values[j].set(map); values[j].setPropID(ids[j]);
                        break;
                    }
                #endif
                case VT_COLLECTION:
                    {
                        // random collection size, random string elements
                        int nElt = MVTRand::getRange(1, MAX_COL_SIZE);
						Value *elts =  batch->createValues(nElt);
                        for (int c=0; c<nElt; c++){
                            std::string str = MVTRand::getString2(1, MAX_STR_LEN, false);
							char *p=(char*)batch->malloc(str.size()+1);
							TVERIFY(p!=NULL);
							strcpy(p,str.c_str());
                            SETVALUE_C(elts[c], ids[j], p, OP_SET, STORE_LAST_ELEMENT);
                        }
						values[j].set(elts,nElt); values[j].setPropID(ids[j]);
                        break;
                    }
                default:
                    assert(0);
                    break;
            }
        }
		TVERIFYRC(batch->createPIN(values,nProps));
		if (batch->getSize()>=0xA00000) {
			TVERIFYRC(batch->process()); batch=NULL;
		}
    }

	if (batch!=NULL) {
		TVERIFYRC(batch->process());
	}

    // define classes
    mLogger.out() << "selectComm: define class... " << endl;
    IStmt* stmt = mSession->createStmt("CREATE CLASS testserviceperf_all AS SELECT * WHERE EXISTS($0)", ids, 1);
    TVERIFY(stmt!=NULL);
    TVERIFYRC(stmt->execute());
    stmt->destroy();

    TVERIFY(countStmt(mSession,"SELECT * FROM testserviceperf_all") == NUM_PINS);
}

//#define LOCALHOST "127.0.0.1"
#define PORT 8095
/*
  * socket communication for random pins(with int, double, string, collection, map properties)
  */
void TestServicePerf::selectComm()
{
    char sql[2048],address[200];CompilationError ce;
    ICursor *res=NULL;
    IStmt *qry=NULL,*stmt=NULL;
    IPIN *pin1=NULL,*pin2=NULL;
    int i = 0;

    mLogger.out() << "Start testservicesperf selectComm... " << endl;

    // create a lot of pins
    populate();
    
    // create a listener on localhost
    sprintf(address, "%d", PORT);
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "CREATE LISTENER testservicesperf_t1_l1 ON %s AS {.srv:sockets,.srv:pathSQL,.srv:affinity,.srv:protobuf,.srv:sockets}", address);
    TVERIFYRC(execStmt(mSession, sql));

    // create a CPIN of sending a SELECT request
    memset(sql, 0, sizeof(sql));
    sprintf(address, "%d", PORT);
    sprintf(sql, "INSERT afy:service={.srv:pathSQL,.srv:sockets,.srv:protobuf},afy:address=%s,\
afy:request=${SELECT * FROM testserviceperf_all ORDER BY afy:pinID}, testservicesperf_t1_p2=1", address);
    if(NULL == (qry = mSession->createStmt(sql, NULL, 0, &ce))) {
        cerr << "Error!!! testservicesperf::selectComm()::line: " << __LINE__ << " ::" << "CE: " << ce.rc << " line:" << ce.line << " pos:" << ce.pos << " " << ce.msg << " query := \"" << sql << "\n";
        TVERIFY(false);
        return;
    } 
    TVERIFYRC(qry->execute());
    if(qry!=NULL) qry->destroy();
    
    stmt = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservicesperf_t1_p2)");
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin1 = res->next()) != NULL) ;
    if (res != NULL) res->destroy();
    if(stmt!=NULL) stmt->destroy();
    
    // select from this CPIN and verify the result
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "SELECT * FROM @" _LX_FM, (pin1->getPID()).pid);
    if(NULL == (qry = mSession->createStmt(sql))) {
        TVERIFY(false);
        return;
    }

    TVERIFYRC(qry->execute(&res));

    // validate results...
    {
      CmvautoPtr<IStmt> lLocalStmt(mSession->createStmt("SELECT * FROM testserviceperf_all ORDER BY afy:pinID"));
      CmvautoPtr<ICursor> lLocalRes;
      TVERIFYRC(lLocalStmt->execute(lLocalRes));
      
      while (pin2 = res->next())
      {
        i++;
        
        CmvautoPtr<IPIN> lDirect(lLocalRes->next());
        TVERIFY(pin2->getPID().pid == lDirect->getPID().pid);
        if (lDirect.IsValid())
        {
          if (!MVTApp::equal(*lDirect.Get(), *pin2, *mSession))
          {
            MVTApp::outputComparisonFailure(lDirect->getPID(), *lDirect.Get(), *pin2, mLogger.out());
            TVERIFY(false && "Detected difference between 'remotely' & 'locally' fetched PIN");
          }
          else
            if (NUM_PINS<=10000) mLogger.out() << "PIN comparison for PID=" << std::hex << lDirect->getPID().pid << std::dec << " passed" << std::endl;
			else if ((i%1000)==0) mLogger.out() << i << " pins passed correctly" << std::endl;
        }
        else
          TVERIFY(false && "Failed to retrieve local PIN");
        
          pin2->destroy();
        
      }
    }

    TVERIFY(i == NUM_PINS);
    cout << i << endl;
    if (res != NULL) res->destroy();
    if(qry!=NULL) qry->destroy();    
    
    mLogger.out() << "testservicesperf selectComm finish! " << endl;
}

/*
  * Interface to manipulate a random stream
  */
class MyRandomStream : public Afy::IStream
{
    protected:
        size_t const mLength;
        const char* mPath;
        size_t mSeek;
        std::ofstream mOut;
    public:
        MyRandomStream (size_t pLength, const char* pPath=NULL):mLength(pLength),mPath(pPath),mSeek(0){
           /* with a valid path means this stream will be written in this file */
           if (mPath!=NULL)   
               mOut.open(mPath,std::ios::out);
        }
        ~MyRandomStream(){if(mOut.is_open()) mOut.close();}
        virtual ValueType dataType() const { return VT_BSTR; }
        virtual uint64_t length() const { return mLength; }
        virtual size_t read(void * buf, size_t maxLength) 
        { 
            size_t const lLength = min(size_t(mLength - mSeek), maxLength); 
            Tstring lStr; 
            MVTRand::getString(lStr, (int)lLength, (int)lLength, true, false);
            for (size_t i = 0; i < lLength; i++) 
                ((char *)buf)[i] = lStr[i];
            mSeek += lLength;
            // output the strings to a file
            mOut << (char *)buf;
            return lLength;
        }
        virtual size_t readChunk(uint64_t pSeek, void * buf, size_t maxLength) { mSeek = (size_t)pSeek; return read(buf, maxLength);}
        virtual IStream * clone() const { return new MyRandomStream(mLength,mPath); }
        virtual RC reset() {mSeek = 0; return RC_OK; }
        virtual void destroy() { delete this; }
};

/*
  * socket communication for pins with long string/stream property.
  * different stream length 1k, 2k, 5k, 10k, 50k, 100k,etc, will be tested.
  */
#define STREAM_NUM 6
static int g_stream_size[STREAM_NUM] = {1024, 1024*2, 1024*5, 1024*10, 1024*20, 1024*30};
void TestServicePerf::longStreamComm(){
    char sql[200],address[20],fname[30], pname[40];
    CompilationError ce; PID pid;
    ICursor *res=NULL;Value val[2];
    IStmt *qry=NULL,*stmt=NULL;
    IPIN *pin1=NULL,*pin2=NULL;
    URIID ids[2];URIMap pmaps[2];

    mLogger.out() << "Start testservicesperf longStreamComm... " << endl;

    for (unsigned int i = 0; i < sizeof(g_stream_size)/sizeof(int); i++) {
        /* 
          * we already have a listener : testservicesperf_t1_l1, created in selectComm().
          * create some pins with long stream 
          */
        CREATEPIN(mSession, &pid, NULL, 0);
        pin1 = mSession->getPIN(pid);
        sprintf(pname, "TestServicePerf.LongStream.prop%d",i);
        MVTApp::mapURIs(mSession, pname, 1, pmaps);
        ids[0] = pmaps[0].uid;

        sprintf(pname, "testservicesperf_longstream_p%d",i);
        pmaps[1].URI = pname;
        TVERIFYRC(mSession->mapURIs(1,&pmaps[1]));
        ids[1] = pmaps[1].uid;

        sprintf(fname, "testlongstreamcomm_%d.txt",g_stream_size[i]);
        IStream *mystream = new MyRandomStream(g_stream_size[i], fname);
        SETVALUE(val[0], ids[0], mystream, OP_SET);
        SETVALUE(val[1], ids[1], 1, OP_SET);
        TVERIFYRC(pin1->modify(val, 2));
        pin1->destroy();
        if(mystream) mystream->destroy();

        // create a CPIN of sending a SELECT request
        sprintf(address, "%d", PORT);
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "INSERT afy:service={.srv:pathSQL,.srv:sockets,.srv:protobuf},afy:address=%s,\
    afy:request=${SELECT * WHERE EXISTS(%s)}, testservicesperf_longstream_cpin%d=1", address,pname,i);
        if(NULL == (qry = mSession->createStmt(sql, NULL, 0, &ce))) {
            cerr << "Error!!! testservicesperf::longStreamComm()::line: " << __LINE__ << " ::" << "CE: " << ce.rc << " line:" << ce.line << " pos:" << ce.pos << " " << ce.msg << " query := \"" << sql << "\n";
            TVERIFY(false);
            return;
        }

        TVERIFYRC(qry->execute());
        if(qry!=NULL) qry->destroy();

        memset(sql, 0, sizeof(sql));
        sprintf(sql, "SELECT RAW * WHERE EXISTS(testservicesperf_longstream_cpin%d)",i);
        TVERIFY((stmt = mSession->createStmt(sql))!=NULL);
        TVERIFYRC(stmt->execute(&res));
        TVERIFY((pin1 = res->next()) != NULL) ;
        if(res != NULL) res->destroy();
        if(stmt!=NULL) stmt->destroy();
        
        // select from this CPIN and verify the result
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "SELECT * FROM @" _LX_FM, (pin1->getPID()).pid);
        if(NULL == (qry = mSession->createStmt(sql))) {
            TVERIFY(false);
            return;
        }

        TVERIFYRC(qry->execute(&res));
		if (res!=NULL) {
			TVERIFY((pin2 = res->next()) != NULL);
			if (pin2!=NULL)
				cout << (pin2->getValue(ids[0]))->length << endl;
			TVERIFY(res->next() == NULL);

	        //verify the result with the output file
			if (pin2!=NULL) {
			    std::ifstream result;
				result.open(fname,std::ios::in);
			    uint32_t fLen = (pin2->getValue(ids[0]))->length;
				char *buf = NULL;
				TVERIFY((buf = (char *)mSession->malloc(fLen)) != NULL);
		        result.read(buf, fLen);
			    TVERIFY(strncmp(buf, (char *)(pin2->getValue(ids[0]))->bstr, fLen) == 0);
				memset(buf, 0, fLen);
		        result.read(buf, fLen);
			    TVERIFY(strncmp(buf, "\0", fLen) == 0);
				TVERIFY(result.eof()== 1);
				if(buf) mSession->free(buf);
				result.close();
			}
		}

        if(pin1) pin1->destroy();
        if(pin2) pin2->destroy();        
        if (res != NULL) res->destroy();
        if(qry!=NULL) qry->destroy();
    }
    mLogger.out() << "testservicesperf longStreamComm finish! " << endl;
}

/*
  * Test case for testing multiple listeners on a same store:
  * 1. create 100 listeners on a same store's different ports.
  * 2. create 10 threads, every thread create a CPIN and send a SELECT to one port.
  */
struct CPINThreadInfo 
{
    Afy::IAffinity *mStoreCtx;
    uint32_t  val;
    uint32_t  port;
    URIID id;
};

THREAD_SIGNATURE TestServicePerf::threadProc(void * pInfo)
{
    char sql[300],address[20];
    IStmt *qry=NULL,*stmt=NULL;
    ICursor *res=NULL; 
    IPIN *pin1=NULL,*pin2=NULL;
	RC rc;
    
    CPINThreadInfo *lInfo = (CPINThreadInfo *)pInfo;
    ISession *mSession = MVTApp::startSession(lInfo->mStoreCtx);
    uint32_t val = lInfo->val;
    uint32_t  port = lInfo->port;
    URIID id = lInfo->id;

    // create a CPIN of sending a SELECT request
    sprintf(address, "%d", port);
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "INSERT afy:service={.srv:pathSQL,.srv:sockets,.srv:protobuf},afy:address=%s,\
afy:request=${SELECT * FROM testservicesperf_t3_c1 WHERE testservicesperf_t3_p1=%d}, \
testservicesperf_multilisteners_cpin%d=1", address,val,val);
    if(NULL == (qry = mSession->createStmt(sql))) {
        assert(false);
        return 0;
    }

    rc = qry->execute(); assert(rc==RC_OK);
    qry->destroy();

    memset(sql, 0, sizeof(sql));
    sprintf(sql, "SELECT RAW * WHERE EXISTS(testservicesperf_multilisteners_cpin%d)",val);
    stmt = mSession->createStmt(sql); assert(stmt!=NULL);
    rc=stmt->execute(&res); assert(rc==RC_OK);
	if (res!=NULL) {
		pin1 = res->next(); assert(pin1 != NULL) ;
		res->destroy();
	}
    if(stmt!=NULL) stmt->destroy();
    
    // select from this CPIN and verify the result
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "SELECT * FROM @" _LX_FM, (pin1->getPID()).pid);
    if(NULL == (qry = mSession->createStmt(sql))) {
        assert(false);
        return 0;
    }

    rc=qry->execute(&res); assert(rc==RC_OK);
	if (res != NULL) {
		pin2 = res->next(); assert(pin2 != NULL);
		if (pin2 != NULL) 
			assert((pin2->getValue(id))->ui == val);
		assert(res->next() == NULL);
		res->destroy();
	}
	qry->destroy();
    mSession->terminate();
    return 0;
}

#define NUM_LISTENERS 100 // when this numer exceeds ~155, kernel returns WSAENOBUFS (10055)
#define NUM_THREADS 10 
void TestServicePerf::multipleListenerComm(){
    char sql[200],address[20];
    int i = 0;
    URIMap pmap[1];
    
    mLogger.out() << "Start testservicesperf multipleListenerComm... " << endl;
    
    // create multiple listeners on localhost
    for(i = 0; i < NUM_LISTENERS; i++) {
        sprintf(address, "%d", PORT+1+i);
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "CREATE LISTENER testservicesperf_t3_l%d ON %s AS {.srv:sockets,.srv:pathSQL,.srv:affinity,.srv:protobuf,.srv:sockets}", i, address);
        TVERIFYRC(execStmt(mSession, sql));
    }

    for(i = 0; i < NUM_LISTENERS; i++) {
        sprintf(sql,"INSERT testservicesperf_t3_p1=%d", i);
        TVERIFYRC(execStmt(mSession, sql));
    }
    TVERIFYRC(execStmt(mSession, "CREATE CLASS testservicesperf_t3_c1 as SELECT * WHERE EXISTS(testservicesperf_t3_p1)"));

    // now create some threads, every thread will send a SELECT request to a listener
    HTHREAD *lThread = new HTHREAD[NUM_THREADS];
    CPINThreadInfo *lInfo = new CPINThreadInfo[NUM_THREADS];
    mStoreCtx = MVTApp::getStoreCtx();
    int base = (NUM_LISTENERS>NUM_THREADS)?MVTRand::getRange(0,NUM_LISTENERS-NUM_THREADS-1):0;
    pmap[0].URI = "testservicesperf_t3_p1";
    TVERIFYRC(mSession->mapURIs(1,&pmap[0]));
    for(unsigned int i=0;i<NUM_THREADS;i++){
        lInfo[i].mStoreCtx = mStoreCtx;
        lInfo[i].val = NUM_LISTENERS>NUM_THREADS?(i+base):(i%(NUM_LISTENERS-1)+base); // value to query
        lInfo[i].port = NUM_LISTENERS>NUM_THREADS?(i+PORT+1+base):(i%(NUM_LISTENERS-1)+PORT+1+base); // on which port
        lInfo[i].id = pmap[0].uid; // property id
        createThread(&threadProc,&lInfo[i],lThread[i]);
    }
    MVTestsPortability::threadsWaitFor(NUM_THREADS,lThread);  

    delete [] lThread;
    delete [] lInfo;
       
    mLogger.out() << "testservicesperf multipleListenerComm finish! " << endl;
}
