#include "app.h"
using namespace std;

#include "mvauto.h"
#ifndef WIN32
#include <limits.h>
#endif
#include <vector>
#include <set>

// Publish this test.
class TestMultiColumnIndex : public ITest
{
    public:
        TEST_DECLARE(TestMultiColumnIndex);
        virtual char const * getName() const { return "testmulticolumnindex"; }
        virtual char const * getDescription() const { return "work-in-progress; a good sweep of multi-variable indexes"; }
        virtual char const * getHelp() const { return ""; } 
        virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
        virtual bool isLongRunningTest()const { return false; }
        virtual bool includeInMultiStoreTests() const { return false; }
        virtual int execute();
        virtual void destroy() { delete this; }
        typedef std::vector<IPIN *> TPins;
        ValueType* mapVT;
        int num_mapVT;
    protected:
        struct DescrCondition
        {
            size_t mPropIndex; // The index of the property involved in this condition (in mProps).
            Afy::ExprOp mCondType; // The operator that compares the property to the indexed parameter.
            DescrCondition(size_t pPropIndex, Afy::ExprOp pCondType) : mPropIndex(pPropIndex), mCondType(pCondType) {}
        };
        typedef std::vector<DescrCondition> DescrIndex; // Multi-column indexes are defined by multiple conditions.
        typedef std::vector<ClassID> ClassIDs;
        typedef std::vector<std::string> Strings;
        typedef std::vector<Value> Values;
        struct DescrFamily
        {
            DescrIndex mDescr; // The logical description.
            std::string mBaseName; // The base name used to generate the various families involved.
            ClassID mMulti; // The class id of the multi-variable implementation.
            ClassIDs mSingles; // The class ids of the corresponding single-variable implementations.
            std::string mMultiName; // The class name of the multi-variable implementation.
            Strings mSinglesNames; // The class names of the single-variable implementations.
            Value mProposedValsI[1024]; // Intermediate values, used to construct mVals (e.g. for ranges).
            Values mProposedVals; // Sets of values that can be used at input for query evaluation; should always contain N*size(mDescr), where N>=1.
            DescrFamily(char const * pBaseName) : mBaseName(pBaseName), mMulti(STORE_INVALID_CLASSID) {}
        };
        typedef std::vector<DescrFamily> Scenarios; // To allow declaring all the indexes of the test (to control when the classes are created: beginning, or end, or progressive).
        int pos_end_collection;     // keep the position of last DescrFamily which is built on multiple columns with collection.
        IExprTree * buildCondition(DescrCondition const & pDescr, size_t pCondIndex = 0)
        {
            // Build an expression that compares mProps[pPropIndex] to the pCondIndex-th parameter of the index.
            Value lV[2];
            lV[0].setVarRef(0,mProps[pDescr.mPropIndex]);
            lV[1].setParam(pCondIndex);
            //lV[1].refV.type=PropMapVT[pDescr.mPropIndex-10];
            return mSession->expr(pDescr.mCondType, 2, lV);
        }
        IStmt * buildIndexSingle(DescrCondition const & pDescr)
        {
            // Build a statement ready to define a class on the given condition.
            IStmt * const lResult = mSession->createStmt();
            QVarID const lVar = lResult->addVariable();
            IExprTree * const lET = buildCondition(pDescr, 0);
            lResult->addCondition(lVar, lET);
            lET->destroy();
            return lResult;
        }
        IStmt * buildIndexMulti(DescrIndex const & pDescr)
        {
            // Build a statement ready to define a class on the given conditions.
            IStmt * const lResult = mSession->createStmt();
            QVarID const lVar = lResult->addVariable();
            size_t i;
            DescrIndex::const_iterator iD;
            for (i = 0, iD = pDescr.begin(); pDescr.end() != iD; i++, iD++)
            {
                CmvautoPtr<IExprTree> lET(buildCondition((*iD), i));
                lResult->addCondition(lVar, lET);
            }
            return lResult;
        }
        void buildFamilies(DescrFamily & pF)
        {
            // Build the multi-variable case.
            {
                CmvautoPtr<IStmt> lStmt(buildIndexMulti(pF.mDescr));
                pF.mMulti = MVTUtil::createUniqueClass(mSession, pF.mBaseName.c_str(), lStmt, &pF.mMultiName);
                TVERIFY(STORE_INVALID_CLASSID != pF.mMulti);
            }
            // Build the single-variable cases.
            DescrIndex::const_iterator iD;
            for (iD = pF.mDescr.begin(); pF.mDescr.end() != iD; iD++)
            {
                CmvautoPtr<IStmt> lStmt(buildIndexSingle(*iD));
                std::string lClsName;
                pF.mSingles.push_back(MVTUtil::createUniqueClass(mSession, pF.mBaseName.c_str(), lStmt, &lClsName));
                pF.mSinglesNames.push_back(lClsName);
                TVERIFY(STORE_INVALID_CLASSID != pF.mSingles.back());
            }
        }
        void proposeValues(DescrFamily & pF, bool fcollection);
        typedef std::set<PID> TPIDs;
        TPIDs evalSingles(DescrFamily const & pF, Value * pValues)
        {
            // Evaluate a join of pF.mSingles with pValues (assumed to match the count of conditions in pF).
            TPIDs lResult;
            CmvautoPtr<IStmt> lStmt(mSession->createStmt());
            size_t i;
            unsigned char lVars[255];
            for (i = 0; i < pF.mDescr.size(); i++)
            {       
                SourceSpec lCS;
                lCS.objectID = pF.mSingles[i];
                lCS.params = &pValues[i];
                lCS.nParams = 1;
                lVars[i] = lStmt->addVariable(&lCS, 1);
            }
            lStmt->setOp(lVars, pF.mDescr.size(), QRY_INTERSECT);
            mLogger.out() << "singles query " << std::endl << lStmt->toString() << std::endl;
            ICursor* lC = NULL;
            TVERIFYRC(lStmt->execute(&lC));
            CmvautoPtr<ICursor> lCursor(lC);
            if (!lCursor.IsValid())
                { TVERIFY2(false, "Invalid/NULL cursor!"); return lResult; }
            IPIN * lP;
            while (NULL != (lP = lCursor->next()))
            {
                lResult.insert(lP->getPID());
                //printf("%s%x\n", "evalsingles : pid : ", lP->getPID());
                lP->destroy();
            }
            return lResult;
        }
        TPIDs evalMulti(DescrFamily const & pF, Value * pValues)
        {
            // Evaluate pF.mMulti with pValues (assumed to match the count of conditions in pF).
            TPIDs lResult;
            CmvautoPtr<IStmt> lStmt(mSession->createStmt());
            SourceSpec lCS;
            lCS.objectID = pF.mMulti;
            lCS.params = pValues;
            lCS.nParams = pF.mDescr.size();
            lStmt->addVariable(&lCS, 1);
            mLogger.out() <<  "Multi query " << std::endl << lStmt->toString() << std::endl;
            ICursor* lC = NULL;
            TVERIFYRC(lStmt->execute(&lC));
            CmvautoPtr<ICursor> lCursor(lC);
            if (!lCursor.IsValid())
                { TVERIFY2(false, "Invalid/NULL cursor!"); return lResult; }
            IPIN * lP;
            while (NULL != (lP = lCursor->next()))
            {
                lResult.insert(lP->getPID());
                //printf("%s%x\n", "evalMulti : pid : ", lP->getPID());
                lP->destroy();
            }
            return lResult;
        }
        void createPins();
        void createPins2();
        void deletePins();
        void evalResults();
    protected:
        ISession * mSession;
        /* 0 ~ 9 used for index created on multiple collections 
          * 10 ~ 11 used for index created on multiple columns with basic type 
          */
        #define SPLIT 10
        PropertyID mProps[20];
        #define NUM_PINS 100
        Scenarios mScenarios;
};
TEST_IMPLEMENT(TestMultiColumnIndex, TestLogger::kDStdOut);

