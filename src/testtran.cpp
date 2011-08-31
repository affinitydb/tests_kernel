/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"
using namespace std;
using namespace MVStoreKernel;

// Note: for coverage of collections with transactions check out testtrancoll.cpp

#define TESTISOLATION 0

#define TEST_DEADLOCK_DETECTION 1

#define DEADLOCK_LOSER_DOES_ROLLBACK 0 //TDB for correct client code handling deadlock

#define SINGLE_THREAD_DEADLOCK 0 // deadlock detection can't fix this because single thread locks itself
					// It can be useful for testing frozen stores

struct SessionHolder
{
	// Represents a session and a specific PID inside that session
	// The TransactionMultiSessionTester holder takes
	// care of the actual initialization and lifetime of the
	// session and pin, because this must be coordinated to
	// only have a single session alive at a time (and hence
	// difficult to wrap in independent class)

	~SessionHolder()
	{
		assert( mSession == NULL ) ;
		assert( mPins.empty() ) ; // DestroyPins should be called before session gone
	}

	void DestroyPins()
	{
		size_t i ;
		for ( i = 0 ; i < mPins.size() ; i++ )
		{
			mPins[i]->destroy() ;
		}
		mPins.clear() ;
	}

	ISession * mSession ;
	vector<IPIN *> mPins ;  // Pins of interest for the session
} ;

class TransactionMultiSessionTester
{
	// Holds two sessions, both interested in the same PIN
	// Automatically switches between sessions so that you 
	// can use both in the same thread
	// (could be generalized to multiple sessions or more pins)

	// Tip: See PSSessionMan in mvstoreex for production quality 
	// multi-session management code
public:
	TransactionMultiSessionTester( PID * inPids, size_t cntPins ) 
	{
		// We assume that any other session has already 
		// called detachFromCurrent.
			
		ms1.mSession = MVTApp::startSession();

		// Both sessions will look at the same PIDs
		size_t i ;
		for ( i = 0 ; i < cntPins ; i++ )
		{
			ms1.mPins.push_back( ms1.mSession->getPIN(inPids[i]) );
		}
		ms1.mSession->detachFromCurrentThread() ;

		ms2.mSession = MVTApp::startSession();
		for ( i = 0 ; i < cntPins ; i++ )
		{
			ms2.mPins.push_back( ms2.mSession->getPIN(inPids[i]) );
		}
		ms2.mSession->detachFromCurrentThread() ;

		// NOTE: before creation of this object any existing session must detach!
		mcurrent = &ms1 ;
		mcurrent->mSession->attachToCurrentThread() ;
	}

	~TransactionMultiSessionTester()
	{
		assert( ms1.mSession == NULL ) ; // Missed call to cleanup
		assert( ms2.mSession == NULL ) ;
	}

	SessionHolder * S1()
	{
		if ( mcurrent != &ms1 )
			if ( Swap() != RC_OK ) return NULL ;
		return &ms1 ;
	}

	SessionHolder * S2()
	{
		if ( mcurrent != &ms2 )
			if ( Swap() != RC_OK ) return NULL ;
		return &ms2 ;
	}

	RC Swap()
	{
		RC rc = mcurrent->mSession->detachFromCurrentThread() ;

		if ( rc != RC_OK ) return rc ;

		if ( mcurrent == &ms1 ) 
			mcurrent = &ms2 ;
		else 
			mcurrent = &ms1 ;

		return mcurrent->mSession->attachToCurrentThread() ;
	}

	void Cleanup()
	{
		// Assumes only ms1 or ms2 is currently attached (not a 3rd session)

		// Make sure ms1 is attached
		if ( mcurrent != &ms1 ) 
			Swap() ;

		// Order of cleanup must be very precise.
		// All pins belonging to a certain session must be destroyed
		// with the correct session active.  The rules are most strictly
		// enforced when IPC is active. (#4815)
	
		ms1.DestroyPins() ;
		ms1.mSession->terminate() ; // I guess this implies an automatic "detach"
		ms1.mSession = NULL ;

		// Now who is attached?

		ms2.mSession->attachToCurrentThread() ;
		ms2.DestroyPins();
		ms2.mSession->terminate() ;
		ms2.mSession = NULL ;

		// Now presumably no thread is attached
	}

private:
	SessionHolder * mcurrent ;

	SessionHolder ms1 ;
	SessionHolder ms2 ;

	TransactionMultiSessionTester() ;
} ;

// Publish this test.
class TestTransactions : public ITest
{
		MVStoreKernel::StoreCtx *mStoreCtx;
	public:
		TEST_DECLARE(TestTransactions);
		virtual char const * getName() const { return "testtran"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of transactions / nested transactions"; }
		virtual bool isPerformingFullScanQueries() const { return true; }

		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void populateStore(ISession *session,URIMap *pm,int npm, PID *pid);
		void simpleTran(ISession *session);
		void pinTransactions(ISession *session) ;
		void simplenestedTran(ISession *session);
		void nestedTran(ISession *session); // TODO add more scenarios
		void multisessionTrans(ISession * session) ;
		void multisessionDeadlock(ISession * session) ;
		void multithreadDeadlock(ISession * session) ;
		void safePinUpdates(ISession * session) ;


		// Helpers
		int findPIN(const string & str, unsigned int propid,ISession *session);
		void VerifyExpected( ISession* session,URIMap *pm,const PID & pid, const char* prop0, const char* prop1, const char* prop2 ) ;
		void SetInitial( ISession* session,URIMap *pm,PID & pid, const char* prop0, const char* prop1, const char* prop2 ) ;

		// For code readability give symbolic names for the Property
		// indexes.  These could be "city,firstname, etc" but that 
		// meaning of property doesn't really matter
		enum PropIDs
		{
			Prop0,
			Prop1,
			Prop2,
			Prop3,
			Prop4,
			Prop5,
			CountProps
		} ;
};
TEST_IMPLEMENT(TestTransactions, TestLogger::kDStdOut);

int TestTransactions::execute()
{
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		mStoreCtx = MVTApp::getStoreCtx();

		simpleTran(session);
		pinTransactions(session) ;

		simplenestedTran(session);
		nestedTran(session);

		multisessionTrans(session) ;
#if TEST_DEADLOCK_DETECTION
		multithreadDeadlock(session) ;  // Deadlock detection should fix this
#endif

#if SINGLE_THREAD_DEADLOCK
		multisessionDeadlock(session) ; // Deadlock detection won't help	
#endif

		safePinUpdates(session);

		session->terminate();
		MVTApp::stopStore();
	}

	else { TVERIFY(!"Unable to start store"); }
	return RC_OK  ;
}

