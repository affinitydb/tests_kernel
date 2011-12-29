/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

#ifndef MODE_VERBOSE
#define MODE_VERBOSE 0
#endif

#define NPINS	1000
#define RANGE	100

// Publish this test.
class TestMerge : public ITest
{
	public:
		TEST_DECLARE(TestMerge);
		virtual char const * getName() const { return "testmerge"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "simple join query test"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestMerge, TestLogger::kDStdOut);

// Implement this test.
int	TestMerge::execute()
{
	bool lSuccess =	true;
	if (MVTApp::startStore())
	{
		ISession * const lSession =	MVTApp::startSession();

		// Declare properties.
		URIMap	lData[2];
		MVTApp::mapURIs(lSession,"TestMerge.prop",2,lData);
		PropertyID const lPropIdVal    = lData[0].uid;
		PropertyID const lPropIdRef   = lData[1].uid;
		PropertyID const lPropIdPinId = PROP_SPEC_PINID;

		char lB[100]; sprintf(lB,"TestMerge.ClassVal.%d",rand());
		Value lV[4]; ClassID lClassVal = STORE_INVALID_CLASSID;
		lV[0].setVarRef(0,lPropIdVal);
		lV[1].setParam(0);
		IExprTree *expr=lSession->expr(OP_LT,2,lV);
		IStmt *lQ = lSession->createStmt();
		lQ->addVariable(NULL,0,expr);
		TVERIFYRC(defineClass(lSession,lB,lQ,&lClassVal));
		lQ->destroy();

		sprintf(lB,"TestMerge.ClassRef.%d",rand());
		ClassID lClassRef = STORE_INVALID_CLASSID;
		lQ = lSession->createStmt();
		lQ->addVariable();
		lQ->setPropCondition(0,&lPropIdRef,1);
		TVERIFYRC(defineClass(lSession,lB,lQ,&lClassRef));
		lQ->destroy();

		/**
		* The following creates the family on reference class...
		**/
		sprintf(lB,"TestMerge.ClassFamilyOnRef.%d",rand());
		ClassID lClassFamilyOnRef = STORE_INVALID_CLASSID; Value lVFoR[2];
		lVFoR[0].setVarRef(0,lPropIdRef);
		lVFoR[1].setParam(0);
		expr=lSession->expr(OP_IN,2,lVFoR);
		lQ = lSession->createStmt();
		lQ->addVariable(NULL,0,expr);
		TVERIFYRC(defineClass(lSession,lB,lQ,&lClassFamilyOnRef));
		lQ->destroy();

		// Create a few pins
		mLogger.out() << "Creating " << NPINS*2 << " pins... ";

		PID pins[NPINS]; bool lfMember[NPINS];
		PID refferedPins[NPINS];
		
		const int threshold = RANGE/3; int i;
		for (i = 0; i < NPINS; i++)
		{
			int val = rand()%RANGE;
			lfMember[i] = val < threshold;
			SETVALUE(lV[0], lPropIdVal, val, OP_ADD);
			CREATEPIN(lSession, pins[i], lV, 1);
		}

		// Create a second group of pins, each of which has reference
		// to a random pin in the set above
		uint64_t lCount = 0;
		for (i = 0; i < NPINS; i++)
		{
			int idx = rand()%NPINS; PID id;
			if (lfMember[idx]) lCount++; // The referenced pin will match the family query
			SETVALUE(lV[0], lPropIdRef, pins[idx], OP_ADD); 
			refferedPins[i] = pins[idx];    // to remember reffered pins;
			CREATEPIN(lSession, id, lV, 1);
		}
		
		mLogger.out() << "DONE" << std::endl;
		
		// Do a simple join search.
		// "All pins, from lClassRef class, where the lPropIdRef property is equal 
		// to the PROP_SPEC_PINID of a PIN matching the lClassVal query of (lPropIdVal < threshhold)"
		// 
		lQ	= lSession->createStmt();
		lV[0].setError(lPropIdRef); lV[1].setError(lPropIdRef); lV[2].setRange(&lV[0]);
		lV[3].set(threshold); // Family query will find all pins with lPropIdVal < threshhold
		ClassSpec classSpec[2]={{lClassFamilyOnRef,1,&lV[2]},{lClassVal,1,&lV[3]}};
		unsigned char lVar1 = lQ->addVariable(&classSpec[0],1),lVar2 = lQ->addVariable(&classSpec[1],1);
		lV[0].setVarRef(0,lPropIdRef);
		lV[1].setVarRef(1,lPropIdPinId);
		expr = lSession->expr(OP_EQ,2,lV);
		lVar1 = lQ->join(lVar1,lVar2,expr);
		OrderSeg ord={NULL,lPropIdRef,0,0,0};
		lQ->setOrder(&ord,1);

		uint64_t lCnt = 0;
		TVERIFYRC(lQ->count(lCnt,NULL,0,~0u,MODE_VERBOSE));
		TVERIFY(lCnt == lCount);

		ICursor *lR= NULL;
		TVERIFYRC(lQ->execute(&lR, NULL, 0,~0u,0,MODE_VERBOSE));
		IPIN *pInResults;
		while((pInResults=lR->next())!=NULL)
		{
			if (isVerbose()) MVTApp::output(*pInResults,mLogger.out());

			// Prove that it is the pins from lClassRef that are returned
			TVERIFY(pInResults->testClassMembership(lClassRef));

			// Prove the reason why this pin is returned in the query
			// In future, reference may be automatically resolved into a PIN pointer
			const Value * refVal = pInResults->getValue(lPropIdRef); 
			if ( refVal!=NULL && refVal->type==VT_REFID )
			{
				IPIN * pRefer(lSession->getPIN(refVal->id));
				TVERIFY(pRefer->getValue(lPropIdVal)->i < threshold );
				pRefer->destroy();
			}
			else
			{
				TVERIFY(!"Not implemented in test");
			}

			pInResults->destroy();
		}

		lR->destroy();
		lQ->destroy();

		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }

	return lSuccess	? 0	: 1;
}
