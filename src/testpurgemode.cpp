#include "app.h"
using namespace std;

#include "mvauto.h"

#define NUM_LOOP 2
#define NUMBER_PINS 1000
#define MAX_STR_LEN 1000

/*
 * This case is testing deletePINs() with two different modes, MODE_PURGE_IDS and MODE_PURGE
 * By design, deletePINs() is called with MODE_PURGE_IDS, the purged slot the PID will be reused for new created PIN.
 * deletePINs() is called with MODE_PURGE, the the purged slot and the PID won't be reused.
 */

// Publish this test.
class TestPurgeMode : public ITest
{
    public:
    TEST_DECLARE(TestPurgeMode);
        virtual char const * getName() const { return "TestPurgeMode"; }
        virtual char const * getHelp() const { return "please always add -newstore in the arguments"; }
        virtual char const * getDescription() const { return "testing MODE_PURGE_IDS and MODE_PURGE"; }
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Simple test..."; return true; }
        virtual bool isLongRunningTest()const {return false;}
        virtual bool includeInMultiStoreTests() const { return false; }
        virtual int execute();
        virtual void destroy() { delete this; }

    protected:
        URIID ids[10];
        ClassID classID;
        std::vector<PID> purged_PIDs;
        void doTest();
        void testPurgeMode(bool fTmp);

    private:
        ISession * mSession;
};
TEST_IMPLEMENT(TestPurgeMode, TestLogger::kDStdOut);

int TestPurgeMode::execute()
{
    doTest();
    return RC_OK;
}

void TestPurgeMode::doTest()
{
    uint64_t count = 0;
    long start = 0, end = 0;
    int retry = 0;
    bool bStarted = MVTApp::startStore() ;
    if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

    mSession = MVTApp::startSession();
    URIMap pmaps[10];
    MVTApp::mapURIs(mSession,"TestPurgeMode",10,pmaps);
    for (int i = 0; i < 10; i++)
        ids[i] = pmaps[i].uid;

    /* create a class as select * where exists $0 and exists $1 */
    IStmt * const iStmt = mSession->createStmt();
    TVERIFY(iStmt != NULL);
    QVarID var = iStmt->addVariable();
    
    Value val1[1], val2[1], vals[2]; 
    val1[0].setVarRef(0,ids[0]);
    IExprTree * expr1 = mSession->expr(OP_EXISTS,1,val1);
    TVERIFY( expr1 != NULL ) ;
    iStmt->addCondition(var,expr1);
    
    val2[0].setVarRef(0,ids[1]);
    IExprTree * expr2 = mSession->expr(OP_EXISTS,1,val2);
    TVERIFY( expr2 != NULL ) ;    
    iStmt->addCondition(var,expr2);

    vals[0].set(expr1);
    vals[1].set(expr2);
    IExprTree *expr = mSession->expr(OP_LAND,2,vals);
    TVERIFYRC(iStmt->addCondition(var,expr));

    classID = STORE_INVALID_CLASSID;
    classID = MVTUtil::createUniqueClass(mSession, "TestPrefixIndex.CLASS", iStmt, NULL);
    TVERIFY(STORE_INVALID_CLASSID != classID);
    expr->destroy();
    iStmt->destroy();

    retry = NUM_LOOP;
    while(retry > 0) {
        IStmt *del_stmt = mSession->createStmt("delete from $0", &classID, 1);
        IStmt *qry_stmt = mSession->createStmt("select from $0", &classID, 1);
        TVERIFYRC(del_stmt->execute());
        TVERIFYRC(qry_stmt->count(count));
        TVERIFY(count == 0);
        start = getTimeInMs();
        testPurgeMode(true);
        end = getTimeInMs(); 
        purged_PIDs.clear();
        cout << "testPurgeMode(true) costs : " << (end - start) << " ms" << endl;

        TVERIFYRC(qry_stmt->count(count));
        TVERIFY(count == (NUMBER_PINS/2));    

        TVERIFYRC(del_stmt->execute());
        TVERIFYRC(qry_stmt->count(count));
        TVERIFY(count == 0);
        start = getTimeInMs();
        testPurgeMode(false);
        end = getTimeInMs();
        cout << "testPurgeMode(false) costs : " << (end - start) << " ms" << endl;
        end = getTimeInMs();
        purged_PIDs.clear();
        TVERIFYRC(qry_stmt->count(count));
        TVERIFY(count == (NUMBER_PINS/2));     

        qry_stmt->destroy();
        del_stmt->destroy();
        retry--;
    }
    mSession->terminate();
    MVTApp::stopStore();
}


/* create a lot of PINs with two string properties 
 *  while create PINs, half of them will be purged 
 *  the purge flag is either MODE_PURGE_IDS or MODE_PURGE
 */
void TestPurgeMode::testPurgeMode(bool fTmp) 
{
    PID pid;
    int reused = 0, purged = 0;
    
    for (int i = 0; i < NUMBER_PINS; i++)  {  
        Value value[2];
        Tstring str1,str2;
        MVTApp::randomString(str1, 1, MAX_STR_LEN);
        value[0].set(str1.c_str());
        value[0].property = ids[0];
        MVTApp::randomString(str2, 1, MAX_STR_LEN);
        value[1].set(str2.c_str());
        value[1].property = ids[1];
        TVERIFYRC(mSession->createPIN(pid,value,2,fTmp?MODE_TEMP_ID:0));
        vector<PID>::iterator it;
        for (it=purged_PIDs.begin();purged_PIDs.end() != it; it++) {
            /* if this PID is reused */
            if (*it == pid)
                reused++;
        }
        if (i % 2 == 1) {
            TVERIFYRC(mSession->deletePINs(&pid, 1, MODE_PURGE));
            /* keep the purged PID into a vector */
            purged_PIDs.push_back(pid);
            purged++;
        }
    }
    
    cout << "Purge mode : " << (fTmp? "MODE_TEMP_ID":"MODE_PURGE") << endl;
    cout << "Purged PID counts : " << purged << endl;
    cout << "Reused PID counts : " << reused << endl;

    /* If mode is MODE_PURGE_IDS, number of reused PID should be larger than zero */
    fTmp?(assert(reused > 0)):(assert(reused == 0));
}