void TestTransactions::simpleTran(ISession *session)
{
	//create a simple pin and modify with tran.
	PID pid;
	Value pvs[2];
	Value newVal ; // For changing property values

	PropertyID propID[5];
	MVTApp::mapURIs(session,"TestTransactions.simpleTran.1",5,propID);
	unsigned npidslatest,npidsorig;

	// Create initial PIN
	SETVALUE(pvs[0], propID[0], "Tran1 prop", OP_SET);
	pvs[1].setURL("http://www.cricinfo.com"); SETVATTR(pvs[1], propID[1], OP_SET);
	TVERIFYRC( session->createPIN(pid,pvs,2) );

	CmvautoPtr<IPIN> pin( session->getPIN(pid) );
	if (isVerbose()) MVTApp::output(*pin.Get(), mLogger.out(), session);

	memset(pvs,0,2*sizeof(Value));

	//Case 1: simple commit of modification to an existing property
	pvs[0].setURL("http://www.google.com"); SETVATTR(pvs[0], propID[1], OP_SET);
	//start trans.
	TVERIFYRC(session->startTransaction());
		TVERIFYRC(pin->modify(pvs,1));
	TVERIFYRC2(session->commit(), "****Case 1(commit)" );

	TVERIFYRC(pin->refresh());;
	TVERIFY( 0 == strcmp( pin->getValue( propID[1] )->str, "http://www.google.com" ) ) ;

	if (isVerbose()) MVTApp::output(*pin.Get(), mLogger.out(), session);

	//case 2: simple rollback of modification to existing Property
	newVal.setURL("http://fantasy.premierleague.com"); SETVATTR(newVal, propID[1], OP_SET);
	//start trans.
	TVERIFYRC(session->startTransaction());
		TVERIFYRC(pin->modify(&newVal,1));
	TVERIFYRC2(session->rollback(),"****Case 2(rollback)");

	// pin has snapshot so it does not initially see the result of the rollback
	TVERIFY( 0 == strcmp( pin->getValue( propID[1] )->str, "http://fantasy.premierleague.com" ) ) ;
	TVERIFYRC(pin->refresh());; 
	if (isVerbose()) MVTApp::output(*pin.Get(), mLogger.out(), session);
	TVERIFY( 0 == strcmp( pin->getValue( propID[1] )->str, "http://www.google.com" ) ) ;

	//case 3: adding a new prop within a transaction
	npidsorig = pin->getNumberOfProperties();	
	SETVALUE(newVal, propID[2], 10000, OP_SET);
	TVERIFYRC(session->startTransaction());
		pin->modify(&newVal,1);
	TVERIFYRC2(session->commit(),"****Case 3(commit)");
		
	npidslatest = pin->getNumberOfProperties();
	TVERIFY2( npidslatest == npidsorig + 1, "****Case 3(add prop)" ) ;
	TVERIFY( pin->getValue( propID[2] )->i == 10000 ) ;
	if (isVerbose()) MVTApp::output(*pin.Get(), mLogger.out(), session);

	//case 4: reverting a new prop within a transaction
	npidsorig = pin->getNumberOfProperties();	
	SETVALUE(newVal, propID[3], 10000, OP_SET);
	TVERIFYRC(session->startTransaction());
		TVERIFYRC(pin->modify(pvs,1));
	TVERIFYRC(session->rollback());		
	TVERIFY( pin->getNumberOfProperties() == npidsorig) ;
	TVERIFY( pin->getValue( propID[3] ) == NULL ) ;

	// (note - some collection related coverage was moved to testtrancoll.cpp)

	//case 5: deleting a property and rollback.
	
	// Set expected value
		SETVALUE(newVal, propID[1], 789, OP_SET);
		TVERIFYRC(pin->modify( &newVal,1));

		npidsorig = pin->getNumberOfProperties();
	TVERIFYRC(session->startTransaction());
		newVal.setDelete(propID[1]);
		TVERIFYRC(pin->modify(&newVal,1));
		if (isVerbose()) MVTApp::output(*pin.Get(), mLogger.out(), session);
	TVERIFYRC(session->rollback());
		TVERIFYRC(pin->refresh());
		npidslatest = pin->getNumberOfProperties();
		if (isVerbose()) MVTApp::output(*pin.Get(), mLogger.out(), session);

		TVERIFY2( npidslatest == npidsorig,"****Case 5(setDelete) ");
		TVERIFY( pin->getValue(propID[1])->i == 789 ) ;

#if 0 // REVIEW - why was this commented out?
	//case 7: add a couple of props and commit or rollback
	TVERIFYRC(session->startTransaction());
		    pvs[0].set("Test prop 1");pvs[0].setPropID(propID[3]);
		    pvs[1].set("Test prop 2");pvs[1].setPropID(propID[4]);
		    TVERIFYRC(pin->modify(pvs,2));
	TVERIFY2(session->rollback(),"****Case 7(rollback) ");

	MVTApp::output(*pin.Get(), mLogger.out(), session);
#endif

	//case 8: Roll back of value added through ISession::modifyPIN
	PID pid2 ;
	pvs[0].set("Modify PIN check");pvs[0].setPropID(propID[3]);
	TVERIFYRC(session->createPIN(pid2,pvs,1));

	// Verify that it shows up in query
	TVERIFY2( 1 == findPIN("Modify PIN check",propID[3],session), "findPIN string query problem" );

	pvs[0].set("Modification string");pvs[0].setPropID(propID[4]);
	TVERIFYRC(session->startTransaction());
		TVERIFYRC(session->modifyPIN(pid2,pvs,1));
	TVERIFYRC(session->rollback());
	
	TVERIFY2(0 == findPIN("Modification string",propID[4],session),"****Case 8(Session modify PIN) ");
	
	// First property set should still be around for Query
	TVERIFY( 1 == findPIN("Modify PIN check",propID[3],session) );

	// Verify other property available in direct
	CmvautoPtr<IPIN> pid2Lookup( session->getPIN( pid2 ) );
	TVERIFY( 0 == strcmp( "Modify PIN check", pid2Lookup->getValue(propID[3])->str )  ) ;
	if (isVerbose()) MVTApp::output(*pid2Lookup.Get(), mLogger.out(), session);
}

void TestTransactions::pinTransactions(ISession *session)
{
	// Creation and deletion of pins within transactions

	//create a simple pin and modify with tran.
	PID pid, pid2;
	Value pvs[2];	
	RC rc;
	PropertyID lPropIDs[5];
	MVTApp::mapURIs(session,"TestTransactions.simpleTran.2",5,lPropIDs);
	int cntBefore,cntAfter;

	//Perparation (in case of any existing data in the store)
	cntBefore = findPIN("This pin is getting slaughtered",lPropIDs[0],session);

	//case 1: delete a PIN (without purge and rollback).
	pvs[0].set("This pin is getting slaughtered");pvs[0].setPropID(lPropIDs[0]);
	pvs[1].set("This PIN wont be purged");pvs[1].setPropID(lPropIDs[1]);
	TVERIFYRC(session->createPIN(pid,pvs,2));

	TVERIFYRC(session->startTransaction());
		TVERIFYRC(session->deletePINs(&pid,1));

		// prove that PIN is gone
		TVERIFY(session->getPIN(pid)==NULL) ;
		TVERIFY(0 == findPIN("This pin is getting slaughtered",lPropIDs[0],session)) ;

	TVERIFYRC(session->rollback());
	
	// Make sure PIN was restored
	CmvautoPtr<IPIN> pinRecovered( session->getPIN(pid) ) ;
	TVERIFY( pinRecovered.IsValid() ) ;
	TVERIFY( 0 == strcmp(pinRecovered->getValue(lPropIDs[0])->str,"This pin is getting slaughtered") ) ;

	//Also make sure pin appears (once) in query 
	cntAfter = findPIN("This pin is getting slaughtered",lPropIDs[0],session);
	TVERIFY2(cntAfter == cntBefore + 1, "****Case 8(Delete PIN) ");

	//case 9: Delete PIN with a purge and rollback.
	cntBefore = findPIN("Purge Me please",lPropIDs[2],session);

	pvs[0].set("Purge Me please");pvs[0].setPropID(lPropIDs[2]);
	pvs[1].set("Under the effect of a transaction");pvs[1].setPropID(lPropIDs[3]);
	TVERIFYRC( session->createPIN(pid2,pvs,2) );

	TVERIFYRC(session->startTransaction());
		TVERIFYRC(session->deletePINs(&pid2,1,MODE_PURGE));
	TVERIFYRC(session->rollback());

	pinRecovered.Attach( session->getPIN(pid2) ) ;
	TVERIFY( pinRecovered.IsValid() ) ;
	TVERIFY( 0 == strcmp(pinRecovered->getValue(lPropIDs[2])->str,"Purge Me please") ) ;

	cntAfter = findPIN("Purge Me please",lPropIDs[2],session);
	TVERIFY2(cntAfter == cntBefore + 1,"****Case 9(Delete PIN with purge) ");

	//case 10: create a pin in transaction and rollback and check
	pid.pid = STORE_INVALID_PID ;
	TVERIFYRC(session->startTransaction());
		pvs[0].set("One property PIN");pvs[0].setPropID(lPropIDs[2]);
		rc = session->createPIN(pid,pvs,1);
	TVERIFYRC(session->rollback());
	TVERIFY(session->getPIN(pid)==NULL) ;	
	cntAfter = findPIN("One property PIN",lPropIDs[2],session);
	TVERIFY2(cntAfter == 0,"****Case 10(Create PIN) ");

	//case 10.5: create a pin in transaction and rollback and check
	//(See BUG 619 - previously didn't work if multiple properties)
	pid.pid = STORE_INVALID_PID ;
	TVERIFYRC(session->startTransaction());
		pvs[0].set("Prop1");pvs[0].setPropID(lPropIDs[2]);
		pvs[1].set(46);pvs[1].setPropID(lPropIDs[3]);
		TVERIFYRC(session->createPIN(pid,pvs,2));
	TVERIFYRC(session->rollback());
	TVERIFY(session->getPIN(pid)==NULL) ;	
	cntAfter = findPIN("Prop1",lPropIDs[2],session);
	TVERIFY2(cntAfter == 0,"****Case 10.5(Create PIN) ");
}

