/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"
#include "collectionhelp.h"

// Basic coverage of ACL support, i.e. Assigning permission to PIN read/write from
// different identities.

// See also testacls.cpp for more sophisticated testing


#define CNT_PIN_QUERY_EXPECTED 2

class TestAclsBasic : public ITest
{
	public:
		Afy::IAffinity *mStoreCtx;
		TEST_DECLARE(TestAclsBasic);
		virtual char const * getName() const { return "testaclsbasic"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "basic test for Access Control List support"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual bool includeInPerfTest() const { return false; }
		virtual int execute();
		virtual void destroy() { delete this; }
	private: 
		void testAccess
		(	
			const PID & inPID, 
			PropertyID inProp, 
			char * identityName, 
			uint8_t inAccess, 
			int inExpected
		) ;
		void testQueryAccess
		(	
			PropertyID inProp, 
			char * identityName,		
			unsigned long inExpectedCnt
		) ;

		void AddACL( ISession * const inSession, const PID & inPID, IdentityID inID, uint8_t inFlags /* META_PROP_READ, META_PROP_WRITE */ ) ;
	protected:
		void doTest();
		void testPINCreatePermissions(const char * inIdentity, bool bExpectedPermission) ;

};
TEST_IMPLEMENT(TestAclsBasic, TestLogger::kDStdOut);

struct IdentityInfo
{
	// Convenience structure to associate identity name with its id
	IdentityInfo( char * name )
	{
		// Mix in random portion so indepentent of any other identity
		// already in the store
		Tstring lRandomStr; 
		MVTRand::getString(lRandomStr,10,10,false,false);
		sprintf(str, "TestAclsBasic.%s.%s",lRandomStr.c_str(), name);
	}

