#include "app.h"
#include <list>
using namespace std;

#include "mvauto.h"

#define NUMBER_PINS 1000
#define MAX_STR_LEN 20

enum VISITOR_FIELDS{
    VISITOR_ID, 
    VISITOR_NAME, 
    VISITOR_ADDRESS,
    VISITOR_ADDRESS_STREET, 
    VISITOR_ADDRESS_CITY, 
    VISITOR_ADDRESS_COUNTRY, 
    VISITOR_ADDRESS_ZIP,
    VISITOR_VISITED,
    VISITOR_VISITED_CITY,
    VISITOR_VISITED_COUNTRY,
    VISITOR_RECORD,
    VISITOR_ALL
};

// Publish this test.
class TestVTStruct : public ITest
{
    public:
    TEST_DECLARE(TestVTStruct);
        virtual char const * getName() const { return "TestVTStruct"; }
        virtual char const * getHelp() const { return ""; }
        virtual char const * getDescription() const { return "testing VT_STRUCT"; }
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Simple test..."; return true; }
        virtual bool isLongRunningTest()const {return false;}
        virtual bool includeInMultiStoreTests() const { return false; }
        virtual int execute();
        virtual void destroy() { delete this; }
        
        typedef struct Visitor{
            unsigned int id;
            Tstring name;
            // address act as a VT_STRUCT in a People pin
            typedef struct Address {
                Tstring street;
                Tstring city;
                Tstring country;             
                unsigned int zip;
            } Address;
            Address address;

            typedef struct Visited {
                Tstring name;
                Tstring country;
            } Visited;
            // cities visited by this people, act as a collection of  VT_STRUCT in a People pin
            Visited* visited;
        } Visitor;        
    protected:
        URIID ids[VISITOR_ALL];
        std::list<Visitor> visitors;  
        ClassID objectID;
        string classname;
        void doTest();
        void insert();
        void insertCollAsScalarsInsideStruct();
    private:
        ISession * mSession;
};

TEST_IMPLEMENT(TestVTStruct, TestLogger::kDStdOut);

int TestVTStruct::execute()
{
    doTest();
    return RC_OK;
}

void TestVTStruct::doTest()
{
    bool bStarted = MVTApp::startStore() ;
    if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

    mSession = MVTApp::startSession();

    /* map property URIs */
    URIMap pmaps[VISITOR_ALL];
    MVTApp::mapURIs(mSession, "TestVTStruct.prop", VISITOR_ALL, pmaps);
    for (int i = 0; i < VISITOR_ALL; i++)
        ids[i] = pmaps[i].uid;

    /* insert PINs with VT_STRUCT */
    insert();
    insertCollAsScalarsInsideStruct();

    mSession->terminate(); 
    MVTApp::stopStore();
}

void TestVTStruct::insert()
{
    mLogger.out() << "Begin to insert pins for testvtstruct... " << endl;

    // insert some pins with VT_STRUCT and a collection of VT_STRUCT
    TVERIFYRC(MVTApp::execStmt(mSession, "insert name =\'li \', \
address={city=\'beijing\', street=\'zhongguancun\', country=\'china\', zip=100190}"));
    TVERIFYRC(MVTApp::execStmt(mSession, "insert name =\'zhang \', \
visited={{city=\'shanghai\',country=\'china\'} , {city=\'tianjin\', country=\'china\'}}"));
    TVERIFYRC(MVTApp::execStmt(mSession, "insert name =\'wang \', \
visited={{city=\'beijing\', country=\'china\'}, {city=\'hangzhou\', country=\'china\'}}"));

    TVERIFY(MVTApp::countStmt(mSession, "select * where exists(visited)") == 2);
    TVERIFY(MVTApp::countStmt(mSession, "select * where exists(address)") == 1);
    
    Visitor visitor;
    for (int i = 0; i < NUMBER_PINS; i++)  {  
        Value values[13], add_vals[4];
        // visitor's id
        values[0].set(i);
        values[0].property = ids[VISITOR_ID];     
        visitor.id = i;

        //visitor's name
        Tstring name;
        MVTApp::randomString(name, 1, MAX_STR_LEN);        
        values[1].set(name.c_str());
        values[1].property = ids[VISITOR_NAME];
        visitor.name = name;

        //visitor's address
        Tstring street, city1, country1;unsigned int zip; 
        MVTApp::randomString(street, 1, MAX_STR_LEN);  
        MVTApp::randomString(city1, 1, MAX_STR_LEN);  
        MVTApp::randomString(country1, 1, MAX_STR_LEN);  
        zip = MVTRand::getRange(100000,999999);
        add_vals[0].set(street.c_str());
        add_vals[0].property = ids[VISITOR_ADDRESS_STREET];
        add_vals[1].set(city1.c_str());
        add_vals[1].property = ids[VISITOR_ADDRESS_CITY];
        add_vals[2].set(country1.c_str());
        add_vals[2].property = ids[VISITOR_ADDRESS_COUNTRY];
        add_vals[3].set(zip);
        add_vals[3].property = ids[VISITOR_ADDRESS_ZIP];
        values[2].setStruct(add_vals, 4);
        values[2].property = ids[VISITOR_ADDRESS];
        visitor.address.street = street;
        visitor.address.city = city1;
        visitor.address.country = country1;
        visitor.address.zip = zip;

        //visitor's visited cities, a collection of VT_STRUCT, the size of this collection is random(1,10)
        Value visited_vals[2];
        unsigned int len = MVTRand::getRange(1,10);
        Tstring city2, country2;
        for(unsigned int j = 0; j < len; j++) {
            MVTApp::randomString(city2, 1, MAX_STR_LEN);  
            MVTApp::randomString(country2, 1, MAX_STR_LEN);  
            visited_vals[0].set(city2.c_str());
            visited_vals[0].property = ids[VISITOR_VISITED_CITY];
            visited_vals[1].set(country2.c_str());
            visited_vals[1].property = ids[VISITOR_VISITED_COUNTRY];            
            SETVALUE_STRUCT_C(values[j+3], ids[VISITOR_VISITED], visited_vals, 2, OP_ADD, STORE_LAST_ELEMENT);
        }

        // create this pin
        PID pid;
        TVERIFYRC(mSession->createPINAndCommit(pid, values, len+3));      
        
        visitors.push_back(visitor);
        if (i % 10 == 0) cout << ".";
        if (i == NUMBER_PINS -1) cout<<endl;   
    }

    TVERIFYRC(MVTApp::execStmt(mSession,"delete where exists(visited)"));      
    TVERIFYRC(MVTApp::execStmt(mSession,"delete where exists(address)"));    
    TVERIFY(MVTApp::countStmt(mSession, "select * where exists(visited)") == 0);   
    TVERIFY(MVTApp::countStmt(mSession, "select * where exists(address)") == 0);   
    cout << "Totally " << NUMBER_PINS << " PINs have been created. " << endl;
    TVERIFY(visitors.size()==NUMBER_PINS);
}