void TestTransactions::simplenestedTran(ISession *session)
{
	// Create properties and pins for use by these scenarios
	URIMap pm[CountProps];
	PID pid[5];
	populateStore(session,pm,CountProps,pid);

	Value pvs[3];

	CmvautoPtr<IPIN> pin(session->getPIN(pid[0]));
	TVERIFY( pin.IsValid() ) ;

	//case1: simple nested tran on multiple props in pin(non coll)--commit(all)
	
	TVERIFYRC(session->startTransaction());{
		SETVALUE(pvs[0], pm[Prop0].uid, "changed01", OP_SET);
		TVERIFYRC(pin->modify(pvs,1));

		TVERIFYRC(session->startTransaction());{
			SETVALUE(pvs[0], pm[Prop1].uid, "changed02", OP_SET);
			TVERIFYRC(pin->modify(pvs,1));

			TVERIFYRC(session->startTransaction());{
				SETVALUE(pvs[0], pm[Prop2].uid, "changed03", OP_SET);
				TVERIFYRC(pin->modify(pvs,1));
			}
		}
	}
	TVERIFYRC(session->commit(true /*all transactions*/)); 
	TVERIFYRC( pin->refresh()) ;
	TVERIFY( 0 == strcmp(pin->getValue(pm[Prop0].uid)->str, "changed01") ) ;
	TVERIFY( 0 == strcmp(pin->getValue(pm[Prop1].uid)->str, "changed02") ) ;
	TVERIFY( 0 == strcmp(pin->getValue(pm[Prop2].uid)->str, "changed03") ) ;
	if (isVerbose())MVTApp::output(*pin.Get(), mLogger.out(), session);

	//case2: nested tran on multiple props in pin(non coll)--rollback(all) 
	pin.Attach(session->getPIN(pid[1]));

	Value initialVals[4] ;
	SETVALUE(initialVals[0], pm[Prop0].uid, "timbuktu", OP_SET);//city
	SETVALUE(initialVals[1], pm[Prop1].uid, "Ray", OP_SET);
	SETVALUE(initialVals[2], pm[Prop2].uid, "Charles", OP_SET);
	SETVALUE(initialVals[3], pm[Prop5].uid, "Somestring", OP_SET);
	TVERIFYRC(pin->modify(initialVals,4));

	TVERIFYRC(session->startTransaction());{
		SETVALUE(pvs[0], pm[Prop0].uid, "changed01", OP_SET);//city
		TVERIFYRC(pin->modify(pvs,1));
		
		TVERIFYRC(session->startTransaction());{
			SETVALUE(pvs[0], pm[Prop1].uid, "changed04", OP_SET);//firstname
			TVERIFYRC(pin->modify(pvs,1));
						
			TVERIFYRC(session->startTransaction());{
				SETVALUE(pvs[0], pm[Prop2].uid, "changed05", OP_SET);//prop2
				TVERIFYRC(pin->modify(pvs,1));
								
				TVERIFYRC(session->startTransaction());{
					SETVALUE(pvs[0], pm[Prop5].uid, "changed08", OP_SET);//somestring
					TVERIFYRC(pin->modify(pvs,1));
 				}
			}
		}
	}
	TVERIFYRC(session->rollback(true));
	TVERIFYRC(pin->refresh()); 
	TVERIFY( 0 == strcmp(pin->getValue(pm[Prop0].uid)->str, "timbuktu") ) ;
	TVERIFY( 0 == strcmp(pin->getValue(pm[Prop1].uid)->str, "Ray") ) ;
	TVERIFY( 0 == strcmp(pin->getValue(pm[Prop2].uid)->str, "Charles") ) ;
	TVERIFY( 0 == strcmp(pin->getValue(pm[Prop5].uid)->str, "Somestring") ) ;

	if (isVerbose()) MVTApp::output(*pin.Get(), mLogger.out(), session);

	//case 3: nested tran with multiple commits and rollbacks on single pins

	PID case3pid ;
	SetInitial( session,pm,case3pid, "Goa", "Chuck", "Cheese" ) ;
	pin.Attach(session->getPIN(case3pid));

	// this helper will verify what is currently set in the pin
	VerifyExpected( session, pm,case3pid,"Goa", "Chuck", "Cheese" ) ;

	TVERIFYRC(session->startTransaction());{
		SETVALUE(pvs[0], pm[Prop0].uid, "chcity TX1", OP_SET);
		SETVALUE(pvs[1], pm[Prop1].uid, "chname TX1", OP_SET);
		TVERIFYRC(pin->modify(pvs,2));//commit/rollback at end

		VerifyExpected( session, pm,case3pid,"chcity TX1", "chname TX1", "Cheese" ) ;

		TVERIFYRC(session->startTransaction());{
			SETVALUE(pvs[0], pm[Prop0].uid, "chcity TX2", OP_SET);
			SETVALUE(pvs[1], pm[Prop1].uid, "chname TX2", OP_SET);
			TVERIFYRC(pin->modify(pvs,2));//commited later

			VerifyExpected( session, pm,case3pid,"chcity TX2", "chname TX2", "Cheese" ) ;

			TVERIFYRC(session->startTransaction());{
				SETVALUE(pvs[0], pm[Prop2].uid, "chlname TX3", OP_SET);
				SETVALUE(pvs[1], pm[Prop1].uid, "chname TX3", OP_SET);
				TVERIFYRC(pin->modify(pvs,2));

				VerifyExpected( session, pm,case3pid,"chcity TX2", "chname TX3", "chlname TX3" ) ;

			} TVERIFYRC(session->rollback());//blow away TX3. (this rolls back TX2 also)

			VerifyExpected( session, pm,case3pid,"chcity TX2", "chname TX2", "Cheese" ) ;

			TVERIFYRC(session->startTransaction());{
				SETVALUE(pvs[0], pm[Prop2].uid, "chlname TX4", OP_SET);
				TVERIFYRC(pin->modify(pvs,1));
			} TVERIFYRC(session->commit());//commit TX4

			VerifyExpected( session, pm,case3pid,"chcity TX2", "chname TX2", "chlname TX4" ) ;

			TVERIFYRC(session->startTransaction());{
				SETVALUE(pvs[0], pm[Prop0].uid, "chcity TX5", OP_SET);
				TVERIFYRC(pin->modify(pvs,1));
			} TVERIFYRC(session->rollback()); //blow away TX5

			VerifyExpected( session, pm,case3pid,"chcity TX2", "chname TX2", "chlname TX4" ) ;

			TVERIFYRC(session->startTransaction());{
				SETVALUE(pvs[0], pm[Prop0].uid, "chfname TX6", OP_SET);
				TVERIFYRC(pin->modify(pvs,1));
			}TVERIFYRC(session->commit());//commit TX6

			VerifyExpected( session, pm,case3pid,"chfname TX6", "chname TX2", "chlname TX4" ) ;
		}TVERIFYRC(session->commit()); 
		TVERIFYRC(pin->refresh());
		if (isVerbose()) MVTApp::output(*pin.Get(), mLogger.out(), session);
	}TVERIFYRC(session->commit()); //outermost tran
	//does not matter if rollback/commit is true or false.
	TVERIFYRC(pin->refresh());
	if (isVerbose()) MVTApp::output(*pin.Get(), mLogger.out(), session);

	//case 4: interspersed commits and rollbacks.
	PID case4pid ;
	SetInitial( session,pm,case4pid, "Paris", "Guy", "Leduc" ) ;
	pin.Attach(session->getPIN(case4pid));

	TVERIFYRC(session->startTransaction()); {
		SETVALUE(pvs[0], pm[Prop0].uid, "City Case4 TX1", OP_SET);
		SETVALUE(pvs[1], pm[Prop1].uid, "Fname case 4 tx1", OP_SET);
		TVERIFYRC(pin->modify(pvs,2));

		VerifyExpected( session, pm,case4pid,"City Case4 TX1", "Fname case 4 tx1", "Leduc" ) ;

		TVERIFYRC(session->startTransaction()); {
			SETVALUE(pvs[0], pm[Prop2].uid, "lName Case4 TX2", OP_SET);
			SETVALUE(pvs[1], pm[Prop3].uid, "pincode case 4 tx2", OP_SET);
			TVERIFYRC(pin->modify(pvs,2));

			// (not checking pincode)
			VerifyExpected( session, pm,case4pid,"City Case4 TX1", "Fname case 4 tx1", "lName Case4 TX2" ) ;

			TVERIFYRC(session->startTransaction()); {
				SETVALUE(pvs[0], pm[Prop4].uid, 3, OP_SET);
				SETVALUE(pvs[1], pm[Prop3].uid, "somestring case 4 tx3", OP_SET);
				TVERIFYRC(pin->modify(pvs,2));
			}TVERIFYRC(session->rollback());//remove this rollback and replace w/ commit thn wrks fine.
			
			TVERIFYRC(session->startTransaction()); {
				//SETVALUE(pvs[0], pm[Prop0].uid, "City Case4 TX4", OP_SET);
				//SETVALUE(pvs[1], pm[Prop1].uid, "Fname case 4 tx4", OP_SET);
				//REVIEW: (no modify call here)
			}TVERIFYRC(session->commit());

			VerifyExpected( session, pm,case4pid,"City Case4 TX1", "Fname case 4 tx1", "lName Case4 TX2" ) ;

		}TVERIFYRC(session->rollback()); //rollback till tx2 but it does not. Goes only one level up.

		VerifyExpected( session, pm,case4pid,"City Case4 TX1", "Fname case 4 tx1", "Leduc" ) ;
	}TVERIFYRC(session->commit()); //commit only tx1.

	VerifyExpected( session, pm,case4pid,"City Case4 TX1", "Fname case 4 tx1", "Leduc" ) ;

	TVERIFYRC(pin->refresh());;
	if (isVerbose()) MVTApp::output(*pin.Get(), mLogger.out(), session);
}

