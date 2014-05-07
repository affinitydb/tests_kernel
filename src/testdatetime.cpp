/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

#define TEST_DATETIME_TO_STR_CONV 0 // Assert Expression: top<stack+lStack, line 33 in piexpr.cpp

using namespace std;
class TestDateTime : public ITest
{
	public:
		static const int lNumPINs = 1000;
		PID lPIDs[lNumPINs];
		TEST_DECLARE(TestDateTime);
		virtual char const * getName() const { return "testdatetime"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "test for date/time transformations to/from external form"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void printDateTime(DateTime dts);
		bool compareDT(DateTime pExpected, DateTime pActual);
		void queryDTPart();
		void exprDTPart();
		void testPinTimeStamps();
		void conversionExpressions();
	protected:
		ISession * mSession ;
};
TEST_IMPLEMENT(TestDateTime, TestLogger::kDStdOut);

int	TestDateTime::execute()
{
	bool lSuccess =	true;
	if (MVTApp::startStore())
	{
		mSession =	MVTApp::startSession();

		conversionExpressions();

		TIMESTAMP lTS;

		//Case #1
		DateTime lDT1 = {2005,07,06,30,20,04,23,968750};//127672274639680750
		DateTime lDT2;		
		mSession->convDateTime(lDT1,lTS);printDateTime(lDT1);
		mSession->convDateTime(lTS,lDT2);printDateTime(lDT2);	

		TVERIFY2(compareDT(lDT1,lDT2),"Error(execute): Case #1 - Mismatch in DateTime");

		//Case #2
		getTimestamp(lTS);TIMESTAMP lTS1;
		mSession->convDateTime(lTS,lDT1);
		printDateTime(lDT1);
		mLogger.out() << "Current TimeStamp: " << lTS << std::endl;
		mSession->convDateTime(lDT1,lTS1);
		mLogger.out() << "Converted TimeStamp from DateTime: " << lTS1 << std::endl;

		TVERIFY2(lTS == lTS1,"Error(execute): Case #2 - Mismatch in DateTime");

		//Case #3
		exprDTPart();

		// Case #4
		queryDTPart();

		testPinTimeStamps();	

		mSession->terminate(); mSession = NULL ;
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
	return lSuccess	? 0	: 1;
}

void TestDateTime::printDateTime(DateTime pDTS)
{
#if TEST_DATETIME_TO_STR_CONV
	// Use the store
	Value v ;
	v.setDateTime(0) ;
	TVERIFYRC(mSession->convDateTime(pDTS,v.ui64));

	CmvautoPtr<IExprNode> et( mSession->expr( OP_TOSTRING, 1, &v, 0 ) );
	CmvautoPtr<IExpr> e(et->compile()) ;

	Value resAsString ;
	TVERIFYRC(e->execute( resAsString, NULL, 0 )) ;	
	mLogger.out() << resAsString.str << endl ; 
	mSession->free( const_cast<char*>(resAsString.str) ) ;
#else
	std::cout<<pDTS.month<<"/"<<pDTS.day<<"/"<<pDTS.year<<"  "<<pDTS.hour<<":"<<pDTS.minute<<":"<<pDTS.second<<" "<<pDTS.microseconds<<"ms"<<std::endl;
#endif
}

bool TestDateTime::compareDT(DateTime pExpected, DateTime pActual)
{
	if(pExpected.day != pActual.day || pExpected.dayOfWeek != pActual.dayOfWeek || pExpected.hour != pActual.hour || pExpected.year != pActual.year)
		return false;
	if(pExpected.microseconds != pActual.microseconds || pExpected.minute != pActual.minute || pExpected.month != pActual.month || pExpected.second != pActual.second)
		return false;
	return true;
}

void TestDateTime::queryDTPart()
{
	bool lSuccess = true;
	int i = 0;
	unsigned int lUI = 0;
	TIMESTAMP lTS;getTimestamp(lTS); // That is time in UTC
	DateTime lDT;mSession->convDateTime(lTS,lDT,true /*UTC*/);
	//Create a PIN to Query on
	PropertyID lPropId;
	{
		URIMap lData;
		lData.URI = "testdatetime.datetime"; lData.uid = STORE_INVALID_URIID; mSession->mapURIs(1, &lData);
		lPropId = lData.uid;
		Value lV[2];
		lV[0].setDateTime(lTS);lV[0].setPropID(lPropId);
		if(RC_OK!= mSession->createPIN(lV,1,NULL,MODE_PERSISTENT|MODE_COPY_VALUES)){
			mLogger.out() << "ERROR(queryDTPart) :Failed to create PIN" << std::endl;
			lSuccess = false;
		}
	}

	for(i = 0; i < 7 && lSuccess; i++)
	{
		IStmt *lQ = mSession->createStmt();
		unsigned const char lVar = lQ->addVariable();
		Value lV[2];
		IExprNode *lE = NULL;
		switch(i)
		{
			case 0: //OP_YEAR
				{
					lV[0].setVarRef(0,lPropId);
					lV[1].set(EY_YEAR);
					IExprNode *lE1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lE1); lUI = lDT.year;
					lV[1].set(lUI);
					lE = mSession->expr(OP_EQ,2,lV);
					mLogger.out() << "Running Query for OP_YEAR: ";
				}
				break;
			case 1: //OP_MONTH
				{
					lV[0].setVarRef(0,lPropId);
					lV[1].set(EY_MONTH);
					IExprNode *lE1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lE1); lUI = lDT.month;
					lV[1].set(lUI);
					lE = mSession->expr(OP_EQ,2,lV);
					mLogger.out() << "Running Query for OP_MONTH: ";
				}
				break;
			case 2: //OP_WDAY
				{
					lV[0].setVarRef(0,lPropId);
					lV[1].set(EY_WDAY);
					IExprNode *lE1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lE1); lUI = lDT.dayOfWeek;
					lV[1].set(lUI);
					lE = mSession->expr(OP_EQ,2,lV);
					mLogger.out() << "Running Query for OP_WDAY: ";
				}
				break;
			case 3: //OP_DAY
				{
					lV[0].setVarRef(0,lPropId);
					lV[1].set(EY_DAY);
					IExprNode *lE1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lE1); lUI = lDT.day;
					lV[1].set(lUI);
					lE = mSession->expr(OP_EQ,2,lV);
					mLogger.out() << "Running Query for OP_DAY: ";
				}
				break;
			case 4: //OP_HOUR
				{
					lV[0].setVarRef(0,lPropId);
					lV[1].set(EY_HOUR);
					IExprNode *lE1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lE1); lUI = lDT.hour;
					lV[1].set(lUI);
					lE = mSession->expr(OP_EQ,2,lV);
					mLogger.out() << "Running Query for OP_HOUR: ";
				}
				break;
			case 5: //OP_MINUTE
				{
					lV[0].setVarRef(0,lPropId);
					lV[1].set(EY_MINUTE);
					IExprNode *lE1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lE1); lUI = lDT.minute;
					lV[1].set(lUI);
					lE = mSession->expr(OP_EQ,2,lV);
					mLogger.out() << "Running Query for OP_MINUTE: ";
				}
				break;
			case 6: //OP_SECOND
				{
					lV[0].setVarRef(0,lPropId);
					lV[1].set(EY_SECOND);
					IExprNode *lE1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lE1); lUI = lDT.second;
					lV[1].set(lUI);
					lE = mSession->expr(OP_EQ,2,lV);
					mLogger.out() << "Running Query for OP_SECOND: ";
				}
				break;
		}
		lQ->addCondition(lVar,lE);
		uint64_t lCount = 0;
		lQ->count(lCount);
		TVERIFY(lCount!=0);
		lE->destroy();
		lQ->destroy();
	}
}

