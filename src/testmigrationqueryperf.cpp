/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

/*
This test investigates the performance
cost (if any) of retrieving pins that have been migrated to another page.
This "forwarding" can happen when properties are added after the initial 
creation of the PIN.
*/

#include "app.h"
#include "photoscenario.h"
#include "mvauto.h"
#include "classhelper.h"

#include "teststream.h"

// To Generate the repro for bug 14407, latest repro on Windows is
// tests testmigrationqueryperf y 1733 0.15 -seed=36080
#define REPRO_14407 1

#define TEST_PAGE_SIZE_ALLOC_PROBLEM 1

class TestMigrationQueryPerf : public PhotoScenario
{
	public:
		TEST_DECLARE(TestMigrationQueryPerf);
		virtual char const * getName() const { return "testmigrationqueryperf"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Compare query performance between regular PINs and forwarded (migrated) PINs.  Args: [--migrate=y/n] [--cntpins] [--freespace]"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "(1)Performance analysis test, erases store files, (2)multi arguments usage"; return false; }
		virtual bool isLongRunningTest()const { return true; }
		virtual bool includeInPerfTest() const { return true; }  
		virtual void destroy() { delete this; }

	protected:
		// Overrides
		virtual int execute() ;
		virtual void preTestInitialize() ;
		virtual void initializeTest() ;
		virtual void doTest() ;
		virtual void populateStore() ;

		PID addPhotoPin(Value * pExtra, unsigned int cntExtra) ;
		void addACLs(PID & inPID, const char ** ids, unsigned int cnt) ;

		void check14407Index();
	private:
		bool mWithPinMigration ;
		long mTestTime ;
		long mPinCountOverride ; // Temporary copy to remember between Execute2() and Initialize()
		float mFreeSpacePercentage ; // How much space to leave on pin page during initial pin creation

		bool m14407PinCreated ;
		AllocCtrl mPinPageAllocCtrl ;
};
TEST_IMPLEMENT(TestMigrationQueryPerf, TestLogger::kDStdOut);

void TestMigrationQueryPerf::preTestInitialize()
{
	mPostfix = "" ;
	mEnableNotifCB = false ;
}

void TestMigrationQueryPerf::initializeTest()
{
	mPinPageAllocCtrl.arrayThreshold = ~0ul ; // Use internal ARRAY_THRESHOLD (256)
	mPinPageAllocCtrl.ssvThreshold = ~0ul ;  // REVIEW
	mPinPageAllocCtrl.pctPageFree = mFreeSpacePercentage ; 
	TVERIFYRC(mSession->setPINAllocationParameters(&mPinPageAllocCtrl)) ;

	PhotoScenario::initializeTest() ;

	mPinCount = mPinCountOverride ;
}

void TestMigrationQueryPerf::addACLs(PID & inPID, const char ** ids, unsigned int cnt )
{		
	for ( unsigned int i = 0 ; i < cnt ; i++ )
	{
		Value aclVal ;
		aclVal.setIdentity( mSession->getIdentityID(ids[i]) ) ;
		aclVal.property = PROP_SPEC_ACL ;
		aclVal.meta = ACL_READ ;
		aclVal.op = OP_ADD ;  // Important for building collection
		TVERIFYRC(mSession->modifyPIN( inPID, &aclVal, 1 )) ;
	}
}

void TestMigrationQueryPerf::populateStore() 
{
	m14407PinCreated=false;

	TVERIFY( mSession != NULL ) ; // For convenience a session already exists

	mLogger.out() << "Generating " << (int) getPinCount() << " pins" << endl ;

	static const size_t lExtraProps = 10 ;

	PropertyID ids[lExtraProps] ;
	MVTApp::mapURIs( mSession, "ExtraProps", 10, ids ) ;

	Value extraVals[lExtraProps] ;
	string strRandom[lExtraProps] ;

	// Additional ACLs were added after the first, originally leading to
	// migration to big collection even for a small list like this.  We compare different
	// of adding all ACL records together versus adding them in two batches which is closer to the reality
	const char * allids[] = { "identA","identB","identC","identD","identE","identF" } ;
	const char * firstid[] = { "identA" } ;
	const char * remaining[] = { "identB","identC","identD","identE","identF" } ;

	size_t i,k ;
	for ( k = 0 ; k < lExtraProps ; k++ )
	{		
		MVTRand::getString( strRandom[k], 50, 10 ) ;
		extraVals[k].set( strRandom[k].c_str() ) ;
		extraVals[k].meta = META_PROP_NOFTINDEX ; /* don't want extra FT index to slow down test results */		
		extraVals[k].property = ids[k] ;
	}

	long const lBef = getTimeInMs();

	mPids.resize( getPinCount()) ;
	if(!mCreateAppPINs)
	{
		for ( size_t i = 0 ; i < getPinCount() ; i++ )
		{		
			if (0 == i % 100)
				mLogger.out() << ".";

			if ( mWithPinMigration )
			{
#if REPRO_14407
				if ( i==1732 && mRandomSeed==36080 )
				{
					// The  pin about to be created in addPhotoPin does not					
					// get properly inserted into family 29, see check14407Index
#ifdef WIN32
					::DebugBreak() ;
#endif
				}
#endif

				mPids[i] = addPhotoPin(NULL, 0) ;
#if TEST_PAGE_SIZE_ALLOC_PROBLEM
				addACLs(mPids[i],firstid,sizeof(firstid)/sizeof(firstid[0])) ;
#endif
			}
			else
			{
				mPids[i] = addPhotoPin(extraVals, lExtraProps) ;
				// mPinPageAllocCtrl seems to be ignored just because of these
				// additional pin modifications (which add only a small amount of data)
#if TEST_PAGE_SIZE_ALLOC_PROBLEM
				addACLs(mPids[i],allids,sizeof(allids)/sizeof(allids[0])) ;
#endif
			}			
		}

#if TEST_PAGE_SIZE_ALLOC_PROBLEM
		addRandomTags(10) ;
#endif
		mLogger.out() << endl ;
	}

#if 1
	if ( mWithPinMigration )
	{
		mLogger.out() << "Adding extra props as second step" << endl ;

		// Add some additional properties to force migration
		// Pin will be forwarded to another page because it doesn't have room on the 
		// original page.  There is no way to detect externally this condition,
		// but it was confirmed with storedr that approx 60% of the pins were forwarded
		// by this data.
		for ( i = 0 ; i < mPids.size() ; i++ )
		{
			// Currently each pin gets exactly the same values

			RC rc;
			TVERIFYRC( rc=mSession->modifyPIN(mPids[i], extraVals, lExtraProps, MODE_NO_EID)) ; /* MODE_NO_EID important if you want to reuse the Value array for each call */
			if (rc != RC_OK)
			{
				mLogger.out() << "Failed at index " << std::dec << (int)i << ", pin " << std::hex << mPids[i].pid << std::dec << endl;
				MVTUtil::output(mPids[i],mLogger.out(),mSession);

				mLogger.out() << "----Extravalues that failed to add" << endl;

				size_t k;
				for ( k = 0 ; k < lExtraProps ; k++  )
				{				
					mLogger.out() << extraVals[k].str << endl;
				}

				for ( k = 0 ; k < lExtraProps ; k++  )
				{				
					mLogger.out() << extraVals[k].property << endl;
				}

			}
			addACLs(mPids[i],remaining,sizeof(remaining)/sizeof(remaining[0])) ;
		}
	}
#endif	
	long const lAft = getTimeInMs();
	mLogger.out() << "\n********TIME RESULTS******\n";
	mLogger.out() << "Tests with pin migration " << ( mWithPinMigration?"On":"Off") 
		<< " data creation time (ms) " << (lAft-lBef) << endl ;
	mLogger.out() << "\n**************************\n";
}

void TestMigrationQueryPerf::doTest() 
{
	long const lBef = getTimeInMs();

	// (Based on runClassQueries() ; but actually retrieving the pins rather than just getting a count) 
	for ( size_t i = 0 ; i < mTagPool.size() ; i++ )
	{
		Value tag ; tag.set( mTagPool[i].c_str() ) ;

		/* each of these querys retrieves approximately 4% of the pins.  Add a call to runClassQueries
		   to see the detailed break down */
		OrderSeg ord={NULL,date_id,0,0,0};
		CmvautoPtr<IStmt> qTags(createClassQuery( "taggedImages",STMT_QUERY,
													1,&tag,		 // Tag to search for
													1,&ord));  // Order by

		// fs_path sometimes being migrated
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0/*variable*/,this->fs_path_id);
#ifdef WIN32
			lV[1].set( "c:/" ); // Should actually match all, see mFilePathPool generation code
#else
			lV[1].set( "/" );
#endif
			lET = mSession->expr(OP_BEGINS, 2, lV);
		}
		qTags->addCondition(0,lET);
		lET->destroy() ;

