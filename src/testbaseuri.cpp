/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

// Basic test
// 
// Demonstrate basic usage of the store API, focusing on expected
// behavior of common functionality.
//
// It uses a simple family-tree structure as example data

#include "app.h"
using namespace std;

#include "mvauto.h"

// Publish this test.
class TestBaseURI : public ITest
{
	public:
		TEST_DECLARE(TestBaseURI);
		virtual char const * getName() const { return "testbaseuri"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Basic set base URI test"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Repro behavior for class family queries when setBaseURI() is used."; return false; }
		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void doTest() ;
		void InitProperties() ;
		void RemoveData() ;

	private:
		PropertyID  name; 
		PropertyID  philosopher;   
		PropertyID  date_of_birth;
		PropertyID  date_of_death;
		PropertyID  occupation; 
		PropertyID  refs_collection;  
		PropertyID  pin_from_setA;
		PropertyID  pin_from_setB;

		PID collection_of_referencedPIDs[3];   // will be used within the 'Search over collection' scenario.

		ISession * mSession ;

};

TEST_IMPLEMENT(TestBaseURI, TestLogger::kDStdOut);

int TestBaseURI::execute()
{
	doTest() ;
	return RC_OK  ;
}

void TestBaseURI::doTest()
{
	RC rc = RC_OK; 
	
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	InitProperties() ;

	cout << "Creating class family:" << endl;

	IStmt * qF1; 
	TVERIFY( NULL != (qF1 = mSession->createStmt("CREATE CLASS qFml AS select * where $0 = :0(String)", &name, 1)));
			    
	ICursor *rslt;
	TVERIFYRC( qF1->execute(&rslt));
			    
	cout << rslt->next()->getValue(PROP_SPEC_CLASSID)->uid << endl;

	 //The next query is based on the created class family. 
     //The name 'Plato' is provided as a parameter. 
			    
	  IStmt * qFamily2 = mSession->createStmt("SELECT * from qFml(:0)");
	  TVERIFY(NULL != qFamily2);
	  
	  Value param; 
	  param.set("Plato");
		
	  ICursor *rqFamily;
	  
	  rc = qFamily2->execute(&rqFamily, &param, 1);
	  TVERIFYRC(rc);
	  if( RC_OK != rc ){
		  mSession->terminate(); // No return code to test
		  MVTApp::stopStore();  // No return code to test
		  return;
	  }
	  
	  //Displaying the properties - Name, date of birth, and date of death within 
	  //the PIN(s), found with the query.
	  for( IPIN * p = rqFamily->next(); NULL != p; p->destroy(), p = rqFamily->next())
	  {
			cout << p->getValue(name)->str << ":::" << p->getValue(date_of_birth)->i << ":::" << p->getValue(date_of_death)->i << endl;
	  }
	  rqFamily->destroy();

	  mSession->terminate(); // No return code to test
	  MVTApp::stopStore();  // No return code to test
}

