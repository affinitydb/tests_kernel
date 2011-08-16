/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

// Demo of how to use the PhotoScenario base class

#include "app.h"
#include "photoscenario.h"
#include "mvauto.h"

class TestPhotoDemo : public PhotoScenario
{
	public:
		TEST_DECLARE(TestPhotoDemo);
		virtual char const * getName() const { return "testphotodemo"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Demo how to use PhotoScenario baseclass"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
		virtual bool includeInPerfTest() const { return true; } /*useful as a baseline for populating the store with photo data*/
		virtual void destroy() { delete this; }
	protected:
		// Overrides
		virtual void preTestInitialize() ;
		virtual void doTest() ;
		virtual void populateStore() ;
};
TEST_IMPLEMENT(TestPhotoDemo, TestLogger::kDStdOut);

void TestPhotoDemo::preTestInitialize()
{
	mPostfix = "" ;
	mPinCount = 100 ;
}

void TestPhotoDemo::populateStore() 
{
	TVERIFY( mSession != NULL ) ; // For convenience a session already exists

	// Test can override the base class to add more specific data to the store
	// Or call the base class to get some random pins and then add some more specific values
	// as is shown in this example

	PhotoScenario::populateStore() ;

	TVERIFY( mPids.size() > 0 ) ; // New pins have been filled in

	// For example add a specific tag
	for ( size_t i = 0 ; i < mPids.size() ; i++ )
	{
		Value tag ;
		tag.set( "SpecificTag" ) ; tag.op = OP_ADD ; tag.property = tag_id ;
		TVERIFYRC( mSession->modifyPIN(mPids[i], &tag, 1)) ;
	}

	mTagPool.push_back( "SpecificTag" ) ; // Basically optional, but base class tracks the tags
}

void TestPhotoDemo::doTest() 
{
	// Each test can do its specific test scenario in DoTest() (we only override Execute if we really 
	// need significantly different behavior than what the baseclass does, in which case we should consider just deriving straight from ITest)
	runClassQueries() ;
}
