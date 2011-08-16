/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

// Basic coverage of Identity feature.
// 
// See ACL tests and testsessionlogin for more specific Identity related testing
struct IdentityInfo
{
	// Convenience structure to associate identity name with its id
	IdentityInfo( const char * inName, const char * inRandom, const char * inPwd )
	{
		// Mix in random portion so indepentent of any other identity
		// already in the store
		sprintf(str, "TestIdentity.%s.%s",inRandom, inName);

		id = STORE_INVALID_IDENTITY ;

		pwd = inPwd ; 
	}

	IdentityID id ;
	char str[255] ;
	string pwd ;
} ;

class TestIdentity : public ITest
{
	public:
		MVStoreKernel::StoreCtx *mStoreCtx;
		TEST_DECLARE(TestIdentity);
		virtual char const * getName() const { return "testidentity"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "basic test for Identities"; }
		virtual bool includeInPerfTest() const { return false; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void doTest();
		void FindIDInDump( ISession * inSession, IdentityInfo & info ) ;
		void testIdentityAndSessions( IdentityInfo & info ) ;
		void testPinCreation( IdentityInfo & info, bool inExpectPinCreation ) ;

		string mRand ; // Random string so that identities don't conflict when test is run again
};
TEST_IMPLEMENT(TestIdentity, TestLogger::kDStdOut);

int TestIdentity::execute()
{
	if (!MVTApp::startStore())
	{
		return RC_FALSE ;
	}
	mStoreCtx = MVTApp::getStoreCtx();
	MVTRand::getString(mRand,10,10,false,false);

	// Demonstrate that an identity must be registered (with storeIdentity)
	// before calling impersonate
	ISession * lSession = MVTApp::startSession();
	TVERIFY( RC_OK != lSession->impersonate( "Not Actually Registered" ) );
	lSession->terminate() ;

	// You can't open a session for an non-existing identity
	lSession = MVTApp::startSession(mStoreCtx, "I don't exist" );
	TVERIFY( lSession == NULL ) ;


	// Create real session to register Other identity
	lSession = MVTApp::startSession();
	TVERIFY( lSession->getCurrentIdentityID() == STORE_OWNER ) ;

	IdentityInfo lOther("Other", mRand.c_str(), "password 1") ;
	lOther.id = lSession->storeIdentity(lOther.str, lOther.pwd.c_str(), false /*mayinsert*/ );
	TVERIFY( lOther.id != STORE_INVALID_IDENTITY ) ;

	// store same identity again
	IdentityID lOtherid2 = lSession->storeIdentity(lOther.str, "", false /*mayinsert*/ );
	TVERIFY( lOtherid2 == lOther.id ) ;

	// Note: ISession::loadIdentity seems almost the same as storeIdentity.
	// However it takes an already encrypted password as argument.  

	#if 0
		FindIDInDump( lSession, lOther ) ;
	#endif

	// Demonstrate the "mayinsert" argument which controls
	// whether pin creation is allowed or not
	IdentityInfo lIdentWithInsertPermission("Inserter", mRand.c_str(), "password 2") ;
	lIdentWithInsertPermission.id = lSession->storeIdentity(lIdentWithInsertPermission.str, lIdentWithInsertPermission.pwd.c_str(), true /*mayinsert*/ );
	TVERIFY( lIdentWithInsertPermission.id != STORE_INVALID_IDENTITY ) ;

	lSession->terminate() ;

	testIdentityAndSessions( lOther ) ;
	testPinCreation( lOther, false ) ;
	testPinCreation( lIdentWithInsertPermission, true ) ;

	MVTApp::stopStore();

	return RC_OK  ;
}

void TestIdentity::testIdentityAndSessions( IdentityInfo & info ) 
{
	// Verify some of the things that an Identity other than 
	// STORE_OWNER can do

	TVERIFY( !info.pwd.empty() ) ; // Test assumes identity has a password

	// Assuming no existing session when this is called

	ISession * lSession = MVTApp::startSession();
	TVERIFYRC(lSession->impersonate( info.str )) ; // REVIEW: no password needed?
	
	lSession->terminate() ;

	// Edge case: Password missing
	lSession = MVTApp::startSession(mStoreCtx, info.str );
	TVERIFY( lSession == NULL ) ;

	// Edge case: Wrong password
	lSession = MVTApp::startSession(mStoreCtx, info.str, "Let me in" );
	TVERIFY( lSession == NULL ) ;

	// Start a session with identity right off the bat
	lSession = MVTApp::startSession(mStoreCtx, info.str, info.pwd.c_str() );
	TVERIFY( lSession != NULL ) ;

	TVERIFY( lSession->getCurrentIdentityID() == info.id ) ;

	size_t cntIDName = strlen( info.str ) ;
	char identityNameLookup[255] ;
	
	TVERIFY( cntIDName == lSession->getIdentityName( info.id, identityNameLookup, 255 ) ) ;
	TVERIFY( 0 == strcmp( identityNameLookup, info.str ) ) ; 
	
	// Edge case - buffer too small for name
	// It will return a truncated string so the caller needs to be careful to 
	// realise that the length returned is bigger than the buffer sent in.
	size_t cnt = lSession->getIdentityName( info.id, identityNameLookup, 3 ) ;
	TVERIFY( cnt == cntIDName  ) ;

	// REVIEW: An identity can't change its own password, only the mvstore owner can
	// (this testing should go elsewhere)
	TVERIFY( RC_NOACCESS == lSession->changePassword( info.id, info.pwd.c_str(), "New Password" ) );

	lSession->terminate() ;
}

void TestIdentity::testPinCreation( IdentityInfo & info, bool inExpectPinCreation )
{
	ISession * lSession = MVTApp::startSession(mStoreCtx, info.str, info.pwd.c_str() );
	TVERIFY( lSession != NULL ) ;

	// Note: A different identity can be given permission to read and write existing PINs.
	// That is covered in the ACL tests.

	if ( inExpectPinCreation)
	{
		// You can create a PIN, which belongs to the owner
		// not the current identity
		PID newPIN ;
		TVERIFYRC( lSession->createPIN( newPIN, NULL, 0, 0 )) ;
		TVERIFY( newPIN.ident == STORE_OWNER ) ;

		// But you can't look it up (because of normal ACL)
		IPIN * pin = lSession->getPIN( newPIN ) ;
		TVERIFY( pin == NULL ) ;

		// This way works also 
		IPIN * newPin = lSession->createUncommittedPIN() ;
		TVERIFYRC( lSession->commitPINs( &newPin, 1, 0 ) );
		newPin->destroy() ;
	}
	else
	{
		// Pin Creation blocked
		PID newPIN ;
		TVERIFY( RC_NOACCESS == lSession->createPIN( newPIN, NULL, 0, 0 ) ) ;
		
		IPIN * newPin = lSession->createUncommittedPIN() ;
		TVERIFY( RC_NOACCESS == lSession->commitPINs( &newPin, 1, 0 ) );
		newPin->destroy() ;
	}

	lSession->terminate() ;
}

void TestIdentity::FindIDInDump( ISession * inSession, IdentityInfo & info )
{
#if 0
	// Use DumpStore to enumerate identities and find the one
	// created by this test

	CmvautoPtr<IDumpStore> dump ;
	TVERIFYRC(inSession->dumpStore(dump.Get(),false)) ;

	bool bFound = false ;
	while(1)
	{
		IdentityID id = STORE_INVALID_IDENTITY ;
		char * identName = NULL ;
		unsigned char * pwd = NULL ;
		bool mayInsert = false ;
		unsigned char *cert = NULL ;
		size_t lcert = 0 ;
		size_t lpwd = 0 ;

		RC rc = dump->getNextIdentity( id, identName, pwd, lpwd, mayInsert, cert, lcert ) ;

		if ( rc != RC_OK )
			break ;

		if ( id == info.id )
		{
			// Found it, now verify
			TVERIFY( !bFound) ;

			bFound = true ;
			TVERIFY( 0 == strcmp( identName, info.str ) ) ;

			if ( pwd != NULL )
			{
				// Password returned is actually the encrypted password, (which
				// can be used to recreate the identity with ISession::loadIdentity)
				// it is a different size and content than the original password
				TVERIFY( lpwd > 0 ) ;
			}
			else
			{
				TVERIFY( info.pwd.empty() ) ;
				TVERIFY( lpwd == 0 ) ;
			}
		}

		inSession->free( identName ) ;
		inSession->free( pwd ) ;
		inSession->free( cert ) ;
	}

	TVERIFY(bFound) ;
#endif
}
