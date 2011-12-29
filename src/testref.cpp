/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

#define DELETE_BAD_REFS 1 // normally avoid sdrcl warnings (see 11626)
		// But when testing you can disable this flag and try "sdrcl -printrefs" or "sdrcl refs"
		// on the resulting store

using namespace std;

// Publish this test.
class TestRef : public ITest
{
	public:
		TEST_DECLARE(TestRef);
		virtual char const * getName() const { return "testref"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Test of References between Pins VT_REF etc"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }

	protected:
		void testBrokenRefs() ;
		void testValidRefs() ;
	private:
		ISession * mSession ;
		PropertyID mPropX ;
		PropertyID mPropY ;
};
TEST_IMPLEMENT(TestRef, TestLogger::kDStdOut);

int TestRef::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	MVTApp::mapURIs(mSession,"TestRef.propX",1,&mPropX) ;
	MVTApp::mapURIs(mSession,"TestRef.propY",1,&mPropY) ;

	testBrokenRefs() ;
	testValidRefs() ;

	mSession->terminate(); 
	MVTApp::stopStore();  

	return RC_OK  ;
}

void TestRef::testValidRefs()
{
	// Scenario 1
	Value valsOnChild[2] ;
	valsOnChild[0].set(99); valsOnChild[0].property=mPropX; valsOnChild[0].op = OP_ADD ; valsOnChild[0].eid = STORE_LAST_ELEMENT;
	valsOnChild[1].set(11); valsOnChild[1].property=mPropX; valsOnChild[1].op = OP_ADD ; valsOnChild[1].eid = STORE_LAST_ELEMENT;

	PID child ;
	TVERIFYRC(mSession->createPIN(child,valsOnChild,2));
	CmvautoPtr<IPIN> childPIN(mSession->getPIN(child));

	ElementID validEID = valsOnChild[1].eid  ;

	PID parent ;
	Value valsOnParent[1];
	RefVID ref ; 
	RefVID *lRef = (RefVID *)mSession->alloc(1*sizeof(RefVID));
	RefP refP ;
	RefP *lRefP = (RefP *)mSession->alloc(1*sizeof(RefP));
	IPIN * p = NULL ;

	// Scenario
	valsOnParent[0].set(child) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));
	p=mSession->getPIN(parent); 
	TVERIFY(p->getValue(mPropX)->type == VT_REFID);
	p->destroy() ;

	// Scenario
	// You can set VT_REF but you get VT_REFID back
	valsOnParent[0].set(childPIN.Get()) ; valsOnParent[0].property = mPropX ; 
	TVERIFY(valsOnParent[0].type==VT_REF);
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));
	p=mSession->getPIN(parent); 
	TVERIFY(p->getValue(mPropX)->type == VT_REFID);
	p->destroy() ;

	// Scenario
	ref.id = child ;
	ref.pid = mPropX ;
	ref.eid = STORE_COLLECTION_ID ;
	ref.vid = 0 ;
	*lRef = ref;
	valsOnParent[0].set(*lRef) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));
	p=mSession->getPIN(parent); 
	TVERIFY(p->getValue(mPropX)->type == VT_REFIDPROP);
	p->destroy() ;

	// Scenario
	ref.id = child ;
	ref.pid = mPropX ;
	ref.eid = validEID ;
	ref.vid = 0 ;
	*lRef = ref;
	valsOnParent[0].set(*lRef) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));
	p=mSession->getPIN(parent); 
	TVERIFY(p->getValue(mPropX)->type == VT_REFIDELT);
	p->destroy() ;

	// Scenario
	refP.pin = childPIN ;
	refP.pid = mPropX ;
	refP.eid = STORE_COLLECTION_ID ;
	refP.vid = 0 ;
	*lRefP = refP;
	valsOnParent[0].set(*lRefP) ; valsOnParent[0].property = mPropX ; 
	TVERIFY(valsOnParent[0].type == VT_REFPROP );
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));
	p=mSession->getPIN(parent); 
	TVERIFY(p->getValue(mPropX)->type == VT_REFIDPROP);
	p->destroy() ;

	// Scenario
	refP.pin = childPIN ;
	refP.pid = mPropX ;
	refP.eid = validEID ;
	*lRefP = refP;
	valsOnParent[0].set(*lRefP) ; valsOnParent[0].property = mPropX ; 
	TVERIFY(valsOnParent[0].type == VT_REFELT );
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));
	p=mSession->getPIN(parent); 
	TVERIFY(p->getValue(mPropX)->type == VT_REFIDELT);
	p->destroy() ;

	// TODO:
	// Ref VersionID not covered as it is not implemented
	// Show query/path resolution (is it available?)
}

