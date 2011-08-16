/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <fstream>
#include <map>
#include <stdlib.h>
#include "mvauto.h"
using namespace std;
#define NOPINS 30
#define NOELTS 100
#define NOELTS_BIG 300

#define TEST_QUERY_TO_STRING_PROBLEM // Fixed!

// This test verifies whether family queries are able to find collection elements

// TODO: add huge collection coverage

// Publish this test.
class TestCollFamily : public ITest
{
	private:
		vector<Tstring> strmap;
	public:
		static const int sNumProps = 3;
		PropertyID mPropIDs[sNumProps];
		std::vector<Tstring> mFamilyNames;
		TEST_DECLARE(TestCollFamily);
		virtual char const * getName() const { return "testcollfamily"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "quick test for family indexing with OP_EQ, OP_CONTAINS etc"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void defineFamilies(ISession *session);
		void testGenDataAndRun(ISession *session, int op);
		void testOperators(ISession *session, int op,Tstring &qstr);
		PID testPIN( ISession *session, PropertyID collPropID, int i /*pin index*/) ;
		void testCollDups(ISession *pSession);

		bool mBigCollection ;
};
TEST_IMPLEMENT(TestCollFamily, TestLogger::kDStdOut);

int TestCollFamily::execute()
{
	if (!MVTApp::startStore()) return RC_FALSE;

	ISession * const session = MVTApp::startSession();		

	mBigCollection = false ;

	MVTApp::mapURIs(session,"TestCollFamily.prop",sNumProps,mPropIDs);
	defineFamilies(session);
	//OP_EQ
	testGenDataAndRun(session,0);
	//OP_BEGINS
	testGenDataAndRun(session,1);
	mFamilyNames.clear();

	mLogger.out() << "Repeating tests with big collections" ;
	mBigCollection = true ;

	MVTApp::mapURIs(session,"TestCollFamily.prop",sNumProps,mPropIDs);
	defineFamilies(session);
	//OP_EQ
	testGenDataAndRun(session,0);
	//OP_BEGINS
	testGenDataAndRun(session,1);
	// Issue with de-duplication of pins returne for OP_BEGINSWITH on a family query for the same PIN
	testCollDups(session);

	mFamilyNames.clear();

	session->terminate();
	MVTApp::stopStore();

	return RC_OK  ;
}
void TestCollFamily::defineFamilies(ISession *session)
{
	// Note: Randoms string are used in the class name,
	// so each test execution is isolated
	ClassID cls = STORE_INVALID_CLASSID;	
	Tstring lName; MVTRand::getString(lName,10,10,false,false);
	char lB[100];
	sprintf(lB,"testCollFamily.OP_EQ%s.%d", lName.c_str(), 1); mFamilyNames.push_back(lB);
	sprintf(lB,"testCollFamily.OP_BEGINS%s.%d", lName.c_str(), 1); mFamilyNames.push_back(lB);

	//OP_EQ family
	if (RC_NOTFOUND == session->getClassID(mFamilyNames[0].c_str(),cls)){
		IStmt *query = session->createStmt();
		unsigned char var = query->addVariable();
		Value args[2],args1[2],argsfinal[2];

		// Class Query is : "prop0 contains "image" AND prop1=parameter 0
		args[0].setVarRef(0,(mPropIDs[0]));
		args[1].set("image");
		IExprTree *expr = session->expr(OP_CONTAINS,2,args,CASE_INSENSITIVE_OP);

		args1[0].setVarRef(0,(mPropIDs[1]));
		args1[1].setParam(0);  // This is what makes it a "family" query. The 
							   // string to match will be provided at query time
		IExprTree *expr1 = session->expr(OP_EQ,2,args1);

		argsfinal[0].set(expr);
		argsfinal[1].set(expr1);
		IExprTree *exprfinal = session->expr(OP_LAND,2,argsfinal);
		query->addCondition(var,exprfinal);		
		TVERIFYRC(defineClass(session,mFamilyNames[0].c_str(), query));

		query->destroy();
		exprfinal->destroy();
	}
	if (RC_NOTFOUND == session->getClassID(mFamilyNames[1].c_str(),cls)){
		IStmt *query = session->createStmt();
		unsigned char var = query->addVariable();
		Value args[2];

		// Class query "prop2 begins with Parameter 0"
		args[0].setVarRef(0,(mPropIDs[2]));
		args[1].setParam(0);
		IExprTree *expr = session->expr(OP_BEGINS,2,args);
		query->addCondition(var,expr);

#ifdef TEST_QUERY_TO_STRING_PROBLEM
		/*
		Used to hit this assert: 
		piquery.cpp
		buf.append("\t\tCondIdx:\t",11); assert(ci->op>=OP_EQ && ci->op<=OP_IN);

		-----testcollfamily Starting---------
		QUERY.100(0) {
				Var:0 {
						CondIdx:        /TestCollFamily.propIbmyXgMnRdl.2 (null) $0
						CondProps:              TestCollFamily.propIbmyXgMnRdl.2
				}
		}
		*/
		char * queryString = query->toString() ;
		mLogger.out() << queryString << endl ;
		session->free( queryString ) ; queryString = NULL ;
#endif

		TVERIFYRC(defineClass(session,mFamilyNames[1].c_str(), query));
		query->destroy();
		expr->destroy();
	}
}

PID TestCollFamily::testPIN( ISession *session, PropertyID collPropID, int i /*pin index*/ )
{
	Tstring str,qstr;
	Value val[5];

	IPIN *pin = session->createUncommittedPIN();

	// Add collection elements of random strings for prop1 or prop2
	int cntElements = mBigCollection ? NOELTS_BIG : NOELTS ;

	for (int j=0; j < cntElements; j++){
		MVTRand::getString(str,30,0,false,false);// all normalized case
		
		strmap.push_back(str); // Remember this value

		val[0].set(str.c_str());val[0].setPropID(collPropID);
		val[0].op = OP_ADD; val[0].eid = STORE_LAST_ELEMENT;

		if ( mBigCollection )
		{
			val[0].meta = META_PROP_SSTORAGE ;
		}
		TVERIFYRC(pin->modify(val,1,MODE_COPY_VALUES));
	}

	//Also string containing image to prop0
	val[0].set("image/jpg");val[0].setPropID(mPropIDs[0]);
	TVERIFYRC(pin->modify(val,1,MODE_COPY_VALUES));
	TVERIFYRC(session->commitPINs(&pin,1));

	PID pid = pin->getPID();

//	mLogger.out() << std::hex << pid.pid << endl ;

	pin->destroy();

	if(0 == i % 5){
		//On every 5th PIN also insert another collection item
		pin = session->getPIN(pid);
		TVERIFY(pin != NULL) ;
		if (NULL != pin){
			MVTRand::getString(str,20,0,false,false);
			strmap.push_back(str);
			val[0].set(str.c_str());val[0].setPropID(collPropID);
			val[0].op = OP_ADD_BEFORE; val[0].eid = STORE_LAST_ELEMENT;
			TVERIFYRC(pin->modify(val,1));
			pin->destroy();
		}
	}
	//modify a specific element id
	if(0 == i % 10){

		CmvautoPtr<IPIN> pin(session->getPIN(pid));
		TVERIFY(pin.IsValid()) ;
		if (pin.IsValid()){
			ElementID eid = STORE_COLLECTION_ID;

			Value dval = *pin->getValue(collPropID);
			size_t len = MVTApp::getCollectionLength(*pin->getValue(collPropID));		
			if (dval.type == VT_COLLECTION) {
				eid = dval.nav->navigate(GO_FIRST)->eid ;

				// Get the eid of a random element
				size_t targetIndex = (size_t)MVTRand::getRange(0,(int)len-1);
				for (size_t x=0; x < targetIndex; x++)
					eid = dval.nav->navigate(GO_NEXT)->eid;

				// Retreive the value of the same random element again via the navigator GO_FINDBYID
				qstr = dval.nav->navigate(GO_FINDBYID,eid)->str;
			} else if (dval.type == VT_ARRAY) {
				size_t targetIndex = (size_t)MVTRand::getRange(0,(int)len-1);
				eid = dval.varray[targetIndex].eid;
				qstr = dval.varray[targetIndex].str;
			} else TVERIFYRC(RC_TYPE);

			// Look for the string in the map of all the strings we added and
			// if found add another element in the collection before the random element
			vector<Tstring>::iterator it;
			for(it=strmap.begin();strmap.end() != it; it++){
				if (it->c_str() == qstr){
					MVTRand::getString(str,30,0,false,false);
					//strmap.erase(it);	 // REVIEW: Why remove it?  it is still in the collection
					strmap.push_back(str);
					val[0].set(str.c_str());val[0].setPropID(collPropID);
					val[0].op = OP_ADD_BEFORE; val[0].eid = eid;
					TVERIFYRC(pin->modify(val,1));
				}
			}
		}
	}

	// test adding collection elements one by one

	val[0].set("image/jpg");val[0].setPropID(mPropIDs[0]);
	pin = session->createUncommittedPIN(val,1,MODE_COPY_VALUES);
	TVERIFYRC(session->commitPINs(&pin,1));

	for (int j=0; j < cntElements; j++){
		MVTRand::getString(str,30,0,false,false);// all normalized case
		
		strmap.push_back(str); // Remember this value

		val[0].set(str.c_str());val[0].setPropID(collPropID);
		val[0].op = OP_ADD; val[0].eid = STORE_LAST_ELEMENT;

		if ( mBigCollection )
		{
			val[0].meta = META_PROP_SSTORAGE ;
		}
		TVERIFYRC(pin->modify(val,1));
	}

	pin->destroy();

	return pid ;
}

void TestCollFamily::testGenDataAndRun(ISession *session,int op)
{
	int i;
	PropertyID propid = STORE_INVALID_PROPID;
	//removing duplicate strings before hand.
	vector<Tstring>::iterator it1,it2;
	for (it1=strmap.begin();strmap.end() != it1; it1++){
		for(it2=strmap.begin();strmap.end() != it2;it2++){
			if(it1==it2) it1->erase();
		}
	}
	switch(op){
		case(0):
			propid=mPropIDs[1];
			break;
		case(1):
			propid=mPropIDs[2];
			break;
		default:
			break;
	}

	PID lastpin ;
	for (i=0; i < NOPINS; i++)
	{	
		PID pid = testPIN( session, propid, i ) ;
		if ( i == NOPINS -1 )
			lastpin = pid ;
	}

	//Delete a random element id from the last PIN added
	Tstring deletedStr ;

	CmvautoPtr<IPIN> pin(session->getPIN(lastpin));
	TVERIFY(pin.IsValid()) ;
	if (pin.IsValid()){
		ElementID eid = STORE_COLLECTION_ID;
		Value dval = *pin->getValue(propid);
		size_t len = MVTApp::getCollectionLength(dval);
		if (dval.type==VT_COLLECTION) {
			dval.nav->navigate(GO_FIRST);
			size_t targetIndex = (1 + rand()%len);
			targetIndex = targetIndex==len?len-1:targetIndex;
			for (size_t x=0; x < targetIndex; x++)
				eid = dval.nav->navigate(GO_NEXT)->eid;

		// Note: this string is still be in the string map,
			deletedStr = dval.nav->navigate(GO_FINDBYID,eid)->str;
		} else if (dval.type == VT_ARRAY) {
			size_t targetIndex = (1 + rand()%len);
			targetIndex = targetIndex==len?len-1:targetIndex;
			eid = dval.varray[targetIndex].eid;
			deletedStr = dval.varray[targetIndex].str;
		} else TVERIFYRC(RC_TYPE);

		Value val ;
		val.setDelete(propid,eid);
		TVERIFYRC(pin->modify(&val,1));
	}

	testOperators(session,op,deletedStr);
	strmap.clear();
}

void TestCollFamily::testOperators(ISession *session, int op,Tstring &deletedstr)
{
	vector<Tstring>::iterator it;
	Tstring str;

	// Go through all the strings that have been added as collection items
	for (it=strmap.begin();strmap.end() != it; it++){
		ClassID cls = STORE_INVALID_CLASSID;
		ClassSpec csp[1];
		IStmt *query;
		unsigned char var;
		uint64_t cnt =0;
		Value args[1];

		switch (op){
			case (0): 
				// Class Query is : "prop0 contains "image" AND prop1=parameter 0
				session->getClassID(mFamilyNames[0].c_str(),cls);
				args[0].set(it->c_str());
				break ;
			case(1):
				// Class query "prop2 begins with Parameter 0"
				session->getClassID(mFamilyNames[1].c_str(),cls);
				str = it->substr(0,10);
				args[0].set(str.c_str());

				break ;
			default: TVERIFY(0) ; break ;
		}

		cnt =0;
		query = session->createStmt();

		csp[0].classID = cls;
		csp[0].nParams=1;
		csp[0].params =args;
		var = query->addVariable(csp,1);
		query->count(cnt);
		if (it->c_str() == deletedstr)
			// Item was intentionally removed from pin but left in strmap
			// so it should not be found
			TVERIFY(0 == cnt); 
		else
			TVERIFY(1 == cnt);
		query->destroy();
	}
}

/*
	Test for duplicate pins returned for a family query.
	If a collection has elements like 'test1', 'test2', 'test3', ... 'testN'
	and running a family like /pin[pin is family("test")] 
	where family($var) = /pin[prop begins with $var]
	returns N duplicate pins having the above collection
*/
void TestCollFamily::testCollDups(ISession *pSession)
{
	Tstring lStr; MVTRand::getString(lStr, 10, 0, false, false);
	Value lV[1];
	IPIN *lPIN = pSession->createUncommittedPIN();
	int j = 0;
	for (j = 0; j < 10; j++ )
	{		
		char lBuf[16];
		sprintf(lBuf, "%s%d", lStr.c_str(), j);
		lV[0].set(lBuf); lV[0].setPropID(mPropIDs[2]);
		lV[0].op = OP_ADD; lV[0].eid = STORE_LAST_ELEMENT;

		TVERIFYRC(lPIN->modify(lV,1));
	}
	TVERIFYRC(pSession->commitPINs(&lPIN,1));
	if(lPIN) lPIN->destroy();

	ClassID lCLSID = STORE_INVALID_CLASSID;
	ClassSpec lCS;
	TVERIFYRC(pSession->getClassID(mFamilyNames[1].c_str(), lCLSID));
	
	Value lParam;
	lParam.set(lStr.c_str());
	lCS.classID = lCLSID;
	lCS.nParams = 1;
	lCS.params = &lParam;
	
	IStmt *lQ = pSession->createStmt();
	lQ->addVariable(&lCS, 1);	
	uint64_t lCount = 0;	
	TVERIFYRC(lQ->count(lCount));

	TVERIFY(1 == lCount); 
	MVTUtil::findDuplicatePins(lQ,mLogger.out());
	lQ->destroy();
}
