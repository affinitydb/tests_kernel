/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

class TestClassInfo : public ITest
{
		ISession * mSession;
		static const int sNumProps = 5;
	public:
		TEST_DECLARE(TestClassInfo);
		virtual char const * getName() const { return "testclassinfo"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "tests class introspection"; }
		virtual bool excludeInIPCSmokeTest(char const *& pReason) const { pReason = "#26199 - Failing in IPC"; return true;}	
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void doTest();
		void createPINs(PropertyID *pPropIDs, const int pNumPINs = 100);
		void testSimpleClass();
		void testSimpleFamily();
		void testClassInfo();
	protected:
		Tstring mClassStr; 
};
TEST_IMPLEMENT(TestClassInfo, TestLogger::kDStdOut);

void TestClassInfo::doTest()
{
	// /pin[pin has prop0]
	testSimpleClass();

	// /pin[prop0 in $var] 
	testSimpleFamily();

	testClassInfo();
}

void TestClassInfo::testClassInfo()
{
	PropertyID lPropIDs[sNumProps];
	MVTUtil::mapURIs(mSession, "TestClassInfo.testClassInfo.prop.", sNumProps, lPropIDs);

	// Create PINs ...
	createPINs(lPropIDs, 100);	
	
	// Create simple family /pin[prop3 ciin $var]
	char lB[64]; sprintf(lB, "TestClassInfo.%s.testClassInfo.%d", mClassStr.c_str(), 0);
	Tstring lClassString, lClassInfoString;
	ClassID lCLSID = STORE_INVALID_CLASSID;
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt()); unsigned char lVar = lQ->addVariable();
		TVERIFYRC(lQ->setPropCondition(lVar, lPropIDs, 2));
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[2]);
		lV[1].setParam(0);
		CmvautoPtr<IExprTree> lET(mSession->expr(OP_IN, 2, lV,CASE_INSENSITIVE_OP));
		TVERIFYRC(lQ->addCondition(lVar,lET));
		lClassString = lQ->toString();
		TVERIFYRC(defineClass(mSession,lB, lQ, &lCLSID));
	}

	{
		IPIN *cpin=NULL; TVERIFYRC(mSession->getClassInfo(lCLSID, cpin));
		if (cpin!=NULL) {
			lClassInfoString = cpin->getValue(PROP_SPEC_PREDICATE)->stmt->toString();
			TVERIFY(strcmp(lClassString.c_str(), lClassInfoString.c_str()) == 0);
			cpin->destroy();
		}
	}	

	char lB1[64]; sprintf(lB1, "TestClassInfo.%s.testClassInfo.%d", mClassStr.c_str(), 1);
	ClassID lCLSID1 = STORE_INVALID_CLASSID;
	{
		IPIN *cpin=NULL; TVERIFYRC(mSession->getClassInfo(lCLSID, cpin));
		if (cpin!=NULL) {
			TVERIFYRC(defineClass(mSession,lB1, cpin->getValue(PROP_SPEC_PREDICATE)->stmt, &lCLSID1));
			cpin->destroy();
		}
	}
}

void TestClassInfo::testSimpleFamily()
{
	PropertyID lPropIDs[sNumProps];
	MVTUtil::mapURIs(mSession, "TestClassInfo.testSimpleFamily.prop.", sNumProps, lPropIDs);

	// Create PINs ...
	createPINs(lPropIDs, 100);	
	
	// Create simple family /pin[prop3 ciin $var]
	char lB[64]; sprintf(lB, "TestClassInfo.%s.testSimpleFamily.%d", mClassStr.c_str(), 0);
	Tstring lClassString, lClassInfoString;
	ClassID lCLSID = STORE_INVALID_CLASSID;
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt()); 
		QVarID lVar = lQ->addVariable();
		Value lV[2];
		lV[0].setVarRef(0,lPropIDs[3]);
		lV[1].setParam(0);
		CmvautoPtr<IExprTree> lET(mSession->expr(OP_IN, 2, lV, CASE_INSENSITIVE_OP));
		TVERIFYRC(lQ->addCondition(lVar,lET));
		lClassString = lQ->toString();
		TVERIFYRC(defineClass(mSession,lB, lQ, &lCLSID));
	}

	{
		IPIN *cpin=NULL; TVERIFYRC(mSession->getClassInfo(lCLSID, cpin));
		if (cpin!=NULL) {
			lClassInfoString = cpin->getValue(PROP_SPEC_PREDICATE)->stmt->toString();
			TVERIFY(strcmp(lClassString.c_str(), lClassInfoString.c_str()) == 0);
			cpin->destroy();
		}
	}
	
	TVERIFYRC(dropClass(mSession,lB));

	{
		IPIN *cpin=NULL; TVERIFY(mSession->getClassInfo(lCLSID, cpin)!=RC_OK);
	}
}

