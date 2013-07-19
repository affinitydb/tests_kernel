#include "app.h"
using namespace std;

#include "mvauto.h"

class TestServices2 : public ITest , public MVTApp
{
    public:
        TEST_DECLARE(TestServices2);
        virtual char const * getName() const { return "TestServices2"; }
        virtual char const * getHelp() const { return ""; }
        virtual char const * getDescription() const { return "testing services2"; }
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Simple test..."; return true; }
        virtual bool isLongRunningTest()const {return false;}
        virtual bool includeInMultiStoreTests() const { return false; }
        virtual int execute();
        virtual void destroy() { delete this; }
        void doCase1();
        void doCase2();
        void doCase3_receiver();
        void doCase3_sender();
        void doCase4_chatting();
    private:
        ISession * mSession;
};
TEST_IMPLEMENT(TestServices2, TestLogger::kDStdOut);

int TestServices2::execute()
{
    bool bStarted = MVTApp::startStore() ;
    if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return -1; }
    mSession = MVTApp::startSession();

    doCase1();
    //doCase2();   // disabled, please see bug#412 comment #2
    //case 3 and 4 are expected to be run manually
    //doCase3_receiver();
    //doCase3_sender();
    //doCase4_chatting();
    mSession->terminate(); 
    MVTApp::stopStore();
    return RC_OK;
}

