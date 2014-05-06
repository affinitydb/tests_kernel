#include "app.h"
using namespace std;

#include "mvauto.h"
#define ELEMENTS_NUM 100
#define MAX_STR_LEN 20

// Publish this test.
class TestVTMap : public ITest
{
    public:
        TEST_DECLARE(TestVTMap);
        virtual char const * getName() const { return "TestVTMap"; }
        virtual char const * getHelp() const { return ""; }
        virtual char const * getDescription() const { return "testing VT_MAP"; }
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
        void doTestEnumeration();
    private:
        ISession * mSession;
};
TEST_IMPLEMENT(TestVTMap, TestLogger::kDStdOut);

int TestVTMap::execute()
{
    doTest();
    return RC_OK;
}

void TestVTMap::doTest()
{
    bool bStarted = MVTApp::startStore() ;
    if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }
    mSession = MVTApp::startSession();
    
    doCase1();
    doCase2();
    doCase3();
    doCase4();
    doCase5();
    doTestEnumeration();

    mSession->terminate(); 
    MVTApp::stopStore();
}

/*
 * Basic Pathsql test case for VT_MAP
 */
void TestVTMap::doCase1()
{    
    mLogger.out() << "Start testvtmap doCase1... " << endl;

    // string to string mapping
    TVERIFYRC(MVTApp::execStmt(mSession, "insert country_capital_mapping={'China'->'Beijing', 'Russia'->'Moscow',\
'Canada'->'Ottawa','USA'->'Washington','Hungary'->'Budapest', 'Japan'->'Tokyo'}"));
    TVERIFY(MVTApp::countStmt(mSession, "select * where exists(country_capital_mapping)") == 1);

    // string to int mapping
    TVERIFYRC(MVTApp::execStmt(mSession, "insert country_area_mapping={'China'->3,'Russia'->1,'Canada'->2,'USA'->4}"));
    TVERIFY(MVTApp::countStmt(mSession, "select * where exists(country_area_mapping)") == 1); 
    
    // int to string mapping
    TVERIFYRC(MVTApp::execStmt(mSession, "insert week_mapping={0->'Sunday', 1->'Monday', 2->'Tuesday'}"));
    TVERIFY(MVTApp::countStmt(mSession, "select * where exists(week_mapping)") == 1); 

    // int to int mapping
    TVERIFYRC(MVTApp::execStmt(mSession, "insert multiply_10_mapping={0->0, 1->10, 2->20, 3->30, 4->40}"));
    TVERIFY(MVTApp::countStmt(mSession, "select * where exists(multiply_10_mapping)") == 1); 

    mLogger.out() << "testvtmap doCase1 finish!" << endl;
}

static Tstring g_strings_c2[ELEMENTS_NUM];

class MyVTMap1 : public IMap {
    private:
        unsigned pos;
        unsigned cnt;
        MapElt *elts;
    public:
        MyVTMap1();
        ~MyVTMap1();
        RC getNext(const Value *&key,const Value *&val,bool fFirst=false);
        unsigned count() const{return cnt;}
        const Value *find(const Value &key);
        IMap *clone() const {return NULL;}; // not implement yet
        void destroy();
};

MyVTMap1::MyVTMap1():pos(0),cnt(ELEMENTS_NUM),elts(NULL){
    elts = (MapElt *)malloc(sizeof(MapElt)*ELEMENTS_NUM);
    memset(elts, 0, sizeof(MapElt)*ELEMENTS_NUM);
    for (unsigned i = 0; i < ELEMENTS_NUM; i++) {
        elts[i].key.set(i);
        MVTApp::randomString(g_strings_c2[i], 1, MAX_STR_LEN);
        elts[i].val.set(g_strings_c2[i].c_str());
    }
}

const Value* MyVTMap1::find(const Value &key) {
    unsigned idx = key.ui;
    if (idx >= cnt) return NULL;
    return &elts[idx].val;
}

RC MyVTMap1::getNext(const Value *&key,const Value *&val,bool fFirst) {
    if(fFirst) pos=0; if(pos>=cnt) return RC_EOF;
    key = &elts[pos].key; val = &elts[pos].val; pos++;
    return RC_OK;
}

void MyVTMap1::destroy(){
    this->~MyVTMap1();
}

MyVTMap1::~MyVTMap1() {
    if(elts!=NULL) free(elts);
}

/*
 * Basic C++ interface test case for VT_MAP
 */
