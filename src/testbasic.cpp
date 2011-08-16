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
class TestBasic : public ITest
{
	public:
		TEST_DECLARE(TestBasic);
		virtual char const * getName() const { return "testbasic"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Basic MVStore Test"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void doTest() ;
		void InitProperties() ;
		void RemoveData() ;

		ClassID CreatePeopleClass() ;

		void CreateAlbertPIN() ;
		void CreateSaraPIN() ;
		void CreateFredPIN() ;
		void AddPerson( char * inName, int inAge, bool inMale ) ;

		void RecordMarriage( char * inHusband, char * inWife ) ;
		bool IsMarried( char * inName, string * outSpouse = NULL ) ;
		unsigned long FindBachelors() ;

		void RecordChild( char * inMother, char * inChild ) ;
		uint32_t GetChildrenCount( char * inMother ) ;
		bool IsChild( char * inMother, char * inChild ) ;

		IPIN * FindPerson( char * inName, bool bAllowMissing = false ) ;

		IStmt * GetPeopleQuery() ;
		IStmt * GetPeopleQuery2(STMT_OP sop=STMT_QUERY) ;
		void VerifyNumberofPeople(unsigned int inExpected ) ;

	private:
		ISession * mSession ;

		// Contains mapping between string URI or property and
		// PropertyID in the store
		vector<URIMap> mProps ;

		// Index in mProps of the Properties used by this
		// test

		enum PropMapIndex
		{
			NameIndex = 0,// String - First name
			AgeIndex,     // Integer - Years since birth
			SpouseIndex,  // Reference to the husband or wife
			ChildrenIndex,// Collection.  For simplicity only mother records children 
			GenderIndex,  // True for male, false female
			CountProps
		} ;
};
TEST_IMPLEMENT(TestBasic, TestLogger::kDStdOut);

int TestBasic::execute()
{
	doTest();
	return RC_OK;
}

void TestBasic::doTest()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	cout << "Number of argc: " << get_argc () << endl; 
 	for(unsigned int ii=0; ii < get_argc(); ii++)
	{
        	cout << "Arg." << ii << " is: " << get_argv (ii) << endl; 
	}
	
	cout << "Test new access to the arguments: " << endl;
	
	cout << "Total number of  parameters: " << mpArgs->get_cntParamTotal() << endl;
	cout << "Number of global parameters: " << mpArgs->get_cntParamGlobal() << endl;
	cout << "Number of local  parameters: " << mpArgs->get_cntParamTest() << endl;
	cout << "Number of none-formated parameters: " << mpArgs->get_cntParamNFrmt()<< endl;
	unsigned long lseed;
	if(mpArgs->get_param("seed",lseed))
	{
	    cout << "Request to use seed=" << lseed << endl;
	}
	
	cout << "Please, check MVTArgs.h for more information about new interface. " << endl << endl; 
	
	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	InitProperties() ;
	CreatePeopleClass() ;

	// Start with clean slate
	// REVIEW: is there a way to just flush everything,
	// including classes and mapped properties?
	RemoveData() ;
	VerifyNumberofPeople( 0 ) ;

	CreateAlbertPIN() ;
	CreateSaraPIN() ;
	CreateFredPIN() ;

	VerifyNumberofPeople( 3 ) ;

	AddPerson( "George", 12, true ) ;
	AddPerson( "Eli", 20, true ) ;
	AddPerson( "Dilip", 80, true ) ;
	AddPerson( "Martha", 78, false ) ;
	AddPerson( "Adela", 30, false ) ;

	TVERIFY( !IsMarried( "George", NULL ) ) ;

	TVERIFY( FindBachelors() == 4 ) ; // Albert, Dilip, Eli and Fred are unmarried adult males

	RecordMarriage( "Albert", "Sara" ) ;
	
	string strSpouseName ;
	TVERIFY( IsMarried( "Albert", &strSpouseName ) ) ;
	TVERIFY( 0 == strcmp( strSpouseName.c_str(), "Sara" ) );
	TVERIFY( IsMarried( "Sara", &strSpouseName ) ) ;
	TVERIFY( 0 == strcmp( strSpouseName.c_str(), "Albert" ) ) ;