#define LOCALHOST "127.0.0.1"
#define PORT 8090
void TestServices2::doCase1()
{  
    char sql[200],sql2[50],address[20];CompilationError ce;
    ICursor *res=NULL;const Value *val=NULL;
    IStmt *qry=NULL,*stmt=NULL;
    IPIN *pin1=NULL,*pin2=NULL;

    mLogger.out() << "Start testservices2 doCase1... " << endl;
    
    // create a listener on localhost
    sprintf(address, "%s:%d", LOCALHOST, PORT);
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "CREATE LISTENER testservices2_t1_l1 ON \'%s\' AS {.srv:sockets,.srv:pathSQL,.srv:affinity,.srv:protobuf,.srv:sockets}", address);
    TVERIFYRC(execStmt(mSession, sql));
    TVERIFYRC(execStmt(mSession, "INSERT testservices2_t1_p1='Hello, service'"));

    // create a CPIN of sending a SELECT request
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "INSERT afy:service={.srv:pathSQL,.srv:sockets,.srv:protobuf},afy:address=\'%s\',\
afy:request=${SELECT * WHERE EXISTS(testservices2_t1_p1)}, testservices2_t1_p2=1", address);
    if(NULL == (qry = mSession->createStmt(sql, NULL, 0, &ce))) {
        cerr << "Error!!! testtservices2::doCase1()::line: " << __LINE__ << " ::" << "CE: " << ce.rc << " line:" << ce.line << " pos:" << ce.pos << " " << ce.msg << " query := \"" << sql << "\n";
        TVERIFY(false);
        return;
    } 
    TVERIFYRC(qry->execute());
    if(qry!=NULL) qry->destroy();
    
    stmt = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservices2_t1_p2)");
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin1 = res->next()) != NULL);
    if (res != NULL) res->destroy();
    if(stmt!=NULL) stmt->destroy();
    
    // select from this CPIN and verify the result
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select * from @" _LX_FM, (pin1->getPID()).pid);
    TVERIFY(1 == countStmt(mSession, sql));
    if(NULL == (qry = mSession->createStmt(sql))) {
        TVERIFY(false);
        return;
    } 
    TVERIFYRC(qry->execute(&res));
    TVERIFY((pin2 = res->next()) != NULL);
    TVERIFY(pin2->getNumberOfProperties() == 1);
    val = pin2->getValueByIndex(0);
    TVERIFY(strncmp(val->str, "Hello, service", val->length) == 0);
    memset(sql, 0, sizeof(sql));
    sprintf(sql, _LX_FM, (pin1->getPID()).pid);
    cout << "Got message : \"" << val->str << "\" from read service @ " <<  sql << endl;
    if (res != NULL) res->destroy();
    if(qry!=NULL) qry->destroy();

    TVERIFYRC(execStmt(mSession, "CREATE CLASS testservices2_t1_c1 AS SELECT * WHERE EXISTS(testservices2_t1_p1)"));

    // create a CPIN of sending a UPDATE request
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "INSERT afy:service={.srv:pathSQL,.srv:sockets,.srv:protobuf},afy:address=\'%s\',\
afy:request=${UPDATE testservices2_t1_c1 SET testservices2_t1_p1='HELLO'}, testservices2_t1_p3=1", address);
    if(NULL == (qry = mSession->createStmt(sql, NULL, 0, &ce))) {
        cerr << "Error!!! testtservices2::doCase1()::line: " << __LINE__ << " ::" << "CE: " << ce.rc << " line:" << ce.line << " pos:" << ce.pos << " " << ce.msg << " query := \"" << sql << "\n";
        TVERIFY(false);
        return;
    } 
    TVERIFYRC(qry->execute());
    if(qry!=NULL) qry->destroy();
    
    stmt = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservices2_t1_p3)");
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin1 = res->next()) != NULL);
    if (res != NULL) res->destroy();
    if(stmt!=NULL) stmt->destroy();    
    
    // select from this CPIN and verify the result
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select * from @" _LX_FM, (pin1->getPID()).pid);
    TVERIFY(1 == MVTApp::countStmt(mSession, sql));
    if(NULL == (qry = mSession->createStmt(sql))) {
        TVERIFY(false);
        return;
    }

    TVERIFYRC(qry->execute(&res));
    TVERIFY((pin2 = res->next()) != NULL);
    TVERIFY(pin2->getNumberOfProperties() == 1);
    val = pin2->getValueByIndex(0);
    // now the value should be changed as expect
    TVERIFY(strncmp(val->str, "HELLO", val->length) == 0);
    cout << "after send UPDATE request, message is changed to \"" << val->str << "\"" << endl;
    TVERIFY(MVTApp::countStmt(mSession, "SELECT * WHERE testservices2_t1_p1='HELLO'")==1);
    TVERIFY(MVTApp::countStmt(mSession, "SELECT * FROM testservices2_t1_c1")==1);
    if (res != NULL) res->destroy();
    if(qry!=NULL) qry->destroy();
    
    // create a CPIN of sending a DELETE request
    memset(sql, 0, sizeof(sql));
    memset(sql2, 0, sizeof(sql2));
    sprintf(sql2, "DELETE @" _LX_FM,  (pin2->getPID()).pid);
    sprintf(sql, "INSERT afy:service={.srv:pathSQL,.srv:sockets,.srv:protobuf},afy:address=\'%s\',afy:request=${%s},testservices2_t1_p4=1", address, sql2);
    if(NULL == (qry = mSession->createStmt(sql, NULL, 0, &ce))) {
        cerr << "Error!!! testtservices2::doCase1()::line: " << __LINE__ << " ::" << "CE: " << ce.rc << " line:" << ce.line << " pos:" << ce.pos << " " << ce.msg << " query := \"" << sql << "\n";
        TVERIFY(false);
        return;
    } 
    TVERIFYRC(qry->execute());
    if(qry!=NULL) qry->destroy();

    stmt = mSession->createStmt("SELECT RAW * WHERE EXISTS(testservices2_t1_p4)");
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin1 = res->next()) != NULL);
    if (res != NULL) res->destroy();
    if(stmt!=NULL) stmt->destroy();    
    
    // select from this CPIN and verify the result
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "select * from @" _LX_FM, (pin1->getPID()).pid);
    TVERIFY(1 == MVTApp::countStmt(mSession, sql));
    if(NULL == (qry = mSession->createStmt(sql))) {
        TVERIFY(false);
        return;
    }
    TVERIFYRC(qry->execute(&res));
    // before execute ICursor::next(), the pin to be deleted still exists.
    TVERIFY(countStmt(mSession, "SELECT * FROM testservices2_t1_c1")==1);
    // return the deleted pin
    TVERIFY((pin2 = res->next()) != NULL);
    // after execute ICursor::next(), the pin should be deleted.
    TVERIFY(countStmt(mSession, "SELECT * FROM testservices2_t1_c1")==0);
    if (res != NULL) res->destroy();
    if(qry!=NULL) qry->destroy();    
    
    mLogger.out() << "testservices2 doCase1 finish! " << endl;
}

/*
  * a test case to simulate sending/receive messages between two stores.
  * (actually it runs on a same store)
  */