void TestRef::testBrokenRefs()
{
	// Create broken-ref scenarios now detected by "sdrcl -refs"
	//
	// In general the store doesn't do validation of pin references
	// mostly for performance reasons and also to give the caller a chance to fix the reference
	// For example replication could create the parent PIN pointing to non-existance child PID,
	// then add the child PID as a second step.
	//
	// However dangling/broken references are almost certainly a app level bug

	PID parent ;
	Value valsOnParent[1];

	// Scenario 1 - Completely invalid PID
	//	PID is clearly invalid so call fails	
	PID noExist ; memset(&noExist,0,sizeof(PID));	
	valsOnParent[0].set(noExist) ; valsOnParent[0].property = mPropX ; 
	TVERIFY(RC_INVPARAM==mSession->createPIN(parent,valsOnParent,1));

	//	PID is clearly invalid so call fails
	noExist.pid=STORE_INVALID_PID ; noExist.ident=STORE_OWNER ;
	valsOnParent[0].set(noExist) ; valsOnParent[0].property = mPropX ; 
	TVERIFY(RC_INVPARAM==mSession->createPIN(parent,valsOnParent,1));

	//	Store does not validate PID is valid (by design)
	memset(&noExist,0xCD,sizeof(PID));	
	valsOnParent[0].set(noExist) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));

	// Scenario 2 - softdeleted PID
	PID deletedChild ;
	TVERIFYRC(mSession->createPIN(deletedChild,NULL,0));
	TVERIFYRC(mSession->deletePINs(&deletedChild,1));
	valsOnParent[0].set(deletedChild) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));

	// Scenario 3 - purged PID
	PID purgedChild;
	TVERIFYRC(mSession->createPIN(purgedChild,NULL,0));
	TVERIFYRC(mSession->deletePINs(&purgedChild,1,MODE_PURGE));
	valsOnParent[0].set(purgedChild) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));

	// Scenario 4 - invalid property on valid PID
	Value valsOnChild[2] ;
	valsOnChild[0].set(99); valsOnChild[0].property=mPropX; valsOnChild[0].op = OP_ADD ;
	valsOnChild[1].set(11); valsOnChild[1].property=mPropX; valsOnChild[1].op = OP_ADD ;

	PID child ;
	TVERIFYRC(mSession->createPIN(child,valsOnChild,2));
	CmvautoPtr<IPIN> childPIN(mSession->getPIN(child));

	RefVID ref ;
	ref.id = child ;
	ref.pid = mPropY ;
	ref.eid = STORE_COLLECTION_ID ;

	valsOnParent[0].set(ref) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));

	// Scenario 5 - invalid collection item within valid property on valid PID
	ref.id = child ;
	ref.pid = mPropX ;
	ref.eid = 9999 ;

	valsOnParent[0].set(ref) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));

	// Scenario 6 - REFIDELT where property doesn't exist (on valid PID)
	ref.id = child ;
	ref.pid = mPropY ;
	ref.eid = 9999 ;

	valsOnParent[0].set(ref) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));

	// Scenario 7 - REFIDELT on invalid PID
	ref.id = noExist ;
	ref.pid = mPropX ;
	ref.eid = 9999 ;

	valsOnParent[0].set(ref) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));

	// Scenario 8 - REFIDVAL on invalid PID
	ref.id = noExist ;
	ref.pid = mPropX ;
	ref.eid = STORE_COLLECTION_ID ;

	valsOnParent[0].set(ref) ; valsOnParent[0].property = mPropX ; 
	TVERIFYRC(mSession->createPIN(parent,valsOnParent,1));

	//
	// TODO: once supported more extensively for queries 
	// try these out to make sure store won't croak
	// 

#if DELETE_BAD_REFS
	IStmt* q=mSession->createStmt(STMT_DELETE) ;
	q->setPropCondition(q->addVariable(),&mPropX,1);
	TVERIFYRC(q->execute(NULL,0,0,~0,0,MODE_PURGE));
	q->destroy();
#endif
}