	TVERIFY( FindBachelors() == 3 ) ; // Marriage should reduce the count

	TVERIFY( 0 == GetChildrenCount( "Sara" ) ) ;
	TVERIFY( !IsChild( "Sara", "George" ) ) ;

	RecordChild( "Sara", "George" ) ;

	TVERIFY( 1 == GetChildrenCount( "Sara" ) ) ;
	TVERIFY( IsChild( "Sara", "George" ) ) ;

	RecordChild( "Sara", "George" ) ; // Redundant, should do nothing
	RecordChild( "Sara", "Eli" ) ;
	RecordChild( "Sara", "Adela" ) ;

	TVERIFY( 3 == GetChildrenCount( "Sara" ) ) ;
	TVERIFY( IsChild( "Sara", "Eli" ) ) ;

	RecordMarriage( "Dilip", "Martha" ) ;
	RecordChild( "Martha", "Sara" ) ;
	RecordChild( "Martha", "Fred" ) ;
	TVERIFY( 2 == GetChildrenCount( "Martha" ) ) ;
	TVERIFY( IsChild( "Martha", "Sara" ) ) ;

	// TODO : Record children relationships to demonstrate collections

	// Comment out this line if you want to dump the store
	// RemoveData() ;

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
}

void TestBasic::InitProperties()
{
	// Demonstrates ISession::mapURIs, getPropertyURI and getPropertyDisplayName
	//
	// Get PropertyIDs for all the properties that will be
	// used in the example

	// Initialize data
	mProps.resize( CountProps ) ;
	for ( size_t i = 0 ; i < CountProps ; i++ )
	{	
		mProps[i].uid = STORE_INVALID_PROPID ;
	}

	mProps[NameIndex].URI = "basictest.name" ;
	mProps[AgeIndex].URI = "basictest.age" ;
	mProps[SpouseIndex].URI = "basictest.spouse" ;
	mProps[ChildrenIndex].URI = "basictest.children" ;
	mProps[GenderIndex].URI = "basictest.gender" ;

	TVERIFYRC( mSession->mapURIs( CountProps, &mProps[0] ) );

	//Expected results is that all the PropertyIDs have been assigned
	for ( size_t i = 0 ; i < CountProps ; i++ )
	{
		TVERIFY( mProps[i].uid != STORE_INVALID_PROPID );
	}

	// Sanity check 
	TVERIFY( mProps[AgeIndex].uid != mProps[NameIndex].uid ) ;

	//To request the same URI again will retrieve the same ID
	URIMap testInfo = { "basictest.name", STORE_INVALID_PROPID } ;
	TVERIFYRC( mSession->mapURIs( 1, &testInfo ) );
	TVERIFY( testInfo.uid == mProps[NameIndex].uid ) ;


	// Now that properties are registered there are lookup services
	// The store should remember the URI and DisplayName
	char infobuf[ 128 ] ; size_t sz = 128;
	mSession->getURI(mProps[AgeIndex].uid, infobuf, sz);
	TVERIFY( sz == strlen( "basictest.age" ) ) ;
	TVERIFY( 0 == strcmp( "basictest.age",  infobuf ) ) ;
}

