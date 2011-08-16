/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

// Example usage:
//
// tests testAllocCtrl 0.80 1000 1 1024 
// 
//	-Create 1000 pins with separate calls.  Each pin is about 1024 bytes big and each
// pin page should be kept 80% free
//
//
// tests testAllocCtrl 0.15 10 100 2048
//
// - Create 1000 pins in 10 batches of 100 pins.  Each pin is about 2048 bytes and
// the pin page should be kept 15% free

using namespace std;

#define ADD_EXTRA_PROP 0 // Was breaking AllocCtrl.pctPageFree setting - now working again

#define SECOND_PASS 0 // Add 3 extra properties as a second pass (each property is inPropSize)
					  // This is useful for experimentation with migration.
					  // For example: use this with inPageFree 0.75 and inPropSize 200
					  // and we should get 800 byte pins that cleanly fit onto pages.

// Publish this test.
class TestAllocCtrl : public ITest
{
	public:
		TEST_DECLARE(TestAllocCtrl);
		virtual char const * getName() const { return "testallocctrl"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Test AlloCtrl.  args: [--npagefree=0-1] [--batchsize] [--cntbatches] [--propsize]"; }
		virtual int execute();
		virtual void destroy() { delete this; }

		virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
		virtual bool isStandAloneTest()const {return true;}
	protected:

		void secondPass(int cntExtraProps) ;
 	private:
		ISession * mSession ;
		PropertyID mPropX ;
		vector<PID> mPINs ;
		long mCntPins ;
		long mCntBatches ;
		long mBatchSize ;
		long mPropSize ;

		AllocCtrl *mPinPageAllocCtrl ;
};
TEST_IMPLEMENT(TestAllocCtrl, TestLogger::kDStdOut);

int TestAllocCtrl::execute()
{
	string inPageFree("0.25"); bool pparsing = true; 

	if(!mpArgs->get_param("npagefree",inPageFree))
	{
	   mLogger.out() << "No --npagefree= parameter, defaulting to 0.25" << endl;
	}
	
	if(!mpArgs->get_param("batchsize",mBatchSize))
	{
		mBatchSize=10;
		mLogger.out() << "No --batchsize= parameter, defaulting to 10" << endl;
 	}

	if(!mpArgs->get_param("cntbatches",mCntBatches))
	{
		mCntBatches=5;
		mLogger.out() << "No --cntbatches= parameter, defaulting to 5" << endl;
	}

	if(!mpArgs->get_param("propsize",mPropSize))
	{
		mPropSize=1024;
		mLogger.out() << "No --propsize= parameter, defaulting to 1024" << endl;
	}
	
	if(false == pparsing) 
	{
	    mLogger.out() << "Test parameters initialization problem! " << endl; 
		mLogger.out() << getDescription() << endl;
		mLogger.out() << "Example: ./tests testallocctrl --npagefree={...} --batchsize={...} --cntbatches={...}  --propsize={...}" << endl;  
	    return RC_FALSE;
	}
		
	// Start with clean store
	TVERIFY(MVTApp::deleteStore()) ;

	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	mPinPageAllocCtrl = (AllocCtrl *) mSession->alloc(sizeof(AllocCtrl) * 1);
	mPinPageAllocCtrl->arrayThreshold = ~0ul ; // Use internal ARRAY_THRESHOLD (256)
	mPinPageAllocCtrl->ssvThreshold = ~0ul ;  // REVIEW: what is default
	mPinPageAllocCtrl->pctPageFree = (float) atof(inPageFree.c_str()) ;  

	if ( mPinPageAllocCtrl->pctPageFree <= 0 || mPinPageAllocCtrl->pctPageFree >= 1 )
	{
		mLogger.out() << "Invalid argument for pctPageFree, specify value between 0 and 1" << endl ;
		return RC_FALSE ;
	}

	TVERIFYRC(mSession->setPINAllocationParameters(mPinPageAllocCtrl)) ;

	MVTApp::mapURIs(mSession,"TestAllocCtrl.propX",1,&mPropX) ;	

	mCntPins = mBatchSize * mCntBatches ;

	mLogger.out() << "Generating " << mCntPins << " pins ( in " << mCntBatches << " batches of " << mBatchSize << ")\n" ;
	mLogger.out() << "Approx Data size " << ( mPropSize * mCntPins ) / 1024 
				<< " (KB) requiring minimum " << 1 + (( mPropSize * mCntPins )/MVTApp::getPageSize()) << " pages" << endl ;

	mPINs.resize( mCntPins ) ;

	string randStr ;
	MVTRand::getString( randStr, mPropSize, 0 ) ;
	Value valProp ; valProp.set( randStr.c_str() ) ; valProp.property = mPropX ;

	Value extra ; extra.set( 10 ) ; extra.property = MVTApp::getProp(mSession,"extraProp") ;

	for ( int iBatch = 0 ; iBatch < mCntBatches ; iBatch++ )
	{
		if ( mBatchSize == 1 )
		{
			TVERIFYRC(mSession->createPIN(mPINs[iBatch],&valProp,1,MODE_NO_EID ) );
#if ADD_EXTRA_PROP			
			TVERIFYRC(mSession->modifyPIN(mPINs[iBatch],&extra,1,MODE_NO_EID));
#endif
		}
		else
		{
			vector<IPIN*> batchPins( mBatchSize ) ;

			int iPinInBatch ;
			for ( iPinInBatch = 0 ; iPinInBatch < mBatchSize ; iPinInBatch++ )
			{
				batchPins[iPinInBatch] = mSession->createUncommittedPIN( &valProp, 1, MODE_NO_EID | MODE_COPY_VALUES ) ;
			}

			if ( mBatchSize > 10 || (0 == (iBatch % 10)))
				mLogger.out() << "." ;

			TVERIFYRC(mSession->commitPINs( &(batchPins[0]), mBatchSize ) );

			for ( iPinInBatch = 0 ; iPinInBatch < mBatchSize ; iPinInBatch++ )
			{
				mPINs[iBatch*mBatchSize + iPinInBatch] = batchPins[iPinInBatch]->getPID() ;
#if ADD_EXTRA_PROP			
				TVERIFYRC(batchPins[iPinInBatch]->modify(&extra,1,MODE_NO_EID));
#endif
				batchPins[iPinInBatch]->destroy() ;
			}
		}
	}

	// See how many different pages are used
	long cntPages = 1 ;		
	long currentPage =(uint32_t) ( mPINs[0].pid >> 16 ) ;
	for ( int i = 1 ; i < mCntPins ; i++ )
	{
		long pinPage = (uint32_t) ( mPINs[i].pid >> 16 ) ;
		if ( currentPage != pinPage )
		{
			currentPage = pinPage ;
			cntPages++ ;
		}
	}
	mLogger.out() << "\n\n\nPINs are spaced on " << cntPages << " pages\n\n\n" ;	

#if SECOND_PASS
	secondPass(3) ;
#endif
	mSession->free(mPinPageAllocCtrl);
	mSession->terminate(); 
	MVTApp::stopStore();  

	return RC_OK  ;
}

void TestAllocCtrl::secondPass( int cntExtraProps )
{
	string extraPropData ;
	MVTRand::getString( extraPropData, mPropSize, 0 ) ;
	PropertyID * extraProps = (PropertyID*) malloc( sizeof(PropertyID) * cntExtraProps ) ;
	MVTApp::mapURIs( mSession, "TestAllocCtrl::secondPass", cntExtraProps, extraProps ) ;

	Value * vals = (Value*) malloc( sizeof( Value ) * cntExtraProps ) ;
	int i ;
	for ( i = 0 ; i < cntExtraProps ; i++ )
	{
		vals[i].set( extraPropData.c_str() ) ;
		vals[i].property = extraProps[i] ;
	}

	for ( i = 0 ; i < (int)mPINs.size() ; i++ )
	{
		TVERIFYRC(mSession->modifyPIN( mPINs[i], vals, cntExtraProps, MODE_NO_EID )) ;
	}

#if 0
	CmvautoPtr<IStmt> lQ( mSession->createStmt() ) ;
	lQ->setPropCondition( lQ->addVariable(), &mPropX, 1 ) ;

	// REVIEW: we could also do a IStmt::modifyPIN()

	CmvautoPtr<ICursor> lR( lQ->execute() ) ;
	IPIN * pin ;
	while( pin = lR->next() )
	{
		TVERIFYRC(pin->modify( vals, cntExtraProps, MODE_NO_EID )) ;
		pin->destroy() ;
	}
#endif

	delete( extraProps ) ;
	delete( vals ) ;
}
