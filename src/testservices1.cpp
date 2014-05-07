#include "app.h"
#include "tests.h"
using namespace std;

#include "mvauto.h"

// Publish this test.
class TestServices1 : public ITest
{
    public:
        TEST_DECLARE(TestServices1);
        virtual char const * getName() const { return "TestServices1"; }
        virtual char const * getHelp() const { return ""; }
        virtual char const * getDescription() const { return "testing services"; }
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Simple test..."; return true; }
        virtual bool isLongRunningTest()const {return false;}
        virtual bool includeInMultiStoreTests() const { return false; }
        virtual int execute();
        virtual void destroy() { delete this; }
    protected:
        void doTest();
        void doCase1();
        void doCase2();
        void doCase3();
        void doCase4();
        void doCase5();
        void doCase6();
        void doCase7();
        void doCase8();
        void doCase9();
        URIID ids[5];
    private:
        ISession * mSession;
};
TEST_IMPLEMENT(TestServices1, TestLogger::kDStdOut);

int TestServices1::execute()
{
    doTest();
    return RC_OK;
}

void TestServices1::doTest()
{
    bool bStarted = MVTApp::startStore() ;
    if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }
    mSession = MVTApp::startSession();

    #if 1
    doCase1();
    doCase2();
    doCase3();
    doCase4();
    doCase5();
    doCase6();
    doCase8();
    doCase9();
    #endif
    #if 0
    doCase7();
    #endif
    
    mSession->terminate(); 
    MVTApp::stopStore();
}

/* 
 * Basic C++ interface test case for IO service, output to STDERR
 */
void TestServices1::doCase1()
{   
    char sql[200];
    IStmt *qry = NULL;
    CompilationError ce; 
    Value values[3];
    URIMap pmaps[1];
    
    mLogger.out() << "Start testservices1 doCase1... " << endl;

    MVTApp::mapURIs(mSession, "testservices1_t1_p1", 1, pmaps);
    values[0].setURIID(SERVICE_IO);
    values[0].property = PROP_SPEC_SERVICE;
    values[1].set(2); //STDERR
    values[1].property = PROP_SPEC_ADDRESS;
    values[2].set(1);
    values[2].property = pmaps[0].uid;
    
    PID pid={0, 0};IPIN *pin;
    TVERIFYRC(mSession->createPIN(values, 3, &pin, MODE_PERSISTENT|MODE_COPY_VALUES)); 
    pid = pin->getPID();
    TVERIFY(pid.pid != STORE_INVALID_PID); 

    memset(sql, 0, sizeof(sql));
    sprintf(sql, "CREATE TIMER testtservices_c1_t1 INTERVAL %s AS UPDATE @" _LX_FM " SET afy:content='abc\n';", "\'00:00:01\'", pid.pid);
    if(NULL == (qry = mSession->createStmt(sql, NULL, 0, &ce))) {
        cerr << "Error!!! testtservices::doCase1()::line: " << __LINE__ << " ::" << "CE: " << ce.rc << " line:" << ce.line << " pos:" << ce.pos << " " << ce.msg << " query := \"" << sql << "\n";
        TVERIFY(false);
        return;
    } 
    TVERIFYRC (qry->execute());    

    cout << "sleep half minute..." << endl;
    MVTestsPortability::threadSleep(1000*30);

    // delete CPIN
    TVERIFYRC(mSession->deletePINs(&pid, 1));
    cout << "sleep 10 seconds... nothing should be printed" << endl;
    MVTestsPortability::threadSleep(1000*10);

    if (qry != NULL) qry->destroy();
    mLogger.out() << "testservices1 doCase1 finish! " << endl;
}

/* 
 * Basic PathSQL test case for IO service, to STDERR and a plain file.
 */