ClassID TestBasic::CreatePeopleClass()
{
	// Define a query for all people
	
	IStmt * pAllPINsWithName = mSession->createStmt() ;

	unsigned char v = pAllPINsWithName->addVariable() ;

	PropertyID nameProp = mProps[NameIndex].uid ; 

	pAllPINsWithName->setPropCondition( v, &nameProp, 1 ) ;

	// See if someone has already run this test on this store
	ClassID lClsid = STORE_INVALID_CLASSID ;
	if ( RC_NOTFOUND == mSession->getClassID("testbasic.allpeople", lClsid) )
	{
		TVERIFY( lClsid == STORE_INVALID_CLASSID ) ;

		TVERIFYRC( defineClass(mSession,"testbasic.allpeople", pAllPINsWithName, &lClsid ) );

		TVERIFY( lClsid != STORE_INVALID_CLASSID ) ;

		// Note - you could call again to retrieve the classid later
		ClassID lClsid2 = STORE_INVALID_CLASSID ;
		TVERIFYRC( mSession->getClassID("testbasic.allpeople", lClsid2) );
		TVERIFY( lClsid == lClsid2 ) ;

		// Side note - you have to define the class before you can find its classid
		ClassID lClsid3 = STORE_INVALID_CLASSID ;
		TVERIFY( mSession->getClassID("bogus", lClsid3) != RC_OK );

		TVERIFY( RC_ALREADYEXISTS == defineClass(mSession,"testbasic.allpeople", pAllPINsWithName )) ;

		ClassID lClsid4 = STORE_INVALID_CLASSID ;
		TVERIFYRC( mSession->getClassID("testbasic.allpeople", lClsid4) );
		TVERIFY( lClsid == lClsid4 ) ;
	}

	pAllPINsWithName->destroy() ;

	return lClsid ;
}

IStmt * TestBasic::GetPeopleQuery()
{
	// Get a Query that finds all members of class "testbasic.allpeople" 
	// See GetPeopleQuery2() for the recommended way

	IStmt *query = NULL; ClassID cid = STORE_INVALID_CLASSID; IPIN *cpin;

	if (mSession->getClassID("testbasic.allpeople",cid)==RC_OK &&
						mSession->getClassInfo(cid,cpin)==RC_OK) {
		const Value *cv = cpin->getValue(PROP_SPEC_PREDICATE);
		if (cv!=NULL && cv->type==VT_STMT) query = cv->stmt->clone();
		cpin->destroy();
	}
	return query ;
}

IStmt * TestBasic::GetPeopleQuery2(STMT_OP sop)
{
	// Get a Query that finds all members of class "testbasic.allpeople" 

	// Another implementation, building a new Query
	// matching on class membership

	// Look up the classID
	ClassID lClsid = STORE_INVALID_CLASSID ;
	TVERIFYRC( mSession->getClassID("testbasic.allpeople", lClsid) );

	//Query that matches by ClassID
	IStmt * query = mSession->createStmt(sop);
	ClassSpec lCS;
	lCS.classID = lClsid ;
	lCS.nParams = 0;
	lCS.params = NULL;
	query->addVariable(&lCS, 1);

	/*
	Side point: It can be useful to look at the query as a string
	*/
	char * strQuery = query->toString() ;  TVERIFY( strlen( strQuery ) > 0 ) ;
	mSession->free( strQuery ) ;

	return query ;
}

void TestBasic::VerifyNumberofPeople( unsigned int inExpected )
{
	// Note: This is doing full-scan...
	IStmt * q1 = GetPeopleQuery() ;
	IStmt * q2 = GetPeopleQuery2() ;

	TVERIFY( q1 != NULL ) ;
	TVERIFY( q2 != NULL ) ;

	uint64_t cnt = 0 ;
	TVERIFYRC( q1->count( cnt ) );
	TVERIFY( cnt == inExpected ) ;

	TVERIFYRC( q2->count( cnt ) );
	TVERIFY( cnt == inExpected ) ;

	q1->destroy() ;
	q2->destroy() ;
}