void TestTransactions::nestedTran(ISession *session)
{
	// Double roll back with two pins

	URIMap pm[CountProps] ;
	MVTApp::mapURIs(session,"TestTransactions.nestedTran",CountProps,pm);

	PID pid1 ;
	SetInitial( session,pm,pid1, "A", "B", "C" ) ;
	CmvautoPtr<IPIN> pin1(session->getPIN(pid1));

	PID pid2 ;
	SetInitial( session,pm,pid2, "1", "2", "3" ) ;
	CmvautoPtr<IPIN> pin2(session->getPIN(pid2));

	Value pvs[3] ;

	TVERIFYRC(session->startTransaction());{
		SETVALUE(pvs[0], pm[Prop0].uid, "D", OP_SET);
		SETVALUE(pvs[1], pm[Prop1].uid, "E", OP_SET);
		TVERIFYRC(pin1->modify(pvs,2));

		SETVALUE(pvs[0], pm[Prop0].uid, "4", OP_SET);
		SETVALUE(pvs[1], pm[Prop2].uid, "5", OP_SET);
		TVERIFYRC(pin2->modify(pvs,2));

		VerifyExpected( session, pm,pid1,"D", "E", "C" ) ;
		VerifyExpected( session, pm,pid2,"4", "2", "5" ) ;

		TVERIFYRC(session->startTransaction());{
			SETVALUE(pvs[0], pm[Prop1].uid, "F", OP_SET);
			SETVALUE(pvs[1], pm[Prop2].uid, "G", OP_SET);
			TVERIFYRC(pin1->modify(pvs,2));//commit/rollback at end

			SETVALUE(pvs[0], pm[Prop0].uid, "7", OP_SET);
			SETVALUE(pvs[1], pm[Prop1].uid, "8", OP_SET);
			SETVALUE(pvs[2], pm[Prop2].uid, "9", OP_SET);
			TVERIFYRC(pin2->modify(pvs,3));//commit/rollback at end

			VerifyExpected( session, pm,pid1,"D", "F", "G" ) ;
			VerifyExpected( session, pm,pid2,"7", "8", "9" ) ;
		}TVERIFYRC(session->rollback());

		VerifyExpected( session, pm,pid1,"D", "E", "C" ) ;
		VerifyExpected( session, pm,pid2,"4", "2", "5" ) ;
	}TVERIFYRC(session->rollback());

	VerifyExpected( session, pm,pid1,"A", "B", "C" ) ;
	VerifyExpected( session, pm,pid2,"1", "2", "3" ) ;
}