#define CASE2_FILENAME "testservices1_case2.log"
void TestServices1::doCase2(){
    uint64_t cnt = 0;
    IPIN* pin = NULL;
    ICursor *res = NULL;
    IStmt *qry = NULL;
    char sql[200];
    CompilationError ce; 
    char buf[4];
    FILE *fp = NULL;

    mLogger.out() << "Start testservices1 doCase2... " << endl;

    // to STDERR
    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT afy:service=.srv:IO, afy:address=2, testservices1_t2_p1=2;"));
    IStmt *stmt = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservices1_t2_p1)");
    TVERIFYRC(stmt->count(cnt)); TVERIFY(cnt==1);
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin = res->next()) != NULL);
    
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "CREATE TIMER testtservices_c2_t1 INTERVAL %s AS UPDATE @" _LX_FM " SET afy:content='def\n';", "\'00:00:01\'", (pin->getPID()).pid);
    if(NULL == (qry = mSession->createStmt(sql, NULL, 0, &ce))) {
        cerr << "Error!!! testtservices::doCase2()::line: " << __LINE__ << " ::" << "CE: " << ce.rc << " line:" << ce.line << " pos:" << ce.pos << " " << ce.msg << " query := \"" << sql << "\n";
        TVERIFY(false);
        return;
    } 
    TVERIFYRC (qry->execute());

    cout << "sleep half minute..." << endl;
    MVTestsPortability::threadSleep(1000*30);

    if (res != NULL) res->destroy();
    if (stmt != NULL) stmt->destroy();
    if (qry != NULL) qry->destroy();

    // to a plain file
    memset(sql, 0, sizeof(sql));
    sprintf(sql,  "INSERT afy:service=.srv:IO, afy:address(CREATE_PERM,WRITE_PERM)='%s', testservices1_t2_p2=3;", CASE2_FILENAME);
    TVERIFYRC(MVTApp::execStmt(mSession,sql));
    stmt = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservices1_t2_p2)");
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin = res->next()) != NULL);
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "UPDATE @" _LX_FM " SET afy:content='ghi\n';", (pin->getPID()).pid);
    TVERIFYRC(MVTApp::execStmt(mSession,sql));

    // verify the result
    fp = fopen(CASE2_FILENAME,"r");
    TVERIFY(fp!=NULL);
    memset(buf, 0, sizeof(buf));
    TVERIFY(fread(buf, 1, sizeof(buf)-1,fp) > 0);
    cout << buf << endl;
    TVERIFY(strncmp(buf, "ghi", sizeof(buf)-1) == 0);
    if(fp!=NULL) fclose(fp);
    if (res != NULL) res->destroy();
    if (stmt != NULL) stmt->destroy();
    
    //delete CPIN
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "DELETE @" _LX_FM " ;", (pin->getPID()).pid);
    TVERIFYRC(MVTApp::execStmt(mSession,sql));
    TVERIFY(MVTApp::countStmt(mSession,"SELECT RAW * WHERE EXISTS(testservices1_t2_p2)") == 0);

    // update this CPIN again
    TVERIFYRC(MVTApp::execStmt(mSession,sql)); // shouldn't it return a RC_NOTFOUND?
    fp = fopen(CASE2_FILENAME,"rb");
    TVERIFY(fp!=NULL);
    fseek(fp,0,SEEK_SET);
    fseek(fp,0,SEEK_END);
    // no more message are written
    TVERIFY(strlen("ghi\n") == ftell(fp));    
    if(fp!=NULL) fclose(fp);
    
    mLogger.out() << "testservices1 doCase2 finish! " << endl;
}

/* 
 * Basic C++ interface test case for IO service, output to a plain file
 */