void TestDateTime::exprDTPart()
{
	bool lSuccess = true;
	int i = 0;
	TIMESTAMP lTS;getTimestamp(lTS);
	DateTime lDT;mSession->convDateTime(lTS,lDT,true/*UTC*/);	
	for(i = 0; i < 7 && lSuccess; i++)
	{
		unsigned int lUI = 0, lExpUI = 0;
		Value lV[2];
		IExprNode *lET = NULL;
		switch(i){
			case 0: //OP_YEAR
				{
					lV[0].setDateTime(lTS);
					lV[1].set(EY_YEAR);
					IExprNode *lET1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lET1); lUI = lDT.year;
					lV[1].set(lUI);
					lET = mSession->expr(OP_PLUS,2,lV);
					lExpUI = lUI * 2;
					mLogger.out() << "exprDTPart test for OP_YEAR: ";
				}
				break;
			case 1: //OP_MONTH
				{
					lV[0].setDateTime(lTS);
					lV[1].set(EY_MONTH);
					IExprNode *lET1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lET1); lUI = 12;
					lV[1].set(lUI);
					lET = mSession->expr(OP_MINUS,2,lV);
					lExpUI = lDT.month - 12;
					mLogger.out() << "exprDTPart test for OP_MONTH: ";
				}
				break;
			case 2: //OP_WDAY
				{
					lV[0].setDateTime(lTS);
					lV[1].set(EY_WDAY);
					IExprNode *lET1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lET1); lUI = lDT.dayOfWeek > 0?lDT.dayOfWeek:1;
					lV[1].set(lUI);
					lET = mSession->expr(OP_MOD,2,lV);
					lExpUI = lUI % lUI;
					mLogger.out() << "exprDTPart test for OP_WDAY: ";
				}
				break;
			case 3: //OP_DAY
				{
					lV[0].setDateTime(lTS);
					lV[1].set(EY_DAY);
					IExprNode *lET1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lET1); lUI = lDT.day;
					lV[1].set(lUI);
					lET = mSession->expr(OP_PLUS,2,lV);
					lExpUI = lUI * 2;
					mLogger.out() << "exprDTPart test for OP_DAY: ";
				}
				break;
			case 4: //OP_HOUR
				{
					lV[0].setDateTime(lTS);
					lV[1].set(EY_HOUR);
					IExprNode *lET1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lET1); lUI = lDT.hour;
					lV[1].set(lUI);
					lET = mSession->expr(OP_PLUS,2,lV);
					lExpUI = lUI * 2;
					mLogger.out() << "exprDTPart test for OP_HOUR: ";
				}
				break;
			case 5: //OP_MINUTE
				{
					lV[0].setDateTime(lTS);
					lV[1].set(EY_MINUTE);
					IExprNode *lET1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lET1); lUI = lDT.minute;
					lV[1].set(lUI);
					lET = mSession->expr(OP_PLUS,2,lV);
					lExpUI = lUI * 2;
					mLogger.out() << "exprDTPart test for OP_MINUTE: ";
				}
				break;
			case 6: //OP_SECOND
				{
					lV[0].setDateTime(lTS);
					lV[1].set(EY_SECOND);
					IExprNode *lET1 = mSession->expr(OP_EXTRACT,2,lV);
					lV[0].set(lET1); lUI = lDT.second;
					lV[1].set(lUI);
					lET = mSession->expr(OP_PLUS,2,lV);
					lExpUI = lUI * 2;
					mLogger.out() << "exprDTPart test for OP_SECOND: ";
				}
				break;
		}
		IExpr *lE = lET->compile();	TVERIFY(lE!=NULL);
		TVERIFYRC(lE->execute(lV[0]));
		if(lV[0].ui != lExpUI){
			TVERIFY(!"ERROR(exprDTPart): Incorrect result returned.");
			mLogger.out() << "Expected: " << lExpUI << " Got: "<< lV[0].i << std::endl;		
		}
		lE->destroy();
		lET->destroy();
		mLogger.out()<<endl;
	}
}