		//mLogger.out() << qTags->toString() ;
		ICursor* lC = NULL;
		TVERIFYRC(qTags->execute(&lC));
		CmvautoPtr<ICursor> lR(lC);

		int cntMatches = 0 ;
		IPIN * p = NULL ;
		while( NULL != (p = lR->next()) )
		{
			p->destroy() ;
			cntMatches++ ;
		}
		TVERIFY( cntMatches > 0 ) ;
	}

	mTestTime = getTimeInMs()-lBef ;
}

int	TestMigrationQueryPerf::execute()
{
    string lMigrate("y"), lpctFreeSpace("0.15"); bool pparsing = true;
	mPinCountOverride=5000 ; 

	if(!mpArgs->get_param("migrate",lMigrate))
	{
		mLogger.out() << "No --migrate argument, defaulting to \"y\"" << endl;
    }
	
	if(!mpArgs->get_param("cntpins",mPinCountOverride))
	{
		mLogger.out() << "No --cntpins parameter, defaulting to 5000" << endl;
    }

	if(!mpArgs->get_param("freespace",lpctFreeSpace))
	{
		mLogger.out() << "No --freespace parameter, defaulting to 0.15" << endl;
    }
	
	if(!pparsing)
	{
	   mLogger.out() << "Parameter initialization problems! " << endl; 
	   mLogger.out() << "Test name: testmigrationqueryperf" << endl; 	
	   mLogger.out() << getDescription() << endl;	
	   mLogger.out() << "Example: ./tests testmigrationqueryperf --migrate={...} --cntpins={...} --freespace={...}" << endl; 
			
	   return RC_INVPARAM;
    }	
	
	mWithPinMigration = ( NULL != strstr( lMigrate.c_str(), "y" ) ) ;

	mFreeSpacePercentage = (float) atof(lpctFreeSpace.c_str()) ;

	if ( mPinCountOverride <= 0 ) 
	{
		mLogger.out() << "No pins specified" << endl ;
		return RC_FALSE ;
	}

	MVTApp::deleteStore() ;
	PhotoScenario::execute() ;
	long t1 = mTestTime  ;

	mLogger.out() << "\n********TIME RESULT******\n\n";
	mLogger.out() << "Tests with pin migration " << ( mWithPinMigration?"On":"Off") << " query time (ms) " << t1 << endl ;

	return RC_OK  ;
}