void TestBasic::CreateAlbertPIN()
{
	// Really step by step creation of entry in store
	// for a guy named Albert, age 48

	// 1. Do it really step by step.  
	// 1.1 First establish the PIN (no values)
	PID pid1 ; memset( &pid1, 0, sizeof( pid1 ) ) ;

	// If database locked you this can fail, e.g. RC_READTX
	TVERIFYRC( mSession->createPIN( pid1, NULL, 0 ) ) ;
	TVERIFY( pid1.pid != STORE_INVALID_PID ) ;
	TVERIFY( pid1.ident != STORE_INVALID_IDENTITY ) ;	

	// 1.2 look up the newly created PIN	
	IPIN * pPIN1 = mSession->getPIN( pid1 ) ;
	TVERIFY( pPIN1 != NULL ) ;
	TVERIFY( pPIN1->getNumberOfProperties() == 0 ) ;

	// 1.3 Set the Name etc as Properties of the new PIN
	Value vals1[3] ;
	vals1[0].set( "Albert" ) ; vals1[0].property = mProps[NameIndex].uid ;
	vals1[1].set( 48 ) ; vals1[1].property = mProps[AgeIndex].uid ;
	vals1[2].set( true ) ; vals1[2].property = mProps[GenderIndex].uid ;
	pPIN1->modify( vals1, 3 ) ;

	TVERIFYRC( pPIN1->modify( vals1, 2 ) );

	// 1.4 Verify the results
	// The existing pPIN1 pointer we have will reflect the modification
	TVERIFY( pPIN1->getNumberOfProperties() == 3 ) ;
	const Value * pValLookup = pPIN1->getValue( mProps[NameIndex].uid ) ;
	TVERIFY( pValLookup != NULL ) ;
	TVERIFY( 0 == strcmp( pValLookup->str, "Albert" ) ) ;

	pValLookup = pPIN1->getValue( mProps[AgeIndex].uid ) ;
	TVERIFY( pValLookup != NULL ) ;
	TVERIFY( pValLookup->i == 48 ) ;

	pValLookup = pPIN1->getValue( mProps[SpouseIndex].uid ) ;
	TVERIFY( pValLookup == NULL ) ; // No property was set

	pPIN1->destroy() ; pPIN1 = NULL ;

	// Verify the results from a new lookup
	IPIN * pPIN2 = mSession->getPIN( pid1 ) ;
	TVERIFY( pPIN2 != NULL ) ;
	TVERIFY( pPIN2->getNumberOfProperties() == 3 ) ;

	pPIN2->destroy() ; pPIN2 = NULL ;
}

void TestBasic::CreateSaraPIN()
{
	// Similar to CreateAlbertPIN, but using different methods
	// Creates a woman named Sara, age 47
	//
	// Demonstrates ISession::alloc, createUncommittedPIN, commitPINs, modifyPIN etc
	// Focuses on the tricky memory handling considerations

	// Strings have to be allocated using store allocator
	char * pSaraMisspelledNameBuffer = (char*)mSession->alloc( strlen( "SaRRra" ) + 1 ) ;
	strcpy(pSaraMisspelledNameBuffer, "SaRRra" ) ; 

	// Create the pin and set the name immediately
	// Note: createUncommittedPIN is a special case.  
	// We have to allocate the memory for the store because
	// it doesn't copy the data.  (otherwise you will see a crash 
	// when we call IPin->modify below).  See coverage of MODE_COPY_VALUES elsewhere.
	Value * pName = (Value*)mSession->alloc( sizeof(Value) ) ;;
	pName->set( pSaraMisspelledNameBuffer ) ;
	pName->property = mProps[NameIndex].uid ;

	IPIN * pSaraPin = mSession->createUncommittedPIN( pName, 1 ) ;
	TVERIFY( pSaraPin != NULL ) ;
	TVERIFY( !pSaraPin->isCommitted() ) ;
	TVERIFY( pSaraPin->getNumberOfProperties() == 1 ) ;
	
	PID saraPID = pSaraPin->getPID() ;

	// PID isn't valid yet, because not committed
	TVERIFY( saraPID.pid == 0 ) ; // REVIEW: it isn't STORE_INVALID_PROPID - is that by design
	TVERIFY( saraPID.ident == STORE_INVALID_IDENTITY ) ;

	// Add the age as a second call
	// NOTE: For calls to modify the structure does not need to be allocated

	Value moreInfo[2] ;
	moreInfo[0].set( false ) ; moreInfo[0].property = mProps[GenderIndex].uid ;
	moreInfo[1].set( 47 ) ; moreInfo[1].property = mProps[GenderIndex].uid ;
	pSaraPin->modify( moreInfo, 2 ) ;

	pName = NULL ; // pName buffer has been freed by store

	// commitPINs will copy the information into database
	// so we will own the memory again
	TVERIFYRC( mSession->commitPINs( &pSaraPin, 1 ) );
	TVERIFY( pSaraPin->isCommitted() ) ;

	// Now PID should be valid
	saraPID = pSaraPin->getPID() ;
	TVERIFY( saraPID.pid != STORE_INVALID_PROPID ) ;
	TVERIFY( saraPID.ident != STORE_INVALID_IDENTITY ) ;

	// We own the memory again
	pSaraPin->destroy() ;
	pSaraPin = NULL ;

	// Fix Sara's misspelled name
	// NOTE: For modifyPin we can use the stack for the memory
	Value name2; name2.set( "Sara" ) ; name2.property = mProps[NameIndex].uid ;

	TVERIFYRC(mSession->modifyPIN( saraPID, &name2, 1 )) ;

	// Verification
	IPIN * pSaraPin2 = mSession->getPIN( saraPID ) ;
	TVERIFY( pSaraPin2->getNumberOfProperties() == 2 ) ;
	
	const Value * pValLookup = pSaraPin2->getValue( mProps[NameIndex].uid ) ;
	TVERIFY( pValLookup != NULL ) ;
	TVERIFY( 0 == strcmp( pValLookup->str, "Sara" ) ) ;

	pSaraPin2->destroy() ;
}