void TestVTMap::doCase2()
{  
    Value values[1]; 
    const Value *key,*value,*ret[1];
    uint64_t cnt=0;
    ICursor *res = NULL;
    IPIN* pin = NULL;
    RC rc = RC_OK;
    unsigned i = 0;
    URIMap pmaps[1];
    URIID ids[1];
    
    mLogger.out() << "Start testvtmap doCase2... " << endl;
    
    MVTApp::mapURIs(mSession, "TestVTMap.prop.c2", 1, pmaps);
    ids[0] = pmaps[0].uid;

    MyVTMap1 *map = new MyVTMap1();
    values[0].set(map);
    values[0].property = ids[0];

    // insert a PIN with a VT_MAP value
    TVERIFYRC(mSession->createPIN(values, 1, NULL, MODE_PERSISTENT|MODE_COPY_VALUES)); 

    //verify the result
    IStmt * const stmt = mSession->createStmt("SELECT * WHERE EXISTS ($0)", ids, 1);
    TVERIFYRC(stmt->count(cnt)); TVERIFY(cnt==1);
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin = res->next())!=NULL);
    ret[0] = pin->getValue(ids[0]);
    TVERIFY(ret[0]->type == VT_MAP);
    IMap* vmap = ret[0]->map;
    
    while((rc = vmap->getNext(key, value, false)) == RC_OK){
        TVERIFY(i == key->ui);
        TVERIFY(strncmp(value->str, g_strings_c2[i].c_str(), strlen(value->str)) == 0);
        TVERIFY(strncmp(value->str,(map->find(*key))->str, strlen(value->str)) == 0);
        i++;
    }
    TVERIFY(rc == RC_EOF);

    if(map) map->destroy();
    if(stmt) stmt->destroy();
    if(res) res->destroy();
    mLogger.out() << "testvtmap doCase2 finish!" << endl;
}

/*
 * Basic test case for duplicate keys in VT_MAP
 */
void TestVTMap::doCase3(){
    const Value *key,*value,*ret[1];
    ICursor *res = NULL;
    IPIN* pin = NULL;
    RC rc = RC_OK;
    unsigned i = 0;
    URIID ids[1];
    
    mLogger.out() << "Start testvtmap doCase3... " << endl;
    ids[0] = MVTApp::getProp(mSession, "testvtmap_c3_p1");

    // insert a pin with duplicate keys in VT_MAP
    // should returns an error?
    TVERIFYRC(MVTApp::execStmt(mSession, "INSERT testvtmap_c3_p1={0->'abc', 0->'efg', 1->'hij', 2->'klm'}"));
    
    IStmt * const stmt = mSession->createStmt("SELECT * WHERE EXISTS($0)", ids, 1);
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin = res->next())!=NULL);
    ret[0] = pin->getValue(ids[0]);
    TVERIFY(ret[0]->type == VT_MAP);
    IMap* vmap = ret[0]->map;
    
    while((rc = vmap->getNext(key, value, false)) == RC_OK){
        cout << key->i << endl;
        cout << value->str << endl;
        i++;
    }
    cout << "i : " << i << endl;
    //TVERIFY(i==3); //not anable it
    TVERIFY(rc == RC_EOF);

    if(stmt) stmt->destroy();
    if(res) res->destroy(); 
    mLogger.out() << "testvtmap doCase3 finish!" << endl;
}


class MyVTMap2 : public IMap {
    private:
        unsigned pos;
        unsigned cnt;
        MapElt *elts;
    public:
        MyVTMap2();
        ~MyVTMap2();
        RC getNext(const Value *&key,const Value *&val,bool fFirst=false);
        unsigned count() const{return cnt;}
        const Value *find(const Value &key);
        IMap *clone() const {return NULL;}; // not implement yet
        void destroy();
};

static char g_strings_c4[ELEMENTS_NUM/2][10];
/*
 * a VT_MAP with such elements:{0->'0','0'->0,1->'1','1'->1,2->'2','2'->2, ...}
 */
MyVTMap2::MyVTMap2():pos(0),cnt(ELEMENTS_NUM),elts(NULL){
    elts = (MapElt *)malloc(sizeof(MapElt)*ELEMENTS_NUM);
    memset(elts, 0, sizeof(MapElt)*ELEMENTS_NUM);
    for (unsigned i = 0; i < ELEMENTS_NUM; i++) {
        if (i%2==0) {
            elts[i].key.set(i/2);
            sprintf(g_strings_c4[i/2], "%d", i/2);
            elts[i].val.set(g_strings_c4[i/2]);
        } else {
            sprintf(g_strings_c4[i/2], "%d", i/2);
            elts[i].key.set(g_strings_c4[i/2]);
            elts[i].val.set(i/2);
        }
    }
}