#define CASE3_FILENAME "testservices1_case3.log"
void TestServices1::doCase3()
{   
    char sql[200];
    IStmt *qry = NULL;
    CompilationError ce; 
    Value values[3];
    URIMap pmaps[1];
    FILE *fp = NULL;
    char buf[4];
    
    mLogger.out() << "Start testservices1 doCase3... " << endl;

    MVTApp::mapURIs(mSession, "testservices1_t3_p1", 1, pmaps);
    values[0].setURIID(SERVICE_IO);
    values[0].property = PROP_SPEC_SERVICE;
    values[1].set(CASE3_FILENAME);
    values[1].property = PROP_SPEC_ADDRESS;
    values[1].meta = META_PROP_CREATE | META_PROP_WRITE; // flag to create a new file
    values[2].set(1);
    values[2].property = pmaps[0].uid;
    
    PID pid={0, 0};IPIN *pin;
    TVERIFYRC(mSession->createPIN(values, 3, &pin, MODE_PERSISTENT|MODE_COPY_VALUES)); 
    pid = pin->getPID();
    TVERIFY(pid.pid != STORE_INVALID_PID); 
    
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "CREATE TIMER testtservices_c3_t1 INTERVAL %s AS UPDATE @" _LX_FM " SET afy:content='def';", "\'00:00:01\'", pid.pid);
    if(NULL == (qry = mSession->createStmt(sql, NULL, 0, &ce))) {
        cerr << "Error!!! testtservices::doCase3()::line: " << __LINE__ << " ::" << "CE: " << ce.rc << " line:" << ce.line << " pos:" << ce.pos << " " << ce.msg << " query := \"" << sql << "\n";
        TVERIFY(false);
        return;
    } 
    TVERIFYRC (qry->execute());
    if (qry != NULL) qry->destroy();

    remove(CASE3_FILENAME);
    
    cout << "sleep half minute..." << endl;
    MVTestsPortability::threadSleep(1000*30);

    // make sure this file is created and populated with expected message. 
    fp = fopen(CASE3_FILENAME,"r");
    TVERIFY(fp!=NULL);
    memset(buf, 0, sizeof(buf));
    TVERIFY(fread(buf, 1, sizeof(buf)-1,fp) > 0);
    cout << buf << endl;
    TVERIFY(strncmp(buf, "def", sizeof(buf)-1) == 0);

    pin = mSession->getPIN(pid);
    TVERIFY(pin != NULL);
    // delete this CPIN
    TVERIFYRC(pin->deletePIN());
    
    // clean messages in the plain file
    fp = fopen(CASE3_FILENAME,"w");
    TVERIFY(fp!=NULL);
    if(fp!=NULL) fclose(fp);

    cout << "sleep half minute..." << endl;
    MVTestsPortability::threadSleep(1000*30);

    fp = fopen(CASE3_FILENAME,"rb");
    TVERIFY(fp!=NULL);
    fseek(fp,0,SEEK_SET);
    fseek(fp,0,SEEK_END);
    // this file should be empty
    TVERIFY(0 == ftell(fp));    
    if(fp!=NULL) fclose(fp);

    mLogger.out() << "testservices1 doCase3 finish! " << endl;
}

/*
 * Test case for IO service: modify CPIN's afy:address
 */
#define CASE4_FILENAME "testservices1_case4.log"
void TestServices1::doCase4()
{   
    char sql[200];
    FILE *fp = NULL;
    char buf[4];
    ICursor *res=NULL;
    IPIN *pin=NULL;
    
    mLogger.out() << "Start testservices1 doCase4... " << endl;

    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT afy:service=.srv:IO, afy:address=1, testservices1_t4_p1=2;"));
    IStmt * const stmt = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservices1_t4_p1)");
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin = res->next()) != NULL);

    // output to STDOUT
    memset(sql, 0, sizeof(sql));
    for(int i = 0; i < 100; i++) {
        sprintf(sql, "UPDATE @" _LX_FM " SET afy:content='%d';", (pin->getPID()).pid, i);
        TVERIFYRC(MVTApp::execStmt(mSession, sql));
    }

    remove(CASE4_FILENAME);

    // change this CPIN, output to a plain file
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "UPDATE RAW @" _LX_FM " DELETE afy:address SET afy:address(CREATE_PERM,WRITE_PERM)='%s';", (pin->getPID()).pid, CASE4_FILENAME);
    TVERIFYRC(MVTApp::execStmt(mSession, sql));

    memset(sql, 0, sizeof(sql));
    for(int i = 0; i < 100; i++) {
        sprintf(sql, "UPDATE @" _LX_FM " SET afy:content='%d';", (pin->getPID()).pid, i);
        TVERIFYRC(MVTApp::execStmt(mSession, sql));
    }

    // make sure this file is created and populated with expected message. 
    fp = fopen(CASE4_FILENAME,"r");
    TVERIFY(fp!=NULL);
    memset(buf, 0, sizeof(buf));
    TVERIFY(fread(buf, 1, sizeof(buf)-1,fp) > 0);
    cout << buf << endl;
    TVERIFY(strncmp(buf, "012", sizeof(buf)-1) == 0);
    
    if(fp!=NULL) fclose(fp);
    if (res != NULL) res->destroy();
    if (stmt != NULL) stmt->destroy();
    mLogger.out() << "testservices1 doCase4 finish! " << endl;
}
 
