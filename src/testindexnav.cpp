#include "app.h"
using namespace std;

#include "mvauto.h"

#define PROP_NUM 10
#define NUMBER_PINS 2000
#define MAX_STR_LEN 100

// Publish this test.
class TestIndexNav : public ITest
{
    public:
    TEST_DECLARE(TestIndexNav);
        virtual char const * getName() const { return "TestIndexNav"; }
        virtual char const * getHelp() const { return ""; }
        virtual char const * getDescription() const { return "testing IndexNav interface"; }
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Simple test..."; return true; }
        virtual bool isLongRunningTest()const {return false;}
        virtual bool includeInMultiStoreTests() const { return false; }
        virtual int execute();
        virtual void destroy() { delete this; }

    protected:
        DataEventID objectID;
        URIID ids[10];
        void doTest();
        void populate();

    private:
        ISession * mSession;
};
TEST_IMPLEMENT(TestIndexNav, TestLogger::kDStdOut);

int TestIndexNav::execute()
{
    doTest();
    return RC_OK;
}

void TestIndexNav::doTest()
{
    IPIN *pin1 = NULL, *pin2 = NULL;
    RC rc = RC_OK;
    PID pid1, pid2, firstPID, lastPID;
    const Value *value = NULL, *v1 =NULL, *v2=NULL;
    int current, next, first, last, tmp; 
    IndexNav* inav = NULL;
    
    bool bStarted = MVTApp::startStore() ;
    if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

    mSession = MVTApp::startSession();
    URIMap pmaps[PROP_NUM];
    MVTApp::mapURIs(mSession, "TestIndexNav", PROP_NUM, pmaps);
    for (int i = 0; i < PROP_NUM; i++)
        ids[i] = pmaps[i].uid;

    populate();
    
    /* create a class family 
      * create class TestIndexNav as select * where $0 = :0;
      */
    Value lV[2];
    IExprNode *iExpr = NULL;
    lV[0].setVarRef(0, ids[0]);
    lV[1].setParam(0);
    iExpr = mSession->expr(OP_EQ, 2, lV);
    TVERIFY(iExpr != NULL);

    IStmt * const iStmt = mSession->createStmt();
    TVERIFY(iStmt != NULL);
    QVarID const lVar = iStmt->addVariable();
    
    iStmt->addCondition(lVar, iExpr);
    cout << iExpr->toString() <<endl;
    iExpr->destroy();
    
    DataEventID objectID = STORE_INVALID_CLASSID;
    objectID = MVTUtil::createUniqueClass(mSession, "testIndexNav.CLASS", iStmt, NULL);
    TVERIFY(STORE_INVALID_CLASSID != objectID);
    iStmt->destroy();

    /* 
     * begin to test IndexNav::next()
     * iterate key one by one, it should be in ascending order. 
     * we keep the values of the first and last key
     * while iterating this btree index to further test GO_FIRST and GO_LAST
     */
    mLogger.out() << "begin to test IndexNav::next() ... " << endl;
    TVERIFYRC(mSession->createIndexNav(objectID,inav));
    TVERIFY(inav != NULL);
    
    value =  inav->next();
    TVERIFY (value != NULL);
    first = tmp = value->i;
    while(value = inav->next()){
        current = tmp;
        next = value->i;
        TVERIFY2(current <= next, "key order is wrong!");
        tmp = next;
    }
    last = tmp;
    inav->destroy();

    /* 
     * begin to test IndexNav::next(PID& id,GO_DIR=GO_NEXT)
     * iterate key one by one, it should be in order. 
     */
    mLogger.out() << "begin to test IndexNav::next(PID&) with GO_NEXT ... " << endl;
    TVERIFYRC(mSession->createIndexNav(objectID,inav));
    TVERIFY(inav != NULL);
    
    TVERIFYRC(inav->next(pid1));
    firstPID = pid1;
    while((rc = inav->next(pid2)) == RC_OK){
        pin1 = mSession->getPIN(pid1);
        v1 = pin1->getValue(ids[0]);
        pin2 = mSession->getPIN(pid2);
        v2 = pin2->getValue(ids[0]);        
        TVERIFY2(v1->i <= v2->i, "key order is wrong!");
        pin1->destroy();
        pin2->destroy();
        pid1 = pid2;
    }
    lastPID = pid2;
    /* should come to the end of btree */
    TVERIFY(rc == RC_EOF);
    
    /* 
     * since we have the first and last key of btree index,
     * begin to test IndexNav::next(PID& id,GO_DIR=GO_FIRST)
     */
    mLogger.out() << "begin to test IndexNav::next(PID&) with GO_FIRST or GO_LAST... " << endl;
    TVERIFYRC(rc = inav->next(pid1, GO_FIRST));
    TVERIFYRC(rc = inav->next(pid2, GO_LAST));
    TVERIFY(firstPID == pid1);
    TVERIFY(lastPID == pid2);
    pin1 = mSession->getPIN(pid1);
    v1 = pin1->getValue(ids[0]);
    pin2 = mSession->getPIN(pid2);
    v2 = pin2->getValue(ids[0]);  
    TVERIFY(first == v1->i);
    TVERIFY(last == v2->i);

    /* 
     * begin to test IndexNav::next(PID& id,GO_DIR=GO_PREVIOUS)
     * iterate key one by one, it should be in descending order. 
     */
    mLogger.out() << "begin to test IndexNav::next(PID&) with GO_PREVIOUS... " << endl;
    TVERIFYRC(inav->next(pid1, GO_LAST));
    while((rc = inav->next(pid2, GO_PREVIOUS)) == RC_OK){
        pin1 = mSession->getPIN(pid1);
        v1 = pin1->getValue(ids[0]);
        pin2 = mSession->getPIN(pid2);
        v2 = pin2->getValue(ids[0]);
        TVERIFY2(v1->i >= v2->i, "key order is wrong!");
        pin1->destroy();
        pin2->destroy();
        pid1 = pid2;
    }
    /* should come to the start of btree */
    TVERIFY(rc == RC_EOF);    
    
    inav->destroy();
    
    mSession->terminate(); // No return code to test
    MVTApp::stopStore();  // No return code to test
}


/*
  * populate a lot of PINs for further testing.
  */
void TestIndexNav::populate() {

    /* create a lot of PINs with at least one integer property 
     *  and optional number of string properties.
     *  The index scan will perform on the integer property
     */
    mLogger.out() << "begin to populate data for testing... " << endl;
    srand((unsigned)time(NULL));
    for (int i = 0; i < NUMBER_PINS; i++)  {  
        Value values[PROP_NUM];
        /* generate a random integer for first property */
        int var =  (rand()%(32767))-(32767/2);
        values[0].set(var);
        values[0].property = ids[0];

        /* generate random string for other properties */
        int cnt = rand()%PROP_NUM;
        for (int j = 0; j < cnt; j++) {
            Tstring str;
            MVTApp::randomString(str, 1, MAX_STR_LEN);        
            values[j+1].set(str.c_str());
            values[j+1].property = ids[j+1];
        }
        TVERIFYRC(mSession->createPIN(values,cnt+1,NULL,MODE_PERSISTENT|MODE_COPY_VALUES));
        if (i % 10 == 0) 
            cout << ".";
        if (i == NUMBER_PINS -1)
            cout<<endl;   
     }
     cout << " Totally " << NUMBER_PINS << " PINs have been created. " << endl;    
}