int TestMultiColumnIndex::execute()
{
    if (!MVTApp::startStore())
        { TVERIFY2(0, "Could not start store, bailing out completely"); return 1; }
    mSession = MVTApp::startSession();
    MVTApp::mapURIs(mSession, "TestMultiColumnIndex.prop", sizeof(mProps) / sizeof(mProps[0]), mProps);

    // index from 10th in mProps is used to mapping mapVT
    ValueType vtarray[] = { VT_INT, VT_DOUBLE, VT_STRING, VT_DOUBLE}; //TODO: more basic types
    mapVT = vtarray;
    num_mapVT = sizeof(vtarray)/sizeof(vtarray[0]);
    pos_end_collection = 0;
    // Describe the logical multi-index 'families'.
    // Note:
    //   The test expands each of these logical definitions into
    //   1) a multi-parameter family, and
    //   2) a join between multiple single-parameter families
    {
        /*
        DescrFamily lF1("TestMultiColumnIndex.A");
        lF1.mDescr.push_back(DescrCondition(0, OP_IN));
        lF1.mDescr.push_back(DescrCondition(1, OP_GT));
        lF1.mDescr.push_back(DescrCondition(2, OP_IN));
        mScenarios.push_back(lF1); // Note: Copied.
        pos_end_collection = 0;
        
        DescrFamily lF2("TestMultiColumnIndex.B");
        lF2.mDescr.push_back(DescrCondition(0, OP_IN));
        lF2.mDescr.push_back(DescrCondition(1, OP_IN));
        mScenarios.push_back(lF2); // Note: Copied.
        
        DescrFamily lF3("TestMultiColumnIndex.C");
        lF3.mDescr.push_back(DescrCondition(0, OP_GT));
        lF3.mDescr.push_back(DescrCondition(1, OP_GT));
        mScenarios.push_back(lF3); // Note: Copied.
        */
        // IF4 ~ IF6 is used to index on many test basic types(not on collections)
        DescrFamily lF4("TestMultiColumnIndex.C");
        lF4.mDescr.push_back(DescrCondition(10, OP_LT));
        lF4.mDescr.push_back(DescrCondition(13, OP_GT));
        mScenarios.push_back(lF4); // Note: Copied.

        DescrFamily lF5("TestMultiColumnIndex.D");
        lF5.mDescr.push_back(DescrCondition(11, OP_LT));
        lF5.mDescr.push_back(DescrCondition(12, OP_LT));
        lF5.mDescr.push_back(DescrCondition(13, OP_IN));
        mScenarios.push_back(lF5); // Note: Copied.

        DescrFamily lF6("TestMultiColumnIndex.E");
        lF6.mDescr.push_back(DescrCondition(10, OP_IN));
        lF6.mDescr.push_back(DescrCondition(12, OP_GT));
        lF6.mDescr.push_back(DescrCondition(13, OP_GT));
        mScenarios.push_back(lF6); // Note: Copied.
    }
    // TODO: more scenarios...

    {
        // TODO:
        //   build+c[r]ud+evaluate in different order
        //   (e.g. build all at beginning, vs all at end, vs progressively)
        //   (e.g. evaluate only some results vs all results)

        // Build the corresponding store families.
        for (Scenarios::iterator iS = mScenarios.begin(); mScenarios.end() != iS; iS++)
            buildFamilies(*iS);

        // Create pins.
        // TODO: all CRUD, not just create...
        //createPins();
        createPins2();

        // Delete some pins
        deletePins();

        // Evaluate results.
        evalResults();

        //delete some PINs and evaluate results.
    }

    mSession->terminate();
    MVTApp::stopStore();
    return 0;
}