/* 
 * Basic test case for IO+formatters (JSON here for the moment), output to a plain file
 */
#define CASE5_FILENAME "testservices1_case5.log"
void TestServices1::doCase5()
{   
    char sql[1024];
    IStmt *qry = NULL;
    CompilationError ce; 
    Value values[10];
    URIMap pmaps[1];
    
    mLogger.out() << "Start testservices1 doCase5... " << endl;

    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT testservices1_t5_bla=1;"));
    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT testservices1_t5_bla=2;"));
    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT testservices1_t5_bla=3;"));
    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT testservices1_t5_bla=4;"));
    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT testservices1_t5_bla=5;"));
    TVERIFYRC(MVTApp::execStmt(mSession, "CREATE CLASS testservices1_c5_bla AS SELECT * WHERE EXISTS(testservices1_t5_bla);"));
    
    MVTApp::mapURIs(mSession, "testservices1_t5_p1", 1, pmaps);
    int iP = 0;
    values[iP].setURIID(SERVICE_IO); values[iP].property = PROP_SPEC_SERVICE; values[iP].op=OP_ADD; iP++;
    values[iP].setURIID(SERVICE_JSON); values[iP].property = PROP_SPEC_SERVICE; values[iP].op=OP_ADD; iP++;
    values[iP].set(CASE5_FILENAME); values[iP].property = PROP_SPEC_ADDRESS; values[iP].meta = META_PROP_CREATE | META_PROP_WRITE; iP++;
    values[iP].set(1); values[iP].property = pmaps[0].uid; iP++;
    
    PID pid={0, 0};IPIN *pin;
    TVERIFYRC(mSession->createPIN(values, iP,&pin,MODE_PERSISTENT|MODE_COPY_VALUES)); 
    pid = pin->getPID();
    pin->destroy();
    TVERIFY(pid.pid != STORE_INVALID_PID); 

     // repro bug#395
    remove(CASE5_FILENAME);
    memset(sql, 0, sizeof(sql)); 
    sprintf(sql, "UPDATE @" _LX_FM " SET afy:content=(SELECT * WHERE EXISTS(testservices1_t5_bla));", pid.pid);
    TVERIFYRC(MVTApp::execStmt(mSession, sql));

    // verify the result
    FILE *fp = NULL;
    char buf[128];memset(buf, 0, sizeof(buf));
    fp = fopen(CASE5_FILENAME,"r");
    TVERIFY(fp!=NULL);
    unsigned int i = 0;
    while (fgets(buf, sizeof(buf), fp) > 0){
        TVERIFY(strstr(buf, "testservices1_t5_bla")!=NULL);
        i++;
        memset(buf, 0, sizeof(buf));
    }
    TVERIFY(i==5);
    if(fp!=NULL) fclose(fp);

    memset(sql, 0, sizeof(sql));
    sprintf(sql, "CREATE TIMER testtservices_c5_t1 INTERVAL %s AS UPDATE @" _LX_FM " SET afy:content=(SELECT * FROM testservices1_c5_bla);", "\'00:00:01\'", pid.pid);
    if(NULL == (qry = mSession->createStmt(sql, NULL, 0, &ce))) {
        cerr << "Error!!! testtservices::doCase5()::line: " << __LINE__ << " ::" << "CE: " << ce.rc << " line:" << ce.line << " pos:" << ce.pos << " " << ce.msg << " query := \"" << sql << "\n";
        TVERIFY(false);
        return;
    } 
    TVERIFYRC (qry->execute());
    
    cout << "sleep half minute..." << endl;
    MVTestsPortability::threadSleep(1000*30);

    /* 
      *  verify the result
      *  make sure this file is created and populated with expected message. 
      *  It's a very simple verification, every line should contain property name at least.
      */
    memset(buf, 0, sizeof(buf));
    fp = fopen(CASE5_FILENAME,"r");
    TVERIFY(fp!=NULL);
    while (fgets(buf, sizeof(buf), fp) > 0){
        TVERIFY(strstr(buf, "testservices1_t5_bla")!=NULL);
        memset(buf, 0, sizeof(buf));
        i++;
    }
    TVERIFY(i>5);
    if(fp!=NULL) fclose(fp);
   
    if (qry != NULL) qry->destroy();
    
    mLogger.out() << "testservices1 doCase5 finish! " << endl;
}

