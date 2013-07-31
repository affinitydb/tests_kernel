/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h" 

// Publish this test.
class testnavigator : public ITest
{
	static const int nProps = 2, nPINs = 5, nColls = 5;
	int mSuccess;
	PropertyID lPropIDs[nProps];
	ISession *mSession;
	public:
		TEST_DECLARE(testnavigator);
		virtual char const * getName() const { return "testnavigator"; }
		virtual char const * getHelp() const { return ""; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "test crashes in smoke test"; return false; }
		virtual bool isStandAloneTest() const { return true; }
		virtual char const * getDescription() const { return "Traversing through collection list"; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void createPINs();
		void queryPINs();
};

TEST_IMPLEMENT(testnavigator, TestLogger::kDStdOut);

void testnavigator::createPINs()
{
	mLogger.out()<<"Creating "<<nPINs<<"PINs(with collection)"<<std::endl;
	for(int i=0;i<nPINs;i++)
	{
		Value vals[10];
		Tstring str;
		for(int k=0;k<nProps;k++)
		{
			for(int j=0;j<nColls;j++)
			{
				vals[k*5+j].set(MVTRand::getRange(0,100)); vals[k*5+j].setPropID(lPropIDs[k]); 
				vals[k*5+j].op = OP_ADD;vals[k*5+j].eid = STORE_LAST_ELEMENT; 
			}
		}
		TVERIFYRC(mSession->createPIN(vals,10,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));
		if(i % 10 == 0) mLogger.out()<<".";
	}
	mLogger.out()<<"\nDone.\n";
}
void testnavigator::queryPINs()
{
	IStmt *lQ = mSession->createStmt();
	lQ->addVariable();
	ICursor *res = NULL;
	TVERIFYRC(lQ->execute(&res));
	IPIN *pin = NULL;
	while( pin = res->next())
	{
		const Value *lV = NULL;
		for(int i=lPropIDs[0];i<nProps;i++)
		{
			lV = pin->getValue(lPropIDs[i]);
			INav *pnav = lV->nav;
			const Value *val = NULL; int nCount = 0;
			
			mLogger.out()<<"Traversing collection list(use GO_PREVIOUS only)\t";					
			for(val = pnav->navigate(GO_LAST); 
				0!=val;
				val = pnav->navigate(GO_PREVIOUS),nCount++)
			{
				if(nCount > nColls) 
				{
					mLogger.out()<<"GO_PREVIOUS implementation probably buggy.. bailing out(skipping rest)\n";
					TVERIFY(0);
					mSuccess = -1; return;
				}
			}
			if(mSuccess >= 0) mLogger.out()<<"Successful\n";
			else mLogger.out()<<"Failure.\n";

			mLogger.out()<<"Traversing collection list(use GO_NEXT only)\t\t";		
			for(val = pnav->navigate(GO_FIRST),nCount = 0; 
				0!=val;
				val = pnav->navigate(GO_NEXT),nCount++)
			{
				int lno = 0;
				lno = val->i;
				if(nCount > nColls) 
				{
					mLogger.out()<<"GO_NEXT implementation probably buggy.. bailing out(skipping rest)\n";
					TVERIFY(0);
					mSuccess = -1; return;
				}
			}
			if(mSuccess >= 0) mLogger.out()<<"Successful\n";
			else mLogger.out()<<"Failure.\n";

			mLogger.out()<<"Traversing collection list(use GO_NEXT & GO_PREVIOUS)\t";		
			for(val = pnav->navigate(GO_FIRST),nCount = 0; 
				0!=val;
				val = pnav->navigate(GO_NEXT),nCount++)
			{
				const Value *pvn = pnav->navigate(GO_NEXT);
				if(pvn == NULL) break;
				pnav->navigate(GO_PREVIOUS);
				if(nCount > nColls)
				{
					mLogger.out()<<"GO_NEXT & GO_PREIVIOUS implementation probably buggy.. bailing out(skipping rest)\n";
					TVERIFY(0);
					mSuccess = -1; return;
				}
			}
			if(mSuccess >= 0) mLogger.out()<<"Successful\n";
			else mLogger.out()<<"Failure.\n";
			}
		mLogger.out()<<".";
		pin->destroy();
	}
	mLogger.out()<<"\nDone.\n";
}
int testnavigator::execute()
{
	mSuccess = 0;
	if (!MVTApp::startStore()) {
		mLogger.out()<<"Failed to start store..\n";
		return (mSuccess = -1);
	}
	if ( NULL == (mSession = MVTApp::startSession() ) ) {
		mLogger.out()<<"Failed to start session..\n";
		return (mSuccess = -2);
	}
	MVTApp::mapURIs(mSession, "testnavigator.prop.", nProps, lPropIDs);
	createPINs(); 
	queryPINs();
	MVTApp::stopStore();
	return mSuccess;
}
