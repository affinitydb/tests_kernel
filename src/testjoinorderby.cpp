/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

class testjoinorderby : public ITest
{
public:
	TEST_DECLARE(testjoinorderby);
	virtual char const * getName() const { return "testjoinorderby"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "Testing the orderby result of a nested join query"; }
	virtual int execute();
	virtual void destroy() { delete this; }
	public:
	PropertyID	mProp[6] ;
};
TEST_IMPLEMENT(testjoinorderby, TestLogger::kDStdOut);

int testjoinorderby::execute()
{
	ISession * mSession ;int i;
	if (!MVTApp::startStore()) {mLogger.print("Failed to start store\n"); return RC_NOACCESS;}
	mSession = MVTApp::startSession();
	
	//Properties - mapping properties
	PropertyID date_original_modifiedPropID;
	char *propName2;char lB[64]; sprintf(lB, "http://vmware.com/core/date_original_modified");propName2=lB;
	date_original_modifiedPropID=MVTUtil::getPropRand(mSession,propName2);
	for(i=0;i<6;i++)		
	{
		char lB[64]; sprintf(lB, "%d.propery", i);
		propName2=lB;
		mProp[i]=MVTUtil::getPropRand(mSession,propName2);
	}
	//DEfining Classes
	const char *lClassList[] = {"unprocessedAudioMP31","unprocessedMeta1","unprocessedPDF1","unprocessedDocx1","unprocessedTXT1"};
	const char *lClassList2[] = {"hostingPinsPushShredder","hostingPinsNotShredded1"};
	size_t nClasses = sizeof(lClassList)/sizeof(lClassList[0]);
	for(i=0;i<6;i++)
	{
		IStmt *lFamilyQ=mSession->createStmt() ;
		unsigned char lVar = lFamilyQ->addVariable() ;
		Value ops[2] ;
		Value ops1[1] ;
		Value ops2[2] ;
		ops[0].setVarRef(0, date_original_modifiedPropID ) ;ops[1].setParam(0);
		IExprNode *lE= mSession->expr(OP_IN, 2, ops )  ;
		ops1[0].setVarRef(0, mProp[i] ) ;
		IExprNode *lE1= mSession->expr(OP_EXISTS, 1, ops1 )  ;
		ops2[0].set(lE);
		ops2[1].set(lE1);
		IExprNode *lE2= mSession->expr(OP_LAND, 2, ops2 ) ;
		TVERIFYRC(lFamilyQ->addCondition( lVar, lE2 )) ;
		if(i<4)
		{
			TVERIFYRC(defineClass(mSession, lClassList[i], lFamilyQ ));
		}
		else
		{
			TVERIFYRC(defineClass(mSession, lClassList2[i-4], lFamilyQ ));
		}
		lE2->destroy();
		lFamilyQ->destroy();
	}

	//Creating Pins
	{
	Value lV[7];
	TIMESTAMP lTS;
	for(i=0;i<15;i++)
	{
		lTS=MVTRand::getDateTime(mSession);
		lV[0].setDateTime(lTS);lV[0].setPropID(date_original_modifiedPropID);
		lV[1].setDateTime(lTS);lV[1].property=mProp[0];
		lV[2].setDateTime(lTS);lV[2].property=mProp[1];
		lV[3].setDateTime(lTS);lV[3].property=mProp[2];
		lV[4].setDateTime(lTS);lV[4].property=mProp[3];
		lV[5].setDateTime(lTS);lV[5].property=mProp[4];
		lV[6].setDateTime(lTS);lV[6].property=mProp[5];
		TVERIFYRC(mSession->createPIN(lV,7,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));	
	}
	}
	//Query
	IStmt *lQuery = mSession->createStmt();
	//union of four classes
	RC rc = RC_OK;
	if(lQuery)
	{
		unsigned char lastvar = 0;bool bSuccess = true;
		for(unsigned long iidx = 0 ; iidx < nClasses && bSuccess; iidx++)
		{
			Afy::SourceSpec lclassSpec;
			lclassSpec.nParams=0;
			lclassSpec.params=NULL;
			if(RC_OK == (rc = mSession->getDataEventID(lClassList[iidx],lclassSpec.objectID)))
			{
				unsigned char tmpVar = lQuery->addVariable(&lclassSpec,1); 
				lastvar = iidx==0 ? tmpVar : lQuery->setOp(lastvar,tmpVar,Afy::QRY_UNION);
			}
			else 
				bSuccess = false;
		}
	//intersection of fifth class
		Afy::Value lrange[2];Afy::Value lparam;
		if(bSuccess)
		{
			Afy::SourceSpec lclassSpec;
			if( RC_OK == (rc =mSession->getDataEventID("hostingPinsPushShredder",lclassSpec.objectID)))
			{
				lclassSpec.nParams=0;
				lclassSpec.params=NULL;
				unsigned char tmpVar = lQuery->addVariable(&lclassSpec,1); 
				lastvar = lQuery->setOp(lastvar,tmpVar,Afy::QRY_INTERSECT);
				if( RC_OK == (rc =mSession->getDataEventID("hostingPinsNotShredded1",lclassSpec.objectID)))
				{
					TIMESTAMP lTS;getTimestamp(lTS);
					DateTime lDT;mSession->convDateTime(lTS,lDT,true/*UTC*/);	
					lrange[0].setDateTime(0);
					lrange[1].setDateTime(lTS);
					lparam.setRange(lrange);
					lclassSpec.nParams=1;
					lclassSpec.params=&lparam;
					tmpVar = lQuery->addVariable(&lclassSpec,1); 
					lastvar = lQuery->setOp(lastvar,tmpVar,Afy::QRY_UNION);
				}
				else
					bSuccess = false;
			}
			else
					bSuccess = false;
		}
		OrderSeg ord = {NULL,date_original_modifiedPropID,ORD_DESC,0,0};
		TVERIFYRC(lQuery->setOrder( &ord,1));
		
	//Execution
		int ncount=9;
		Afy::ICursor *lResult = NULL;
		TVERIFYRC(lQuery->execute(&lResult,NULL, 0,(unsigned int)ncount,0)) ;
		//Afy::ICursor *lResult = lQuery->execute();
		IPIN *pin;
		const Value * rV;
		uint16_t tempYear=0; i=0;
		while( pin = lResult->next() )
		{
			DateTime lDT2;
			rV=pin->getValue(date_original_modifiedPropID);
			TVERIFYRC(mSession->convDateTime(rV->ui64,lDT2));
			if(i!=0)
			{
				TVERIFY(lDT2.year<=tempYear);
			}
			tempYear=lDT2.year;i++;
		}
		
	}
	lQuery->destroy();
	mSession->terminate();
	MVTApp::stopStore();
	return RC_OK;
}