//temp here till this is made availble in app.h
int TestTransactions::findPIN(const string& str, unsigned int propid,ISession *session)
{
	// Find the number of PINs that have a certain string as the value
	// of the property specified by propid.

	IStmt *query = session->createStmt();
	unsigned var = query->addVariable();
	Value op[2];
	PropertyID prop = propid;
	int count = 0;
	IPIN * pin;

	op[0].setVarRef(0,prop);
	op[1].set(str.c_str());

	IExprTree *exprfinal = session->expr(OP_EQ,2,op);
	query->addCondition(var,exprfinal);

	// Direct way to get the count
	uint64_t count2 ;
	TVERIFYRC(query->count( count2 ) );

	// Long way to get the number of matches

	ICursor *result = NULL;
	TVERIFYRC(query->execute(&result));
	
	for ( ; (pin=result->next())!=NULL; ) {
		count++; 
		if ( isVerbose() ) MVTApp::output(*pin, mLogger.out(), session);		
		pin->destroy() ;
	}
	result->destroy();
	exprfinal->destroy();
	query->destroy() ;

	TVERIFY( count2 == uint64_t(count) ) ;

	return count;
}

void TestTransactions::populateStore(ISession *session,URIMap *pm,int npm, PID *pid)
{
	// REVIEW: entire function could probably be removed.  Originally it was setting
	// actual property values but these weren't being explicitly verified anywhere 
	//(other than print statements)
	
	// arguments
	// session
	// pm - filled in with newly generated URI and PropIDs 
	// npm - number of items in the PM
	// pid - filled in with the new PINs added to store

	TVERIFY( npm == CountProps ) ;

	// TODO: randomize this + provide as a service in app.h

	MVTApp::mapURIs(session,"TestTransactions.prop",npm,pm);

	session->createPIN(pid[0],NULL,0);
	session->createPIN(pid[1],NULL,0);
	session->createPIN(pid[2],NULL,0);
	session->createPIN(pid[3],NULL,0);
	session->createPIN(pid[4],NULL,0);
}

void TestTransactions::SetInitial( ISession* session, URIMap *pm, PID & outpid, const char* prop0, const char* prop1, const char* prop2 )
{
	// Create a PIN with some specific values for the properties
	Value pvs[3];
	SETVALUE(pvs[0], pm[Prop0].uid, prop0, OP_SET);
	SETVALUE(pvs[1], pm[Prop1].uid, prop1, OP_SET);
	SETVALUE(pvs[2], pm[Prop2].uid, prop2, OP_SET);

	session->createPIN(outpid,pvs,3);
}

void TestTransactions::VerifyExpected( ISession* session, URIMap *pm, const PID & pid, const char* prop0, const char* prop1, const char* prop2 )
{
	CmvautoPtr<IPIN> pin( session->getPIN( pid ) ) ;

	TVERIFY( 0 == strcmp(pin->getValue(pm[Prop0].uid)->str, prop0) ) ;
	TVERIFY( 0 == strcmp(pin->getValue(pm[Prop1].uid)->str, prop1) ) ;
	TVERIFY( 0 == strcmp(pin->getValue(pm[Prop2].uid)->str, prop2) ) ;
}

void TestTransactions::multisessionTrans(ISession * session)
{
	// This test mostly focuses on how two separate PIN snapshots 
	// behave as a SINGLE PIN is modified from two sessions
	RC rc ;

	PID pid ;
	TVERIFYRC(session->createPIN(pid,NULL,0));

	PropertyID propids[5] ; 
	MVTApp::mapURIs(session,"TestTransactions.multisessionTrans",sizeof(propids)/sizeof(propids[0]),propids);

	TVERIFYRC(session->detachFromCurrentThread()) ;
	
	TransactionMultiSessionTester tester(&pid,1) ;

	TVERIFY(tester.S1()->mSession != NULL) ;
	TVERIFY(tester.S2()->mSession != NULL) ;

	Value val ;
	const Value * vallookup = NULL ;

	// Case 1: One session adds property

	val.set("WideStr") ; val.property = propids[0] ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;

	// The different sessions have their own snapshots of the PIN
	TVERIFY( tester.S2()->mPins[0]->getValue(propids[0]) == NULL ) ;

	// Refresh will bring them in sync
	TVERIFYRC(tester.S2()->mPins[0]->refresh() ) ; 
	vallookup = tester.S2()->mPins[0]->getValue(propids[0])  ;
	TVERIFY( 0 == strcmp(vallookup->str,"WideStr") ) ;
	
	// Case 2: One session removes property
	val.setDelete(propids[0]) ;
	TVERIFYRC(tester.S2()->mPins[0]->modify(&val,1)) ;

	// Again you must refresh to see the change
	TVERIFY( tester.S1()->mPins[0]->getValue(propids[0]) != NULL ) ;
	TVERIFYRC(tester.S1()->mPins[0]->refresh() ) ; 	
	TVERIFY( tester.S1()->mPins[0]->getValue(propids[0]) == NULL ) ;

	// Case 3: Both sessions add separate properties

	val.set(999) ; val.property = propids[1] ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;

	val.set(19.9) ; val.property = propids[2] ;
	TVERIFYRC(tester.S2()->mPins[0]->modify(&val,1,0)) ;

	// Now this is the interesting thing, the modify has done an automatic refresh
	// of the pin and it sees the other session.  The other modify is completely
	// finished its transaction so this makes sense
	TVERIFY( tester.S2()->mPins[0]->getValue(propids[1])->i == 999 ) ;

	// Similarily, Session 1 doesn't see the new property from Session 2 until
	// it does another modify
	TVERIFY( tester.S1()->mPins[0]->getValue(propids[2]) == NULL ) ;
	val.set(888) ; val.property = propids[1] ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;
	TVERIFY( tester.S1()->mPins[0]->getValue(propids[2])->d == 19.9 ) ;

	// Case 4: Both sessions remove properties
	// (propids[1] and propids[2] were set by the previous test)

	val.setDelete(propids[2]) ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;

	val.setDelete(propids[1]) ;
	TVERIFYRC(tester.S2()->mPins[0]->modify(&val,1,0)) ;

	// Modify has removed propids[2] for session 2
	TVERIFY( tester.S2()->mPins[0]->getValue(propids[2]) == NULL ) ;

	// Session 1 still sees propids[1] until it does a refresh or modify
	TVERIFY( tester.S1()->mPins[0]->getValue(propids[1])->i == 888) ;
	TVERIFYRC( tester.S1()->mPins[0]->refresh() ) ;
	TVERIFY( tester.S1()->mPins[0]->getValue(propids[1]) == NULL ) ;

	// Case 5: Both sessions change the same property

	val.set(101) ; val.property = propids[1] ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;

	val.set(202) ; val.property = propids[1] ;
	TVERIFYRC(tester.S2()->mPins[0]->modify(&val,1,0)) ;

	TVERIFYRC( tester.S1()->mPins[0]->refresh() ) ;
	TVERIFY( tester.S1()->mPins[0]->getValue(propids[1])->i == 202 ) ;

	// Case 6: Both sessions try to delete the same property

	val.setDelete(propids[1]) ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;

	// S2 snapshot has no idea that S1 killed the property
	TVERIFY( tester.S2()->mPins[0]->getValue(propids[1])->i == 202 ) ;

	// but deletion attempt will fail
	val.setDelete(propids[1]) ;
	rc = tester.S2()->mPins[0]->modify(&val,1,0) ;
	TVERIFY(RC_OK != rc) ;

#if SINGLE_THREAD_DEADLOCK 
	// Case 7: Deleting the same property within a transaction

	// Make sure both sessions see propids[3] with true in it
	val.set(true) ; val.property = propids[3] ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;
	TVERIFYRC(tester.S2()->mPins[0]->refresh()) ;


	TVERIFYRC(tester.S1()->mSession->startTransaction()) ;
	TVERIFYRC(tester.S2()->mSession->startTransaction()) ;

	val.setDelete(propids[3]) ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;

	// S2 snapshot has no idea that S1 killed the property
	TVERIFY( tester.S2()->mPins[0]->getValue(propids[3])->b == true ) ;

	// Deletion attempt - S1 transaction is still going
	// and S2 also is inside transaction

	// This now causes a deadlock, because S2 will wait until S1 finishes,
	// but we never give S1 a chance to finish its transaction.
	// This scenario should be redone as a two threaded scenario 
	// - deadlock detection can't help in this artificial single threaded scenario
	val.setDelete(propids[3]) ;
	rc = tester.S2()->mPins[0]->modify(&val,1,0) ;
	TVERIFY(RC_OK == rc) ;

	// Behavior beyond this point isn't really defined 

	TVERIFYRC(tester.S2()->mSession->commit()) ;

	// With isolation working probably it would be this second commit that must fail,
	// as S2 commit came in first
#if TESTISOLATION
	TVERIFY(RC_OK != tester.S1()->mSession->commit()) ;
#else
	TVERIFYRC(tester.S1()->mSession->commit()) ;
#endif

#endif

	// Case 8: Safer version of case 7 above - S1 transaction allowed to finish before
	// second session comes into play

	val.set(true) ; val.property = propids[3] ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;
	TVERIFYRC(tester.S2()->mPins[0]->refresh()) ;

	TVERIFYRC(tester.S1()->mSession->startTransaction()) ;
	TVERIFYRC(tester.S2()->mSession->startTransaction()) ;

	val.setDelete(propids[3]) ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;

	// S2 snapshot has no idea that S1 killed the property
	TVERIFY( tester.S2()->mPins[0]->getValue(propids[3])->b == true ) ;

	// Now its official
	TVERIFYRC(tester.S1()->mSession->commit()) ;

	// S2 should fail cleanly because pin is gone
	val.setDelete(propids[3]) ;
	rc = tester.S2()->mPins[0]->modify(&val,1,0) ;
	TVERIFY(RC_OK != rc) ;

	TVERIFYRC(tester.S2()->mSession->commit()) ;

	// Case 9: Delete the entire PIN underneath the PIN pointers

	// Put something inside
	val.set(true) ; val.property = propids[3] ;
	TVERIFYRC(tester.S1()->mPins[0]->modify(&val,1,0)) ;
	size_t numProps = tester.S1()->mPins[0]->getNumberOfProperties() ;

	TVERIFYRC(tester.S1()->mSession->deletePINs(&pid, 1) ) ;

	// S1 and S2 both still have snap shots of the ill fated PIN
	// any update attempts should fail but ideally not crash
	TVERIFY(RC_OK != tester.S1()->mPins[0]->refresh()) ;
	// S1 is still keeping a sad memory of the PIN's content
	TVERIFY( numProps == tester.S1()->mPins[0]->getNumberOfProperties() );

	val.set(pid) ; val.property = propids[4] ;
	TVERIFY(RC_OK != tester.S2()->mPins[0]->modify(&val,1,0)) ;

	// Restore the original
	tester.Cleanup() ;
	TVERIFYRC(session->attachToCurrentThread()) ;

	//
	// TODO: For isolation there could be a lot more scenarios, e.g. more nesting of
	// the transactions and some rollbacks and pin deletions that should remain
	// completely invisible in the other scenario.
	// Also for isolation confirm whether PINs appear in a query while still inside
	// transactions.
	// 
	// Any other methods that can change the internal state, 
	// e.g makePart?
	//
	// Notification behavior within transactions - probably should be deferred until transactions is finished
	// 
}