#define LAST_INDEX_IN_ARRAY(pArray) \
    (sizeof(pArray) / sizeof(pArray[0]) - 1)
#define RANDOM_INDEX_IN_ARRAY(pArray) \
    (size_t(MVTRand::getRange(0, LAST_INDEX_IN_ARRAY(pArray))))
#define SET_RANDOM_RANGE(pArray) \
    { \
        size_t const lAt1 = min(RANDOM_INDEX_IN_ARRAY(pArray), LAST_INDEX_IN_ARRAY(pArray) - 1); \
        size_t const lAt2 = lAt1 + MVTRand::getRange(1, LAST_INDEX_IN_ARRAY(pArray) - lAt1); \
        SETVALUE(pF.mProposedValsI[iValI], mProps[(*iD).mPropIndex], pArray[lAt1], OP_SET); \
        SETVALUE(pF.mProposedValsI[iValI + 1], mProps[(*iD).mPropIndex], pArray[lAt2], OP_SET); \
    }
void TestMultiColumnIndex::proposeValues(DescrFamily & pF, bool fcollection)
{
    // Propose a series of random (or not) values/ranges that match pF,
    // for evalResults.
    // TODO: more types (e.g. dates etc.), more diversity of values (e.g. shorter/longer strings), etc.
    // TODO: not just random proposed values, some controlled ones as well...
    pF.mProposedVals.clear();
    int lOrderedInts[] = {INT_MIN, -1000000000, -100000000, -10000000, -1000000, -100000, -10000, -1000, -100, -10, 0, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000, INT_MAX};
    char * lOrderedStrings[] = {"A", "AAA", "Alphabet", "Cabbage", "Gorilla", "Xylophone", "abracadabra", "polygon", "zzz"};
    double lOrderedDoubles[] = {-1000000.25, -100000.25, -1000.25, 0.0, 0.5, 1.0, 3.141592654, 1000.25, 100000.25};
    float lOrderedFloats[] = {-199.2F, -10.1F, 0.1F, 2.1F, 100.7F};
    ValueType lProposedVT[] = {Afy::VT_INT, VT_STRING, VT_FLOAT, VT_DOUBLE}; // TODO: more...
    int i, iValI, cValue = 0;
    DescrIndex::const_iterator iD;
    // caculate the min number of values we need to create
    for (iD = pF.mDescr.begin(); pF.mDescr.end() != iD; iD++)
    {
            switch ((*iD).mCondType)
            {
                case OP_IN:
                    cValue += 2;
                    break;
                default:
                    cValue += 1;
            }
    }
    //printf("%s%d\n", "created values : ", cValue);
    #define BASE 5
    for (i = 0, iValI = 0; i < (BASE * cValue); i++)
    {
        for (iD = pF.mDescr.begin(); pF.mDescr.end() != iD; iD++)
        {
            pF.mProposedVals.push_back(Value());
            size_t iVt=0;
            ValueType type;
            if (fcollection)
            {
                iVt = RANDOM_INDEX_IN_ARRAY(lProposedVT);
                type = lProposedVT[iVt];
            }   
            else
            {
                iVt = (*iD).mPropIndex - SPLIT;
                type = mapVT[iVt];
            }
            switch ((*iD).mCondType)
            {
                case OP_IN:
                {
                    switch (type)
                    {
                        case VT_INT: SET_RANDOM_RANGE(lOrderedInts); break;
                        case VT_STRING: SET_RANDOM_RANGE(lOrderedStrings); break;
                        case VT_DOUBLE: SET_RANDOM_RANGE(lOrderedDoubles); break;
                        case VT_FLOAT: SET_RANDOM_RANGE(lOrderedFloats); break;
                        default: assert(0); break;
                    }  
                    pF.mProposedVals.back().setRange(&pF.mProposedValsI[iValI]);
                    pF.mProposedVals.back().setPropID(mProps[(*iD).mPropIndex]);
                    iValI += 2;
                    break;
                }
                default:
                    switch (type)
                    {
                        case VT_INT: SETVALUE(pF.mProposedVals.back(), mProps[(*iD).mPropIndex], lOrderedInts[RANDOM_INDEX_IN_ARRAY(lOrderedInts)], OP_SET); break;
                        case VT_STRING: { std::string lS = MVTRand::getString2(5, -1, false); SETVALUE(pF.mProposedVals.back(), mProps[(*iD).mPropIndex], strdup(lS.c_str()), OP_SET); break; }
                        case VT_DOUBLE: SETVALUE(pF.mProposedVals.back(), mProps[(*iD).mPropIndex], lOrderedDoubles[RANDOM_INDEX_IN_ARRAY(lOrderedDoubles)], OP_SET); break;                        
                        case VT_FLOAT: SETVALUE(pF.mProposedVals.back(), mProps[(*iD).mPropIndex], lOrderedFloats[RANDOM_INDEX_IN_ARRAY(lOrderedFloats)], OP_SET); break;   
                        default: assert(0); break;
                    }
                    break;
            }
        }
    }
}