void TestClassInfo::testSimpleClass()
{
	PropertyID lPropIDs[sNumProps];
	MVTUtil::mapURIs(mSession, "TestClassInfo.testSimpleClass.prop.", sNumProps, lPropIDs);

	// Create PINs ...
	createPINs(lPropIDs, 32000);	
	
	// Create simple class /pin[pin has prop0]
	char lB[64]; sprintf(lB, "TestClassInfo.%s.testSimpleClass.%d", mClassStr.c_str(), 0);
	Tstring lClassString, lClassInfoString;
	ClassID lCLSID = STORE_INVALID_CLASSID;
	{
		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned char lVar = lQ->addVariable();
		TVERIFYRC(lQ->setPropCondition(lVar, &lPropIDs[0], 1));
		lClassString = lQ->toString();
		TVERIFYRC(defineClass(mSession,lB, lQ, &lCLSID, true));
	}

	// Get Class info and Query
	IPIN *cpin=NULL; TVERIFYRC(mSession->getClassInfo(lCLSID, cpin));
	if (cpin!=NULL) {
		lClassInfoString = cpin->getValue(PROP_SPEC_PREDICATE)->stmt->toString();
		TVERIFY(strcmp(lClassString.c_str(), lClassInfoString.c_str()) == 0);
		if(!MVTApp::isRunningSmokeTest())
		{
			uint64_t lCount = 0; TVERIFYRC(cpin->getValue(PROP_SPEC_PREDICATE)->stmt->count(lCount));
			TVERIFY(lCount == 32000);
			const Value *cv = cpin->getValue(PROP_SPEC_NINSTANCES);
			TVERIFY(cv->ui64 == 32000);
			std::cout << "Actual count: " << lCount <<" and PROP_SPEC_NINSTANCES returns: " << cv->ui64 << std::endl;
		}
		const Value *cv = cpin->getValue(PROP_SPEC_CLASS_INFO);
		TVERIFY(cv!=NULL && (cv->type==VT_UINT||cv->type==VT_INT) && (cv->ui&CLASS_SDELETE) != 0);
		cpin->destroy();
	}
}

void TestClassInfo::createPINs(PropertyID *pPropIDs, const int pNumPINs)
{
	int lRand = MVTRand::getRange(1, 10) * 10;
	int i = 0;
	mSession->startTransaction();
	for( i = 0; i < pNumPINs; i++)
	{
		PID lPID;
		Value lV[15];
		SETVALUE(lV[0], pPropIDs[0], (lRand + i), OP_SET);
		SETVALUE(lV[1], pPropIDs[1], "prop1", OP_SET); lV[1].meta = META_PROP_NOFTINDEX;
		
		int lRandVal = MVTRand::getRange(10, 100);
		SETVALUE_C(lV[2], pPropIDs[2], lRandVal, OP_ADD, STORE_LAST_ELEMENT);
		SETVALUE_C(lV[3], pPropIDs[2], (lRandVal+lRand), OP_ADD, STORE_LAST_ELEMENT);
		
		Tstring lStr; MVTRand::getString(lStr, 5, 10, false, false);
		SETVALUE(lV[4], pPropIDs[3], lStr.c_str(), OP_SET); lV[4].meta = META_PROP_NOFTINDEX;
		
		CREATEPIN(mSession, lPID, lV, 5);		
	}
	mSession->commit(true);
}

int TestClassInfo::execute()
{
	bool lSuccess = true;
	MVTRand::getString(mClassStr, 5, 10, false, false);
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();
		doTest();
		mSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
	return lSuccess ? 0 : 1;
}