const Value* MyVTMap2::find(const Value &key) {
    if(key.type == VT_UINT) {
        unsigned idx = key.ui;
        if (idx*2 >= cnt) return NULL;
        return &elts[idx*2].val;
    }
    else if (key.type == VT_STRING) {
        int idx = atoi(key.str);
        if ((unsigned)idx*2 >= cnt) return NULL;
        return &elts[idx*2+1].val;
    }
    else {
        assert(0);
        return NULL;
    }
}

RC MyVTMap2::getNext(const Value *&key,const Value *&val,bool fFirst) {
    if(fFirst) pos=0; if(pos>=cnt) return RC_EOF;
    key = &elts[pos].key; val = &elts[pos].val; pos++;
    return RC_OK;
}

void MyVTMap2::destroy(){
    this->~MyVTMap2();
}

MyVTMap2::~MyVTMap2() {
    if(elts!=NULL) free(elts);
}
/*
 * Basic test case for keys with different types in VT_MAP
 */
void TestVTMap::doCase4(){
    Value values[1]; 
    const Value *key,*value,*val,*ret[1];
    uint64_t cnt=0;
    ICursor *res = NULL;
    IPIN* pin = NULL;
    RC rc = RC_OK;
    URIMap pmaps[1];
    URIID ids[1];
    
    mLogger.out() << "Start testvtmap doCase4... " << endl;
    
    MVTApp::mapURIs(mSession, "TestVTMap.prop.c4", 1, pmaps);
    ids[0] = pmaps[0].uid;

    MyVTMap2 *map = new MyVTMap2();
    values[0].set(map);
    values[0].property = ids[0];

    // insert a PIN with a VT_MAP value
    TVERIFYRC(mSession->createPIN(values, 1, NULL, MODE_PERSISTENT|MODE_COPY_VALUES)); 

    //verify the result
    IStmt * const stmt = mSession->createStmt("SELECT * WHERE EXISTS ($0)", ids, 1);
    TVERIFYRC(stmt->count(cnt)); TVERIFY(cnt==1);
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin = res->next())!=NULL);
    ret[0] = pin->getValue(ids[0]);
    TVERIFY(ret[0]->type == VT_MAP);
    IMap* vmap = ret[0]->map;
    
    while((rc = vmap->getNext(key, value, false)) == RC_OK){
        if(key->type == VT_UINT){
            val = map->find(*key);
            TVERIFY(val != NULL);
            TVERIFY(strncmp(value->str, val->str, strlen(value->str)) == 0);
        } else if(key->type == VT_STRING){
            val = map->find(*key);
            TVERIFY(val != NULL);
            TVERIFY(val->ui == value->ui);
        } else 
            assert(0);
    }
    TVERIFY(rc == RC_EOF);
    
    if(map) map->destroy();
    if(stmt) stmt->destroy();
    if(res) res->destroy();
    mLogger.out() << "testvtmap doCase4 finish!" << endl;
}

/*
 * Basic test case for ISession::createMap()
 */