void TestBaseURI::InitProperties()
{
	// Demonstrates ISession::mapURIs, getPropertyURI and getPropertyDisplayName
	//
	// Get PropertyIDs for all the properties that will be
	// used in the example

	// Initialize data

	URIMap prMap[8]; PID pid;

	mSession->setURIBase("mvsdk://www.mvware.com/mvstore?kernel_sdk#");

	//The first property is mapped before the setURIBase(...) has been called...
	prMap[0].URI 	  = "name"; 
	prMap[0].uid      = STORE_INVALID_PROPID;
 
	//The rest of the properties are mapped after the setURIBase(...) has been called... 
	prMap[1].URI 	  = "philosopher"; 
	prMap[1].uid      = STORE_INVALID_PROPID;

	prMap[2].URI 	  = "date_of_birth"; 
	prMap[2].uid      = STORE_INVALID_PROPID;

	prMap[3].URI 	  = "date_of_death"; 
	prMap[3].uid      = STORE_INVALID_PROPID;

	prMap[4].URI 	  = "occupation"; 
	prMap[4].uid      = STORE_INVALID_PROPID;

	prMap[5].URI 	  = "refs_collection"; 
	prMap[5].uid      = STORE_INVALID_PROPID;

	prMap[6].URI 	  = "set_A_PIN"; 
	prMap[6].uid      = STORE_INVALID_PROPID;

	prMap[7].URI 	  = "set_B_PIN"; 
	prMap[7].uid      = STORE_INVALID_PROPID;

	TVERIFYRC ( mSession->mapURIs(sizeof(prMap)/sizeof(URIMap), &prMap[1]) ); 

	name            = prMap[0].uid;
	philosopher     = prMap[1].uid;
	date_of_birth   = prMap[2].uid;
	date_of_death   = prMap[3].uid;
	occupation      = prMap[4].uid; 
	refs_collection = prMap[5].uid;
	pin_from_setA   = prMap[6].uid;
	pin_from_setB   = prMap[7].uid;
	
	//!Second - populating the store with some data...  
	Value lV[4];

	lV[0].set("a person who studies or is an expert in philosophy"); 
	lV[0].setPropID(philosopher); lV[0].op = OP_ADD;

	lV[1].set("a person who lives by or expounds a system of philosophy"); 
	lV[1].setPropID(philosopher); lV[1].op = OP_ADD;

	lV[2].set("a person who meets difficulties with calmness and composure"); 
	lV[2].setPropID(philosopher); lV[2].op = OP_ADD;

	lV[3].set("a person given to philosophizing"); 
	lV[3].setPropID(philosopher); lV[3].op = OP_ADD;

	TVERIFYRC( mSession->createPIN(pid, &lV[0], 4)); 

	//Attention! We can use the same Value variable(s) again for creating another PIN ...
	PID pid1;
	lV[0].set("Socrates"); lV[0].setPropID(name);         lV[0].op = OP_ADD;
	lV[1].set(469);    lV[1].setPropID(date_of_birth);    lV[1].op = OP_ADD;
	lV[2].set(399);    lV[2].setPropID(date_of_death);    lV[2].op = OP_ADD;
	lV[3].set(pid);    lV[3].setPropID(occupation);       lV[3].op = OP_ADD;

	TVERIFYRC( mSession->createPIN(pid1, &lV[0], 4));

	collection_of_referencedPIDs[0] = pid1;  //pid is remembered for 'search over collection' scenario... 

	lV[0].set("Plato"); lV[0].setPropID(name);             lV[0].op = OP_ADD;
	lV[1].set(428);     lV[1].setPropID(date_of_birth);    lV[1].op = OP_ADD;
	lV[2].set(348);     lV[2].setPropID(date_of_death);    lV[2].op = OP_ADD;
	lV[3].set(pid);     lV[3].setPropID(occupation);       lV[3].op = OP_ADD;

	TVERIFYRC( mSession->createPIN(pid1, &lV[0], 4));
	
	collection_of_referencedPIDs[1] = pid1;   //pid is remembered for 'search over collection' scenario... 

	lV[0].set("Aristotle"); lV[0].setPropID(name);             lV[0].op = OP_ADD;
	lV[1].set(384);         lV[1].setPropID(date_of_birth);    lV[1].op = OP_ADD;
	lV[2].set(392);         lV[2].setPropID(date_of_death);    lV[2].op = OP_ADD;
	lV[3].set(pid);         lV[3].setPropID(occupation);       lV[3].op = OP_ADD;

	TVERIFYRC( mSession->createPIN(pid1, &lV[0], 4));
	
	collection_of_referencedPIDs[2] = pid1;   //pid is remembered for 'search over collection' scenario... 

	lV[0].set("Desiderius ERASMUS"); lV[0].setPropID(name);             lV[0].op = OP_ADD;
	lV[1].set(1466);                 lV[1].setPropID(date_of_birth);    lV[1].op = OP_ADD;
	lV[2].set(1536);                 lV[2].setPropID(date_of_death);    lV[2].op = OP_ADD;
	lV[3].set(pid);                  lV[3].setPropID(occupation);       lV[3].op = OP_ADD;

	TVERIFYRC( mSession->createPIN(pid1, &lV[0], 4));

	lV[0].set("Thomas MORE"); 		lV[0].setPropID(name);             lV[0].op = OP_ADD;
	lV[1].set(1478);          		lV[1].setPropID(date_of_birth);    lV[1].op = OP_ADD;
	lV[2].set(1535);          		lV[2].setPropID(date_of_death);    lV[2].op = OP_ADD;
	lV[3].set(pid);           		lV[3].setPropID(occupation);       lV[3].op = OP_ADD;

	TVERIFYRC( mSession->createPIN(pid1, &lV[0], 4));

	lV[0].set("Nicolaus COPERNICUS"); lV[0].setPropID(name);             lV[0].op = OP_ADD;
	lV[1].set(1473);                  lV[1].setPropID(date_of_birth);    lV[1].op = OP_ADD;
	lV[2].set(1543);                  lV[2].setPropID(date_of_death);    lV[2].op = OP_ADD;
	lV[3].set(pid);                   lV[3].setPropID(occupation);       lV[3].op = OP_ADD;

	TVERIFYRC( mSession->createPIN(pid1, &lV[0], 4));

	lV[0].set("Jean-Paul Sartre"); 	lV[0].setPropID(name);             lV[0].op = OP_ADD;
	lV[1].set(1905);               	lV[1].setPropID(date_of_birth);    lV[1].op = OP_ADD;
	lV[2].set(1980);               	lV[2].setPropID(date_of_death);    lV[2].op = OP_ADD;
	lV[3].set(pid);                	lV[3].setPropID(occupation);       lV[3].op = OP_ADD;

	TVERIFYRC( mSession->createPIN(pid1, &lV[0], 4));

	lV[0].set("Alan Turing"); 		lV[0].setPropID(name);             lV[0].op = OP_ADD;
	lV[1].set(1912);    	  		lV[1].setPropID(date_of_birth);    lV[1].op = OP_ADD;
	lV[2].set(1954);          		lV[2].setPropID(date_of_death);    lV[2].op = OP_ADD;
	lV[3].set(pid);           		lV[3].setPropID(occupation);       lV[3].op = OP_ADD;

	TVERIFYRC( mSession->createPIN(pid1, &lV[0], 4));

	lV[0].set("Sir Karl Popper"); 	lV[0].setPropID(name);             lV[0].op = OP_ADD;
	lV[1].set(1902);              	lV[1].setPropID(date_of_birth);    lV[1].op = OP_ADD;
	lV[2].set(1993);              	lV[2].setPropID(date_of_death);    lV[2].op = OP_ADD;
	lV[3].set(pid);               	lV[3].setPropID(occupation);       lV[3].op = OP_ADD;

	TVERIFYRC( mSession->createPIN(pid1, &lV[0], 4));
}