/* 
 * Basic PathSQL test case for service stack:protobuf+IO, output to a plain file,
 * and read by read service stack
 */
#define CASE6_FILENAME "testservices1_case6.log"
void TestServices1::doCase6()
{   
    char sql[400];
    ICursor *res=NULL;
    IPIN *pin1=NULL, *pin2=NULL;
    
    mLogger.out() << "Start testservices1 doCase6... " << endl;

    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT testservices1_t6_bla=1;"));
    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT testservices1_t6_bla=2;"));
    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT testservices1_t6_bla=3;"));
    TVERIFYRC(MVTApp::execStmt(mSession, "CREATE CLASS testservices1_c6_bla AS SELECT * WHERE EXISTS(testservices1_t6_bla);"));

    sprintf(sql, "INSERT afy:service={.srv:IO,.srv:protobuf}, afy:address(CREATE_PERM,WRITE_PERM,READ_PERM)='%s', testservices1_t6_p1=6;", CASE6_FILENAME);
    TVERIFYRC(MVTApp::execStmt(mSession, sql));
    IStmt * const stmt1 = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservices1_t6_p1)");
    TVERIFYRC(stmt1->execute(&res));
    TVERIFY((pin1 = res->next()) != NULL);
    res->destroy();
    mLogger.out() << "  read+write service: @" << std::hex << pin1->getPID().pid << std::dec << std::endl;

    sprintf(sql, "INSERT afy:service={.srv:IO,.srv:protobuf}, afy:address(READ_PERM)='%s', testservices1_t6_p2=6;", CASE6_FILENAME);
    TVERIFYRC(MVTApp::execStmt(mSession, sql));
    IStmt * const stmt2 = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservices1_t6_p2)");
    TVERIFYRC(stmt2->execute(&res));
    TVERIFY((pin2 = res->next()) != NULL);
    res->destroy();
    mLogger.out() << "  read-only service: @" << std::hex << pin2->getPID().pid << std::dec << std::endl;

    remove(CASE6_FILENAME);

    // output to a plain file
    memset(sql, 0, sizeof(sql));
    for(int i = 0; i < 10; i++) {
        sprintf(sql, "UPDATE @" _LX_FM " SET afy:content=(SELECT * FROM testservices1_c6_bla);", (pin1->getPID()).pid);
        TVERIFYRC(MVTApp::execStmt(mSession, sql));
    }

    sprintf(sql, "UPDATE @" _LX_FM " SET afy:position=0u;", (pin1->getPID()).pid);
    TVERIFYRC(MVTApp::execStmt(mSession, sql));

    // input from that file, via the same stack
    sprintf(sql, "SELECT * FROM @" _LX_FM, (pin2->getPID()).pid); // Note: INSERT SELECT here produces queryprc.cpp:315
    printf("trying to read with: %s\n", sql);
    ICursor * lCursor;
    TVERIFYRC(MVTApp::execStmt(mSession, sql, &lCursor));
    if (lCursor)
    {
        bool lGotSomething = false;
        for (IPIN * lP = lCursor->next(); lP; lP = lCursor->next())
        {
            printf("pin: "_LX_FM"\n", lP->getPID().pid);
            lP->destroy();
            lGotSomething = true;
        }
        lCursor->destroy();
        TVERIFY(lGotSomething && "No Result");
    }
    else
        TVERIFY(false && "No Result");
    
    if (stmt1 != NULL) stmt1->destroy();
    if (stmt2 != NULL) stmt2->destroy();
    pin1->destroy();
    pin2->destroy();
    mLogger.out() << "testservices1 doCase6 finish! " << endl;
}