void TestServices2::doCase2(){
    char sql[300],sql2[100],address[20];
    ICursor *res=NULL; IStmt *qry=NULL;
    IPIN *pin=NULL; int i;

    mLogger.out() << "Start testservices2 doCase2... " << endl;
    
    /** -- Store1 (Receiver) START -- **/
    // create a IO service pin to display the message
    TVERIFYRC(execStmt(mSession, "INSERT afy:service=.srv:IO, afy:address=1, testservices2_t2_p1=2;"));
    TVERIFYRC(execStmt(mSession, "CREATE CLASS testservices2_t2_c1 AS SELECT * WHERE EXISTS(testservices2_t2_p1)"));
    
    // create a trigger to update afy:content
    TVERIFYRC(execStmt(mSession, "CREATE CLASS testservices2_t2_c2 AS SELECT * WHERE EXISTS(testservices2_t2_update)\
    SET afy:onEnter=${UPDATE testservices2_t2_c1 SET afy:content=@.testservices2_t2_update}"));
    
    // create a listener to accept the operations to the above IO service pin
    sprintf(address, "%s:%d", LOCALHOST, PORT+1);
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "CREATE LISTENER testservices2_t2_l1 ON \'%s\' AS {.srv:sockets,.srv:pathSQL,.srv:affinity,.srv:protobuf,.srv:sockets}", address);
    TVERIFYRC(execStmt(mSession, sql));
    /** -- Store1 (Receiver)  END -- **/
    
    /**  -- Store2 (Sender) START -- **/
    memset(sql, 0, sizeof(sql));
    memset(sql2, 0, sizeof(sql2));
    // This message should display on STDOUT
    sprintf(sql2, "INSERT OPTIONS(TRANSIENT) testservices2_t2_update='Hello from Store2!\n'");
    sprintf(sql, "INSERT afy:service={.srv:pathSQL,.srv:sockets,.srv:protobuf},afy:address=\'%s\',afy:request=${%s}, testservices2_t2_p2=1", address, sql2);
    TVERIFYRC(execStmt(mSession,sql,&res));
    TVERIFY((pin = res->next()) != NULL);

    // select from this CPIN and verify the result
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "SELECT * FROM @" _LX_FM, (pin->getPID()).pid);
    TVERIFY(1 == countStmt(mSession, sql));
    if(NULL == (qry = mSession->createStmt(sql))) {
        TVERIFY(false);
        return;
    }
    TVERIFYRC(qry->execute(&res));
    while(res->next()!=NULL);
    if(res!= NULL) res->destroy();

    // send many messages by updating afy:request of this CPIN
    i = 0;
    while (i < 10) {
        memset(sql, 0, sizeof(sql));
        memset(sql2, 0, sizeof(sql2));
        sprintf(sql2, "INSERT OPTIONS(TRANSIENT) testservices2_t2_update='Hello from Store2! UPDATE#%d\n'", i);
        sprintf(sql, "UPDATE RAW @" _LX_FM " SET afy:request=${%s}", (pin->getPID()).pid, sql2);
        TVERIFYRC(execStmt(mSession, sql));
        TVERIFYRC(qry->execute(&res));
        while(res->next()!=NULL); // this message should display here.
        if(res!= NULL) res->destroy();  
        i++;
    }

#if 0
    // create a timer to update CPIN
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "CREATE TIMER testservices2_t2_t2 INTERVAL \'00:00:01\' AS \
UPDATE RAW @" _LX_FM  " SET afy:request=${INSERT OPTIONS(TRANSIENT) testservices2_t2_update=CURRENT_TIMESTAMP}", (pin->getPID()).pid);
    cout << sql << endl;
    TVERIFYRC(execStmt(mSession,sql)); 
    // sleep 10 seconds
    MVTestsPortability::threadSleep(1000*10);

    TVERIFYRC(qry->execute(&res));
    while(res->next()!=NULL);    
    if(res != NULL) res->destroy();
    if(qry!=NULL) qry->destroy();
