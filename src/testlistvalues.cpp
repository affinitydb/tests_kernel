/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

// ISession::listValues

#include "app.h"
#include "photoscenario.h"
#include "mvauto.h"
#include "classhelper.h"

class TestListValues : public PhotoScenario
{
	public:
		TEST_DECLARE(TestListValues);
		virtual char const * getName() const { return "testlistvalues"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Test for ISession::listValues (see 7469)"; }
        
		// Until feature more implemented
		virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
		virtual bool isStandAloneTest()const { return true; }
		virtual bool includeInBashTest(char const *& pReason) const { return true; }

		virtual void destroy() { delete this; }
	protected:
		virtual void doTest() ;
		virtual void initializeTest();
		void checkClass( const char * inName, PropertyID inProp, ValueType inExpectedType, unsigned int inCntVars = 0, Value * inVars = NULL, bool pGuest = false ) ;
};
TEST_IMPLEMENT(TestListValues, TestLogger::kDStdOut);

void TestListValues::checkClass( 
		const char * inName, 
		PropertyID inProp, 
		ValueType inExpectedType, 
		unsigned int inCntVars,				
		Value * inVars,
		bool pGuest)
{
	//NOTE: a more generic version of this has been established in classhelper.h,
	//so don't cut and paste this specialized one!

	ClassID lCls = getClass( inName ) ;
	if ( lCls == STORE_INVALID_CLASSID ) { TVERIFY(0) ; return ; }

	CmvautoPtr<IndexNav> lValEnum ; 

	//If you don't know inExpectedType then VT_ANY can be passed,
	//REVIEW: then the return type may not match the real index type, 
	//e.g. strings can come back as VT_BSTR with no null termination
	RC rc = mSession->listValues( lCls, inProp, lValEnum.Get() ) ;

	if(pGuest)	
	{
		TVERIFY(RC_NOACCESS == rc);
		TVERIFY(!lValEnum.IsValid() ) ;
		return ;
	}
	else
	{
		TVERIFY( RC_OK == rc ) ; 
	}

	size_t cntValues = 0 ;	
	size_t cntPins = 0 ;

	bool bNeedRange=ClassHelper::isRangeQuery(mSession,inName);

	for(;;)
	{
		const Value *lVal = lValEnum->next();
		if(lVal==NULL) break;

		if ( inExpectedType != VT_ANY )
			TVERIFY(lVal->type == inExpectedType);

		if ( isVerbose() )
		{
			if(lVal->type == VT_STRING) 
			{
				mLogger.out() << lVal->str << std::endl;
				TVERIFY(lVal->length == strlen(lVal->str));
			}
			else
				MVTApp::output(*lVal,mLogger.out(),mSession);
		}

		// Run family query based on this value
		CmvautoPtr<IStmt> q( mSession->createStmt() );
		ClassSpec r; 
		r.classID = lCls ;
		r.nParams=1 ;

		// OP_IN must have a VT_RANGE, so pass same value as start/end
		Value twoVals[2]; twoVals[0]=*lVal; twoVals[1]=*lVal;
		Value asRange; asRange.setRange(twoVals);

		if ( bNeedRange )
			r.params=&asRange;
		else
			r.params=lVal;

		q->addVariable(&r,1);

		uint64_t cnt = 0 ;
		TVERIFYRC(q->count(cnt));

		TVERIFY(cnt>0);

		if ( isVerbose() )
		{
			std::cout << cnt << " pins have this value" << endl;
		}

		cntPins+=(size_t)cnt;

		cntValues++ ;

		// VT_ANY as input to listValues means to return the "official" 
		// type of the class, rather than converting to the requested type
		if ( inExpectedType != VT_ANY )
			TVERIFY( lVal->type == inExpectedType );
	}

	mLogger.out() << "Total for " << inName << ":" << (int)cntValues << " values, " << (int)cntPins << " pins (may include dups)" << endl;
}

void TestListValues::initializeTest() 
{	
	mCreateAppPINs = true;
	mPinCount = 100;
	mNumPINsPerFolder = 10;
	mTagCount = 15;	
}

void TestListValues::doTest() 
{
	//image pins where tag_id = $tagName]
	Value lParam[1];
	lParam[0].set(mTagPool[0].c_str());
	checkClass( "taggedImages", tag_id, VT_STRING, 1, lParam ) ;

	//The date is of type VT_DATETIME, and the class is defined with 
	//a VT_RANGE.  		
	checkClass( "imageCluster", date_id, VT_DATETIME ) ;

	//Although VT_UINT64 has the same data size, it is not
	//considered interchangeable so this does not succeed: 	
	//checkClass( "imageCluster", date_id, VT_UINT64 ) ;
	
	// Its a recent improvement that you don't HAVE to know the type that you expect
	// in the family - the kernel can handle VT_ANY
	checkClass( "imageCluster", date_id, VT_ANY ) ;

	// test based on the PhotoScenario::createPhotoClasses() classes
	checkClass( "allImportedFiles", fs_path_id, VT_STRING ) ;
	checkClass( "allImportedFiles", fs_path_id, VT_ANY ) ;

	// TODO: need to add feedinfo and feedtype to pins so class is not empty
	// Right now we are adding only 1 type of feedtype (1)
	checkClass( "feed", feedtype_id, VT_INT ) ;	

	// Keep this as the last case
	// Check for ACLs on ISession::listValues()
	Tstring lIdentity = "TestListValues.Identity0";
	mSession->storeIdentity(lIdentity.c_str(), NULL, false);
	mSession->terminate();

	mSession = MVTApp::startSession(MVTApp::getStoreCtx(), lIdentity.c_str(), NULL);
	TVERIFY(mSession!=NULL);
	checkClass( "feed", feedtype_id, VT_INT, 0, 0, true );
}
