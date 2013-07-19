/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

// testorderby is the main test for sorting of query results
// This test offers specific control for excercising external sort scenarios

//
// For normal usage:
// tests testexternalsort y 5000
//
// Once you have pins in the store you can run the same test more quickly 
// with 
//
// tests testexternalsort n 0
//
// You may need to recompile kernel with lower RAM limit (see TESTES in pisort.cpp)
// in order to actually hit the external sort threshhold.  And use TESTES to turn on 
// tracing

#include "app.h"
#include "mvauto.h"

#ifndef WIN32
#include <limits.h>
#endif

#define TEST_VT_IDENTITY 0 // Was repro for 11921

using namespace std;

class TestExternalSort : public ITest
{
	public:
		TEST_DECLARE(TestExternalSort);
		virtual char const * getName() const { return "testexternalsort"; }
		virtual char const * getHelp() const { return "args: [y/n]-genpins, number of pins to gen"; }
		virtual char const * getDescription() const { return "External sort"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
		virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
		virtual bool isPerformingFullScanQueries() const { return true; }
	
	protected:
		void doTest(int newpins);
	private:
		ISession * mSession ;
		PropertyID mProp1 ;
		PropertyID mProp2 ;
};
TEST_IMPLEMENT(TestExternalSort, TestLogger::kDStdOut);

int TestExternalSort::execute()
{
	string inGen("y"); int lcntPins=20000; bool pparsing = true;
	
	if(!mpArgs->get_param("ingen",inGen) && inGen.empty())
	{
		mLogger.out() << "Problem with --ingen parameter initialization!" << endl;
		pparsing = false;
	}
	if(!mpArgs->get_param("inpins",lcntPins) && !lcntPins)
	{
		mLogger.out() << "Problem with --inpins parameter initialization!" << endl;
		pparsing = false;
	}
	if(!pparsing)
	{
		mLogger.out() << "Parameter initialization problems! " << endl; 
		mLogger.out() << "Test name: testexternalsort" << endl; 	
		mLogger.out() << getHelp() << endl;	
		mLogger.out() << "Example: ./tests testexternalsort --ingen={y/n} --inpins={int}  " << endl; 
		return 1;
	}
	
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	// Creates unique properties so that the exact number of FT matches can be confirmed
	mProp1 = MVTApp::getProp( mSession, "TestExternalSort" ) ; 
	mProp2 = MVTApp::getProp( mSession, "TestExternalSort.2" ) ; 
    
	int cntPins=0;
	if ( inGen.c_str()[0] == 'y' || inGen.c_str()[0] == 'Y' )
	{
		cntPins = lcntPins;
	}
	
	doTest(cntPins);
	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
	return RC_OK  ;
}

void TestExternalSort::doTest(int newpins)
{
	// Sorting massive strings bloats the 
	// memory usage so that external sort boundary is easier to
	// hit
	static const int strlen=0x10; 

	char baseStr[strlen+1];
	memset(baseStr,'a',strlen); baseStr[strlen]=0;

	if (newpins>0)
	{
		// Create store
		mLogger.out() << "Generation phase" << endl ;
		mSession->startTransaction();
		for ( int i = 0 ; i < newpins ; i++ )
		{
			Value v[2];

			// Although huge string, only the first character will
			// differ so we will have lots of duplicates
			baseStr[0]='a'+MVTRand::getRange(0,25);

			v[0].set(baseStr); 
			v[0].property=mProp1;

			ulong cntProps=1;
			if (MVTRand::getRange(1,100)>20)
			{
				// Most pins have a second sorting property
				cntProps=2;
#if TEST_VT_IDENTITY
				//BUG REPRO
				v[1].setIdentity(STORE_OWNER) ; 
				v[1].property = PROP_SPEC_CREATEDBY ; 
#else
				v[1].set(MVTRand::getRange(0,RAND_MAX-1));
				v[1].property=mProp2;
#endif
			}

			PID pid ;
			TVERIFYRC(mSession->createPINAndCommit(pid,v,cntProps));
		}
		mSession->commit(true);
	}

	// query
	mLogger.out() << "Order by" << endl ;
	CmvautoPtr<IStmt> lQ(mSession->createStmt());	
	unsigned char var = lQ->addVariable() ;
	TVERIFYRC(lQ->setPropCondition(var,&mProp1,1));
#if TEST_VT_IDENTITY
	const OrderSeg sortProps[2] = {{NULL,PROP_SPEC_CREATEDBY,0,0,0},{NULL,mProp1,ORD_DESC,0,0}}; 
#else
	const OrderSeg sortProps[2] = {{NULL,mProp1,0,0,0},{NULL,mProp2,ORD_DESC,0,0}}; 
#endif
	TVERIFYRC(lQ->setOrder(sortProps,2));

	// Complete Pass
	ICursor* lC = NULL;
	TVERIFYRC(lQ->execute(&lC));
	CmvautoPtr<ICursor> lR(lC);
	ulong cntEnum=0;
	PID p5 ; // Remember for later comparison
	INITLOCALPID(p5); p5.pid = STORE_INVALID_PID;
	int iPrev = INT_MAX ;
#if TEST_VT_IDENTITY
	char cPrev = 'z'; 
#else
	char cPrev = '\0'; 
#endif
	IPIN *p = NULL;
	while(p = lR->next())
	{
		if (cntEnum==5) p5=p->getPID();
		cntEnum++;

		const Value * v1 = p->getValue(mProp1);
		TVERIFY(v1->type == VT_STRING);
		char cThis=v1->str[0];// Only first character varies in each string

#if TEST_VT_IDENTITY
		// Treat identity as integer
		//
		// Some pins don't have the primary ordering key
		// Give them a HIGH value because they will come after the others 
		const Value * v2 = p->getValue(PROP_SPEC_CREATEDBY);
		int iThis=(v2==NULL)?9999:v2->i;  

		TVERIFY( iThis>=iPrev);
		if ( iThis<iPrev)
		{
			mLogger.out() << "iThis " << iThis << " versus " << iPrev << endl;
		}
		if(iThis==iPrev)
		{
			TVERIFY( cThis<=cPrev ); 
			if (cThis>cPrev) 
				mLogger.out() << "cThis " << cThis << " versus " << cPrev << endl;
		}
#else

		const Value * v2 = p->getValue(mProp2);
		int iThis=(v2==NULL)?-1:v2->i;

		// Prove that the sorting worked
		TVERIFY( cThis>=cPrev ); // Ascending
		if(cThis==cPrev) TVERIFY( iThis<=iPrev ); // Descending
#endif

		if (isVerbose())
			mLogger.out() << cThis << "," << ((v2==NULL)?-1:v2->i) << endl ;

		cPrev=cThis;
		iPrev=iThis;

		p->destroy();
	}
	TVERIFY2(cntEnum>15,"Test expects at least 15 pins generated");

	uint64_t cnt;
	TVERIFYRC(lQ->count(cnt)) ; // Count with no sorting 
	TVERIFY(cnt==cntEnum);

	// Try skip/pagination
	CmvautoPtr<ICursor> lR2;

	if ( cntEnum > 15 )
	{
		mLogger.out() << "Retrieve first chunk of pins" << endl << endl ;
		ICursor* lC = NULL;
		TVERIFYRC(lQ->execute(&lC,NULL, 0,10/*nReturn*/, 5/*nSkip*/));
		CmvautoPtr<ICursor> lR(lC);
		lR2.Attach(lC);
		ulong cntEnum2=0;
		while(p = lR2->next())
		{
			if(cntEnum2==0)
			{
				TVERIFY(p->getPID()==p5);
			}

			cntEnum2++;
			p->destroy();
		}
		TVERIFY(cntEnum2==10);
	}

#if 1
	if ( cntEnum > 25 )
	{
		mLogger.out() << endl << endl << "Retrieve second chunk of pins" << endl ;
	
		// PERFORMANCE REVIEW: To get the next 10 results like this results 
		// in complete regeneration of the sort,
		// so it is more efficient to paginate yourself with single ICursor
		ICursor* lC = NULL;
		TVERIFYRC(lQ->execute(&lC, NULL,0,10/*nReturn*/, 15/*nSkip*/));
		lR2.Attach(lC);
		ulong cntEnum2=0;
		while(p = lR2->next())
		{
			cntEnum2++;
			p->destroy();
		}
		TVERIFY(cntEnum2==10);
	}
#endif
}
