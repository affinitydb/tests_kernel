/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h"
#include "mvauto.h"

class TestFamilyOrder: public ITest
{
		static const int sNumProps = 2;
		ISession *mSession;
		PropertyID mPropIDs[sNumProps];
		static const int sNumPINs = 50;	
        DataEventID fID;
		PID vPIDS1[sNumPINs], vPIDS2[sNumPINs];
		int mCase;

	public:
		TEST_DECLARE(TestFamilyOrder);
		virtual char const * getName() const { return "testfamilyorder"; }
		virtual char const * getHelp() const { return "test the ordering of family with different case combinations"; }		
		virtual char const * getDescription() const { return ""; }
		virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
		virtual bool isLongRunningTest() const { return false; }
		virtual void destroy() { delete this; }		
		virtual int execute();
        	
	protected:
		void createPINs(int count = sNumPINs);
		void createFamily(unsigned int);
		void queryFamily(DataEventID,int);
		void validate();
};
static Tstring valuestr[] = {"Apple","aPple","ApPle","appLe","APPlE"};

TEST_IMPLEMENT(TestFamilyOrder, TestLogger::kDStdOut);

void TestFamilyOrder::createPINs(int pNumPINs)
{
	mLogger.out() << " Creating " << pNumPINs << " PINs ..."<<endl;
	mLogger.out()<<endl<<"--------------------------"<<endl;

	for(int i = 0; i < pNumPINs; i++)
	{
		PID lPID;
		Value lV[sNumProps];
		Tstring lStr1,lStr; MVTRand::getString(lStr, 5, 10);
		lStr1 = valuestr[i%5];
		if(lStr1 == valuestr[0])
			lStr1 += lStr;
		if(isVerbose()) mLogger.out()<<"prop1:"<<lStr1.c_str()<<endl;
		SETVALUE(lV[0], mPropIDs[0], lStr1.c_str(), OP_SET);

		Tstring lStr2; //MVTRand::getString(lStr, 5, 10);
		lStr2 = valuestr[i%4];
		MVTRand::getString(lStr, 5, 10);
		if(lStr2 == valuestr[0])
			lStr2 += lStr;
		if(isVerbose()) mLogger.out()<<"prop2:"<<lStr2.c_str()<<endl;
		SETVALUE(lV[1], mPropIDs[1], lStr2.c_str(), OP_SET);

		CREATEPIN(mSession, &lPID, lV, sNumProps);
		if(isVerbose()) mLogger.out() << "PIN ID: \n" << lPID.pid << std::endl;
	}
	mLogger.out()<<"--------------------------"<<endl;

	mLogger.out() << " DONE " << std::endl;
}

void TestFamilyOrder::createFamily(unsigned int flag)
{
	IStmt *lFamilyQ = mSession->createStmt();
	unsigned char lVar = lFamilyQ->addVariable() ;
	Value ops[2] ; 

	ops[0].setVarRef(lVar, mPropIDs[0]);
	ops[1].setParam(0);
	IExprNode *lE = mSession->expr(OP_BEGINS, 2, ops, flag);
	lFamilyQ->addCondition( lVar, lE ) ;
	Tstring randStr;
	MVTApp::randomString(randStr,10,20);
	randStr += ".family";
	fID = STORE_INVALID_CLASSID;
	TVERIFYRC(defineClass(mSession, randStr.c_str(), lFamilyQ, &fID));
	lFamilyQ->destroy();
}

void TestFamilyOrder::validate()
{
	for(int i=0; i<mCase; i++)
		TVERIFY(vPIDS1[i] == vPIDS2[mCase-i-1]);
	mLogger.out()<<"Mcase:"<<mCase<<endl;
}