void TestBasic::CreateFredPIN()
{
	// Simpler usage of createUncommittedPIN

	// Create the pin and set the name immediately
	//
	// Use the mode flag MODE_COPY_VALUES to avoid having to 
	// allocate memory the same way as in CreateSaraPIN

	Value name ; name.set( "Fred" ) ; name.property = mProps[NameIndex].uid ;

	IPIN * pFredPin = mSession->createUncommittedPIN( &name, 1, MODE_COPY_VALUES ) ;
	TVERIFY( pFredPin != NULL ) ;
	
	// Add the age as a second call
	Value moreInfo[2] ;
	moreInfo[0].set( true ) ; moreInfo[0].property = mProps[GenderIndex].uid ;
	moreInfo[1].set( 18 ) ; moreInfo[1].property = mProps[AgeIndex].uid ;	
	TVERIFYRC( pFredPin->modify( moreInfo, 2 ) );

	TVERIFYRC( mSession->commitPINs( &pFredPin, 1 ) );
	TVERIFY( pFredPin->isCommitted() ) ;

	PID fredPID = pFredPin->getPID() ;
	TVERIFY( fredPID.pid != STORE_INVALID_PROPID ) ;
	TVERIFY( fredPID.ident != STORE_INVALID_IDENTITY ) ;

	pFredPin->destroy() ;
	pFredPin = NULL ;
}

void TestBasic::AddPerson( char * inName, int inAge, bool inMale )
{
	// Generalization of CreateAlbertPIN for creating
	// other people

	/*
	// It is not necessary to copy the string buffer, in fact it would leak
	char * pNameBuffer = (char*)mSession->alloc( strlen( inName ) + 1 ) ;
	strcpy(pNameBuffer, inName ) ; 
	*/

	Value vals[3] ;
	vals[0].set( inName ) ; vals[0].property = mProps[NameIndex].uid ;
	vals[1].set( inAge ) ; vals[1].property = mProps[AgeIndex].uid ;
	vals[2].set( inMale ) ; vals[2].property = mProps[GenderIndex].uid ;

	PID pid1 ; memset( &pid1, 0, sizeof( pid1 ) ) ;
	TVERIFYRC( mSession->createPIN( pid1, vals, 3 ) ) ;
}

void TestBasic::RecordMarriage( char * inHusband, char * inWife )
{
	// Set the spouse property on both entries
	CmvautoPtr<IPIN> pHusband( FindPerson( inHusband ) );
	CmvautoPtr<IPIN> pWife( FindPerson( inWife ) );

	PID husbandPID = pHusband->getPID() ;
	PID wifePID = pWife->getPID() ;

	Value spouseRef ;
	spouseRef.set(wifePID) ; spouseRef.property = mProps[SpouseIndex].uid ;

	TVERIFYRC(pHusband->modify( &spouseRef, 1 )) ;

	spouseRef.set(husbandPID) ; spouseRef.property = mProps[SpouseIndex].uid ;
	TVERIFYRC(pWife->modify( &spouseRef, 1 )) ;
}

