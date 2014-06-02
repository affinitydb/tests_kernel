#include "app.h"
using namespace std;

#include "mvauto.h"

/*
 *  This case is testing prefix indexing/sorting and order by substr()
 *  the step is : 
 *  a) insert some pins,
 *  b) create class TestPrefixIndex as select * where substr($0, sub_len) OP_LT/OP_GT :0(String);
 *  c) query through the class family to check if the PINs retrieved are in order on that property.
 *  d) full scan query order by substr()
 *  
 *  note that :
 *  if the string is very large(>10000) and the page size is 4096, when create class family, it returns RC_TOOBIG
 */
#define LOOP_CNT 10
#define NUMBER_PINS 1000
#define MAX_STR_LEN 1000
#define MAX_SUB_STR_LEN 1000

// Publish this test.
class TestPrefixIndex : public ITest
{
    public:
    TEST_DECLARE(TestPrefixIndex);
        virtual char const * getName() const { return "TestPrefixIndex"; }
        virtual char const * getHelp() const { return "please always add -newstore in the arguments"; }
        virtual char const * getDescription() const { return "testing prefix indexing/sorting"; }
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
TEST_IMPLEMENT(TestPrefixIndex, TestLogger::kDStdOut);

int TestPrefixIndex::execute()
{
    doTest();
    return RC_OK;
}

void TestPrefixIndex::doTest()
{
    IPIN *pin1 = NULL, *pin2 = NULL;
    bool bStarted = MVTApp::startStore() ;
    if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

    mSession = MVTApp::startSession();
    URIMap pmaps[10];
    MVTApp::mapURIs(mSession,"TestPrefixIndex",10,pmaps);
    URIID ids[10];
    for (int i = 0; i < 10; i++)
        ids[i] = pmaps[i].uid;

    /* create a lot of PINs with single string property */
    for (int i = 0; i < NUMBER_PINS; i++)  {  
        Value value[1];
        Tstring str;
        MVTApp::randomString(str, 1, MAX_STR_LEN);
        value[0].set(str.c_str());
        value[0].property = ids[0];
        TVERIFYRC(mSession->createPIN(value,1,NULL,MODE_PERSISTENT|MODE_COPY_VALUES));
        if (i % 10 == 0) 
            cout << ".";
        if (i == NUMBER_PINS -1)
            cout<<endl;   
     }
    cout << " Totally " << NUMBER_PINS << " PINs have been created. " << endl;

    /* test prefix indexing/sorting */
    int retry = LOOP_CNT;
    while(retry > 0) 
     {
        /* create a class family 
          * create class TestPrefixIndex as select * where substr($0, sub_len) OP :0(String);
          * sub_len is a random int, OP is either OP_LT or < OP_GT
          */
        int sub_len = MVTApp::randInRange(0, MAX_SUB_STR_LEN);
        ExprOp op = MVTApp::randBool() ? OP_GT:OP_LT;
        Tstring str1;
        MVTApp::randomString(str1, sub_len, sub_len);
        
        Value lV[2];
        IExprNode *iExpr1 = NULL, *iExpr2 = NULL;
        lV[0].setVarRef(0,ids[0]);
        lV[1].set(sub_len);
        iExpr1 = mSession->expr(OP_SUBSTR, 2, lV);
        TVERIFY(iExpr1 != NULL);

        lV[0].set(iExpr1);
        lV[1].setParam(0);
        iExpr2 = mSession->expr(op, 2, lV);
        TVERIFY(iExpr2 != NULL);

        IStmt * const iStmt = mSession->createStmt();
        TVERIFY(iStmt != NULL);
        QVarID const lVar = iStmt->addVariable();
        
        iStmt->addCondition(lVar, iExpr2);
        // cout << iStmt->toString() <<endl;
        iExpr2->destroy();
        
        DataEventID objectID = STORE_INVALID_CLASSID;
        objectID = MVTUtil::createUniqueClass(mSession, "TestPrefixIndex.CLASS", iStmt, NULL);
        TVERIFY(STORE_INVALID_CLASSID != objectID);
        iStmt->destroy();
                
        /* validate the results, string values should be sorted in order */
        cout << " Begin to validate results..." << endl;
        IStmt *qry = mSession->createStmt();
        TVERIFY(qry != NULL);
        
        Value value[1]; SourceSpec spec;
        value[0].set(str1.c_str()); value[0].property = ids[0];
        spec.objectID = objectID; spec.params = &value[0]; spec.nParams = 1; 
        qry->addVariable(&spec,1);
        // cout << qry->toString() << endl;

        uint64_t cnt = 0;
        TVERIFYRC(qry->count(cnt));
        cout << " Totally " << cnt << " PINs are retrived." << endl;
        ICursor *res = NULL;
        TVERIFYRC (qry->execute(&res));
        pin1 = res->next();
        if(pin1) {
            while (pin2 = res->next() ) {
                const Value* v1 = pin1->getValue(ids[0]);
                const Value* v2 = pin2->getValue(ids[0]);
                int ret = strncmp(v1->str, v2->str, sub_len);
                if (ret > 0) {
                    cout << " string order is wrong !" << endl;
                    cout << v1->str << endl;
                    cout << v2->str << endl;
                    TVERIFY(ret <=0 );
                }
                pin1->destroy();
                pin1 = pin2;
            }
            pin1->destroy();
        }
        res->destroy();
        qry->destroy();
        retry--;
    }

    /* testing order by substr() */
    retry = LOOP_CNT;
    while(retry > 0) 
     {
        int sub_len = MVTApp::randInRange(0, MAX_SUB_STR_LEN);
        uint16_t flag = MVTApp::randBool() ? 0:ORD_DESC;

        IStmt * const iStmt = mSession->createStmt();
        TVERIFY(iStmt != NULL);
        QVarID lVar = iStmt->addVariable();
        OrderSeg lSortBy = {NULL, ids[0], uint8_t(flag), 0, (uint16_t)sub_len};
        iStmt->setOrder(&lSortBy, 1);
        iStmt->setPropCondition(lVar,ids,1);
        // cout << iStmt->toString() <<endl;

        uint64_t cnt = 0;
        TVERIFYRC(iStmt->count(cnt));
        cout << " Totally " << cnt << " PINs are retrived." << endl;
        ICursor *res = NULL;
        TVERIFYRC (iStmt->execute(&res));
        pin1 = res->next();
        if(pin1) {
            while (pin2 = res->next() ) {
                const Value* v1 = pin1->getValue(ids[0]);
                const Value* v2 = pin2->getValue(ids[0]);
                int ret = strncmp(v1->str, v2->str, sub_len);
                if (flag == ORD_DESC) 
                    ret = -ret;
                if (ret > 0) {
                    cout << " string order is wrong !" << endl;
                    cout << v1->str << endl;
                    cout << v2->str << endl;
                    TVERIFY(ret <=0 );
                }
                pin1->destroy();
                pin1 = pin2;
            }
            pin1->destroy();
        }
        res->destroy();
        iStmt->destroy();
        retry--;
    }

    mSession->terminate(); // No return code to test
    MVTApp::stopStore();  // No return code to test
}