/* 
 * Basic PathSQL test case for service stack:protobuf+IO, output to a plain file,
 * and read by read service stack
 */
#define CASE7_FILENAME "testservices1_case7.log"
void TestServices1::doCase7()
{   
    char sql[400];
    ICursor *res=NULL;
    IPIN *pin1=NULL, *pin2=NULL;
    
    mLogger.out() << "Start testservices1 doCase7... " << endl;

    sprintf(sql, "INSERT afy:service={.srv:IO,.srv:protobuf}, afy:address(CREATE_PERM,WRITE_PERM,READ_PERM)='%s', testservices1_t7_p1=7;", CASE7_FILENAME);
    TVERIFYRC(MVTApp::execStmt(mSession, sql));
    IStmt * const stmt1 = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservices1_t7_p1)");
    TVERIFYRC(stmt1->execute(&res));
    TVERIFY((pin1 = res->next()) != NULL);
    res->destroy();
    mLogger.out() << "  read+write service: @" << std::hex << pin1->getPID().pid << std::dec << std::endl;

    sprintf(sql, "INSERT afy:service={.srv:IO,.srv:protobuf}, afy:address(READ_PERM)='%s', testservices1_t7_p2=7;", CASE7_FILENAME);
    TVERIFYRC(MVTApp::execStmt(mSession, sql));
    IStmt * const stmt2 = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservices1_t7_p2)");
    TVERIFYRC(stmt2->execute(&res));
    TVERIFY((pin2 = res->next()) != NULL);
    res->destroy();
    mLogger.out() << "  read-only service: @" << std::hex << pin2->getPID().pid << std::dec << std::endl;

    remove(CASE7_FILENAME);

    // output to a plain file, single value message
    memset(sql, 0, sizeof(sql));
    for(int i = 0; i < 10; i++) {
        sprintf(sql, "UPDATE @" _LX_FM " SET afy:content='hello';", (pin1->getPID()).pid);
        TVERIFYRC(MVTApp::execStmt(mSession, sql));
    }

    sprintf(sql, "UPDATE @" _LX_FM " SET afy:position=0u;", (pin1->getPID()).pid);
    TVERIFYRC(MVTApp::execStmt(mSession, sql));

    // input from that file, via the same stack
    sprintf(sql, "SELECT * FROM @" _LX_FM, (pin2->getPID()).pid);
    printf("trying to read with: %s\n", sql);
    ICursor * lCursor;
    TVERIFYRC(MVTApp::execStmt(mSession, sql, &lCursor));
    if (lCursor)
    {
        bool lGotSomething = false;
        for (IPIN * lP = lCursor->next(); lP; lP = lCursor->next())
        {
            // should get a temp pin with afy:content ?
            const Value* val = lP->getValue(PROP_SPEC_CONTENT);
            TVERIFY(val->type == VT_STRING);
            TVERIFY(strncmp(val->str, "hello", strlen("hello")) == 0);
            lP->destroy();
            lGotSomething = true;
        }
        lCursor->destroy();
        TVERIFY(lGotSomething && "No Result");
    }
    else
        TVERIFY(false && "No Result");
    
    if (stmt1 != NULL) stmt1->destroy();
    if (stmt2 != NULL) stmt2->destroy();
    pin1->destroy();
    pin2->destroy();
    mLogger.out() << "testservices1 doCase7 finish! " << endl;
}