bool TestBasic::IsMarried( char * inName, string * outSpouse )
{
	bool ret=false ;

	CmvautoPtr<IPIN> pPerson( FindPerson( inName ) );
	TVERIFY( pPerson.IsValid() ) ;

	if ( pPerson.IsValid() )
	{
		const Value * pSpouseRef = pPerson->getValue( mProps[SpouseIndex].uid ) ;
		if ( pSpouseRef )
		{
			if ( outSpouse )
			{
				// Caller wants to know the name
				IPIN * pSpouse = mSession->getPIN(*pSpouseRef) ; // using shortcut version of getPIN()
				TVERIFY( pSpouse != NULL ) ; // Data is corrupt 
				const Value * pSpouseName = pSpouse->getValue(mProps[NameIndex].uid) ;
				TVERIFY( pSpouseName != NULL ) ;
				*outSpouse = pSpouseName->str ;

				pSpouse->destroy() ;
			}

			ret = true ;
		}
	}

	return ret ;
}

void TestBasic::RecordChild( char * inMother, char * inChild )
{	
	if ( IsChild( inMother, inChild ) )
		return ; // Do nothing is already recorded

	// For simplicity children are only tracked via the mother
	CmvautoPtr<IPIN> pMother(FindPerson( inMother ) );
	CmvautoPtr<IPIN> pChild(FindPerson( inChild ) );
	PID childPID = pChild->getPID() ;

	// Initialize array element that will represent this child
	Value childElem ;
	childElem.set( childPID ) ;
	childElem.property = mProps[ChildrenIndex].uid ;
	childElem.op = OP_ADD ;
	childElem.eid = STORE_LAST_ELEMENT ;

	// Check if the child already exists 
	const Value * children = pMother->getValue( mProps[ChildrenIndex].uid ) ;

	if ( children == NULL )
	{
		// First child - establish the VT_ARRAY
		// (If this is not done then the first child will be recorded without an 
		// array, and the array will be established as the second child is added.
		// That is convenient, but would potentially make the code in GetChildCount
		// and elsewhere more complicated)
		Value childrenArray ;
		childrenArray.set( &childElem, 1 ) ;
		childrenArray.property = mProps[ChildrenIndex].uid ;

		TVERIFY( childrenArray.type == VT_ARRAY ) ;
		TVERIFYRC( pMother->modify( &childrenArray, 1 ) ) ;
	}
	else
	{
		// Because we specify OP_ADD, MVStore will automatically
		// insert this item to the existing collection, we don't need to manually
		// modify the structure
		TVERIFYRC( pMother->modify( &childElem, 1 ) ) ;

		// Expect to discover the eid
		TVERIFY( childElem.eid != STORE_COLLECTION_ID ) ;
	}
}

bool TestBasic::IsChild( char * inMother, char * inChild )
{
	// Test whether inChild is already recorded as a child of inMother

	bool bFound = false ;

	CmvautoPtr<IPIN> pMother(FindPerson( inMother ) );
	CmvautoPtr<IPIN> pChild(FindPerson( inChild ) );
	PID childPID = pChild->getPID() ;

	const Value * children = pMother->getValue( mProps[ChildrenIndex].uid ) ;

	if ( children != NULL )
	{
		if ( children->type == VT_COLLECTION )
		{
			// 	VT_COLLECTION is currently the default case.  Even though
			// we build a VT_ARRAY initially (see RecordChild), the 
			// store returns a Navigator
			INav * pNav = children->nav ;
			const Value * elem = pNav->navigate( GO_FIRST ) ;

			do 
			{
				TVERIFY( elem->type == VT_REFID) ;
				if ( elem->id == childPID )
				{
					bFound = true ;
					break ;
				}

				// REVIEW: is any cleanup necessary for the elements?

				elem = pNav->navigate( GO_NEXT ) ;
			} while ( elem != NULL ) ;
		}
		else 		
		{
			// Also handle the VT_ARRAY case, which can arise when ITF_COLLECTIONS_AS_ARRAYS is specified
	
			TVERIFY( children->type == VT_ARRAY ) ; 

			for ( uint32_t i = 0 ; i < children->length ; i++ )
			{
				TVERIFY( children->varray[i].type == VT_REFID) ;
				if ( children->varray[i].id == childPID )
				{
					bFound = true ;
					break ;
				}
			}
		}
	}

	return bFound ;
}

