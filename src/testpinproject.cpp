/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h" // Mv auto pointers
#include "collectionhelp.h"			// If reading collections
#include "teststream.h"				// If using streams
using namespace std;

class TestPinProject : public ITest
{
	public:
		TEST_DECLARE(TestPinProject);

		// By convention all test names start with test....
		virtual char const * getName() const { return "testpinproject"; }
		virtual char const * getDescription() const { return "IPIN::Project - Cover new m2.3 method"; }
		virtual char const * getHelp() const { return ""; } // Optional
		
		virtual int execute();
		virtual void destroy() { delete this; }
		
	protected:
		void doTest() ;
	private:	
		ISession * mSession ;
};
TEST_IMPLEMENT(TestPinProject, TestLogger::kDStdOut);

int TestPinProject::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }
	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;
	doTest() ;
	mSession->terminate() ;
	MVTApp::stopStore() ;
	return RC_OK ;
}

void TestPinProject::doTest()
{
	PropertyID propU=MVTUtil::getPropRand(mSession,"TestPINProjectU");
	PropertyID propV=MVTUtil::getPropRand(mSession,"TestPINProjectV");
	PropertyID propW=MVTUtil::getPropRand(mSession,"TestPINProjectW");
	PropertyID propX=MVTUtil::getPropRand(mSession,"TestPINProjectX");
	PropertyID propY=MVTUtil::getPropRand(mSession,"TestPINProjectY");
	PropertyID propZ=MVTUtil::getPropRand(mSession,"TestPINProjectZ");


	Value vals[5];

	PropertyID srcProps[6];
	PropertyID * destProps = (PropertyID*) mSession->malloc(sizeof(PropertyID) * 6 ) ;

	// Values on original pin
	vals[0].set(100); vals[0].property=propX;
	vals[1].set(101); vals[1].property=propY;
	vals[2].set(102); vals[2].property=propZ;

	
	Value arrayOfVals[3];
	arrayOfVals[0].set(200); arrayOfVals[0].property=propW; 
	arrayOfVals[1].set(201); arrayOfVals[1].property=propW; 
	arrayOfVals[2].set(202); arrayOfVals[2].property=propW; 
	vals[3].set(arrayOfVals,3); vals[3].property=propW;

	// Detail - its unnecessary to commit the original pin, but it won't be
	// persisted
	CmvautoPtr<IPIN> pOrig(mSession->createPIN(vals,4,MODE_COPY_VALUES));

	//Case 1 - boring direct copy

	srcProps[0] = propX; destProps[0] = propX;
	srcProps[1] = propY; destProps[1] = propY;
	srcProps[2] = propZ; destProps[2] = propZ;
	
	CmvautoPtr<IPIN> p1(pOrig->project(srcProps,3,destProps,MODE_PERSISTENT));

	TVERIFY( p1->getValue(propX)->i == 100 );
	TVERIFY( p1->getValue(propY)->i == 101 );
	TVERIFY( p1->getValue(propZ)->i == 102 );

	//Case 2 - swap values

	srcProps[0] = propX; destProps[0] = propZ;
	srcProps[1] = propY; destProps[1] = propY;
	srcProps[2] = propZ; destProps[2] = propX;
	
	CmvautoPtr<IPIN> p2(pOrig->project(srcProps,3,destProps,MODE_PERSISTENT));

	TVERIFY( p2->getValue(propZ)->i == 100 );
	TVERIFY( p2->getValue(propY)->i == 101 );
	TVERIFY( p2->getValue(propX)->i == 102 );

	//Case 3 - only some values

	srcProps[0] = propX; destProps[0] = propY;
	srcProps[1] = propY; destProps[1] = propZ;
	
	CmvautoPtr<IPIN> p3(pOrig->project(srcProps,2,destProps,MODE_PERSISTENT));

	TVERIFY( p3->getValue(propY)->i == 100 );
	TVERIFY( p3->getValue(propZ)->i == 101 );
	TVERIFY( p3->getValue(propX) == NULL );

	//Case 4 - Values missing from src

	srcProps[0] = propU; destProps[0] = propX;
	srcProps[1] = propX; destProps[1] = propY;
	
	CmvautoPtr<IPIN> p4(pOrig->project(srcProps,2,destProps,MODE_PERSISTENT));

	TVERIFY( p4->getValue(propX) == NULL );
	TVERIFY( p4->getValue(propY)->i == 100 );


	//Case 5 - All values missing from src

	srcProps[0] = propU; destProps[0] = propX;
	srcProps[1] = propV; destProps[1] = propY;
	
	CmvautoPtr<IPIN> p5(pOrig->project(srcProps,2,destProps,MODE_PERSISTENT));

	TVERIFY( p5->getNumberOfProperties() == 0 );

#if 0
	//Case 6 - duplicate prop in destination
	//Hits assert, but this is basically an error by the caller

	srcProps[0] = propX; destProps[0] = propX;
	srcProps[1] = propY; destProps[1] = propX;
	
	CmvautoPtr<IPIN> p6(pOrig->project(srcProps,2,destProps,MODE_PERSISTENT));

	TVERIFY( p6->getNumberOfProperties() == 1 );
	MVTUtil::output( *(p6.Get()),mLogger.out(),mSession );
#endif


	//Case 7 - Copy of array

	srcProps[0] = propW; 
	
	CmvautoPtr<IPIN> p7(pOrig->project(srcProps,1,NULL,MODE_PERSISTENT));

	TVERIFY( p7->getNumberOfProperties() == 1 );
	TVERIFY( p7->getValue(propW)->type == VT_ARRAY ); // REVIEW: if you retrieve it from
													// scratch it will appear as VT_COLLECTION
	TVERIFY( p7->getValue(propW)->length == 3 );
	if ( isVerbose()) MVTUtil::output( *(p7.Get()),mLogger.out(),mSession );
}