void TestDateTime::conversionExpressions()
{
	// Convert VT_NOW to VT_DATETIME
	Value vNow[2]; vNow[0].setNow() ; vNow[1].set((unsigned)VT_DATETIME);

//	MVTApp::output( vNow, mLogger.out(), mSession ) ;

	CmvautoPtr<IExprNode> et0( mSession->expr( OP_CAST, 2, vNow, 0 ) );
	CmvautoPtr<IExpr> e0(et0->compile()) ; 

	Value vNowDT ;
	TVERIFYRC(e0->execute( vNowDT, NULL, 0 )) ;	
	DateTime dts ;
	TVERIFY( vNowDT.type == VT_DATETIME ) ;
	TVERIFYRC(mSession->convDateTime( vNowDT.ui64, dts ));
	printDateTime( dts ) ;

	//VT_NOW to VT_STRING
	vNow[1].set((unsigned)VT_STRING);
	CmvautoPtr<IExprNode> et1( mSession->expr( OP_CAST, 2, vNow, 0 ) );
	CmvautoPtr<IExpr> e1(et1->compile()) ; 
	Value vNowStr ;
	TVERIFYRC(e1->execute( vNowStr, NULL, 0 )) ;	

	// REVIEW: Is it by design that the time is in UTC and not according to
	// session time zone settings?
	mLogger.out() << "Current time (UTC) is  " << vNowStr.str << endl;

	TVERIFY( vNowStr.type == VT_STRING ) ;
	mSession->free( const_cast<char*>(vNowStr.str) ) ;

	//Parameterized version of VT_STRING conversion
	//(Same IExpr could be used for multiple operations)
	Value vParam[2] ;
	vParam[0].setParam(0) ; vParam[1].set((unsigned)VT_STRING);
	CmvautoPtr<IExprNode> etParameterized( mSession->expr( OP_CAST, 2, vParam, 0 ) );
	CmvautoPtr<IExpr> eParameterized(etParameterized->compile()) ; 

	// Use it to convert VT_NOW to VT_STRING
	Value vNowStr2 ;
	TVERIFYRC(eParameterized->execute( vNowStr2, vNow, 1 )) ;	
	MVTApp::output( vNowStr2, mLogger.out(), mSession ) ;
	TVERIFY( vNowStr2.type == VT_STRING ) ;
	mSession->free( const_cast<char*>(vNowStr2.str) ) ;

	// Use it again to convert VT_DATETIME to VT_STRING
	Value vNowStr3 ;
	TVERIFYRC(eParameterized->execute( vNowStr3, &vNowDT, 1 )) ;	
	MVTApp::output( vNowStr3, mLogger.out(), mSession ) ;
	TVERIFY( vNowStr3.type == VT_STRING ) ;
	mSession->free( const_cast<char*>(vNowStr3.str) ) ;

	//VT_DATETIME to VT_STRING
#if TEST_DATETIME_TO_STR_CONV
	CmvautoPtr<IExprNode> et3( mSession->expr( OP_TOSTRING, 1, &vNowDT, 0 ) );
	CmvautoPtr<IExpr> e3(et3->compile()) ; 
	Value vNowStr4 ;
	TVERIFYRC(e3->execute( vNowStr4, NULL, 0 )) ;	
//	MVTApp::output( res3, mLogger.out(), mSession ) ;
	TVERIFY( vNowStr4.type == VT_STRING ) ;
	mSession->free( const_cast<char*>(vNowStr4.str) ) ;
#endif
}