//
// TODO - these scenarios are similar to testlocking and can be moved
// there.
//

void TestTransactions::multisessionDeadlock(ISession * session)
{
	// This test mostly focuses on how two separate PIN snapshots 
	// behave as a SINGLE PIN is modified from two sessions (within the same thread)
	PropertyID propids[1] ;
	MVTApp::mapURIs(session,"TestTransactions.multisessionDeadlock",1,propids);

	PropertyID strProp =propids[0] ;

	PID pids[2] ;
	TVERIFYRC(session->createPIN(pids[0],NULL,0));
	TVERIFYRC(session->createPIN(pids[1],NULL,0));

	PID pidX = pids[0] ; // For test readability
	PID pidY = pids[1] ;

	TVERIFYRC(session->detachFromCurrentThread()) ;
	TransactionMultiSessionTester tester(NULL,0) ;

	tester.S1()->mSession->startTransaction() ;
	tester.S2()->mSession->startTransaction() ;

	// Session1 modifies pinX and Session2 modifies pin1

	Value valpinXs1 ; valpinXs1.set( "info about pinX from session1" ) ; valpinXs1.property = strProp ;
	Value valpinXs2 ; valpinXs2.set( "info about pinX from session2" ) ; valpinXs2.property = strProp ;
	Value valpinYs1 ; valpinYs1.set( "info about pinY from session1" ) ; valpinYs1.property = strProp ;
	Value valpinYs2 ; valpinYs2.set( "info about pinY from session2" ) ; valpinYs2.property = strProp ;

	TVERIFYRC( tester.S1()->mSession->modifyPIN( pidX, &valpinXs1, 1 ) );
	TVERIFYRC( tester.S2()->mSession->modifyPIN( pidY, &valpinYs2, 1 ) );

	// Now each session attempts to modify each others PIN
	
	// This pin is already modified by another transaction, should the
	// modify call block until the other transaction is complete?  if so execution 
	// would block here
	TVERIFYRC( tester.S1()->mSession->modifyPIN( pidY, &valpinYs1, 1 ) );

	TVERIFYRC( tester.S2()->mSession->modifyPIN( pidX, &valpinXs2, 1 ) );

	TVERIFYRC(tester.S1()->mSession->commit(false)) ;
	TVERIFYRC(tester.S2()->mSession->commit(false)) ;

	tester.Cleanup() ;
	TVERIFYRC(session->attachToCurrentThread()) ;

	// REVIEW, probably because of the absense of isolation there is no blocking and
	// the basically it is the last modifications that "win" even though the session 2 commits happen
	// afterwards

	Value pidXVal ;
	session->getValue( pidXVal, pidX, strProp ) ;
	mLogger.out() << "PinX strProp: " << pidXVal.str << endl ;

	TVERIFY( 0 == strcmp( pidXVal.str, valpinXs2.str ) ) ;
	
	// REVIEW: although const char* we are responsible to free the value, otherwise it would leak
	session->free( const_cast<char*>(pidXVal.str) ) ; 
	pidXVal.str = NULL ;

	Value pidYVal ;
	session->getValue( pidYVal, pidY, strProp ) ;
	TVERIFY( 0 == strcmp( pidYVal.str, valpinYs1.str ) ) ;
	mLogger.out() << "PinY strProp: " << pidYVal.str << endl ;

	session->free( const_cast<char*>(pidYVal.str) ) ; 
	pidYVal.str = NULL ;
}

//
// The following code re-implements multisessionDeadlock
// as a truly multi-threaded scenario.  Both threads have
// their own session and they grab opposite pins to
// try to simulate the 2PL deadlock 
// 
struct DeadlockThreadInfo
{
	DeadlockThreadInfo( ITest * inCtxt, volatile long * inSyncPoint , MVStoreKernel::StoreCtx *pStoreCtx, int idx)
	{
		ctxt = inCtxt ;
		syncPoint = inSyncPoint ;
		mStoreCtx = pStoreCtx;
		index=idx;
		bLoser=false;
		unrelatedPID.pid=STORE_INVALID_PID;
	}