void TestMultiColumnIndex::createPins()
{
    // Create pins containing random fields likely to be selected by
    // some of the 'scenarios' defined by the test.
    mLogger.out() << "Creating pins...";
    size_t i, j;
    TPins lPins;
    for (i = 0; i < NUM_PINS; i++)
    {
        if ((i % 10) == 0)
            mLogger.out() << "." << std::flush;

        // TODO: better control of everything (collections, data types, values etc.)
        // TODO: better control of distribution, to hit a better proportion of the 'scenarios'
        // TODO: have a flavor that will commit pins by smaller batches, just in case it impacts indexing differently...
        // Note: the values here are crucial for good coverage.
        Value * lV = (Value *)mSession->malloc(10 * sizeof(Value));
        for (j = 0; j < 10; j++)
        {
            if (MVTRand::getBool())
                { SETVALUE_C(lV[j], mProps[j % 3], MVTRand::getRange(1, 10000), OP_ADD, STORE_LAST_ELEMENT); }
            else
            {
                std::string const lS = MVTRand::getString2(5, -1, false);
                char * const lStr = (char *)mSession->malloc(1 + lS.length());
                memcpy(lStr, lS.c_str(), lS.length());
                lStr[lS.length()] = 0;
                SETVALUE_C(lV[j], mProps[j % 3], lStr, OP_ADD, STORE_LAST_ELEMENT);
            }
        }
        lPins.push_back(mSession->createPIN(lV, 10));
    }

    mLogger.out() << std::endl << "Committing pins..." << std::endl;
    TVERIFYRC(mSession->commitPINs(&lPins[0], lPins.size()));
    for (i = 0; i < lPins.size(); i++)
        lPins[i]->destroy();
}