#endif

    // send many messages by creating many CPINs
    i = 0;
    while (i < 10) {
        memset(sql, 0, sizeof(sql));
        memset(sql2, 0, sizeof(sql2));
        // This message should display on STDOUT
        sprintf(sql2, "INSERT OPTIONS(TRANSIENT) testservices2_t2_update='Hello from Store2! NUM #%d\n'", i);
        sprintf(sql, "INSERT afy:service={.srv:pathSQL,.srv:sockets,.srv:protobuf},afy:address=\'%s\', \
afy:request=${%s}, testservices2_t2_p3=%d", address, sql2,i);
        TVERIFYRC(execStmt(mSession,sql,&res));
        TVERIFY((pin = res->next()) != NULL);

        // select from this CPIN and verify the result
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "SELECT * FROM @" _LX_FM, (pin->getPID()).pid);
        TVERIFYRC(execStmt(mSession,sql,&res));
        while(res->next()!=NULL);
        if(res != NULL) res->destroy();
        i++;
    }
    /** -- Store2 (Sender)  END -- **/
    
    mLogger.out() << "testservices2 doCase2 finish! " << endl;
}

/*
  * case 3 's steps is very similar to case 2, except that 
  * doCase3_sender() and doCase3_receiver() are expected to run on different machines.
  *
  * To run it, get ip address of machine1 and set for the MACRO CASE3_IPADDRESS
  * enable the case doCase3_receiver(), disable the case doCase3_sender(), build and run.
  * While AffinityNG is waitting on machine1, 
  * enable the case doCase3_sender(), disable the case doCase3_receiver(), build and run from another machine.
  * Then, AffinityNG sends messages from machine2.
  */

#define CASE3_IPADDRESS "192.168.0.2" // ip address of receiver, have to modify it manually according to testing environment
#define CASE3_PORT 9000
/*
  * send many messages to a known listener.
  */
void TestServices2::doCase3_sender(){
    char sql[300],sql2[100],address[20];
    ICursor *res=NULL; IStmt *qry=NULL;
    IPIN *pin=NULL; int i;

    mLogger.out() << "Start testservices2 doCase3_sender... " << endl;
    sprintf(address, "%s:%d", CASE3_IPADDRESS, CASE3_PORT);
    memset(sql, 0, sizeof(sql));
    memset(sql2, 0, sizeof(sql2));
    // This message should display on STDOUT
    sprintf(sql2, "INSERT testservices2_t3_update='Hello from Store2!\n'");
    sprintf(sql, "INSERT afy:service={.srv:pathSQL,.srv:sockets,.srv:protobuf},afy:address=\'%s\',afy:request=${%s}, testservices2_t3_p2=1", address, sql2);
    TVERIFYRC(execStmt(mSession,sql,&res));
    TVERIFY((pin = res->next()) != NULL);

    // select from this CPIN and verify the result
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "SELECT * FROM @" _LX_FM, (pin->getPID()).pid);
    TVERIFY(1 == countStmt(mSession, sql));
    if(NULL == (qry = mSession->createStmt(sql))) {
        TVERIFY(false);
        return;
    }
    TVERIFYRC(qry->execute(&res));
    TVERIFY(res->next()!=NULL);
    if(res!= NULL) res->destroy();

        // send many messages by updating afy:request of this CPIN
    i = 0;
    while (i < 10) {
        memset(sql, 0, sizeof(sql));
        memset(sql2, 0, sizeof(sql2));
        sprintf(sql2, "INSERT testservices2_t3_update='Hello from Store2! UPDATE#%d\n'", i);
        sprintf(sql, "UPDATE RAW @" _LX_FM " SET afy:request=${%s}", (pin->getPID()).pid, sql2);
        TVERIFYRC(execStmt(mSession, sql));
        TVERIFYRC(qry->execute(&res));
        TVERIFY(res->next()!=NULL); // this message should display here.
        if(res!= NULL) res->destroy();  
        i++;
    }

    mLogger.out() << "testservices2 doCase3_sender finish! " << endl;
}

/* 
  * create a listener, then sleep
  */
