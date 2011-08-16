/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h" 

// Publish this test.
class TestOverflowUpdProp : public ITest
{
	static const int sNumProps = 80;
	PropertyID mPropIds[sNumProps];
	std::vector<PID> mPIDs;
	public:
		TEST_DECLARE(TestOverflowUpdProp);
		virtual char const * getName() const { return "testoverflowupdprop"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test for overflow in update prop in pin->modify()"; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:

};

TEST_IMPLEMENT(TestOverflowUpdProp, TestLogger::kDStdOut);

int TestOverflowUpdProp::execute()
{
	bool lSuccess = true; 
	Value lV[100]; Tstring lStr; int i = 0, j = 0, k = 0;
	if (MVTApp::startStore())
	{
		ISession * lSession	=	MVTApp::startSession();
		MVTApp::mapURIs(lSession, "TestOverflowUPDPROP.prop.", sNumProps, mPropIds);
		//create a normal pin
		for (i=0; i < 100; i ++)
		{
			IPIN *lPIN = lSession->createUncommittedPIN();
			MVTApp::randomString(lStr,100,0,false);				
			for (j = 0; j < 40; j++)
			{
				SETVALUE(lV[j], mPropIds[j], lStr.c_str(), OP_SET); 
				lV[j].meta = META_PROP_NOFTINDEX;				
			}
			
			int lIndex = 0;
			for (k = 0; k < 40; k++)
			{
				lIndex = k+j;
				SETVALUE(lV[lIndex], mPropIds[lIndex], 0, OP_SET);
			}
			TVERIFYRC(lPIN->modify(lV, k+j, MODE_COPY_VALUES));
			TVERIFYRC(lSession->commitPINs(&lPIN, 1, MODE_COPY_VALUES));
			mPIDs.push_back(lPIN->getPID());
			if(lPIN) lPIN->destroy();
		}
		
		vector<PID>::iterator lIter = mPIDs.begin();
		for (; mPIDs.end() != lIter; lIter++)
		{
			IPIN *lPIN = lSession->getPIN(*lIter);
			MVTApp::randomString(lStr,200,0,false);
			for (j = 0; j < 20; j++)
			{
				SETVALUE(lV[j], mPropIds[j], lStr.c_str(), OP_SET); 
				lV[j].meta = META_PROP_NOFTINDEX;				
			}

			for (k = 0; k < 20; k++) SETVALUE(lV[k+j], mPropIds[k+j], 0, OP_SET);
			
			TVERIFYRC(lPIN->modify(lV, k+j));
			lPIN->destroy();
		}
		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
	return lSuccess ? 0 : 1;
}