void TestMultiColumnIndex::createPins2()
{
    /*
     * create many PINs, the type of property are basic types, int, double, float, string, etc.
     * the goal is to test multi-column index on many basic type properties.
     */
    mLogger.out() << "Creating pins2...";
    int i, j, nProps;
    TPins lPins;
    std::string lS;
    char * lStr;
        
    for (i = 0; i < NUM_PINS; i++)
    {
        if ((i % 10) == 0)
            mLogger.out() << "." << std::flush;

        // create pin with random number of properties, with random value 
        // TODO:  more randomly
        nProps = MVTRand::getRange(2, num_mapVT);
        Value * values = (Value *)mSession->malloc(nProps * sizeof(Value));
        for (j = 0; j < nProps; j++)
        {
            ValueType type = mapVT[j];
            switch(type)
            {
                case VT_INT:
                    SETVALUE(values[j], mProps[SPLIT+j], MVTRand::getRange(-1000, 1000), OP_SET);
                    break;
                case VT_FLOAT:
                    SETVALUE(values[j], mProps[SPLIT+j], MVTRand::getFloatRange(-1000, 1000), OP_SET);
                    break;
                case VT_STRING:
                    lS = MVTRand::getString2(5, -1, false);
                    lStr = (char *)mSession->malloc(1 + lS.length());
                    memcpy(lStr, lS.c_str(), lS.length());
                    lStr[lS.length()] = 0;
                    SETVALUE(values[j], mProps[SPLIT+j], lStr, OP_SET);
                    break;
                case VT_DOUBLE:
                    SETVALUE(values[j], mProps[SPLIT+j], MVTRand::getDoubleRange(-1000, 1000), OP_SET);
                    break;
                default:
                    assert(0);
                    break;
            }
        }
        IPIN * pin = mSession->createPIN(values, nProps);
        if(pin != NULL)
            lPins.push_back(pin);
    }

    mLogger.out() << std::endl << "Committing " <<  i  << " pins..." << std::endl;
    TVERIFYRC(mSession->commitPINs(&lPins[0], lPins.size()));
    for (size_t k = 0; k < lPins.size(); k++)
        lPins[k]->destroy();
    
}