void TestServices2::doCase3_receiver(){
    char sql[300],address[20];

    mLogger.out() << "Start testservices2 doCase3_receiver... " << endl;

    /** -- Store1 (Receiver) START -- **/
    // create a IO service pin to display the message
    TVERIFYRC(execStmt(mSession, "INSERT afy:service=.srv:IO, afy:address=1, testservices2_t3_p1=2;"));
    TVERIFYRC(execStmt(mSession, "CREATE CLASS testservices2_t3_c1 AS SELECT * WHERE EXISTS(testservices2_t3_p1)"));

    // create a trigger to update afy:content
    TVERIFYRC(execStmt(mSession, "CREATE CLASS testservices2_t3_c2 AS SELECT * WHERE EXISTS(testservices2_t3_update)\
    SET afy:onEnter=${UPDATE testservices2_t3_c1 SET afy:content=@.testservices2_t3_update}"));

    // create a listener to accept the operations to the above IO service pin
    sprintf(address, "%s:%d", CASE3_IPADDRESS, CASE3_PORT);
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "CREATE LISTENER testservices2_t3_l1 ON \'%s\' AS {.srv:sockets,.srv:pathSQL,.srv:affinity,.srv:protobuf,.srv:sockets}", address);
    cout << sql << endl;
    TVERIFYRC(execStmt(mSession, sql));

    MVTestsPortability::threadSleep(1000*10*60);

    /** -- Store1 (Receiver)  END -- **/
    mLogger.out() << "testservices2 doCase3_receiver finish! " << endl;
}

/* 
  * This test case simulates a chatting program between two machines(stores)
  * To run this simple chatting program, prepare two machines and their own ip addresses,
  * one each machine, modify CASE4_IPADDRESS_MINE to its own ip address and 
  * CASE4_IPADDRESS_PEER to another machine's ip address, complile and run.
  * type "quit;" to finish the case.
  */
#define CASE4_IPADDRESS_MINE "10.117.37.106"
#define CASE4_IPADDRESS_PEER "10.117.37.139"
#define CASE4_PORT 9002
void TestServices2::doCase4_chatting(){
    char sql[1024],address[20],message[800],hostname[50];
    ICursor *res=NULL; IPIN *pin=NULL;

    mLogger.out() << "Start testservices2 doCase4_chatting... " << endl;

    // create a IO service pin to display the message
    TVERIFYRC(execStmt(mSession, "INSERT afy:service=.srv:IO, afy:address=1, testservices2_t4_p1=2;"));
    TVERIFYRC(execStmt(mSession, "CREATE CLASS testservices2_t4_c1 AS SELECT * WHERE EXISTS(testservices2_t4_p1)"));

    // create a trigger to update afy:content
    TVERIFYRC(execStmt(mSession, "CREATE CLASS testservices2_t4_c2 AS SELECT * WHERE EXISTS(testservices2_t4_update)\
    SET afy:onEnter=${UPDATE testservices2_t4_c1 SET afy:content=@.testservices2_t4_update}"));

    // create a listener to accept the operations to the above IO service pin
    sprintf(address, "%s:%d", CASE4_IPADDRESS_MINE, CASE4_PORT);
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "CREATE LISTENER testservices2_t4_l1 ON \'%s\' AS {.srv:sockets,.srv:pathSQL,.srv:affinity,.srv:protobuf,.srv:sockets}", address);
    TVERIFYRC(execStmt(mSession, sql));

    memset(sql, 0, sizeof(address));
    sprintf(address, "%s:%d", CASE4_IPADDRESS_PEER, CASE4_PORT);

    memset(hostname, 0 ,sizeof(hostname));
    TVERIFYRC(MVTUtil::getHostname(hostname));
    
    // wait for input and send message to the known peer
    for(;;){
        
        cout << hostname << ">";
        memset(message, 0, sizeof(message));
        cin.getline(message,sizeof(message),'\n');
        // TODO: deal with escape character here
        cout << hostname << ">" << message<<endl;

        // won't send an empty message
        if(strlen(message) == 0)
            continue;

        // enter "quit;" to finish the test
        if(strcmp(message,"quit;") == 0)
            break;
        
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "INSERT afy:service={.srv:pathSQL,.srv:sockets,.srv:protobuf},afy:address=\'%s\',\
afy:request=${INSERT testservices2_t4_update='\n%s>%s\n'}",address,hostname,message);
        TVERIFYRC(execStmt(mSession,sql,&res));
        TVERIFY((pin = res->next()) != NULL);

        // select from this CPIN to send the message
        memset(sql, 0, sizeof(sql));
        sprintf(sql, "SELECT * FROM @" _LX_FM, (pin->getPID()).pid);
        TVERIFYRC(execStmt(mSession,sql,&res));
        TVERIFY(res->next()!=NULL);
        if(res != NULL) res->destroy();
    }

    mLogger.out() << "testservices2 doCase4_chatting finish! " << endl;
}

