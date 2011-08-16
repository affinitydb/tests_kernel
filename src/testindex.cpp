/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h"
#include "mvauto.h"
#include "classhelper.h"

#define TESTCLASS 0
class CStoreClass 
{
	public:	
		// Represent a single class in the store	
		MVStore::ISession * mSession;
		MVStore::IStmt* mClassQuery;
		MVStore::ClassID mClassID;
		std::string mClassName;
		int mPinCnt; // To cache result if calculated
		bool mFamily;
		int mCntKeys;
		PropertyID mIndexedProp;
		bool mbIsRange;

		CStoreClass(MVStore::ISession* inSession)
			: mSession(inSession)
			, mClassQuery(NULL)
			, mClassID(STORE_INVALID_CLASSID)
			, mPinCnt(-1)
			, mFamily(false)
			, mCntKeys(-1)
			, mIndexedProp(STORE_INVALID_PROPID)
			, mbIsRange(false)
		{
		}

		~CStoreClass()
		{
			if ( mClassQuery ) { mClassQuery->destroy() ; } 
			mSession = NULL ; // Not belonging to this object
		}
		MVStore::IStmt* getQueryUsingClass() const 
		{
			return ClassHelper::getQueryUsingClass(mSession, mClassID);
		}

		void init( const char * inClassName, MVStore::ClassID inClsid, MVStore::IStmt * inQuery ) 
		{
			mClassName = inClassName ;
			mClassID = inClsid ;
			mClassQuery = inQuery ; // Take ownership
			mFamily = false;
			IPIN *cpin=NULL;
			if (mSession->getClassInfo(inClsid,cpin)==RC_OK) {
				assert(cpin!=NULL);
				mFamily = cpin->getValue(PROP_SPEC_INDEX_INFO)!=NULL;
				cpin->destroy();
			}
		}
		bool testClass(std::stringstream & outResultInfo)
		{
			int cntPins;
			return ClassHelper::testClass(mSession,mClassID,mClassQuery,outResultInfo,cntPins);
		}
		
		bool isFamily() { return mFamily; }

		int getCount()
		{
			if (mPinCnt==-1)
			{
				if ( isFamily() )
				{
					// Requires multiple queries
					// so we collect as much info as possible
					uint8_t t=0;
					ClassHelper::familyInfo(
						mClassID,
						mSession,
						mIndexedProp,
						t,
						mCntKeys,
						mPinCnt,
						mbIsRange );
				}
				else
				{
					CmvautoPtr<IStmt> lClassQ( getQueryUsingClass() ) ;
					uint64_t cnt =0 ;
					lClassQ->count( cnt ) ;
					mPinCnt = (int)cnt;
				}
			}
			return mPinCnt;
		}
};

class CStoreInfo
{
	private:
		MVStore::ISession * mSession;
		#if 0
			IStoreInspector * mStoreInspector;
		#endif
	
	public:
		std::vector<CStoreClass*> mClasses ;
		CStoreInfo(MVStore::ISession * inSession)
			:mSession(inSession)
			#if 0
				,mStoreInspector(NULL)
			#endif
			{}
		~CStoreInfo()
		{
			for ( size_t i = 0 ; i < mClasses.size() ; i++ )
				delete(mClasses[i]);
		}

		void storeScan()
		{
			#if 0
				mSession->storeInspector(mStoreInspector,true);
			#endif

			std::vector<Tstring> lCNames;
			std::vector<ClassID> lCIDs;
			std::vector<IStmt *> lCPredicates;
			MVTApp::enumClasses(*mSession, &lCNames, &lCIDs, &lCPredicates);
			for (size_t i = 0; i < lCNames.size(); i++)
			{
				CStoreClass * cls = new CStoreClass(mSession) ;
				cls->init(lCNames[i].c_str(),lCIDs[i],lCPredicates[i]) ;
				
#if TESTCLASS
				std::stringstream output;
				cls->testClass(output);
				cout<<output.str();
#endif
				cls->getCount();
				mClasses.push_back(cls) ;
			}
			cout<<"Total no of classes:"<<mClasses.size()<<endl;
		}

		void ftScan(std::stringstream &os)
		{
			#if 0
				mStoreInspector->ftStats(os,-1);
			#endif
		}
} ;