void TestVTMap::doCase5(){
    MapElt *elts = NULL;
    Value values[2]; 
    const Value *key,*value,*ret[2];
    uint64_t cnt=0;
    ICursor *res = NULL;
    IPIN* pin = NULL;
    RC rc = RC_OK;
    URIMap pmaps[2];
    IMap *map=NULL;

    mLogger.out() << "Start testvtmap doCase5... " << endl;
    MVTApp::mapURIs(mSession, "TestVTMap.prop.c5", 2, pmaps);

    elts = (MapElt *)mSession->malloc(sizeof(MapElt)*3);
    elts[0].key.set("zero");
    elts[0].val.set(0);
    elts[1].key.set("one");
    elts[1].val.set(1);
    elts[2].key.set("two");
    elts[2].val.set(2);
    TVERIFY(mSession->createMap(NULL, 3, map) != RC_OK); 
    TVERIFY(mSession->createMap(elts, 0, map)!=RC_OK); 
    TVERIFYRC(mSession->createMap(elts, 3, map));
    values[0].set(map);
    values[0].property = pmaps[0].uid;
    values[1].set("HELLO");
    values[1].property = pmaps[1].uid;

    // insert a PIN with a VT_MAP value
    TVERIFYRC(mSession->createPIN(values, 2, NULL, MODE_PERSISTENT|MODE_COPY_VALUES)); 

    //verify the result
    IStmt * const stmt = mSession->createStmt("SELECT * WHERE EXISTS ($0)", &pmaps[0].uid, 1);    
    TVERIFYRC(stmt->count(cnt)); TVERIFY(cnt==1);
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin = res->next())!=NULL);
    ret[0] = pin->getValue(pmaps[0].uid);
    TVERIFY(ret[0]->type == VT_MAP);
    IMap* vmap = ret[0]->map;

    unsigned int i =0;
    while((rc = vmap->getNext(key, value, false)) == RC_OK){
        TVERIFY(key->type==VT_STRING);
        if (strncmp(key->str, "zero", key->length) == 0) {
            TVERIFY(value->type == VT_INT);
            TVERIFY(value->i == 0);
        } else if (strncmp(key->str, "one", key->length) == 0){
            TVERIFY(value->type == VT_INT);
            TVERIFY(value->i == 1);            
        } else if (strncmp(key->str, "two", key->length) == 0){
            TVERIFY(value->type == VT_INT);
            TVERIFY(value->i == 2);            
        } else TVERIFY(false);
        i++;
    }
    TVERIFY(rc == RC_EOF);
    TVERIFY(i==3);

// not enable this
#if 0 
    // modify the string property to a map
    if(elts) mSession->free(elts);
    if(map) map->destroy();

    elts = (MapElt *)mSession->malloc(sizeof(MapElt)*2);
    elts[0].key.set(0);
    elts[0].val.set("zero");
    elts[1].key.set(1);
    elts[1].val.set("one"); 
    TVERIFYRC(mSession->createMap(elts, 2, map));
    //SETVALUE(values[0], pmaps[1].uid, map, OP_SET);
    values[0].set(map);
    values[0].property = pmaps[1].uid;
    values[0].op = OP_SET;
    TVERIFYRC(pin->modify(&values[0], 1));

    // modify the map property to a string
    values[1].set("WORLD");
    values[1].property = pmaps[0].uid;
    values[1].op = OP_SET;
    TVERIFYRC(pin->modify(&values[1], 1));    
#endif

    if(map) map->destroy();
    if(stmt) stmt->destroy();
    if(res) res->destroy();
    if(elts) mSession->free(elts);
    mLogger.out() << "testvtmap doCase5 finish! " << endl;
}

/*
  * test case for testing ENUMERATION
  */