class Case8Service : public IService
{
  protected:
    friend class Proc8;
    class Proc8 : public IService::Processor
    {
      public:
        Proc8(Case8Service & pService, IServiceCtx * pCtx) {}
        virtual ~Proc8() {}
        virtual RC invoke(IServiceCtx * ctx,const Value & inp,Value & out,unsigned & mode)
        {
          if (0 != (mode & ISRV_WRITE) && VT_STRING == inp.type)
            std::cout << "** received: " << std::string(inp.str, inp.length) << std::endl;
          return RC_OK;
        }
        virtual void cleanup(IServiceCtx *ctx,bool fDestroy) { if (fDestroy) { this->~Proc8(); ctx->free(this); } }
    };
  public:
    Case8Service() {}
    virtual ~Case8Service() {}
    virtual RC create(IServiceCtx * ctx,uint32_t & dscr,Processor *& ret)
    {
      switch (dscr&ISRV_PROC_MASK &~ ISRV_ENDPOINT/*pending Q to Mark...*/)
      {
        case ISRV_WRITE:
          dscr|=ISRV_ALLOCBUF;
        case ISRV_READ:
          if (NULL == (ret = new(ctx->malloc(sizeof(Proc8))) Proc8(*this, ctx))) return RC_NOMEM;
          break;
        default:
          return RC_INVOP;
      }
      return RC_OK;
    }
};
void TestServices1::doCase8()
{   
    mLogger.out() << "Start testservices1 doCase8... " << endl;
    MVTApp::getStoreCtx()->registerService(AFFINITY_SERVICE_PREFIX"case8", new Case8Service());
    // TVERIFYRC(MVTApp::execStmt(mSession, "CREATE CLASS srv:case8_srv AS SELECT * WHERE EXISTS(srv:\"case8/uid\")")); // pending Q to Mark
    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT afy:service=.srv:case8, srv:\"case8/uid\"=1"));
    TVERIFYRC(MVTApp::execStmt(mSession, "UPDATE * SET afy:content='hello' WHERE EXISTS(srv:\"case8/uid\")"));
    TVERIFYRC(MVTApp::execStmt(mSession, "CREATE TIMER case8_fun INTERVAL '00:00:01' AS UPDATE * SET afy:content=('notified at ' || CURRENT_TIMESTAMP) WHERE EXISTS(srv:\"case8/uid\")"));
    MVTestsPortability::threadSleep(1000*5);
    mLogger.out() << "testservices1 doCase8 finished! " << endl;
}

void TestServices1::doCase9()
{   
    FILE *fp = NULL;
    char buf[128];
    int i = 0;
    
    mLogger.out() << "Start testservices1 doCase9... " << endl;

    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT afy:service=.srv:IO, afy:address(CREATE_PERM,WRITE_PERM)=\'testservices1_case9.log\', testservices1_t9_p1=2;"));
    TVERIFYRC(MVTApp::execStmt(mSession, "CREATE CLASS testservices1_t9_c1 AS SELECT * WHERE EXISTS(testservices1_t9_p1)"));

    // create a trigger to update afy:content
    TVERIFYRC(MVTApp::execStmt(mSession, "CREATE CLASS testservices1_t9_c2 AS SELECT * WHERE EXISTS(testservices1_t9_update)\
    SET afy:onEnter=${UPDATE testservices1_t9_c1 SET afy:content=@.testservices1_t9_update}"));

    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT testservices1_t9_update='===>Hello,doCase9\n'"));

    memset(buf, 0, sizeof(buf));
    fp = fopen("testservices1_case9.log","r");
    TVERIFY(fp!=NULL);
    memset(buf, 0, sizeof(buf));
    while (fgets(buf, sizeof(buf), fp) > 0){
        TVERIFY(strncmp(buf, "===>Hello,doCase9", strlen("===>Hello,doCase9"))==0);
        memset(buf, 0, sizeof(buf));
        i++;
    }
    TVERIFY(i==1);
    if(fp!=NULL) fclose(fp);
    
    mLogger.out() << "testservices1 doCase9 finished! " << endl;
}