class testindex : public ITest
{
		ISession *lsession;
		
	public:
		TEST_DECLARE(testindex);
		virtual char const * getName() const { return "testindex"; }
		virtual char const * getHelp() const { return ""; }		
		virtual char const * getDescription() const { return "To verify index tables(class and ft) after rebuilds"; }
		virtual bool includeInSmokeTest(char const *& pReason) const {pReason = "takes a long time, store index validation test"; return false; }
		virtual bool isLongRunningTest() const { return false; }
		virtual void destroy() { delete this; }		
		virtual int execute();
};
TEST_IMPLEMENT(testindex, TestLogger::kDStdOut);

int testindex::execute()
{
	size_t mVerifyCnt = 0;
	size_t bSuccess = 0;

	if(!MVTApp::startStore()) {
		mLogger.out()<<"failed to start store...\n";
		return -1;
	}

	if((lsession = MVTApp::startSession())==NULL){
		mLogger.out()<<"failed to create session....\n";
		return -1;
	}
	
	CStoreInfo indexInfo_bef(lsession),indexInfo_aft(lsession);
	std::stringstream ft_bef,ft_aft; 
	
	//get class details before rebuild
	mLogger.out()<<"Store Scan in progress....\n";
	indexInfo_bef.storeScan();
	mLogger.out()<<"DONE..\n\n";

	mLogger.out()<<"FT scan in progress.....\n";
	indexInfo_bef.ftScan(ft_bef);
	mLogger.out()<<"DONE...\n\n";

	//rebuild ALL classes and ftindex.
	mLogger.out()<<"Store rebuild in progres....\n";
	TVERIFYRC(updateClass(lsession,NULL,NULL));
	TVERIFYRC(lsession->rebuildIndexFT());
	mLogger.out()<<"DONE...\n\n";

	//get class details before rebuild
	mLogger.out()<<"Store Scan in progress....\n";
	indexInfo_aft.storeScan();
	mLogger.out()<<"DONE..\n\n";

	mLogger.out()<<"FT scan in progress.....\n";
	indexInfo_aft.ftScan(ft_aft);
	mLogger.out()<<"DONE..\n\n";

	//compare class results
	mLogger.out()<<"Validating results.....\n";
	for(size_t i=0;i<indexInfo_bef.mClasses.size();i++)
	{
		TVERIFY(indexInfo_bef.mClasses[i]->mClassID == indexInfo_aft.mClasses[i]->mClassID);
		//mLogger.out()<<"ClassID:"<<indexInfo_bef.mClasses[i]->mClassID<<"\t";

		TVERIFY(indexInfo_bef.mClasses[i]->mClassName == indexInfo_aft.mClasses[i]->mClassName);
		//mLogger.out()<<"ClassName:"<<indexInfo_bef.mClasses[i]->mClassName<<"\t";

		TVERIFY(indexInfo_bef.mClasses[i]->mFamily == indexInfo_aft.mClasses[i]->mFamily);
		//mLogger.out()<<"IsFamily:"<<indexInfo_bef.mClasses[i]->mFamily<<"\t";

		TVERIFY(indexInfo_bef.mClasses[i]->mPinCnt == indexInfo_aft.mClasses[i]->mPinCnt);
		//mLogger.out()<<"PIN count:"<<indexInfo_bef.mClasses[i]->mPinCnt<<"\t";

		TVERIFY(indexInfo_bef.mClasses[i]->mIndexedProp == indexInfo_aft.mClasses[i]->mIndexedProp);
		//mLogger.out()<<"IndexedPropID:"<<indexInfo_bef.mClasses[i]->mIndexedProp<<"\n";
		mLogger.out()<<"ClassID:"<<indexInfo_bef.mClasses[i]->mClassID<<"\t"<<"verified successfully..."<<i+1<<endl;
		mVerifyCnt++;
	}

	//compare ft results
	if(ft_aft.str()== ft_bef.str())
	{
		bSuccess = 1;
		mLogger.out()<<ft_aft.str()<<endl;
		mLogger.out()<<"Ft validation is successful..\n";
	}

	lsession->terminate();
	MVTApp::stopStore();
	return bSuccess&(indexInfo_bef.mClasses.size() == mVerifyCnt?0:-1);
}	