uint32_t TestBasic::GetChildrenCount( char * inMother )
{
	// Demonstrate how to get the number of elements in a collection
	uint32_t cntChildren = 0 ;

	CmvautoPtr<IPIN> pMother(FindPerson( inMother )) ;

	const Value * children = pMother->getValue( mProps[ChildrenIndex].uid ) ;

	// Null means no children at all
	if ( children != NULL )
	{
		// Handle both possible collection representations
		if ( children->type == VT_COLLECTION )
		{
			INav * pNav = children->nav ;
			cntChildren = pNav->count() ;
		}
		else 		
		{
			TVERIFY( children->type == VT_ARRAY ) ; 
			cntChildren = children->length ;
		}
	}

	return cntChildren ;
}

IPIN * TestBasic::FindPerson( char * inName, bool bAllowMissing )
{
	// Based on TestTransactions::findPIN
	// For purpose of test only expects 1 instance of a given name
	// The test will fail when called with missing person, unless bAllowMissing is set
	//
	// Note: if we found this query was slow we could try defining a "family"
	// query with a variable based on the name.  That would result in the names being indexed for fast lookup.
	//
	// Caller must destroy returned IPIN

	// Specify the ClassID of the data we want to search.  (Otherwise a "WARNING: Full scan query!!!" is logged)
	ClassID lClsid = STORE_INVALID_CLASSID ;
	TVERIFYRC( mSession->getClassID("testbasic.allpeople", lClsid) );
	ClassSpec lCS;
	lCS.classID = lClsid ;
	lCS.nParams = 0;
	lCS.params = NULL;

	// Build Query
	IStmt *query = mSession->createStmt();

	Value op[2];
	PropertyID prop = mProps[NameIndex].uid;
	
	op[0].setVarRef(0 /*variable not defined yet, but Mark says 0 is safe assumption*/,prop);
	op[1].set(inName);

	IExprTree *exprfinal = mSession->expr(OP_EQ,2,op);

	//Rather than calling "query->addCondition(exprfinal)";
	//it is possible to provide the expression immediately at query creation time
	query->addVariable(&lCS,1,exprfinal);

	/*
	// Example of the query created:
	QUERY.100(10000000) {
			Var:0 {
			        Classes:        testbasic.allpeople
					Condition:      EXPR.100(2,4096) {
			LOAD    &0/basictest.name
			LOAD    "George"
			EQ              1
	}
					CondProps:              basictest.name
			}
	}
	*/
	char * strQuery = query->toString() ;  TVERIFY( strlen( strQuery ) > 0 ) ; 
	//mLogger.out() << strQuery ;
	mSession->free( strQuery ) ;

	ICursor *result = NULL;
	TVERIFYRC(query->execute(&result));

	TVERIFY( bAllowMissing || result != NULL ) ;
	
	if ( result == NULL )
	{
		exprfinal->destroy() ;	
		query->destroy() ;
		return NULL ;
	}

	IPIN * pin = result->next() ;

	TVERIFY( bAllowMissing || pin != NULL ) ;

	// Expect only one match in this test
	IPIN * pin2 = result->next() ;		
	TVERIFY2( pin2 == NULL, "Found more than one entry with same name, perhaps store wasn't properly cleaned" ) ;

	IPIN * pin3 = result->next() ;
	TVERIFY( pin3 == NULL ) ;

	result->destroy() ;
	exprfinal->destroy() ;	
	query->destroy() ;

	// Extra little Sanity check - the PIN we return 
	// should belong to the class of people
	if ( pin )
	{
		ClassID classID = STORE_INVALID_CLASSID; 
		TVERIFYRC( mSession->getClassID( "testbasic.allpeople", classID ) );
		TVERIFY( pin->testClassMembership( classID ) );
	}

	return pin ;
}