void TestVTMap::doTestEnumeration(){
    ICursor *res = NULL;
    IPIN* pin = NULL;
    uint64_t cnt=0;
    URIMap pm[2];
    const Value *key,*value,*ret;
    IMap *vmap=NULL;
    RC rc = RC_OK;
    
    mLogger.out() << "Start testvtmap doTestEnumeration... " << endl;

    TVERIFYRC(MVTApp::execStmt(mSession,"CREATE ENUMERATION TESTENUM_WEEK AS {'SUNDAY', 'MONDAY', 'TUESDAY', 'WEDNESDAY', 'THURSDAY', 'FRIDAY', 'SATURDAY'};"));
    TVERIFYRC(MVTApp::execStmt(mSession,"INSERT (testenum_p1, testenum_p2) VALUES ('2012-11-26', TESTENUM_WEEK#MONDAY)"));
    TVERIFYRC(MVTApp::execStmt(mSession,"INSERT (testenum_p1, testenum_p2) VALUES ('2012-11-26', TESTENUM_WEEK#MONDAY)"));
    TVERIFYRC(MVTApp::execStmt(mSession,"INSERT (testenum_p1, testenum_p2) VALUES ('2012-12-11', TESTENUM_WEEK#TUESDAY)"));
    TVERIFYRC(MVTApp::execStmt(mSession,"INSERT (testenum_p1, testenum_p2) VALUES ('2012-12-12', TESTENUM_WEEK#WEDNESDAY)"));
    TVERIFYRC(MVTApp::execStmt(mSession,"INSERT (testenum_p1, testenum_p2) VALUES ('2012-12-13', TESTENUM_WEEK#THURSDAY)"));
    TVERIFYRC(MVTApp::execStmt(mSession,"INSERT (testenum_p1, testenum_p2) VALUES ('2012-12-14', TESTENUM_WEEK#FRIDAY)"));
    TVERIFYRC(MVTApp::execStmt(mSession,"INSERT (testenum_p1, testenum_p2) VALUES ('2012-12-15', TESTENUM_WEEK#SATURDAY )"));
    TVERIFYRC(MVTApp::execStmt(mSession,"CREATE CLASS testenum_c1 AS SELECT * WHERE EXISTS(testenum_p2)"));

    TVERIFY(MVTApp::countStmt(mSession,"SELECT * FROM testenum_c1 WHERE testenum_p2 = TESTENUM_WEEK#TUESDAY;") == 1);
    TVERIFY(MVTApp::countStmt(mSession,"SELECT * FROM testenum_c1 WHERE testenum_p2 = TESTENUM_WEEK#MONDAY;") == 2);

    //enumeration comparison    
    TVERIFY(MVTApp::countStmt(mSession,"SELECT * FROM testenum_c1 WHERE testenum_p2 > TESTENUM_WEEK#WEDNESDAY;") == 3);
    TVERIFY(MVTApp::countStmt(mSession,"SELECT * FROM testenum_c1 WHERE testenum_p2 >= TESTENUM_WEEK#WEDNESDAY;") == 4);  
    TVERIFY(MVTApp::countStmt(mSession,"SELECT * FROM testenum_c1 WHERE testenum_p2 < TESTENUM_WEEK#WEDNESDAY;") == 3);   
    TVERIFY(MVTApp::countStmt(mSession,"SELECT * FROM testenum_c1 WHERE testenum_p2 <= TESTENUM_WEEK#WEDNESDAY;") == 4);    
    TVERIFY(MVTApp::countStmt(mSession,"SELECT * FROM testenum_c1 WHERE testenum_p2 <> TESTENUM_WEEK#WEDNESDAY;") == 6);

    pm[0].URI = "week_mapping_with_map_as_values";
    pm[1].URI = "week_mapping_with_map_as_keys";
    TVERIFYRC(mSession->mapURIs(2,pm));

    // use enumeration elements in a VT_MAP as a value
    TVERIFYRC(MVTApp::execStmt(mSession, "insert week_mapping_with_map_as_values={0->TESTENUM_WEEK#SUNDAY, 1->TESTENUM_WEEK#MONDAY}"));    
    IStmt *stmt = mSession->createStmt("SELECT * WHERE EXISTS (week_mapping_with_map_as_values)");    
    TVERIFYRC(stmt->count(cnt)); TVERIFY(cnt==1);
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin = res->next())!=NULL);
    ret = pin->getValue(pm[0].uid);
    TVERIFY(ret->type == VT_MAP);
    vmap = ret->map;

    unsigned int i =0;
    while((rc = vmap->getNext(key, value, false)) == RC_OK){
        TVERIFY(key->type==VT_INT);
        TVERIFY(value->type==VT_ENUM);
        i++;
    }
    TVERIFY(key->i==1);
    TVERIFY(i==2);
    if (stmt) stmt->destroy();
    if (res) res->destroy();
    if (vmap) vmap->destroy();
    
    // use enumeration elements in a VT_MAP as a key
    TVERIFYRC(MVTApp::execStmt(mSession, "insert week_mapping_with_map_as_keys={TESTENUM_WEEK#SUNDAY->0, TESTENUM_WEEK#TUESDAY->2}"));
    TVERIFY(MVTApp::countStmt(mSession, "select * where exists(week_mapping_with_map_as_keys)") == 1); 

    stmt = mSession->createStmt("SELECT * WHERE EXISTS (week_mapping_with_map_as_keys)");    
    TVERIFYRC(stmt->count(cnt)); TVERIFY(cnt==1);
    TVERIFYRC(stmt->execute(&res));
    TVERIFY((pin = res->next())!=NULL);
    ret = pin->getValue(pm[1].uid);
    TVERIFY(ret->type == VT_MAP);
    vmap = ret->map;

    i = 0;
    while((rc = vmap->getNext(key, value, false)) == RC_OK){
        TVERIFY(key->type==VT_ENUM);
        TVERIFY(value->type==VT_INT);
        i++;
    }
    TVERIFY(value->i==2);
    TVERIFY(i==2);

    if (stmt) stmt->destroy();
    if (res) res->destroy();
    if (vmap) vmap->destroy();
    mLogger.out() << "testvtmap doTestEnumeration finish! " << endl;
}

