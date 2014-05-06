/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <fstream>
#include <stdlib.h>
#include "mvauto.h"
using namespace std;

// Publish this test.
class TestClassMembership : public ITest
{
		ISession * mSession;
	public:
		TEST_DECLARE(TestClassMembership);
		virtual char const * getName() const { return "testclassmembership"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test for testing class membership (IPIN::testClassMembership)"; }
		virtual bool isPerformingFullScanQueries() const { return true; }

		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testClassMembership();
		void testFamilyMembership();
};
TEST_IMPLEMENT(TestClassMembership, TestLogger::kDStdOut);

int TestClassMembership::execute()
{
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();
		testClassMembership();		
		testFamilyMembership();
		mSession->terminate();
		MVTApp::stopStore();
	}
	else
	{
		return RC_FALSE;
	}
	return RC_OK;
}

#define lAllClassNotifs (CLASS_NOTIFY_JOIN | CLASS_NOTIFY_LEAVE | CLASS_NOTIFY_CHANGE | CLASS_NOTIFY_DELETE | CLASS_NOTIFY_NEW)
void TestClassMembership::testClassMembership()
{
	Value val[2];
	Value args[2];
	ClassID	cls = STORE_INVALID_CLASSID, clsopexist = STORE_INVALID_CLASSID;
	PropertyID pm[3];
	IPIN *pin;
	bool isinclass;
	RC rc;

	MVTApp::mapURIs(mSession,"TestClassMembership",3,pm);

	//Case 1: Create a class with a simple OP_EQ

	IStmt *classquery = mSession->createStmt();
	unsigned var = classquery->addVariable();

	//class 1 with full scan query to first pid
	args[0].setVarRef(0,(pm[0])); 
	args[1].set("Bipasha");

	IExprTree *expr = mSession->expr(OP_EQ,2,args);
	TVERIFYRC(classquery->addCondition(var,expr));

	// Generate classname with random component
	char lB[100];
	Tstring lStr; MVTRand::getString(lStr,10,10,false,false);
	sprintf(lB,"TestClassMembership.%s", lStr.c_str());
	TVERIFYRC(defineClass(mSession,lB,classquery,&cls));
	TVERIFYRC(mSession->enableClassNotifications(cls,lAllClassNotifs)); 

	val[0].set("Bipasha");val[0].setPropID(pm[0]);
	val[1].set("jism");val[1].setPropID(pm[1]);

	TVERIFYRC(mSession->createPIN(val,2,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));

	//test for membership
	isinclass = pin->testClassMembership(cls);
	TVERIFY(isinclass);

	expr->destroy();
	classquery->destroy();

	//
	//Case 1.B  Class member can be tested with query that has 
	// expression of OP_IN for VT_CLASS
	//
	CmvautoPtr<IStmt> lQ1B( mSession->createStmt());
	QVarID v = lQ1B->addVariable();
	Value operands[2] ; 
	operands[0].setVarRef(0);
	operands[1].setURIID( cls ) ;
	CmvautoPtr<IExprTree> lE1B( mSession->expr( OP_IS_A, 2, operands ) );
	TVERIFYRC(lQ1B->addCondition( v, lE1B )) ;
	uint64_t cnt ;
	TVERIFYRC(lQ1B->count( cnt )) ;
	TVERIFY( cnt == 1 ) ;

	//Case 2: test class membership with two op exists.
	//Any PIN with either pm[0]
	IStmt *classopexist = mSession->createStmt();
	unsigned varexist = classopexist->addVariable();

	args[0].setVarRef(0,(pm[0]));
	IExprTree *classopexistexpr1 = mSession->expr(OP_EXISTS,1,args);

	Value args1[1];

	args1[0].setVarRef(0,(pm[1]));
	IExprTree *classopexistexpr2 = mSession->expr(OP_EXISTS,1,args1);

	Value final[2];
	final[0].set(classopexistexpr1);
	final[1].set(classopexistexpr2);
	IExprTree *exprexistfinal = mSession->expr(OP_LOR,2,final);

	TVERIFYRC(classopexist->addCondition(varexist,exprexistfinal));
	
	MVTRand::getString(lStr,10,10,false,false);
	sprintf(lB,"TestClassMembership.%s.raju", lStr.c_str());

	defineClass(mSession,lB,classopexist,&clsopexist);
	mSession->enableClassNotifications(clsopexist,lAllClassNotifs); 

	//should return the above pin cos it has both props
	isinclass = pin->testClassMembership(clsopexist);
	if ( !isinclass )
	{
		TVERIFY2(0,"Expected pin to be member of class" ) ;
		MVTApp::output(*pin,mLogger.out(),mSession) ;
		char * strQuery = classopexist->toString() ;
		mLogger.out() << strQuery << endl ;
		mSession->free(strQuery) ;
	}

	classopexist->destroy();
	pin->destroy();
	exprexistfinal->destroy();

	// Verify that pin having only one of the properties also shows up
	val[0].set("Somethingelse");val[0].setPropID(pm[0]);
	IPIN* pin2;
	TVERIFYRC(mSession->createPIN(val,1,&pin2,MODE_PERSISTENT|MODE_COPY_VALUES));
	TVERIFY(pin2->testClassMembership(clsopexist)) ;
	pin2->destroy();
	// PIN with other props should not show up
	val[0].set("uninteresting prop");val[0].setPropID(pm[2]);
	IPIN* pin3;
	TVERIFYRC(mSession->createPIN(val,1,&pin3,MODE_PERSISTENT|MODE_COPY_VALUES));
	TVERIFY(!pin3->testClassMembership(clsopexist)) ;
	pin3->destroy();

	//Case 3: test for app issue after upgrade: get pin with specific pids and test for class membership
	//REVIEW: This only works with some specific store that had some specific predefined classes,
	//unclear how it could be used in future, perhaps it should be removed.
	IPIN *clspin;
	PID clspid;
	ClassID clsapp = STORE_INVALID_CLASSID;
	clspid.pid=1641843539153715255LL;  // HARDCODED
	clspid.ident=STORE_OWNER;
	clspin = mSession->getPIN(clspid);
	if (!clspin)
		return; // Normally test exists here

	MVTApp::output(*clspin,mLogger.out(),mSession);
	//Defn: ("blog","/pin[pin has uriname]")
	rc = mSession->getClassID("blog",clsapp);
	mSession->enableClassNotifications(clsapp,lAllClassNotifs);
	isinclass = clspin->testClassMembership(clsapp);

	//("document","/pin[pin has title]")
	rc = mSession->getClassID("document",clsapp);
	mSession->enableClassNotifications(clsapp,lAllClassNotifs);
	isinclass = clspin->testClassMembership(clsapp);

	//("filetype","/pin[pin has mime]") -- should not satisfy but does
	rc = mSession->getClassID("filetype",clsapp);
	mSession->enableClassNotifications(clsapp,lAllClassNotifs);
	isinclass = clspin->testClassMembership(clsapp);

	//("systempin","/pin[prop_system='true']") -- works fine
	rc = mSession->getClassID("systempin",clsapp);
	mSession->enableClassNotifications(clsapp,lAllClassNotifs);
	isinclass = clspin->testClassMembership(clsapp);

	//("contact","/pin[pin has email and pin has name]") should not satisfy but does.
	rc = mSession->getClassID("contact",clsapp);
	mSession->enableClassNotifications(clsapp,lAllClassNotifs);
	isinclass = clspin->testClassMembership(clsapp);

	rc = mSession->getClassID("searchSet",clsapp);
	mSession->enableClassNotifications(clsapp,lAllClassNotifs);
	isinclass = clspin->testClassMembership(clsapp);

	rc = mSession->getClassID("searchSet2",clsapp);
	mSession->enableClassNotifications(clsapp,lAllClassNotifs);
	isinclass = clspin->testClassMembership(clsapp);
	//works fine.
	rc = mSession->getClassID("raju",clsapp);
	mSession->enableClassNotifications(clsapp,lAllClassNotifs);
	isinclass = clspin->testClassMembership(clsapp);

	clspin->destroy();
}