PID TestMigrationQueryPerf::addPhotoPin(Value * pExtra, unsigned int cntExtra)
{
	// Add fake photo pin using random values.
	// this is variation of the base class implementation, with added support for including 
	// extra values to the initial pin creation

	static const int basePropCount = 7 ;

	Value * vals = (Value*) malloc(sizeof(Value)* ( basePropCount + cntExtra )) ;
	std::string randPath ;
	std::string fileName ;

	MVTRand::getString( fileName, 10, 40, true /*words*/ ) ;
	fileName += ".JPG" ;
	vals[0].set( fileName.c_str() ) ; vals[0].property = name_id ;

	assert( !mFilePathPool.empty() ) ;
	size_t index = MVTRand::getRange(0,(int)(mFilePathPool.size()-1)) ;
	randPath = mFilePathPool[index] ; 
	randPath += fileName ; 

	vals[1].set( randPath.c_str() ) ; 
	vals[1].property = fs_path_id ;

	unsigned long lStrmsize = MVTRand::getRange(1,10000) ;

	IStream *lStream = MVTApp::wrapClientStream(mSession,new TestStringStream(lStrmsize,VT_BSTR));
	vals[2].set( lStream ) ; vals[2].property=binary_id ; vals[2].meta = META_PROP_NOFTINDEX | META_PROP_SSTORAGE;

	uint64_t lDate = MVTRand::getDateTime(mSession) ;

	vals[3].setDateTime( lDate ) ; vals[3].property=date_id ; 
	vals[4].set( (int)MVTRand::getBool() ) ; vals[4].property=cache_id ; 
	vals[5].set( mHostID.c_str() ) ; vals[5].property=refreshNodeID_id ; 
	vals[6].set("image/jpeg") ; vals[6].property=mime_id ; 

	if ( cntExtra > 0 )
	{
		memcpy(&(vals[7]),pExtra,sizeof(Value)*cntExtra) ;
	}

	PID newpin ;
	TVERIFYRC( mSession->createPIN( newpin, vals, basePropCount + cntExtra ) );

#if 1  //#ifdef WIN32
	if ( newpin.pid == 0x1000000001400073ll )
	{
		m14407PinCreated=true;
		check14407Index();
	}
#endif

	delete(vals);

	return newpin ;
}

void TestMigrationQueryPerf::check14407Index()
{
	if ( m14407PinCreated && mRandomSeed==36080 )
	{
		Value v; 
		v.set("c:/gDbJP/YKmAg/mkYti/uEYdw/uDXQwYCEoQWPlTe pDdVAun.JPG");
			
		v.property=6;

		ClassSpec cs;
		cs.classID=29;
		cs.nParams=1;
		cs.params=&v;

		CmvautoPtr<IStmt> q(mSession->createStmt());
		q->addVariable(&cs,1);
                      
		PID expected={0x1000000001400073ll,STORE_OWNER};

		// If this fails then a lookup into the index doesn't work
		TVERIFY(MVTUtil::checkQueryPIDs( q,1,&expected,mLogger.out(),true/*verbose*/,0,NULL));
		
//		//Another check - comparing family definition query with index for this particular value
//		int cnt;
//		TVERIFY(ClassHelper::testClass(mSession,"importedFile.yuLiSAIVPj",mLogger.out(),cnt,1,&v));

/*
Info from store dr
		
ClassId: 29
Dropped: false
Family with 1 parameter(s)
Pins (with any duplicates caused by collections): 1732  <<< SHOULD BE 1733
Keys: 1733

		QUERY.100(10) {
        Var:0 {
                Classes:        importedFile.yuLiSAIVPj
                CondIdx:        /fs_path.yuLiSAIVPj =~(24) $0
                CondProps:              fs_path.yuLiSAIVPj
        }
}

Family based on this class:

>class=importedFile.yuLiSAIVPj
importedFile.yuLiSAIVPj
ClassId: 12
Dropped: false
1733 PINs belong to this class
Root Page: 0x0
QUERY.100(10) {
        Var:0 {
                CondProps:              fs_path.yuLiSAIVPj
        }
}
*/
	}
}
