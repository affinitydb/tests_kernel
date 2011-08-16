/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

// Publish this test.
class TestRandomPIDs : public ITest
{
	public:
		TEST_DECLARE(TestRandomPIDs);
		virtual char const * getName() const { return "testrandompids"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "bashes on mvstore with random (local) PIDs"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestRandomPIDs, TestLogger::kDStdOut);

// Implement this test.
int TestRandomPIDs::execute()
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		ISession * const lSession =	MVTApp::startSession();

		for (int i = 0; i < 1000; i++)
		{
			PID lPID; 
			
			unsigned storeid=MVTApp::Suite().mStoreID;

			//Big range to ensure we go beyond the default alloc
			ulong page= MVTRand::getRange(0,5000);
			unsigned pageslot = MVTRand::getRange(1,100);
				
			lPID.pid = BUILDPID(storeid,page,pageslot) ;
			lPID.ident = STORE_OWNER;
			
			IPIN * const lPIN = lSession->getPIN(lPID);
			if (lPIN){
				// Unlikely, but we can generate a real pin id and get something valid
				// if we get anything is should be valid so attempt an operation
				lPIN->getNumberOfProperties();
				lPIN->destroy();
			}
		}

		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	return lSuccess ? 0 : 1;
}