	IdentityID id ;
	char str[255] ;
} ;


int TestAclsBasic::execute()
{
	if (!MVTApp::startStore())
	{
		return RC_FALSE ;
	}
	mStoreCtx = MVTApp::getStoreCtx();
	doTest() ;

	MVTApp::stopStore();

	return RC_OK  ;
}


void TestAclsBasic::doTest()
{
	// Based partially on TestAcls::quickTest1

	// Open regular session as store owner and add some data
	ISession * lSession =	MVTApp::startSession();

	PropertyID propids[1];
	MVTApp::mapURIs(lSession,"TestBasicAcls.case1",1,propids);

	// Register new identities.  Although ACL is actually tracked on a per-pin
	// level, we characterize each ID by its expected access rights

	char * lIdentPassword = "" ;
	IdentityInfo lReader("R"), lWriter("W"), lRW( "RW"), lRW2( "RW2"), lNoAccess( "NoAccess" ), lNoACL( "NoACL" ), lInserter( "Inserter") ;

	lReader.id = lSession->storeIdentity(lReader.str, lIdentPassword, false /*mayinsert*/ );
	TVERIFY( lReader.id != STORE_INVALID_IDENTITY ) ;

	lWriter.id = lSession->storeIdentity(lWriter.str, lIdentPassword, true /*mayinsert*/ );
	TVERIFY( lWriter.id != STORE_INVALID_IDENTITY ) ;

	lRW.id = lSession->storeIdentity(lRW.str, lIdentPassword, true /*mayinsert*/ );
	TVERIFY( lRW.id != STORE_INVALID_IDENTITY ) ;

	lRW2.id = lSession->storeIdentity(lRW2.str, lIdentPassword, true /*mayinsert*/ );
	TVERIFY( lRW2.id != STORE_INVALID_IDENTITY ) ;

	lNoAccess.id = lSession->storeIdentity(lNoAccess.str, lIdentPassword, true /*mayinsert*/ );
	TVERIFY( lNoAccess.id != STORE_INVALID_IDENTITY ) ;

	// Identity with no ACL record on the PIN at all
	lNoACL.id = lSession->storeIdentity(lNoACL.str, lIdentPassword, true /*mayinsert*/ );
	TVERIFY( lNoACL.id != STORE_INVALID_IDENTITY ) ;

	lInserter.id = lSession->storeIdentity(lInserter.str, lIdentPassword, false /*mayinsert*/ );
	TVERIFY( lInserter.id != STORE_INVALID_IDENTITY ) ;
	TVERIFYRC(lSession->setInsertPermission( lInserter.id, true ) );


	// Create Pins.  Number must be kept in sync with CNT_PIN_QUERY_EXPECTED

	// Normal access controlled PIN
	PID pidRoot, pidChild, pidGrandChild ;

	Value val ;
	val.set( 100 ) ; 
	val.property = propids[0] ;

	TVERIFYRC( lSession->createPINAndCommit( pidRoot, &val, 1 ) );

	// Set up the expected ACL values
	AddACL( lSession, pidRoot, lReader.id, META_PROP_READ ) ;
	AddACL( lSession, pidRoot, lWriter.id, META_PROP_WRITE ) ;
	AddACL( lSession, pidRoot, lRW.id, META_PROP_WRITE | META_PROP_READ ) ;

	// Access split to two separate values
	AddACL( lSession, pidRoot, lRW2.id, META_PROP_WRITE ) ;
	AddACL( lSession, pidRoot, lRW2.id, META_PROP_READ ) ;

	// Presumably redundant, as having no ACL means no access,
	// but this verifies that a empty access property is accepted
	AddACL( lSession, pidRoot, lNoAccess.id, 0 ) ;  

	// PIN that that points to first pin as its document,
	// with no ACL information stored on it.  It will inherit
	// the documents ACL information
	Value vals[2] ;
	vals[0].set( 800 ) ; 
	vals[0].property = propids[0] ;
	vals[1].set(pidRoot) ; 
	vals[1].property = PROP_SPEC_DOCUMENT ;
	TVERIFYRC( lSession->createPINAndCommit( pidChild, vals, 2 ) );


	// "Nested" document Pin
	vals[0].set( 600 ) ; 
	vals[0].property = propids[0] ;
	vals[1].set(pidChild) ; 
	vals[1].property = PROP_SPEC_DOCUMENT ;
	TVERIFYRC( lSession->createPINAndCommit( pidGrandChild, vals, 2 ) );

			
	lSession->terminate();

	//
	// Verify expected access to the different identities
	//

	testAccess(	pidRoot, propids[0], lReader.str, META_PROP_READ, 100 ) ;
	testAccess(	pidRoot, propids[0], lWriter.str, META_PROP_WRITE, 100 ) ;
	testAccess(	pidRoot, propids[0], lRW.str, META_PROP_WRITE|META_PROP_READ, 100 ) ;
	testAccess(	pidRoot, propids[0], lRW2.str, META_PROP_WRITE|META_PROP_READ, 100 ) ;
	testAccess(	pidRoot, propids[0], lNoAccess.str, 0, 100 ) ;
	testAccess(	pidRoot, propids[0], lNoACL.str, 0, 100 ) ;

	testAccess(	pidChild, propids[0], lReader.str, META_PROP_READ, 800 ) ;
	testAccess(	pidChild, propids[0], lWriter.str, META_PROP_WRITE, 800 ) ;
	testAccess(	pidChild, propids[0], lRW.str, META_PROP_WRITE|META_PROP_READ, 800 ) ;
	testAccess(	pidChild, propids[0], lRW2.str, META_PROP_WRITE|META_PROP_READ, 800 ) ;
	testAccess(	pidChild, propids[0], lNoAccess.str, 0, 800 ) ;
	testAccess(	pidChild, propids[0], lNoACL.str, 0, 800 ) ;

	testAccess(	pidGrandChild, propids[0], lReader.str, 0, 600 ) ;
	testAccess(	pidGrandChild, propids[0], lWriter.str, 0, 600 ) ;
	testAccess(	pidGrandChild, propids[0], lRW.str, 0, 600 ) ;
	testAccess(	pidGrandChild, propids[0], lRW2.str, 0, 600 ) ;
	testAccess(	pidGrandChild, propids[0], lNoAccess.str, 0, 600 ) ;
	testAccess(	pidGrandChild, propids[0], lNoACL.str, 0, 600 ) ;

	testQueryAccess( propids[0], lReader.str, CNT_PIN_QUERY_EXPECTED ) ;
	testQueryAccess( propids[0], lWriter.str, 0 ) ;
	testQueryAccess( propids[0], lRW.str, CNT_PIN_QUERY_EXPECTED ) ;
	testQueryAccess( propids[0], lRW2.str, CNT_PIN_QUERY_EXPECTED ) ;
	testQueryAccess( propids[0], lNoAccess.str, 0 ) ;
	testQueryAccess( propids[0], lNoACL.str, 0 ) ;

	// Verify the mayinsert argument
	testPINCreatePermissions(lReader.str,false) ;
	testPINCreatePermissions(lWriter.str,true) ;
	testPINCreatePermissions(lInserter.str,true) ;
}

void TestAclsBasic::AddACL
( 
	ISession * const inSession, 
	const PID & inPID, 
	IdentityID inID, 
	uint8_t inFlags /* META_PROP_READ, META_PROP_WRITE */  
)
{
	// Give inID access to pin inPID
	Value aclVal ;
	aclVal.setIdentity( inID ) ;
	aclVal.property = PROP_SPEC_ACL ;
	aclVal.meta = inFlags ;
	aclVal.op = OP_ADD ;  // Important for building collection

	TVERIFYRC(inSession->modifyPIN( inPID, &aclVal, 1 )) ;

	// Sanity check
	CmvautoPtr<IPIN> lPin( inSession->getPIN(inPID) ) ;
	MvStoreEx::CollectionIterator lAcls( lPin, PROP_SPEC_ACL ) ;
	bool bFoundIt = false ;
	for ( const Value * lVal = lAcls.getFirst() ;
		lVal !=NULL ;
		lVal = lAcls.getNext() )
	{
		if ( lVal->iid == inID )
		{
			bFoundIt = true ;
			break ;
		}
	}
	TVERIFY( bFoundIt ) ;
}


void TestAclsBasic::testAccess
(	
	const PID & inPID, 
	PropertyID inProp, 
	char * identityName,		// Name of identity who will own the session
	uint8_t inAccess,			// Expected Access
	int inExpected
)
{
	// Create a separate session (assuming no session already active on this thread)
	// that will act like a different identity

	ISession * lSession = MVTApp::startSession(mStoreCtx, identityName, NULL );
	TVERIFY( lSession != NULL ) ;

//	ISession * lSession = MVTApp::startSession();
//	TVERIFYRC( lSession->impersonate( identityName ) );

	Value valUpdate ;
	valUpdate.set( 101 ) ;
	valUpdate.property = inProp ;

	// Read Scenarios

	if ( inAccess & META_PROP_READ )
	{
		// Read the whole PIN and the particular property
		IPIN* pin1 = lSession->getPIN( inPID ) ;
		TVERIFY( pin1 != NULL ) ;
		TVERIFY( pin1->getValue( inProp )->i == inExpected ) ;
		pin1->destroy() ;

		// Read value from Session
		Value valRead ;
		lSession->getValue( valRead, inPID, inProp ) ;
		TVERIFY( valRead.i == inExpected ) ;
	}
	else
	{
		// No Read
		IPIN* pin1 = lSession->getPIN( inPID ) ;
		TVERIFY( pin1 == NULL ) ;
	}

	// Write Scenarios

	if ( inAccess & META_PROP_WRITE )
	{
		// Write via session
		TVERIFYRC( lSession->modifyPIN( inPID, &valUpdate, 1 ) );

		// Write from the PIN (only available if also read access)
		if ( inAccess & META_PROP_READ )
		{
			IPIN* pin = lSession->getPIN( inPID ) ;

			// Only RW access can actually confirm expected result of the write
			TVERIFY( pin->getValue( inProp )->i == 101 );

			valUpdate.i = 102 ;
			TVERIFYRC( pin->modify( &valUpdate, 1 ) );

			TVERIFYRC( pin->refresh() ) ;
			TVERIFY( pin->getValue( inProp )->i == 102 );

			pin->destroy() ;
		}

		// Restore original value
		valUpdate.i = inExpected ;
		lSession->modifyPIN( inPID, &valUpdate, 1 ) ;
	}
	else
	{
		// Expect write failure
		TVERIFY( RC_OK != lSession->modifyPIN( inPID, &valUpdate, 1 ) );

		if ( inAccess & META_PROP_READ )
		{
			// Write through PIN should also fail
			IPIN* pin = lSession->getPIN( inPID ) ;
			valUpdate.i = 102 ;
			TVERIFY( RC_OK != pin->modify( &valUpdate, 1 ) ) ;			
		}
	}



	lSession->terminate();
}

void TestAclsBasic::testQueryAccess
(	
	PropertyID inProp, 
	char * identityName,		// Name of identity who will own the session
	unsigned long inExpectedCnt
)
{
	// Verfiy that only Identities with read access to a PIN will see it in the
	// query (currently done purely by query count but it could also pass in expected
	// PIDs for really accurate test) See bug 1890.

	ISession * lSession = MVTApp::startSession(mStoreCtx, identityName, NULL );
	TVERIFY( lSession != NULL ) ;

	//
	// Look up the PINs via a query
	// 

	IStmt * pAllPINsWithProp = lSession->createStmt() ;
	unsigned char v = pAllPINsWithProp->addVariable() ;
	pAllPINsWithProp->setPropCondition( v, &inProp, 1 ) ;

	uint64_t cnt = 0 ;
	TVERIFYRC(pAllPINsWithProp->count( cnt ));

	//mLogger.out() << identityName << " found " << cnt << " pins" << endl ;

	TVERIFY( inExpectedCnt == cnt ) ;

	ICursor * pResult = NULL;
	TVERIFYRC(pAllPINsWithProp->execute(&pResult)) ;
	unsigned long cntFound = 0 ;
	while(1)
	{
		CmvautoPtr<IPIN> pPin( pResult->next()) ;
		if ( !pPin.IsValid()) 
			break;
		cntFound++ ;
	}

	TVERIFY( cntFound == cnt ) ;
	pResult->destroy() ;
	pAllPINsWithProp->destroy() ;
	lSession->terminate();
}


void TestAclsBasic::testPINCreatePermissions( const char * inIdentity, bool bExpectedPermission ) 
{
	// See also TestIdentity::testPinCreation

	// Pin creation is not handled by ACL system but closely related - see "mayinsert" arguments to storeIdentity calls.
	ISession * lSession = MVTApp::startSession(mStoreCtx, inIdentity, NULL );
	TVERIFY( lSession != NULL ) ;
	
	PID newpin ;
	Value readMeIfYouDare ;
	readMeIfYouDare.set(666) ; readMeIfYouDare.property = MVTApp::getProp( lSession, "TestAclsBasic.testPINCreatePermissions" ) ;

	RC rc = lSession->createPINAndCommit( newpin, &readMeIfYouDare, 1 ) ;

	if ( bExpectedPermission )
	{
		TVERIFYRC(rc) ;
		CmvautoPtr<IPIN> pin( lSession->getPIN( newpin ) ) ;

		// You can't create the PIN that you have created
		TVERIFY( !pin.IsValid() ) ;
		TVERIFY( newpin.ident == STORE_OWNER ) ; // REVIEW: Why doesn't the identity match the identity who created the pin

		/*
		TVERIFY( pin->isLocal() ) ;

		const Value * pPropVal = pin->getValue( readMeIfYouDare.property ) ;
		TVERIFY( pPropVal == NULL ) ;
		
		CmvautoPtr<IPIN> pinClone( pin->clone(NULL,0,MODE_PERSISTENT) ) ;
		TVERIFY( pinClone.IsValid() ) ;
		*/
	}
	else
	{
		TVERIFY( rc == RC_NOACCESS ) ;
	}

	lSession->terminate() ;
}