void TestFamilyOrder::queryFamily(DataEventID cid,int opt)
{
	IStmt *lQ = mSession->createStmt();
	Value paramVals;
	unsigned char lVar;
	SourceSpec cs ;
	ICursor *lR;
	OrderSeg os={NULL,mPropIDs[0],0,0,0};
	
	IPIN * pResult ;

	switch(opt)
	{
	case 0: 
		mLogger.out()<<"Query all the entries of the family"<<endl;	
		mLogger.out()<<"-----------------------------------------\n";

		cs.objectID = cid;
		cs.nParams = 0 ;
		cs.params = NULL;

		lVar = lQ->addVariable( &cs, 1 ) ;
		break;

	case 1:
		mLogger.out()<<"Query all the entries of the family ordered by the 1st propery in ASCENDING"<<endl;
		mLogger.out()<<"--------------------------------------------------------------------------------\n";

		cs.objectID = cid;
		cs.nParams = 0 ;
		cs.params = NULL;

		lVar = lQ->addVariable( &cs, 1 ) ;

		os.flags=0;
		lQ->setOrder(&os,1);
		break;

	case 2: 
		mLogger.out()<<"Query all the entries of the family ordered by the 1st property,in ASCENDING|NCASE order "<<endl;
		mLogger.out()<<"-----------------------------------------------------------------------------------------\n";

		cs.objectID = cid;
		cs.nParams = 0 ;
		cs.params = NULL;
	
		lVar = lQ->addVariable( &cs, 1 ) ;

		os.flags=ORD_NCASE;

		lQ->setOrder( &os,1);
		break;
	
	case 3: 
		mLogger.out()<<"Query all the entries of the family based on the parameter and ordered by prop1, in ASCENDING|NCASE order"<<endl;
		mLogger.out()<<"----------------------------------------------------------------------------------------------------------------\n";

		cs.objectID = cid;
		cs.nParams = 1 ;
		paramVals.set(valuestr[0].c_str());
		cs.params = &paramVals ;
		
		lVar = lQ->addVariable( &cs, 1 ) ;

		os.flags=ORD_NCASE;
		lQ->setOrder( &os, 1);
		break;

	case 4: 
		mLogger.out()<<"Query all the entries of the family ordered by the 2nd propery in ASCENDING"<<endl;
		mLogger.out()<<"--------------------------------------------------------------------------------\n";

		cs.objectID = cid;
		cs.nParams = 0 ;
		cs.params = NULL;

		lVar = lQ->addVariable( &cs, 1 ) ;

		os.pid=mPropIDs[1];
		lQ->setOrder( &os,1);
		break;

	case 5:
		mLogger.out()<<"Query all the entries of the family ordered by the 2nd property,in ASCENDING|NCASE order "<<endl;
		mLogger.out()<<"-----------------------------------------------------------------------------------------\n";

		cs.objectID = cid;
		cs.nParams = 0 ;
		cs.params = NULL;
		lVar = lQ->addVariable( &cs, 1 ) ;

		os.pid=mPropIDs[1]; os.flags=ORD_NCASE;
		lQ->setOrder( &os, 1);
		break;
	
	case 6: 
		mLogger.out()<<"Query all the entries of the family based on the parameter n ordered by prop2, in ASCENDING|NCASE order"<<endl;
		mLogger.out()<<"----------------------------------------------------------------------------------------------------------------\n";

		cs.objectID = cid;
		cs.nParams = 1 ;
		paramVals.set(valuestr[0].c_str());
		cs.params = &paramVals ;
		
		lVar = lQ->addVariable( &cs, 1 ) ;

		os.pid=mPropIDs[1]; os.flags=ORD_NCASE;
		lQ->setOrder( &os, 1);
		break;

	case 7: 
		mLogger.out()<<"Query all the entries of the family based on the parameter n ordered by prop2, in DESCENDING|NCASE order"<<endl;
		mLogger.out()<<"--------------------------------------------------------------------------------------------------------\n";

		cs.objectID = cid;
		cs.nParams = 1 ;
		paramVals.set(valuestr[0].c_str());
		cs.params = &paramVals ;

		lVar = lQ->addVariable( &cs, 1 ) ;

		os.pid=mPropIDs[1]; os.flags=ORD_NCASE|ORD_DESC;
		lQ->setOrder( &os, 1);
		break;	
	}
	TVERIFYRC(lQ->execute(&lR));

	mLogger.print("Family queries...\n\n");
	int cnt = 0;
	mCase = 0;

	while ( NULL != ( pResult = lR->next() ) )
	{
		const Value *pIndex[2];
		pIndex[0] = pResult->getValue(mPropIDs[0]) ;
		if(isVerbose()) mLogger.print("Prop1:%s\n",pIndex[0]->str);

		pIndex[1] = pResult->getValue(mPropIDs[1]) ;
		if(isVerbose()) mLogger.print("Prop2:%s\n",pIndex[1]->str);

		if(opt == 6)
			vPIDS1[cnt++]=pResult->getPID();

		if(opt == 7)
		{
				vPIDS2[cnt++]=pResult->getPID();
				mCase++;
		}
		if(isVerbose()) mLogger.out() << "PIN ID " << pResult->getPID().pid << std::endl;
	
		if ( pIndex == NULL ) { TVERIFY(!"No index prop") ; pResult->destroy() ; continue ; }
		pResult->destroy() ;
	}	
	lQ->destroy();
}

int TestFamilyOrder::execute()
{
	bool lSuccess = true;	
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();
		MVTApp::mapURIs(mSession, "TestAbortQuery.prop.", sNumProps, mPropIDs);
		fID = STORE_INVALID_CLASSID;
		DataEventID cid1,cid2;
		
		/*write the code here*/
		createFamily(CASE_INSENSITIVE_OP);
		cid1 = fID;

		createFamily(0);
		cid2 = fID;

		createPINs();
		mLogger.out()<<"Quering CASE INSENSITIVE family...\n";
		mLogger.out()<<endl<<"--------------------------"<<endl;
	
		for(int i=0;i<8;i++)
			queryFamily(cid1,i);
		//validate();

		mLogger.out()<<"Quering CASE SENSITIVE family...\n";
		mLogger.out()<<endl<<"--------------------------"<<endl;

		for(int i=0;i<8;i++)
			queryFamily(cid2,i);
		//validate();

		mSession->terminate();
		MVTApp::stopStore();
	}
	else{ TVERIFY(!"Unable to start store"); }
	return lSuccess?0:1;
}
