/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

//
// This test covers collections with many elements
// These are loosely called "big collections"
// A big collection in correct store terminology is reserved for 
// a special data structure used only for the largest of collections,
// so some, but not all of the collections tested here are "big collections"
//
//
// (Testing of many properties now occurs in testmanyproppins.cpp)


#include "app.h"
#include <fstream>
#include "serialization.h"
#include "mvauto.h"
#include "teststream.h" // MyStream
#include "collectionhelp.h"

using namespace std;
#define USE_VT_ARRAY 1 /* workaround to avoid passing to many values to IPIN::modify*/

#define TEST_12322 0 // When enable nothing else si done 

#define TEST_9824 0 // Narrow down
#define MAKE_ONE_PIN 0
#define TEST_ROLLBACK_BIGCOLLECTION 1 // Bug 3294
#define TEST_COLLECTION_TRANSITION_8357 1
#define TEST_DATA_LEAK_8358 0 // When enabled nothing else is done in this test

static void reportRunningCase(const char *str){
	std::cout<<"Running test case ... "<<str<<std::endl;
}



// Publish this test.
class TestBig : public ITest
{
	public:
		RC mRCFinal;
		TEST_DECLARE(TestBig);
		virtual char const * getName() const { return "testbig"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of big pins /collections"; }
		virtual bool isLongRunningTest()const {return true;}
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testBigCollectionDownsize(bool);

		void oneBigPin(bool bBigCollection, bool BLOB) ;
		void test9824(ISession *session, unsigned int cntElements, unsigned int sizeElement );
		void doTest(ISession *session); 
		void rcNoResources(ISession *session, int cntVals); 
		void bigCollection(ISession *session, unsigned int cntElements, unsigned int cntElementSize );
		void testBigCollectionTransition(ISession * session, int elemSize) ;
		void testCollectionDataLeak(ISession * session, int elemSize) ;
		void testTransaction( ISession * session, PropertyID propid, vector<string>& strCollItems, bool inbRollback /*rollback*/ ) ;
		void verifyExpectedCollection(ISession *session,
									   IPIN * pin,
									   PropertyID inProp,
									   const vector<string>& expectedStrs,
									   const char * testDescription ) ;
	
		ISession* mSession;
};
#define lAllClassNotifs (CLASS_NOTIFY_JOIN | CLASS_NOTIFY_LEAVE | CLASS_NOTIFY_CHANGE | CLASS_NOTIFY_DELETE | CLASS_NOTIFY_NEW)

TEST_IMPLEMENT(TestBig, TestLogger::kDStdOut);


// Implement this test.



int TestBig::execute()
{
	mRCFinal = RC_FALSE;
	if (MVTApp::startStore())
	{
		mRCFinal=RC_OK;
		mSession = MVTApp::startSession();

#if TEST_DATA_LEAK_8358
		// You have to look at the store size or storedr to see
		// the symptom of this issue
		mLogger.out() << "WARNING: ONLY TESTING DATA LEAK SCENARIO" << endl ;
		testCollectionDataLeak( mSession, 32768 ) ;
#elif TEST_9824 
		mLogger.out() << "WARNING: ONLY TESTING 9824 SCENARIO" << endl ;
		test9824(mSession, 165/*cntElements*/, 200/*elementSize*/);
#elif MAKE_ONE_PIN 
		mLogger.out() << "WARNING: ONLY CREATING SINGLE PIN" << endl ;
		MVTApp::deleteStore();
 		oneBigPin(false, true) ;
#elif TEST_12322
		mLogger.out() << "WARNING: ONLY TESTING testBigCollectionDownsize" << endl ;
		testBigCollectionDownsize(true);
#else
		doTest(mSession) ;
#endif

		mSession->terminate();
		MVTApp::stopStore();
	}
	if (RC_OK != mRCFinal)
		std::cout<<"\n Some tests might have failed. Please check testbigresults.txt for details"<<std::endl;
 	return mRCFinal;
}

void TestBig::doTest( ISession * session )
{
	int sesflags= session->getInterfaceMode();

	// Create various representative formats
	oneBigPin(false/*bigc*/, false /*lob*/) ;
	oneBigPin(false, true) ;
	oneBigPin(true, false) ;
	oneBigPin(true, true) ;

	rcNoResources(session,256);
	rcNoResources(session,4096);
	rcNoResources(session,6668);
	rcNoResources(session,10232);

	// All 256 elements can fit on the page
	testBigCollectionTransition(session, 32) ;
	testBigCollectionTransition(session, 64) ;
#if TEST_COLLECTION_TRANSITION_8357
	// These sizes have trouble because each element is
	// smaller than the pin page size, but there isn't
	// room for 256 of them
	testBigCollectionTransition(session, 128) ;
	testBigCollectionTransition(session, 256) ;
	testBigCollectionTransition(session, 4096) ;
#endif
	testBigCollectionTransition(session, 8192) ;

	// No trouble - not even one elements can fit on the page
	testBigCollectionTransition(session, 32768) ;

	testBigCollectionDownsize(true);
	testBigCollectionDownsize(false);

#if 0
	// Sanity pass, mostly for fast test debugging
	mLogger.out() << endl << "--------Preliminary pass - many small elements" << endl ;
	bigCollection(session, 500/*cntElements*/, 4/*elementSize*/);
#endif

	mLogger.out()  << endl << "--------First pass - small collection with huge elements - varray"  << endl;
	bigCollection(session, 20/*cntElements*/, 2000/*elementSize*/);

	session->setInterfaceMode(sesflags);

	mLogger.out()  << endl << "--------Second pass - small collection with huge elements - CNavigator"  << endl;
	bigCollection(session, 20/*cntElements*/, 2000/*elementSize*/);

	// Really huge collection sizes
	mLogger.out()  << endl << "--------Third pass - many huge elements" << endl ;
	bigCollection(session, 5000/*cntElements*/, 200/*elementSize*/);
}



void TestBig::bigCollection(ISession *session, unsigned int cntElements, unsigned int sizeElement )
{
	PropertyID lProps[2]; 
	MVTApp::mapURIs(session, "TestBig.bigCollection", 2, lProps ) ;
	PropertyID propid = lProps[0], propid1 = lProps[1] ;

	// Prepare pool of random strings
	unsigned int i;
	vector<string> strCollItems(cntElements) ;
	for (i=0; i<cntElements; i++){
		MVTRand::getString(strCollItems[i],sizeElement,0,true,true);
	}

	// Give warning about potential pin size
	mLogger.out() << "Generating pins with collection size " << ( cntElements * ( sizeElement + 1 + sizeof(Value) ) ) / ( 1024.) << "(KB) (or more)" << endl ;

	//case 1: build big collection on uncommited pin
	Value * const val = (Value *)session->malloc(sizeof(Value) * cntElements);
	Tstring str;
	IPIN *pin;
	PID pid;
	RC rc;

	session->createPIN(NULL,0,&pin,MODE_PERSISTENT);
	reportRunningCase("uncommited pin collection.");
	for (i=0; i<cntElements; i++){
		// two properties so double the size
		val[0].set(strCollItems[i].c_str());val[0].setPropID(propid);
		val[0].op = OP_ADD; /*val[0].eid = STORE_LAST_ELEMENT;*/
		val[1].set(strCollItems[i].c_str());val[1].setPropID(propid1);
		val[1].op = OP_ADD; /*val[1].eid = STORE_LAST_ELEMENT;*/
		pin->modify(val,2,MODE_COPY_VALUES);
	}


	verifyExpectedCollection(session,pin,propid,strCollItems,"case1.prop0.beforecommit") ;
	verifyExpectedCollection(session,pin,propid1,strCollItems,"case1.prop1.beforecommit") ;

	//TVERIFYRC(session->commitPINs(&pin,1));
		// REVIEW: this fails in case of small collection with huge elements
		// is that by design?
	TVERIFY(pin->getValue(propid)->type == VT_COLLECTION) ;

	verifyExpectedCollection(session,pin,propid,strCollItems,"case1.prop0") ;
	verifyExpectedCollection(session,pin,propid1,strCollItems,"case1.prop1") ;

	pid = pin->getPID() ;

	pin->destroy();
	pin = NULL;

	// Sanity check with fresh PIN
	pin = session->getPIN(pid) ;

	if (isVerbose())
		mLogger.out() << "\t\tNew pid: " << std::hex << pid.pid << std::dec << endl ;

	//Review:	
	TVERIFY(pin->getValue(propid)->type == VT_COLLECTION) ;

	verifyExpectedCollection(session,pin,propid,strCollItems,"case1.prop0") ;
	verifyExpectedCollection(session,pin,propid1,strCollItems,"case1.prop1") ;

	pin->destroy();
	pin = NULL;


	//case 2: Commited pin with large collection elements.
	reportRunningCase("big collection at creation time");

	for (i=0;i<cntElements; i++){
		val[i].set(strCollItems[i].c_str());val[i].setPropID((unsigned)propid);
		val[i].op = OP_ADD; val[i].eid = STORE_LAST_ELEMENT;
	}
	TVERIFYRC(session->createPIN(val,cntElements,&pin,MODE_NO_EID | MODE_COPY_VALUES | MODE_PERSISTENT));
	pid= pin->getPID();
	verifyExpectedCollection(session,pin,propid,strCollItems,"case2") ;
	pin->destroy();
	pin = NULL;

	//case 3: uncommited pin with MODE_COPY_VALUES
	reportRunningCase("uncommitted MODE_COPY_VALUES");
	for (i =0; i < cntElements; i ++){
		// We don't need to allocate string memory because MODE_COPY_VALUES
		// means store will do the copy
		val[i].set(strCollItems[i].c_str());val[i].setPropID((unsigned)propid);
		val[i].op = OP_ADD_BEFORE; val[i].eid = STORE_LAST_ELEMENT;
	}
	TVERIFYRC(session->createPIN(val,cntElements,&pin,MODE_COPY_VALUES|MODE_PERSISTENT));TVERIFY(pin!=NULL) ;
	verifyExpectedCollection(session,pin,propid,strCollItems,"case3") ;

	//case 3a: modify this pin and elements.
	val[0].set(strCollItems[0].c_str());val[0].setPropID((unsigned)propid);
	val[0].eid = STORE_LAST_ELEMENT;val[0].op = OP_ADD_BEFORE;
	TVERIFYRC(pin->modify(val,1));
	TVERIFY2( MVTApp::getCollectionLength(*pin->getValue(propid)) == cntElements+1, "case 3a" ) ;

	//case 4: clone a big collection pin with overwrite values
	reportRunningCase("clone with more collection items");
	IPIN *clpin;
	Value cval[200];
	TVERIFY( pin!=NULL );

	// 200 integer values added to the same property
	for (i=0;i<200;i++){
		cval[i].set(i);cval[i].setPropID(propid);
		cval[i].op = OP_ADD_BEFORE; cval[i].eid = STORE_LAST_ELEMENT;
	}
	clpin = pin->clone(cval,200,MODE_PERSISTENT);

	TVERIFY2( MVTApp::getCollectionLength(*clpin->getValue(propid)) == (cntElements+1+200), "case 4" ) ;
	// Sanity check that original PIN is ok
	TVERIFY( MVTApp::getCollectionLength(*pin->getValue(propid)) == (cntElements+1) ) ;
	
	clpin->destroy();
	clpin=NULL;

	//case 5: Reload of a big collection pin.
	reportRunningCase("pin refresh");
	TVERIFY( pin!=NULL );
	TVERIFYRC(pin->refresh());
	TVERIFY( MVTApp::getCollectionLength(*pin->getValue(propid)) == (cntElements+1) ) ;
	pin->destroy(); pin=NULL;

	//case 6: Modify of a big collection
	reportRunningCase("modify big collection");
	
	// First element added at creation time
	val[0].set(strCollItems[0].c_str());val[0].setPropID(propid);
	val[0].op = OP_ADD; val[0].eid = STORE_COLLECTION_ID;
	TVERIFYRC(session->createPIN(val,1,&pin,MODE_PERSISTENT | MODE_COPY_VALUES));

	// Add remaining elements
	Value * val1 = (Value *)session->malloc(sizeof(Value) * (cntElements-1));

	for (i=0;i<cntElements-1; i++){
		val1[i].set(strCollItems[i+1].c_str());
		val1[i].setPropID(propid);val1[i].op=OP_ADD;val1[i].eid = STORE_LAST_ELEMENT;
	}
#if 1	
	//Workaround - create VT_COLLECTION value pointing to elements
	Value valArrayWrapper[1];
	valArrayWrapper[0].set(val1,cntElements-1);
	valArrayWrapper[0].setPropID(propid);valArrayWrapper[0].op=OP_ADD;valArrayWrapper[0].eid = STORE_COLLECTION_ID;
	TVERIFYRC(pin->modify(valArrayWrapper,1));	   
#else
	TVERIFYRC(pin->modify(val1,cntElements-1));
#endif
	verifyExpectedCollection(session,pin,propid,strCollItems,"case6") ;

	session->free(val1);

	//case b: Modify of a big collection

	/*MVTRand::getString(str,100,150,true,true);
	val[0].set(str.c_str());val[0].setPropID(propid);
	val[0].op = OP_ADD; val[0].eid = STORE_COLLECTION_ID;
	rc = session->createPINAndCommit(pid,val,1);
	pin = session->getPIN(pid);
	if (NULL != pin){
		for (i=0;i<cntElements; i++){
			MVTRand::getString(str,10,150,true,true);
			val[0].set(str.c_str());val[0].setPropID(propid);
			val[0].op=OP_ADD;val[0].eid = STORE_COLLECTION_ID;
			rc = pin->modify(val,1);
			if (RC_OK != rc)
				std::cout<<"Modify failed at element: "<<i<<std::endl;
		}
	}*/

	//case 6 b: modify a big c again.
	reportRunningCase("add many more elements");

	MVTRand::getString(str,100,150,true,true);
	for (unsigned int z =0; z < (2*cntElements); z ++){ 
		val[0].set(str.c_str());val[0].setPropID(propid);
		val[0].op = OP_ADD; val[0].eid = STORE_LAST_ELEMENT;
		TVERIFYRC(pin->modify(val,1));
	}

	//case 7: Delete of a big collection pin:
	reportRunningCase("delete pin");
	if (NULL != pin){
		TVERIFYRC(session->deletePINs(&pin,1,MODE_PURGE));
	}
	TVERIFY(pin==NULL) ;

	//case 8: delete a multiple elements at random places.
	reportRunningCase("delete random pin elements");
	Value *vals = (Value *)session->malloc(sizeof(Value) * cntElements);
	TVERIFY(vals!=NULL);
	for (i=0;i<cntElements; i++){
		vals[i].set(strCollItems[i].c_str());vals[i].setPropID(propid);
		vals[i].op = OP_ADD; vals[i].eid=STORE_COLLECTION_ID;
	}
	TVERIFYRC(session->createPIN(vals,cntElements,&pin,MODE_COPY_VALUES|MODE_PERSISTENT));
	session->free(vals);
	//pin->refresh();//to force VT_COLLECTION
			
	const Value *tmpVal = pin->getValue(propid);	
	const unsigned int cntElementsToDelete = cntElements / 10 ;

	if (VT_COLLECTION == tmpVal->type) {
		if (tmpVal->isNav()) {
			for (i=0; i < cntElementsToDelete && NULL != tmpVal; i++){
				const Value * candidate = tmpVal->nav->navigate(i+1==cntElementsToDelete?
												GO_LAST :
												GO_FIRST );
				if ( candidate == NULL ) {
					break ;
				}

				ElementID elid=tmpVal->nav->getCurrentID();

				if (i!=0 && i+1!=cntElementsToDelete){
					// Skip ahead
					for (int z=0,n=rand()%10; z < n; z++) {
						elid = tmpVal->nav->navigate(GO_NEXT)->eid;
					}				
				}

				if (isVerbose())
					mLogger.out() << "\t\tDelete 0x" << std::hex << elid << std::dec << endl ;

				val[0].setDelete(propid,elid);
				TVERIFYRC(rc = pin->modify(val,1));
				if (RC_OK != rc) break;
				tmpVal = pin->getValue(propid); // Get updated navigator
			}
		}
		else {
			for (i=0; i < cntElementsToDelete && tmpVal && tmpVal->type == VT_COLLECTION && !tmpVal->isNav() ; i++){
				int randElement = MVTRand::getRange(0,tmpVal->length-1) ;
				ElementID elid = tmpVal->varray[randElement].eid;
				char buf[100]; sprintf(buf,"Del: %08X\n",elid); 
				#ifdef WIN32
					OutputDebugString(buf);
				#endif
				if (isVerbose()) mLogger.out() << buf ;
				val[0].setDelete(propid,elid);
				TVERIFYRC(rc = pin->modify(val,1));
				if (RC_OK != rc) break;
				tmpVal = pin->getValue(propid);
			}
		}
	}
	TVERIFY( MVTApp::getCollectionLength(*pin->getValue(propid)) == (cntElements - cntElementsToDelete) ) ;
	pin->destroy();
	pin = NULL;

	//case 9: transaction effect on big collections
	//case a: commit transaction
	reportRunningCase(" transaction a ");
	testTransaction( session, propid, strCollItems, false/*rollback*/ ) ;

	//case b: rollback transaction.
#if TEST_ROLLBACK_BIGCOLLECTION
	reportRunningCase(" transaction b");
	testTransaction( session, propid, strCollItems, true/*rollback*/ ) ;
#endif


	//case 10: ssv big collections
	//(See linux bug #3823)
	reportRunningCase("SSV");
	vals = (Value *)session->malloc(sizeof(Value) * cntElements);
	TVERIFY(vals!=NULL);
	for (i =0; i < cntElements; i++){
		unsigned long len = 1 + rand() % 15000;
		vals[i].set(MVTApp::wrapClientStream(session, new MyStream(len)));vals[i].setPropID(propid);
		vals[i].op=OP_ADD;val[i].eid = STORE_LAST_ELEMENT;
	}
	TVERIFYRC(session->createPIN(vals,cntElements,&pin,MODE_COPY_VALUES|MODE_PERSISTENT));
	session->free(vals);
	if(MVTApp::getCollectionLength(*pin->getValue(propid)) != (cntElements))
		TVERIFY(!"Case 10: SSV BIG PIN ");
	pin->destroy();
	pin = NULL;


	//case 11: OP_MOVE and OP_MOVE_BEFORE of big collections

	//case 12: replication big pins
	//REVIEW: without checking notifications etc this isn't really
	//a very deep test!
	reportRunningCase("Replication BIG PIN");
	const int oldmode = session->getInterfaceMode();
	session->setInterfaceMode(ITF_REPLICATION | ITF_DEFAULT_REPLICATION);
	
	for (i=0; i<cntElements; i++){
		val[i].set(strCollItems[i].c_str());val[i].setPropID(propid);
		val[i].op = OP_ADD; /*val[0].eid = STORE_LAST_ELEMENT;*/
	}
	TVERIFYRC(session->createPIN(val,cntElements,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	verifyExpectedCollection(session,pin,propid,strCollItems,"case12.prop0.beforecommit") ;
	//TVERIFYRC(session->commitPINs(&pin,1));
	verifyExpectedCollection(session,pin,propid,strCollItems,"case12.prop0") ;

	pin->destroy();
	pin = NULL;
	
	//case 13: remote big pins and refresh
	reportRunningCase("Remote pin");
	string identity ;
	MVTRand::getString(str,5,0,false,false) ;
	identity="testbig.identity.bigcollection"+str;
	IdentityID const iid = session->storeIdentity(identity.c_str(), NULL, 0);
	ushort const storeid = 0x1002;  // fake remote pin
	PID rpid;
	rpid.ident = iid;
	LOCALPID(rpid) = (uint64_t(storeid) << STOREID_SHIFT) + 5;
	IPIN *rpin;

	TVERIFYRC(session->createPIN(NULL, 0, &rpin, PIN_REPLICATED|MODE_PERSISTENT, &rpid));

	for (i =0; i < cntElements; i ++){
		val[0].set(strCollItems[i].c_str());val[0].setPropID((unsigned)propid);
		val[0].op = OP_ADD;
		TVERIFYRC(rpin->modify(val,1,MODE_COPY_VALUES));
	}

	verifyExpectedCollection(session,rpin,propid,strCollItems,"case13") ;

	//case 14: remote pin refresh

	TVERIFYRC(rpin->refresh());
	verifyExpectedCollection(session,rpin,propid,strCollItems,"case14") ;
	rpin->destroy();
	rpin = NULL;

	//case 14b: retrieve remote pin again
	rpin = session->getPIN(rpid);
	verifyExpectedCollection(session,rpin,propid,strCollItems,"case14b") ;
	rpin->destroy();
	rpin = NULL;

	//case 15 : Add collection elemtns one by one (look at testCustom24) will add here later.
	session->setInterfaceMode(oldmode);	
	session->free(val);
}

void TestBig::test9824(ISession *session, unsigned int cntElements, unsigned int sizeElement )
{
	PropertyID lProps[2]; 
	MVTApp::mapURIs(session, "TestBig.bigCollection", 2, lProps);
	PropertyID propid = lProps[0];

	// Prepare pool of random strings
	unsigned int i;
	vector<string> strCollItems(cntElements) ;
	for (i=0; i<cntElements; i++){
		MVTRand::getString(strCollItems[i],sizeElement,0,true,true);
	}

	// Give warning about potential pin size
	mLogger.out() << "Generating pins with collection size " << ( cntElements * ( sizeElement + 1 + sizeof(Value) ) ) / ( 1024.) << "(KB) (or more)" << endl ;

	//case 1: build big collection on uncommited pin
	Value * const val = (Value *)session->malloc(sizeof(Value) * cntElements);
	Tstring str;
	IPIN *pin;

	//case 10: ssv big collections
	//(See linux bug #3823)
	reportRunningCase("SSV");
	for (i =0; i < cntElements; i++){
		unsigned long len = 1 + rand() % 15000;
		val[0].set(MVTApp::wrapClientStream(session, new MyStream(len)));val[0].setPropID(propid);
		val[0].op=OP_ADD;val[0].eid = STORE_LAST_ELEMENT;
	}
	TVERIFYRC(session->createPIN(val,cntElements,&pin,MODE_PERSISTENT));
	if(MVTApp::getCollectionLength(*pin->getValue(propid)) != (cntElements))
		TVERIFY(!"Case 10: SSV BIG PIN ");
	pin->destroy();
	pin = NULL;
	session->free(val);

}

void TestBig::verifyExpectedCollection(ISession *session,
									   IPIN * pin,
									   PropertyID inProp,
									   const vector<string>& expectedStrs,
									   const char * testDescription )
{
	MvStoreEx::CollectionIterator lCollHelp(pin,inProp);

	int cntElements = (int)lCollHelp.getSize();

	if ( expectedStrs.size()!=size_t(cntElements)) {
		stringstream error ; error << "Expected: " << (int)expectedStrs.size()<< " Got: " << cntElements << " " << testDescription ;
		TVERIFY2(expectedStrs.size()==size_t(cntElements),error.str().c_str()) ;
		return ;
	}
		
	for (const Value * val = lCollHelp.getFirst(); val != NULL ; val = lCollHelp.getNext() )
	{
		int i = lCollHelp.getCurrentPos() ;

		if ( val->type!=VT_STRING )
		{
			TVERIFY2(val->type==VT_STRING,testDescription);
			break ;
		}
		const char * expect = expectedStrs[i].c_str() ;
		const char * got = val->str ;
		bool bMatch = (0==strcmp(expect,got)) ;
		TVERIFY2(bMatch,testDescription);

		if (!bMatch ) break ;
	}
}

void TestBig::testTransaction( ISession * session, PropertyID propid, vector<string>& strCollItems, bool inbRollback /*rollback*/ )
{
	// Test big collection added during transaction (commit or rollback)
	int cntElements = (int) strCollItems.size() ;
	int i ;

	IPIN * pin ;

	// First element 
	Value val[1] ;
	val[0].set(strCollItems[0].c_str());val[0].setPropID(propid);
	val[0].op=OP_ADD; val[0].eid=STORE_LAST_ELEMENT;

    	TVERIFYRC(session->createPIN(val,1,&pin, MODE_PERSISTENT|MODE_COPY_VALUES));

	//remaining elements
#if USE_VT_ARRAY
	Value *val3 = (Value *)session->malloc(sizeof(Value) * cntElements);
	for (i=1;i<cntElements; i++){
//		buf = (char *)session->malloc((str.length() + 1)*sizeof(char));
//		strcpy(buf,str.c_str());
//		val3[i].set(buf);		
		val3[i-1].set(strCollItems[i].c_str());
		val3[i-1].setPropID(propid);val3[i-1].op=OP_ADD;val3[i-1].eid = STORE_LAST_ELEMENT;
	}
	// Wrap in VT_COLLECTION
	Value val2[1];
	val2[0].set(val3,cntElements-1);
	val2[0].setPropID(propid);val2[0].op=OP_ADD;val2[0].eid = STORE_COLLECTION_ID;
	TVERIFYRC(session->startTransaction());
	TVERIFYRC(pin->modify(val2,1));
#else
	for (i=1; i< cntElements; i++){
		val[i-1].set(strCollItems[i].c_str());
//		buf = (char *)session->malloc((str.length() + 1)*sizeof(char));
//		strcpy(buf,str.c_str());
//		val[i].set(buf);
		val[i-1].op = OP_ADD;
		val[i-1].setPropID(propid);val[i-1].eid=STORE_FIRST_ELEMENT;
	}
	session->startTransaction();
	TVERIFYRC(pin->modify(val,cntElements));
#endif


	if ( inbRollback )
	{
		TVERIFYRC(session->rollback());
		// #3294

		TVERIFYRC(pin->refresh());

		const Value * backToOne = pin->getValue(propid);
		TVERIFY(backToOne!=NULL);
		if (backToOne!=NULL )
		{
			if(backToOne->type==VT_COLLECTION)
			{
				if (backToOne->isNav())
					TVERIFY(backToOne->nav->count()==1);
				else
				{
					TVERIFY(backToOne->length==1);
					TVERIFY(strCollItems[0]==backToOne->varray[0].str);
				}
			}
			else if ( backToOne->type==VT_STRING )
			{
				TVERIFY(strCollItems[0]==backToOne->str);
			}
			else
			{
				TVERIFY(!"Unexpected type");
			}
		}
	}
	else
	{
		TVERIFYRC(session->commit());
		verifyExpectedCollection(session,pin,propid,strCollItems,"transaction with commit") ;

		// Also check rollback after a later modification (12045)
		TVERIFYRC(session->startTransaction());
		Value collectionReplacer; collectionReplacer.set(1); collectionReplacer.setPropID(propid);
		TVERIFYRC(session->modifyPIN(pin->getPID(),&collectionReplacer,1));
		TVERIFYRC(session->rollback());

		verifyExpectedCollection(session,pin,propid,strCollItems,"transaction with commit") ;
	}

	pin->destroy();
	pin = NULL;

#if USE_VT_ARRAY
	session->free(val3);
#endif
}

void TestBig::testBigCollectionTransition(ISession * session, int elemSize)
{
	// Specific scenarios of 
	// a local collection moving to a "big collection" format
	// See #8357

	mLogger.out() << "Building collection, each element is " << elemSize << " bytes" << endl ;

	unsigned char * largeData = (unsigned char*) malloc( elemSize ) ;
	memset( largeData, 0, elemSize ) ;

	PropertyID prop = MVTApp::getProp( session, "bigCollectionElement" ) ;
	Value vals[256];
	for ( int i = 0 ; i < 256 ; i++ )
	{
		vals[i].set(largeData, elemSize ) ; 
		vals[i].property = prop ;
		vals[i].op = OP_ADD ;
		// v.meta = META_PROP_SSTORAGE ; // This fixes the issue
	}
	RC rc = session->createPIN(vals, 256, NULL, MODE_COPY_VALUES|MODE_PERSISTENT) ;
	if ( rc != RC_OK )
	{
		//RC_NOMEM, bug #8357
		mLogger.out() << "Failure to add element " << endl ;
	}
	delete(largeData);
}

void TestBig::testCollectionDataLeak(ISession * session, int elemSize)
{
	//#8358 scenario
	mLogger.out() << "Adding and deleting collection data. Each element is " << elemSize << " bytes" << endl ;
	IPIN *pin;
	session->createPIN(NULL, 0,&pin, MODE_PERSISTENT) ;

	unsigned char * largeData = (unsigned char*) malloc( elemSize ) ;
	memset( largeData, 0, elemSize ) ;

	PropertyID prop = MVTApp::getProp( session, "bigCollectionElement" ) ;

	for ( int i = 0 ; i < 2000 ; i++ )
	{
		Value v ;
		v.set( largeData, elemSize ) ; 
		v.property = prop ;
		v.op = OP_ADD ;
		// v.meta = META_PROP_SSTORAGE ; // This fixes the issue
		RC rc ;
		TVERIFYRC( rc = pin->modify(&v, 1 ) ) ;
		if ( rc != RC_OK )
		{
			mLogger.out() << "Failure to add element " << i << endl ;
			break ;
		}

		// Now delete newly added element
		Value v2 ;
		v2.setDelete( prop, v.eid ) ;
		TVERIFYRC( rc = pin->modify(&v2, 1 ) ) ;
		if ( rc != RC_OK )
		{
			mLogger.out() << "Failure to delete element " << i << endl ;
			break ;
		}
	}
	if(pin!=NULL) pin->destroy();
	delete( largeData ) ;
}

void TestBig::oneBigPin(bool bBigCollection, bool LOB)
{
	/* simple scenario for testing/debugging local or big collection + SSV layout.

	   WARNING Trial and error has been used to find values that produce the various possible layouts.
	   Any performance tweaking inside the kernel might break the assumptions.


	   Possible array layouts:

		Collection On Pin Page, Element Data On Pin Page       >> Basic case, no demo
		Collection On Pin Page, Element Data On SSV Page       >> Demonstrated when bBigCollection is false
		Collection On Pin Page, Element Data As BLOB           >> Demonstrated when bBigCollection is false
		Collection in Tree (BigC), Element Data remains on Big C Page  >> (Happens when bBigCollection is but sizeElement = 2000)
		Collection in Tree (BigC), Element Data On SSV Page    >> Demo when bBigCollection is true, LOB false REVIEW: MIGHT NOT BE WORKING!
		Collection in Tree (BigC), Element Data As BLOB        >> Demo when bBigCollection is true, LOB true
	*/
	int cntElements = bBigCollection ? 1000 : 10;	

	int sizeElement = 20000 ; //bBigCollection ? 20000 : 2000 ; 
	if ( LOB ) sizeElement = MVTApp::Suite().mPageSize * 2 ; // More than one page

	string randStr = MVTRand::getString2(sizeElement);
	PropertyID prop = MVTApp::getProp( mSession, "testbig.motherload" ) ;	

	Value *v = (Value*)mSession->malloc(sizeof(Value)*cntElements);
	for ( int i = 0 ; i < cntElements ; i++ )
	{
		v[i].set( randStr.c_str() ) ; v[i].meta = META_PROP_SSTORAGE; v[i].property=prop; v[i].op = OP_ADD;
	}

	Value vArr;
	vArr.set(v,cntElements);
	vArr.property=prop;
	if ( bBigCollection ) { vArr.meta=META_PROP_SSTORAGE; }
	IPIN *pin1;
	TVERIFYRC(mSession->createPIN(&vArr, 1, &pin1, MODE_PERSISTENT|MODE_COPY_VALUES)) ;

	int cnt=0 ;
	const Value * val = pin1->getValue(prop);

	TVERIFY( val!=NULL );

	if (val->type==VT_COLLECTION && val->isNav()) {
		TVERIFY( val->nav != NULL ) ;
		Value const * collElement = val->nav->navigate(GO_FIRST);
		while (collElement)
		{
			cnt++;
			collElement = val->nav->navigate(GO_NEXT);
		}

		TVERIFY(cnt==cntElements);
	}
	if(pin1!=NULL) pin1->destroy();
	mSession->free(v);
}

void TestBig::rcNoResources(ISession *session, int cntVals)
{
	// Searching for more scenarios that might have the
	// RC_NOMEM problem when building large collections
	
	mLogger.out() << "Building collections size " << cntVals << endl ;

	// It also demonstrates some of the many valid ways to build a collection

	PropertyID prop = MVTApp::getProp( session, "largeColl" ) ;

	// Same data will go on each pin
	vector<string> strVals(cntVals);
	Value * vals = (Value*)session->malloc(sizeof(Value)*cntVals);

	int i ;
	for ( i = 0 ; i < cntVals ; i++ )
	{
		strVals[i]=MVTRand::getString2(25,50) ;
		vals[i].set(strVals[i].c_str());
		vals[i].property=prop ;
		vals[i].op = OP_ADD ;
	}

	//Case 1
	TVERIFYRC( session->createPIN(vals, cntVals,NULL,MODE_COPY_VALUES|MODE_PERSISTENT)) ;

	//Case 2
	for (i=0;i<cntVals;i++) { vals[i].eid=STORE_COLLECTION_ID; }
	TVERIFYRC(session->createPIN(vals, cntVals, NULL, MODE_COPY_VALUES|MODE_PERSISTENT));

	//Case 3
	for (i=0;i<cntVals;i++) 
	{ 
		vals[i].eid=STORE_COLLECTION_ID; 
	}
	TVERIFYRC(session->createPIN(vals, cntVals,NULL, MODE_COPY_VALUES|MODE_PERSISTENT)) ;

	//Case 4
	//See 8357 and testBigCollectionTransition
	for (i=0;i<cntVals;i++) 
	{ 
		vals[i].eid=STORE_COLLECTION_ID; 		
	}
	RC rc = session->createPIN(vals, cntVals, NULL, MODE_COPY_VALUES|MODE_PERSISTENT) ;
#if TEST_COLLECTION_TRANSITION_8357
	TVERIFYRC2(rc,"case4");
#else
	if ( rc == RC_NOMEM)
	{
		mLogger.out() << "RC_NOMEM hit in case 4" << endl ;
	}
#endif

	// Case 5
	for (i=0;i<cntVals;i++) { vals[i].eid=STORE_COLLECTION_ID; }
	TVERIFYRC(session->createPIN(vals,cntVals,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));
	
	// Case 6
	IPIN* pin6;
	TVERIFYRC(session->createPIN( vals, cntVals, &pin6, MODE_NO_EID|MODE_COPY_VALUES|MODE_PERSISTENT) ) ;
	for (i=0;i<cntVals;i++) 
	{ 
		vals[i].eid=STORE_COLLECTION_ID; 
		TVERIFYRC(pin6->modify(&(vals[i]), 1, MODE_NO_EID)) ;
	}
	pin6->destroy() ;

	// Case 7: based on PinHelper.cpp with uncommitted pins
	// it works with batchs
	assert( cntVals%4==0) ;// test assumption
	const int pinChunk = cntVals/4;
	IPIN* pin7;
	TVERIFYRC(session->createPIN(vals, pinChunk,&pin7, MODE_COPY_VALUES|MODE_PERSISTENT));
	TVERIFYRC(pin7->modify( &(vals[pinChunk]),pinChunk, MODE_NO_EID )) ;
	TVERIFYRC(pin7->modify( &(vals[2*pinChunk]),pinChunk, MODE_NO_EID )) ;
	TVERIFYRC(pin7->modify( &(vals[3*pinChunk]),pinChunk, MODE_NO_EID )) ;
	pin7->destroy() ;

	// Case 8: based on PinHelper.cpp with uncommitted pins
	IPIN* pin8;
	TVERIFYRC(session->createPIN(vals, pinChunk, &pin8, MODE_COPY_VALUES|MODE_PERSISTENT)) ;
	TVERIFYRC(pin8->modify( &(vals[pinChunk]),pinChunk, MODE_NO_EID )) ;
	TVERIFYRC(pin8->modify( &(vals[2*pinChunk]),pinChunk, MODE_NO_EID )) ;
	TVERIFYRC(pin8->modify( &(vals[3*pinChunk]),pinChunk, MODE_NO_EID )) ;
	pin8->destroy() ;

	// TODO: more possibilities exist, e.g. with ISession::modifyPIN,
	// with VT_COLLECTION wrapper value etc

	session->free(vals);
}

void TestBig::testBigCollectionDownsize(bool bDeleteFromEnd)
{
	mLogger.out() << "testBigCollectionDownsize" <<endl ;

	// Variation of the "case 8" deletion scenario, but specifically bashing on
	// cases where the collection spans multiple pages and is being reduced in size
	const static int cntElements=10000; // Can repro 12322 even with 1000,10
	const static int batchSize=100;
	
	// Array to track what elements remain in the collection.
	// As an element is deleted we will set the corresponding position to 0
	ElementID *elems=(ElementID*)malloc(cntElements*sizeof(ElementID));

	PropertyID prop=MVTUtil::getProp(mSession,"testBigCollectionDownsize");

	//
	// Build the pin
	//
	PID pid;IPIN *pin;
	TVERIFYRC(mSession->createPIN(NULL,0,&pin,MODE_PERSISTENT));
	pid = pin->getPID();
	if(pin!=NULL) pin->destroy();

	assert(cntElements%batchSize==0);
	int cntBatches=cntElements/batchSize;
	int i,j;
	Value addBatch[batchSize]; string stringData[batchSize];

	for (j=0; j<batchSize; j++) 
	{
		// Using a mix of string sizes to try to excercise many different possible 
		// tree page arrangements.  (We don't want this data to be SSV)
		stringData[j]=MVTRand::getString2(1,1000); 
	}

	for (i=0; i<cntBatches;i++)
	{
		for (j=0; j<batchSize; j++) 
		{
			addBatch[j].set(stringData[j].c_str()); addBatch[j].property=prop;
			addBatch[j].op=OP_ADD;
		}
		TVERIFYRC(mSession->modifyPIN(pid,addBatch,batchSize));

		for (j=0; j<batchSize; j++) 
		{
			elems[i*batchSize+j]=addBatch[j].eid; // Remember the eids
		}
	}


	//	
	// Now brutal downsizing
	//
	int cntRemaining=cntElements;

	//TIP: To stop just before the corruption, run with verbose flag "-v"
	//and note the cntRemaining printed out.  Then set that value as the
	//stopping condition here:
	while(cntRemaining>0)
	{
		//
		//Find a batch of remaining elements to delete
		//
		int deleteBatch = 0;
		
		if (bDeleteFromEnd)
		{
			// Straightforward downsizing
			for( int z=cntRemaining-batchSize;z<cntRemaining;z++ )
			{
				addBatch[deleteBatch].setDelete(prop,elems[z]);
				deleteBatch++;			
				elems[z]=0; // Remember that we will erase this
			}
		}
		else
		{
			// More randomized 
			int pos=MVTRand::getRange(0,cntElements-1);

			if ( MVTRand::getBool() )
			{
				// Move forward
				while( pos < cntElements && deleteBatch < batchSize  )
				{
					if ( elems[pos]!=0 )
					{
						addBatch[deleteBatch].setDelete(prop,elems[pos]);
						deleteBatch++;			
						elems[pos]=0; // Remember that we will erase this
					}

					if (cntRemaining>cntElements/4)
					{
						pos+=MVTRand::getRange(1,10);
					}
					else
					{
						// Collection already fragmented, so delete more 
						pos++;
					}
				}
			}
			else
			{
				// Move backward
				while( pos >= 0 && deleteBatch < batchSize  )
				{
					if ( elems[pos]!=0 )
					{
						addBatch[deleteBatch].setDelete(prop,elems[pos]);
						deleteBatch++;			
						elems[pos]=0; // Remember that we will erase this
					}

					if (cntRemaining>cntElements/4)
					{
						pos-=MVTRand::getRange(1,10);
					}
					else
					{
						// Collection already fragmented, so delete more 
						pos--;
					}
				}

			}
		}

		if ( deleteBatch > 0 )
		{
			RC rc;
			TVERIFYRC(rc=mSession->modifyPIN(pid,addBatch,deleteBatch));

			if (rc!=RC_OK)
			{		
				mLogger.out() << "Failed to delete " << deleteBatch << " elements" << endl;
				mLogger.out() << "cntRemaining expected: " << cntRemaining <<endl;

				CmvautoPtr<IPIN> p(mSession->getPIN(pid));
				const Value * coll = p->getValue(prop);

				if (coll->type==VT_COLLECTION && coll->isNav()) for ( int z=0;z<deleteBatch;z++)
				{
					if (isVerbose())
						mLogger.out()<<"Verifying "<<std::hex<<addBatch[z].eid<<std::dec<<endl;

					Value v;
					if (RC_OK!=coll->nav->getElementByID(addBatch[z].eid,v))
					{
						mLogger.out() << "First missing eid is " <<std::hex<< addBatch[z].eid << dec<<endl;
						break;
					}
				}
				break;
			}
			cntRemaining-=deleteBatch;
		}

		if (isVerbose())
			mLogger.out() << cntRemaining << endl;

		//Check that collection is still valid

		CmvautoPtr<IPIN> p(mSession->getPIN(pid));
		const Value * coll = p->getValue(prop);
		TVERIFY(coll!=NULL||cntRemaining==0);
		if ( coll != NULL )
		{
			if ( coll->type == VT_STRING )
			{
				TVERIFY(cntRemaining==1);
			}
			else if ( coll->type==VT_COLLECTION && coll->isNav() )
			{
				bool bad=(coll->nav->count()!=uint32_t(cntRemaining));
				TVERIFY(coll->nav->count()==uint32_t(cntRemaining));

				int k=0;
				Value const * collElement = coll->nav->navigate(GO_FIRST);
				while (collElement)
				{
					while(elems[k]==0&&k<cntElements) k++;
					if(k==cntElements) 
					{
						TVERIFY(!"Expected no more elements but found something in navigator");
						MVTUtil::output(*collElement,mLogger.out(),mSession);
						bad=true;
						break;
					}
					TVERIFY(collElement->eid==elems[k]);
					collElement = coll->nav->navigate(GO_NEXT);
					k++;
				}
				
				if (!bad && k<cntElements) 
				{
					// Finished navigator so verify
					// that we don't expect anymore items
					TVERIFY(elems[k++]==0);
				}

				if (bad) break ; // No use going further becase it will be corruption from here on in

			}
		}
	}
}