void TestMultiColumnIndex::deletePins()
{
    /*
     * delete some pins
     * delete where mProps[10] in (-100, 100)
     */
    // TODO:delete PINs more randomly(random range on random properties)
    Value  minmax[2]; 
    minmax[0].set(-100); // Min Value .
    minmax[1].set(100);   // Max Value 

    Value  exprArgs[2]; 
    exprArgs[0].setVarRef(0,* &mProps[SPLIT]);
    exprArgs[1].setRange(&minmax[0]);

    IExprTree *exprRange; 
    exprRange=mSession->expr(OP_IN,2,&exprArgs[0]);
    TVERIFY(exprRange != NULL);

    IStmt * stmt; unsigned char qV1;
    if(NULL == (stmt=mSession->createStmt(STMT_DELETE))){
        exprRange->destroy();
        throw string("Error, failed to create statement! ");
    }
    
    qV1 = stmt->addVariable(); 
    stmt->addCondition(qV1, exprRange); 

    uint64_t cnt = 0;
    TVERIFYRC(stmt->execute(NULL,NULL,0,~0,0,MODE_PURGE, &cnt));
    mLogger.out() << cnt << " PINs have been deleted " << std::endl;

    exprRange->destroy(); 
    stmt->destroy();
}

void TestMultiColumnIndex::evalResults(/*TODO:optionally specify the scenario etc.*/)
{
    // Evaluate all/some of the 'scenarios' and see if their various implementations
    // select the same pins (n.b. the point is that multi-column indexes are more complex
    // to maintain in the store, hence more error-prone, but logically should return
    // the same results as their join equivalent).
    // TODO: we could also add full-scan as a third validation point, like in testmultivarjoin.
    mLogger.out() << "Evaluating queries..." << std::endl;
    int iScen;
    Scenarios::iterator iS;
    for (iScen = 0, iS = mScenarios.begin(); mScenarios.end() != iS; iScen++, iS++)
    {
        if (iScen < pos_end_collection)
            proposeValues(*iS, true);
        else
            proposeValues(*iS, false);
        mLogger.out() << "  number of proposed values for scenario" << iScen << ": " << (*iS).mProposedVals.size() << std::endl;
        size_t const lNumValSeries = (*iS).mProposedVals.size() / (*iS).mDescr.size();
        for (size_t i = 0; i < lNumValSeries; i++)
        {
            mLogger.out() << "  evaluating scenario " << iScen << " with series of values " << i << std::endl;
            size_t const lValStart = i * (*iS).mDescr.size();
            #if 0
                for (size_t j = lValStart; j < lValStart + (*iS).mDescr.size(); j++)
                    MVTApp::output((*iS).mProposedVals[j], mLogger.out());
            #endif

            Value * const lV = &(*iS).mProposedVals[lValStart];
            TPIDs const lR1 = evalSingles((*iS), lV);
            TPIDs const lR2 = evalMulti((*iS), lV);
            mLogger.out() << "  selected " << lR1.size() << " in 'singles (aka join)' vs " << lR2.size() << " in 'multi-column'" << std::endl;
            TVERIFY2(lR1 == lR2, "Comparison between singles and multi failed!");
        }
    }
}