void TestDateTime::testPinTimeStamps()
{
	// PROP_SPEC_CREATED is persisted according to UTC time
	Value vCreateProp; vCreateProp.set(1); vCreateProp.property=PROP_SPEC_CREATED; 

	IPIN *pin;
	TVERIFYRC(mSession->createPIN(&vCreateProp,1,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
	const Value * v=pin->getValue(PROP_SPEC_CREATED);
	TVERIFY(v!=NULL);

	TVERIFY(v->type==VT_DATETIME);
	
	DateTime dtUTC;
	mLogger.out() << "PIN creation time in UTC: " ;
	TVERIFYRC(mSession->convDateTime(v->ui64,dtUTC,true));
	printDateTime(dtUTC);

	DateTime dtLocal;
	mLogger.out() << "PIN creation time in local time: " ;
	TVERIFYRC(mSession->convDateTime(v->ui64,dtLocal,false));
	printDateTime(dtLocal);

#ifdef WIN32
	SYSTEMTIME systimeUTC;
	GetSystemTime(&systimeUTC);

	TVERIFY(dtUTC.hour==systimeUTC.wHour);
	TVERIFY(dtUTC.day==systimeUTC.wDay);

	SYSTEMTIME systimeLocal;
	GetLocalTime(&systimeLocal);

	TVERIFY2(dtLocal.hour==systimeLocal.wHour,"bug 12461");
	TVERIFY(dtLocal.day==systimeLocal.wDay);
#endif
	if(pin!=NULL) pin->destroy();
}