void TestVTStruct::insertCollAsScalarsInsideStruct()
{
    // Note (maxw):
    //   This is a very specific, minor issue... by analogy/symmetry with pin creation, I expected to be able to
    //   flatten a new collection among the fields of a VT_STRUCT; the result presently only keeps the
    //   last element of that collection, instead of the whole collection.
    Value lStruct;
    Value lStructElms[6];
    Value lCollElms[3];
    lStructElms[0].set("Fred Johnson"); lStructElms[0].setPropID(ids[VISITOR_NAME]);
    lStructElms[1].set("Argentina"); lStructElms[1].setPropID(ids[VISITOR_ADDRESS_COUNTRY]);
    lStructElms[2].set("1234 Orange Street"); lStructElms[2].setPropID(ids[VISITOR_ADDRESS_STREET]);
    lCollElms[0].set("Winnipeg"); lCollElms[0].setPropID(ids[VISITOR_VISITED_CITY]); lCollElms[0].op = OP_ADD; lCollElms[0].eid = STORE_LAST_ELEMENT;
    lCollElms[1].set("Los Angeles"); lCollElms[1].setPropID(ids[VISITOR_VISITED_CITY]); lCollElms[1].op = OP_ADD; lCollElms[1].eid = STORE_LAST_ELEMENT;
    lCollElms[2].set("Prague"); lCollElms[2].setPropID(ids[VISITOR_VISITED_CITY]); lCollElms[2].op = OP_ADD; lCollElms[2].eid = STORE_LAST_ELEMENT;

    IPIN * lP;
    Value const * lVRecord;
    #if 1 // This case works as expected until commitPINs.
        mLogger.out() << "part 1 of insertCollAsScalarsInsideStruct... " << endl;
        lStructElms[3].set(lCollElms, sizeof(lCollElms) / sizeof(lCollElms[0])); lStructElms[3].setPropID(ids[VISITOR_VISITED_CITY]);
        lStruct.setStruct(lStructElms, 4); lStruct.setPropID(ids[VISITOR_RECORD]);
        lP = mSession->createPIN(&lStruct, 1, MODE_COPY_VALUES);
        MVTApp::output(*lP, mLogger.out(), mSession);
        lVRecord = lP->getValue(ids[VISITOR_RECORD]);
        TVERIFY(VT_STRUCT == lVRecord->type);
        TVERIFY(4 == lVRecord->length);
        TVERIFY(lVRecord->varray[lVRecord->length - 1].length == 3);
        TVERIFYRC(mSession->commitPINs(&lP, 1));
        MVTApp::output(*lP, mLogger.out(), mSession);
        lP->destroy();
        mLogger.out() << endl;
    #endif
    #if 1 // This case does not work as I expected.
        mLogger.out() << "part 2 of insertCollAsScalarsInsideStruct... " << endl;
        lStructElms[3] = lCollElms[0];
        lStructElms[4] = lCollElms[1];
        lStructElms[5] = lCollElms[2];
        lStruct.setStruct(lStructElms, 6); lStruct.setPropID(ids[VISITOR_RECORD]);
        lP = mSession->createPIN(&lStruct, 1, MODE_COPY_VALUES);
        MVTApp::output(*lP, mLogger.out(), mSession);
        lVRecord = lP->getValue(ids[VISITOR_RECORD]);
        TVERIFY(VT_STRUCT == lVRecord->type);
        TVERIFY(4 == lVRecord->length);
        TVERIFY(lVRecord->varray[lVRecord->length - 1].length == 3);
        TVERIFYRC(mSession->commitPINs(&lP, 1));
        MVTApp::output(*lP, mLogger.out(), mSession);
        lP->destroy();
        mLogger.out() << endl;
    #endif
}