void TestClassMembership::testFamilyMembership()
{
		mLogger.out() << " Testing IPIN::testClassMembership for Families ... " << std::endl;
		static const int sNumProps = 10;
		PropertyID lPropIDs[sNumProps];
		MVTApp::mapURIs(mSession, "TestClassMembership.testFamilyMembership.prop.",sNumProps, lPropIDs);
		Tstring lClassStr; MVTRand::getString(lClassStr, 5, 10, false, true);
		int lFamilyIndex = 0;

		mLogger.out() << "\t Case #1 : Testing Simple Family /pin[propA ciin $var) " << std::endl;
		{
			// Create a simple family
			// Family1($var) = /pin[propA in $var)
			ClassID lFamilyID = STORE_INVALID_CLASSID;
			{
				CmvautoPtr<IStmt> lQ( mSession->createStmt());
				unsigned char lVar = lQ->addVariable();
				IExprTree *lET;
				{
					Value lV[2];
					lV[0].setVarRef(0,lPropIDs[0]);
					lV[1].setParam(0);
					lET = mSession->expr(OP_IN, 2, lV);
				}
				TVERIFYRC(lQ->addCondition(lVar,lET));	lET->destroy();		
				char lB[128]; sprintf(lB, "TestClassMembership.%s.Family%d", lClassStr.c_str(), lFamilyIndex++);
				TVERIFYRC(defineClass(mSession,lB, lQ, &lFamilyID));
			}

			// Create few PINs
			std::vector<PID> lPIDs;
			int i = 0;
			for(i = 0; i < 10; i++)
			{
				PID lPID = {STORE_INVALID_PID, STORE_OWNER};
				Value lV[2];
				SETVALUE(lV[0], lPropIDs[0], i, OP_SET);
				CREATEPIN(mSession, &lPID, lV, 1); TVERIFY(lPID.pid != STORE_INVALID_PID);
				if(lPID.pid != STORE_INVALID_PID) lPIDs.push_back(lPID);
			}
			
			// List all PINs in the Family
			if(isVerbose())
			{
				mLogger.out() << "PINs part of Family " << std::endl;
				CmvautoPtr<IStmt> lQ( mSession->createStmt());
				SourceSpec lCS = {lFamilyID, 0, NULL};
				lQ->addVariable(&lCS, 1);
				ICursor* lC = NULL;
				TVERIFYRC(lQ->execute(&lC));
				CmvautoPtr<ICursor> lR(lC);
				PID lPID;
				if(lR.IsValid()) while(RC_OK == lR->next(lPID))
					mLogger.out() << std::hex << lPID.pid << std::endl;
			}
			// Test Each PIN if it belongs to Family1 or NOT			
			for(i = 0; i < (int)lPIDs.size(); i++)
			{
				Value lParam[3]; 
				if(isVerbose()) mLogger.out() << " Testing PIN(" << std::hex << lPIDs[i].pid << ")" << std::endl;
				IPIN *lPIN = mSession->getPIN(lPIDs[i]); TVERIFY(lPIN != NULL);
				// Check with no Params as kernel now supports Family queries with NULL params(returns all pins using index)
				TVERIFY(lPIN->testClassMembership(lFamilyID));

				// Check with params
				lParam[0].set(i); lParam[1].set(i); lParam[2].setRange(&lParam[0]); 
				TVERIFY(lPIN->testClassMembership(lFamilyID, &lParam[2], 1));

				// Check with huge range
				lParam[0].set(0); lParam[1].set(100); lParam[2].setRange(&lParam[0]); 
				TVERIFY(lPIN->testClassMembership(lFamilyID, &lParam[2], 1));

				// Check with wrong params
				lParam[0].set(i+1); lParam[1].set(i+100); lParam[2].setRange(&lParam[0]); 
				TVERIFY(!lPIN->testClassMembership(lFamilyID, &lParam[2], 1));
				
				if(lPIN) lPIN->destroy();
			}
		}
}
