/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/
#include "app.h"
/*
 * This case is a part of testmulticolumnindex.cpp.
 * in order to file a bug, isolate it.
 * 1). define a multi-column index, 
 * 2). create pins with collections satisfying the class family definition in step 1
 * 3). commit pins 
 */

// Publish this test.
class TestMultiColIdxonCollections: public ITest
{
    public:
        TEST_DECLARE(TestMultiColIdxonCollections);
        virtual char const * getName() const { return "testmulticolidxoncollections"; }
        virtual char const * getDescription() const { return "part of testmulticolumnindex.cpp, in order to file a bug, isolate it"; }
        virtual char const * getHelp() const { return ""; } 
        virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
        virtual bool isLongRunningTest()const { return false; }
        virtual bool includeInMultiStoreTests() const { return false; }
        virtual int execute();
        virtual void destroy() { delete this; }
        typedef std::vector<IPIN *> TPins;

    protected:
        void createPins();
        void defineMultiColIdx();
        ISession * mSession;
        PropertyID mProps[10];
};
TEST_IMPLEMENT(TestMultiColIdxonCollections, TestLogger::kDStdOut);

int TestMultiColIdxonCollections::execute()
{
    if (!MVTApp::startStore())
    { TVERIFY2(0, "Could not start store, bailing out completely"); return 1; }
    mSession = MVTApp::startSession();
    MVTApp::mapURIs(mSession, "TestMultiColIdxonCollections.prop", sizeof(mProps) / sizeof(mProps[0]), mProps);

    // Define class family(multi-colunmn index)
    defineMultiColIdx();

    // Create pins.
    createPins();

    mSession->terminate();
    MVTApp::stopStore();
    return 0;
}

void TestMultiColIdxonCollections::defineMultiColIdx()
{      
        IExprTree *iExpr1 = NULL, *iExpr2 = NULL;

        // create two conditions on two properties
        Value lV[2];
        lV[0].setVarRef(0,mProps[0]);
        lV[1].setParam(0);
        iExpr1 = mSession->expr(OP_LT, 2, lV);
        TVERIFY(iExpr1 != NULL);
        
        lV[0].setVarRef(0,mProps[1]);
        lV[1].setParam(1);
        iExpr2 = mSession->expr(OP_GT, 2, lV);
        TVERIFY(iExpr2 != NULL);

        // create a statement        
        IStmt * const iStmt = mSession->createStmt();
        TVERIFY(iStmt != NULL);
        QVarID const lVar = iStmt->addVariable();
        
        iStmt->addCondition(lVar, iExpr1);
        iStmt->addCondition(lVar, iExpr2);

        iExpr1->destroy();
        iExpr2->destroy();

        // create a class family(multi-column index)
        ClassID mMulti = MVTUtil::createUniqueClass(mSession, "TestMultiColIdxonCollections.A", iStmt, NULL);
        TVERIFY(STORE_INVALID_CLASSID != mMulti);
        iStmt->destroy();
}

#define NUM_PINS 100
void TestMultiColIdxonCollections::createPins()
{
    // create pins with collections
    mLogger.out() << "Creating pins...";
    size_t i, j;
    TPins lPins;
    for (i = 0; i < NUM_PINS; i++)
    {
        if ((i % 10) == 0)
            mLogger.out() << "." << std::flush;
        
        Value * lV = (Value *)mSession->alloc(10 * sizeof(Value));
        for (j = 0; j < 10; j++)
        {
            if (MVTRand::getBool())
            { SETVALUE_C(lV[j], mProps[j % 2], MVTRand::getRange(1, 1000), OP_ADD, STORE_LAST_ELEMENT); }
            else
            {
                std::string const lS = MVTRand::getString2(5, -1, false);
                char * const lStr = (char *)mSession->alloc(1 + lS.length());
                memcpy(lStr, lS.c_str(), lS.length());
                lStr[lS.length()] = 0;
                SETVALUE_C(lV[j], mProps[j % 2], lStr, OP_ADD, STORE_LAST_ELEMENT);
            }
        }
        lPins.push_back(mSession->createUncommittedPIN(lV, 10));
    }

    mLogger.out() << std::endl << "Committing pins..." << std::endl;
    TVERIFYRC(mSession->commitPINs(&lPins[0], lPins.size()));
    for (i = 0; i < lPins.size(); i++)
        lPins[i]->destroy();
}