unsigned long TestBasic::FindBachelors( )
{
	/* Execute Compound query Gender = male AND Age >= 16 AND Not married

	QUERY.100(10000000) {
			Var:0 {
					Classes:        testbasic.allpeople
					Condition:      EXPR.100(2,4096) {
			LOAD    &0/basictest.gender
			LOAD    true
			EQ              13
			LOAD    &0/basictest.age
			LOAD    16
			GE              13
			LOAD    &(0)/basictest.spouse
			EXIST           68
	}
			}
	}
	*/

	IStmt *qry = mSession->createStmt();     

	ClassID lClsid = STORE_INVALID_CLASSID ;
	TVERIFYRC( mSession->getClassID("testbasic.allpeople", lClsid) );
	ClassSpec lCS;
	lCS.classID = lClsid ;
	lCS.nParams = 0;
	lCS.params = NULL;

	unsigned char var = qry->addVariable(&lCS,1);

	Value args[2];

	args[0].setVarRef(0,(mProps[GenderIndex].uid) );
	args[1].set(true);
	IExprTree * exprMale = mSession->expr(OP_EQ,2,args);
	TVERIFY( exprMale != NULL ) ;

	// Note: args have been copied internally so we can reuse the args array
	args[0].setVarRef(0,(mProps[AgeIndex].uid) );
	args[1].set(16);
	IExprTree * exprAdult = mSession->expr(OP_GE,2,args);
	TVERIFY( exprAdult != NULL) ;

	args[0].setVarRef(0,(mProps[SpouseIndex].uid) );
	IExprTree * exprMarried = mSession->expr(OP_EXISTS,1,args);
	TVERIFY( exprMarried != NULL ) ;

	args[0].set(exprMarried );
	IExprTree * exprNotMarried = mSession->expr(OP_LNOT,1,args);
	TVERIFY( exprNotMarried != NULL ) ;

	// OP_LAND only supports 2 arguments so we need to use two nodes to AND the 3 conditions

	args[0].set(exprMale);
	args[1].set(exprAdult);
	IExprTree * exprMaleAdult = mSession->expr(OP_LAND,2,args);
	TVERIFY( exprMaleAdult != NULL ) ;

	args[0].set(exprMaleAdult);
	args[1].set(exprNotMarried);
	IExprTree * exprCompound = mSession->expr(OP_LAND,2,args);
	TVERIFY( exprCompound != NULL ) ;

	qry->addCondition(var,exprCompound);

	char * strQuery = qry->toString() ;  TVERIFY( strlen( strQuery ) > 0 ) ; 
	//mLogger.out() << strQuery ;
	mSession->free( strQuery ) ;

	/*
	//Only the final IExprTree should be destroyed - it will clean everything else
	//up recursively
	exprMale->destroy() ;
	exprAdult->destroy() ;
	exprNotMarried->destroy() ;
	*/
	exprCompound->destroy() ;	

	// We could return the query, but for the moment just return the count

	uint64_t cnt = 0 ;
	qry->count( cnt ) ;

	/* //To print out the names:
	ICursor *result = qry->execute();
	while(true)
	{
		IPIN* ipin = result->next() ;
		if ( ipin == NULL ) break ;

		mLogger.out() << ipin->getValue( mProps[NameIndex].uid )->str << endl ;

		ipin->destroy() ;
	}
	result->destroy() ;
	*/

	qry->destroy() ;
	return (unsigned long)cnt ;
}

void TestBasic::RemoveData()
{
	// The test framework may re-use the same store 
	// each time the test is run, so be sure to remove the 
	// pins. 

	IStmt * q = GetPeopleQuery2(STMT_DELETE);
	q->execute() ;
	q->destroy() ;
}