	Value valpin0 ;  // Value to stick in the first pin
	Value valpin1 ; 
	PID pids[2] ;    // Two pids that will get the threads into deadlock trouble
	ITest * ctxt ;  // TestTransactions object
	volatile long * syncPoint ;
	MVStoreKernel::StoreCtx *mStoreCtx;
	int index ;  // Gives a readable name to the thread
	bool bLoser; // Whether this thread failed
	PID unrelatedPID; // An unrelated PIN that doesn't contribute to deadlock
} ;

static THREAD_SIGNATURE threadTestDeadlock(void * pDeadlockThreadInfo)
{
	DeadlockThreadInfo * pTI = (DeadlockThreadInfo*)pDeadlockThreadInfo ;
	ISession * session = MVTApp::startSession(pTI->mStoreCtx) ;

	//TIP: Thread can't use TVERIFY because not part of the test.  But TV variants redirected to the test

	TVRC_R(session->startTransaction(), pTI->ctxt) ; 

	//
	// Also create a dummy pin as part of this transaction
	// to see whether deadlock will roll back entire transaction
	// 
	TVRC_R(session->createPIN( pTI->unrelatedPID,NULL,0), pTI->ctxt);


	TVRC_R(session->modifyPIN( pTI->pids[0], &pTI->valpin0, 1 ), pTI->ctxt) ;

	INTERLOCKEDD(pTI->syncPoint); // first thread reduces from 2 to 1.  Second thread does 1 to 0
	while( *(pTI->syncPoint) > 0 )
	{
		MVTestsPortability::threadSleep(50); 
	}

	// At this point we know that the other thread has already modified
	// pids[1].  	
	// It may be blocked from finished its own transaction
	// until we finish this threads transaction and commit the change to pids[0]. 
	// But we potentially can't modify this PIN until it finishes its own transaction.
	// Result: deadlock
	// 

	RC rc=session->modifyPIN( pTI->pids[1], &pTI->valpin1, 1 );
	
	pTI->bLoser=(rc==RC_DEADLOCK);

	if ( pTI->ctxt->isVerbose() )
	{
		if ( rc==RC_DEADLOCK )
		{
			pTI->ctxt->getLogger().out() << "I lost the deadlock resolution (thread " << pTI->index << ")" << endl;
		}
		else if ( rc==RC_OK )
		{
			pTI->ctxt->getLogger().out() << "I won the deadlock resolution (thread " << pTI->index << ")" << endl;
		}
	}

	TV_R(rc==RC_OK || rc==RC_DEADLOCK, pTI->ctxt) ;


#if DEADLOCK_LOSER_DOES_ROLLBACK
	if (rc==RC_DEADLOCK) TVRC_R( session->rollback(), pTI->ctxt ) ;
	else TVRC_R( session->commit(), pTI->ctxt ) ;
#else
	// Rollback happens automatically
	if (rc==RC_OK)
		TVRC_R( session->commit(), pTI->ctxt ) ;
#endif
	session->terminate() ;

	return 0 ;
}

void TestTransactions::multithreadDeadlock(ISession * session)
{
	// This test mostly focuses on how two separate PIN snapshots 
	// behave as a SINGLE PIN is modified from two sessions (within the same thread)
	PropertyID propids[1] ;
	MVTApp::mapURIs(session,"TestTransactions.multisessionDeadlock",1,propids);

	PropertyID strProp =propids[0] ;

	PID pids[2] ;
	TVERIFYRC(session->createPIN(pids[0],NULL,0));
	TVERIFYRC(session->createPIN(pids[1],NULL,0));
	PID pidX = pids[0] ; // For test readability
	PID pidY = pids[1] ;

	long volatile lSyncPoint = 2;

	DeadlockThreadInfo ti0(this,&lSyncPoint,mStoreCtx,0), ti1(this, &lSyncPoint,mStoreCtx,1) ;

	ti0.pids[0] = pidX ; ti0.pids[1] = pidY ; 
	ti0.valpin0.set( "info about pinX from thread0" ) ; ti0.valpin0.property = strProp ;
	ti0.valpin1.set( "info about pinY from thread0" ) ; ti0.valpin1.property = strProp ;

	ti1.pids[0] = pidY ; ti1.pids[1] =pidX ; 
	ti1.valpin0.set( "info about pinY from thread1" ) ; ti1.valpin0.property = strProp ;
	ti1.valpin1.set( "info about pinX from thread1" ) ; ti1.valpin1.property = strProp ;
	
	const size_t sNumThreads = 2 ; 
	HTHREAD lThreads[sNumThreads];
	createThread(&threadTestDeadlock, &ti0, lThreads[0]);
	createThread(&threadTestDeadlock, &ti1, lThreads[1]);

// NOTE: If you want to experiment with store forced shutdown replace the waitfor with
// this sleep call:
//	MVTestsPortability::threadSleep(	10000 ); 
	MVTestsPortability::threadsWaitFor(sNumThreads, lThreads);

	// Check that the values actually set on the pins match the winning threads
	// assigned task
	Value pidXVal ;
	session->getValue( pidXVal, pidX, strProp ) ;
	mLogger.out() << "PinX strProp: " << pidXVal.str << endl ;
	if (ti1.bLoser) TVERIFY(0==strcmp(pidXVal.str,"info about pinX from thread0"));
	else TVERIFY(0==strcmp(pidXVal.str,"info about pinX from thread1"));
	session->free( const_cast<char*>(pidXVal.str) ) ; 

	Value pidYVal ;
	session->getValue( pidYVal, pidY, strProp ) ;
	mLogger.out() << "PinY strProp: " << pidYVal.str << endl ;
	if (ti1.bLoser) TVERIFY(0==strcmp(pidYVal.str,"info about pinY from thread0"));
	else TVERIFY(0==strcmp(pidYVal.str,"info about pinY from thread1"));
	session->free( const_cast<char*>(pidYVal.str) ) ; 

	// Check whether entire transaction was rolled back
	// by seeing if another PIN was removed
	if (ti1.bLoser) 
	{
		TVERIFY(!ti0.bLoser);
		IPIN * p = session->getPIN(ti0.unrelatedPID); 
		TVERIFY(p!=NULL);
		p->destroy();		
		TVERIFY(session->getPIN(ti1.unrelatedPID)==NULL);
	}
	else 
	{
		TVERIFY(ti0.bLoser);
		IPIN * p = session->getPIN(ti1.unrelatedPID); 
		TVERIFY(p!=NULL);
		p->destroy();		

		IPIN * pLoser=session->getPIN(ti0.unrelatedPID);
		TVERIFY(pLoser==NULL);
		if (pLoser!=NULL)
		{
			mLogger.out() << "PIN created by losing transaction didn't get rolled back " 
				<<hex<<ti0.unrelatedPID.pid<<endl
				<<"Other pid was "<<ti1.unrelatedPID.pid<<dec<< endl;
			pLoser->destroy();
		}
	}
}

/*

This scenario demonstrates one technique permitting multiple threads
to all read and update the same PIN property safely.

This approach is recommending in cases when the thread must read the current
state of the PIN, then do some long calculation based on that state
and then update the PIN with the result.  Examples might be calculating
a thumbnail for a bitmap in a background "shredding" thread.  The approach
is "optimistic" meaning that the thread "hopes" that the PIN doesn't
change when it has released the PIN lock.  If the PIN does happen to change
then it has to discard the changes,  so this approach should be used when 
the chance of such parallel updates is relatively small.
*/ 

