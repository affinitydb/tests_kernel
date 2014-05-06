/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

// See also testdocmodel.cpp

// Publish this test.
class TestPropSpec : public ITest
{
	public:
		TEST_DECLARE(TestPropSpec);
		virtual char const * getName() const { return "testpropspec"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "basic tests for PROP_SPEC_xxx properties"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestPropSpec, TestLogger::kDStdOut);

// Implement this test.
int TestPropSpec::execute()
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		ISession * const lSession =	MVTApp::startSession();
		PropertyID lPropID[1]; MVTApp::mapURIs(lSession,"TestPropSpec.prop",1,lPropID);
		// Create a document, with some special properties.
		Value lPV[4];
		SETVALUE(lPV[0], PROP_SPEC_CREATED, 0, OP_ADD);
		SETVALUE(lPV[1], PROP_SPEC_CREATEDBY, 0, OP_ADD);
		SETVALUE(lPV[2], PROP_SPEC_UPDATED, 0, OP_ADD);
		SETVALUE(lPV[3], PROP_SPEC_UPDATEDBY, 0, OP_ADD);
		PID lPIDDoc;
		CREATEPIN(lSession, &lPIDDoc, lPV, sizeof(lPV) / sizeof(lPV[0]));
		IPIN * const lPINDoc = lSession->getPIN(lPIDDoc);

		// Sleep, to observe an interesting elapsed time between "created" and "updated" properties.
		int const lWaitMS = 100;
		MVTestsPortability::threadSleep(lWaitMS);

		// Modify the document.
		lPINDoc->getValue(PROP_SPEC_PINID); // Not allowed?
		Value const * lPropCreated = lPINDoc->getValue(PROP_SPEC_CREATED); uint64_t const lBefore = lPropCreated->ui64;
		Value const * lPropCreatedBy = lPINDoc->getValue(PROP_SPEC_CREATEDBY);
		TVERIFY(STORE_OWNER == lPropCreatedBy->iid);
		SETVALUE(lPV[0], lPropID[0], "My Title", OP_ADD);
		if (RC_OK != lPINDoc->modify(lPV, 1))
		{
			lSuccess = false;
			mLogger.out() << "Error: Failed to modify pin!" << std::endl;
		}
		Value const * lPropUpdated = lPINDoc->getValue(PROP_SPEC_UPDATED);
		double const lElapsedMS = double(lPropUpdated->ui64 - lBefore) / 1000.0;
		mLogger.out() << "time elapsed between creation and update: " << lElapsedMS << "ms" << std::endl;
		if (lElapsedMS + 10.0 < lWaitMS)
		{
			lSuccess = false;
			mLogger.out() << "Error: Incorrect elapsed time!" << std::endl;
		}

		// Attach children to this document.
		bool lFoundChildren[10], lFoundDocParts[10];
		PID lPIDChildren[10];
		PID lPIDDocParts[10];
		int i;
		for (i = 0; i < 10; i++)
		{
			lFoundChildren[i] = lFoundDocParts[i] = false;
			SETVALUE(lPV[0], PROP_SPEC_PARENT, lPINDoc, OP_ADD);
			CREATEPIN(lSession, &lPIDChildren[i], lPV, 1);
			SETVALUE(lPV[0], PROP_SPEC_CREATED, 0, OP_ADD);
			SETVALUE(lPV[1], PROP_SPEC_UPDATED, 0, OP_ADD);
			SETVALUE(lPV[1], PROP_SPEC_DOCUMENT, lPINDoc, OP_ADD);
			CREATEPIN(lSession, &lPIDDocParts[i], lPV, 3);
		}

		Value lV[2];
		IPIN * lQRIter;

		// Query for all doc parts (i.e. all pins that consider they belong to our document), and check results.
		IStmt * const lQ1 = lSession->createStmt();
		unsigned char const lVar1 = lQ1->addVariable();
		PropertyID lPropIds1[] = {PROP_SPEC_DOCUMENT};
		lV[0].setVarRef(lVar1, *lPropIds1);
		lV[1].set(lPINDoc->getPID());
		TExprTreePtr const lExprTree1 = EXPRTREEGEN(lSession)(OP_EQ, 2, lV, 0);
		lQ1->addCondition(lVar1,lExprTree1);
		ICursor * lQR1 = NULL;
		TVERIFYRC(lQ1->execute(&lQR1));
		for (lQRIter = lQR1->next(); NULL != lQRIter; lQRIter = lQR1->next()){
			for (i = 0; i < 10; i++)
				if (lPIDDocParts[i] == lQRIter->getPID())
					{ lFoundDocParts[i] = true; break;}
			lQRIter->destroy();
		}
		#if 1
			// Note: Curiously, not doing this affects the next query.
			lQR1->destroy();
			lQ1->destroy();
		#endif
		lExprTree1->destroy();

		// Query for all children (i.e. all pins that consider our document as their parent), and check results.
		IStmt * const lQ2 = lSession->createStmt();
		unsigned char const lVar2 = lQ2->addVariable();
		PropertyID lPropIds2[] = {PROP_SPEC_PARENT};
		lV[0].setVarRef(lVar2, *lPropIds2);
		lV[1].set(lPINDoc->getPID());
		TExprTreePtr const lExprTree2 = EXPRTREEGEN(lSession)(OP_EQ, 2, lV, 0);
		lQ2->addCondition(lVar2,lExprTree2);
		ICursor * lQR2 = NULL;
		TVERIFYRC(lQ2->execute(&lQR2));
		for (lQRIter = lQR2->next(); NULL != lQRIter; lQRIter = lQR2->next()){
			for (i = 0; i < 10; i++)
				if (lPIDChildren[i] == lQRIter->getPID())
					{ lFoundChildren[i] = true; break; }
			lQRIter->destroy();
		}
		lExprTree2->destroy();
		lQR2->destroy();
		lQ2->destroy();

		// Display any error.
		for (i = 0; i < 10; i++)
		{
			if (!lFoundDocParts[i])
			{
				lSuccess = false;
				mLogger.out() << "Error: Part " << std::hex << LOCALPID(lPIDDocParts[i]) << " was not found in document " << LOCALPID(lPINDoc->getPID()) << std::endl;
			}
			if (!lFoundChildren[i])
			{
				lSuccess = false;
				mLogger.out() << "Error: Child " << std::hex << LOCALPID(lPIDChildren[i]) << " was not found in " << LOCALPID(lPINDoc->getPID()) << std::endl;
			}
		}
		if(lPINDoc) lPINDoc->destroy();
		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	return lSuccess ? 0 : 1;
}