struct UpdateThreadInfo
{
	// Data sent to each thread
	UpdateThreadInfo( ITest * inCtxt, PID inPid, PropertyID inProp, MVStoreKernel::StoreCtx *pStoreCtx)
	{
		mTest = inCtxt ;
		mStoreCtx = pStoreCtx;
		mPid = inPid;
		mProperty = inProp ;
	}

	PID mPid ;
	PropertyID mProperty ;
	ITest * mTest ;  
	MVStoreKernel::StoreCtx *mStoreCtx;
} ;


static long gThreadCnt = 0;
static const char START_CHAR = 'z' ;

int pinUpdateMethod1( UpdateThreadInfo * pTI, ISession * session, long threadId )
{
	// This simpler methods relies on MODE_CHECK_STAMP
	// Do an atomic read to get the current pin state
	IPIN * pin = session->getPIN(pTI->mPid) ;
	
	std::string newString( pin->getValue(pTI->mProperty)->str ) ;

	// Sleep, during which time the pin might change
	// This simulates some long calculation.
	MVTestsPortability::threadSleep(	MVTRand::getRange(50,500) ); 

	// Each append to the property repeats the last character in the string
	// and then adds its own.
	// Therefore we expect a resulting string something like this:
	// "zz bb cc cc cc cc dd dd aa aa aa cc dd bb b"
	
	char appendStr[4] ;
	sprintf(appendStr, "%c %c", 
				newString[newString.length()-1], 
				'a' + (char)threadId - 1 ) ;
	newString += appendStr ;
	
	Value newValueWrapper ;
	newValueWrapper.set( newString.c_str() ) ; newValueWrapper.property = pTI->mProperty ;

	RC rc = pin->modify(&newValueWrapper,1,MODE_CHECK_STAMP);

	if ( rc == RC_REPEAT )
		return 0 ;

	TV_R(rc==RC_OK,pTI->mTest);

	return 1;
}

int pinUpdateMethod2( UpdateThreadInfo * pTI, ISession * session, long threadId )
{
	// This is a more elaborate way to accomplish the same thing

	// Do an atomic read to get the current string and associated pin version	
	Value pinVals[2] ;
	pinVals[0].setError(pTI->mProperty) ;
	pinVals[1].setError(PROP_SPEC_STAMP) ;

	TVRC_R(session->getValues( pinVals, 2, pTI->mPid ),pTI->mTest) ; 
	TV_R(pinVals[0].type == VT_STRING,pTI->mTest) ;
	TV_R(pinVals[1].type == VT_UINT,pTI->mTest) ;

	std::string newString( pinVals[0].str ) ;
	TV_R(!newString.empty(),pTI->mTest);
	TV_R(newString[0]==START_CHAR,pTI->mTest);
	session->free( const_cast<char*>(pinVals[0].str) ) ; 

	VersionID existingVersion = pinVals[1].i ;

	// Sleep, during which time the pin might change
	// This simulates some long calculation.
	MVTestsPortability::threadSleep(	MVTRand::getRange(50,500) ); 
	
	char appendStr[4] ;
	sprintf(appendStr, "%c %c", 
				newString[newString.length()-1], 
				'a' + (char)threadId - 1 ) ;
	newString += appendStr ;
	
	Value newValueWrapper ;
	newValueWrapper.set( newString.c_str() ) ; newValueWrapper.property = pTI->mProperty ;

#if 0
	// Naive approach.
	// Calling modifyPIN means that the PIN might have changed during the sleep
	// and this modify would ERASE those changes.
	TVRC_R(session->modifyPIN( pTI->mPid, &newValueWrapper, 1 ), pTI->mTest );		
	cntSuccessModifications++ ;
#else
	// Create query that will update PIN only if its version still matches the original version
	IStmt * lQ = session->createStmt(STMT_UPDATE) ;
	unsigned char var = lQ->addVariable();

	lQ->setPIDs(var,&pTI->mPid,1) ; // Specific PIN to query

	PropertyID propSpecStamp = PROP_SPEC_STAMP ;
	Value args[2];
	args[0].setVarRef(0,propSpecStamp );
	args[1].set((unsigned int)existingVersion); /* PIN version when it was last read */
	IExprTree * expr = session->expr( OP_EQ, 2, args ) ;

	lQ->addCondition(var,expr) ;
	expr->destroy() ;

	lQ->setValues(&newValueWrapper, 1);

	uint64_t nModified = 0 ;
	TVRC_R(lQ->execute( NULL,NULL,0,	~0u,0,0,&nModified ), pTI->mTest) ;
	TV_R( nModified == 0 /* means that version changed so this modification must be discarded */
		|| nModified == 1 /* meaned we were able to modify the pin */, pTI->mTest ) ;

	lQ->destroy() ;

	return (int)nModified ;
#endif
}

THREAD_SIGNATURE threadPinUpdate(void * pUpdateThreadInfo)
{
	// Simple thread that just tries to safely update an string property several times
	// 

	// Make sure each thread has a different seed
	long threadId = InterlockedIncrement(&gThreadCnt) ;
	srand(threadId);

	UpdateThreadInfo * pTI = (UpdateThreadInfo*)pUpdateThreadInfo ;
	ISession * session = MVTApp::startSession(pTI->mStoreCtx) ;
	int cntSuccessModifications = 0 ;

	for ( int i = 0 ; i < 10 ; i++ )
	{
		cntSuccessModifications += pinUpdateMethod1( pTI, session, threadId ) ;
		cntSuccessModifications += pinUpdateMethod2( pTI, session, threadId ) ;
	}
	// Thread should have been successful at least part of the time,
	// but not assert because it isn't 100% sure
	if ( pTI->mTest->isVerbose() )
		pTI->mTest->getLogger().out() << "Managed to modify PIN " << cntSuccessModifications << " out of 10 tries" << endl ;

	session->terminate() ;

	return 0 ;
}

void TestTransactions::safePinUpdates(ISession * session)
{
	// This test mostly focuses on how two separate PIN snapshots 
	// behave as a SINGLE PIN is modified from two sessions (within the same thread)

	PropertyID strProp ;
	MVTApp::mapURIs(session,"TestTransactions.safepinupdates",1,&strProp);

	char initialString[2] ; initialString[0]=START_CHAR ; initialString[1]=0 ;
	Value initial ;
	initial.set( initialString ) ; initial.property = strProp ;

	PID pid ;
	TVERIFYRC(session->createPIN(pid,&initial,1));
	
	UpdateThreadInfo info(this,pid,strProp,mStoreCtx) ;
	
	const size_t sNumThreads = 5 ; 
	HTHREAD lThreads[sNumThreads];

	for ( size_t i = 0 ; i < sNumThreads ; i++ )
	{
		createThread(&threadPinUpdate, &info, lThreads[i]);
	}

	MVTestsPortability::threadsWaitFor(sNumThreads, lThreads);

	// Should see some of the data appended by the threads
	Value finalValue ;
	session->getValue( finalValue, pid, strProp ) ;

	// Confirm expected values 
	TVERIFY( finalValue.str[0] == START_CHAR ) ;
	TVERIFY( finalValue.str[1] == START_CHAR ) ;
	TVERIFY( finalValue.str[2] == ' ' ) ;
	TVERIFY( finalValue.str[3] == finalValue.str[4] ) ; 

	mLogger.out() << "Final strProp: " << finalValue.str << endl ;

	session->free( const_cast<char*>(finalValue.str) ) ; 
}
