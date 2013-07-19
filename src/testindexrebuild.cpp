/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"

class TestIndexRebuild : public ITest
{
		static const int sNumProps = 20;
		static const int sNumPINs = 50;
		static const int sNumFamiliesPerType = 10;
		static const int sNumClassesPerType = 10;

		PropertyID mPropIDs[sNumProps];
		ISession *mSession;
		std::vector<Tstring> mClasses, mFamilies;
		Tstring mClassStr;
		bool mUseCollection;
		int mClassType, mFamilyType;
	public:
		TEST_DECLARE(TestIndexRebuild);
		virtual char const * getName() const { return "testindexrebuild"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Tests Class/Family Index rebuild"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { return false; }
		virtual bool isLongRunningTest() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void doTest();
		void createClasses();
		void createFamilies();
		void createData();

		// Helpers
		void  getClassName(Tstring &pClassName, int pClassType, int pClassIndex);
		void getFamilyName(Tstring &pFamilyName, int pFamilyType, int pFamilyIndex);
		unsigned long getCountForClass(const char *pClassName);
		unsigned long getCountForClass(ClassID pCLSID);
		void dropClass(const char *pClassName = NULL);
		void initialize(bool pDrop = false);

		void rebuildClassIndex();
		void rebuildFamilyIndex();
		void rebuildAllClassIndexAtOnce();
		void rebuildDroppedClass();
		void rebuildDroppedFamily();

};
TEST_IMPLEMENT(TestIndexRebuild, TestLogger::kDStdOut);

void TestIndexRebuild::doTest()
{
	initialize();
	createClasses();
	createFamilies();
	createData();

	mLogger.out() << " Case #1: Rebuild Individual Class and Family: " << std::endl;
	{
		rebuildClassIndex();
		rebuildFamilyIndex();		
	}

	mLogger.out() << " Case #2: Rebuild Complete Class index: " << std::endl;	
	{		
		rebuildAllClassIndexAtOnce();
	}

	mLogger.out() << " Case #3: Try rebuilding an Class with its alias: " << std::endl;

	mLogger.out() << " Case #4: Try rebuilding an Class which is dropped: " << std::endl;
	{
		rebuildDroppedClass();
		rebuildDroppedFamily();
	}

	// Reset all variables
	//initialize(true);

}

void TestIndexRebuild::dropClass(const char *pClassName)
{
	if(pClassName)
	{
		ClassID lCLSID = STORE_INVALID_CLASSID;
		TVERIFYRC(mSession->getClassID(pClassName, lCLSID)); TVERIFY(lCLSID != STORE_INVALID_CLASSID);
		TVERIFYRC(ITest::dropClass(mSession,pClassName));	
		std::vector<Tstring>::iterator lIt;
		bool lFound = false;
		for(lIt = mClasses.begin(); lIt != mClasses.end(); lIt++)
			if(strcmp((*lIt).c_str(), pClassName) == 0) 
			{ lFound = true; break; }
		if(lFound) mClasses.erase(lIt);
	}
	else
	{
		std::vector<Tstring> lClasses;
		MVTApp::enumClasses(*mSession, &lClasses);
		std::vector<Tstring>::iterator lIter;
		for(lIter = lClasses.begin(); lClasses.end() != lIter; lIter++)
		{
			if(isVerbose())  mLogger.out() << "Dropping class: " << *lIter <<std::endl;
			std::vector<Tstring>::iterator lIt;
			bool lCurrentTest = false;
			for(lIt = mClasses.begin(); lIt != mClasses.end(); lIt++)
				if(strcmp((*lIt).c_str(), (*lIter).c_str()) == 0) 
				{ lCurrentTest = true; break; }
				if(lCurrentTest) TVERIFYRC(ITest::dropClass(mSession,(*lIter).c_str()));
		}
		lClasses.clear(); 
		mClasses.clear(); mFamilies.clear();
	}
}

void TestIndexRebuild::initialize(bool pDrop)
{
	if(pDrop) dropClass();
	mClassType =  mFamilyType = 0;
	MVTApp::mapURIs(mSession, "TestIndexRebuild.prop", sNumProps, mPropIDs);
	MVTRand::getString(mClassStr, 5, 10, false, false);	
}

unsigned long TestIndexRebuild::getCountForClass(ClassID pCLSID)
{
	TVERIFY(pCLSID != STORE_INVALID_CLASSID);
	CmvautoPtr<IStmt> lQ(mSession->createStmt());
	SourceSpec lCS; lCS.objectID = pCLSID; lCS.nParams = 0; lCS.params = NULL;
	lQ->addVariable(&lCS, 1);
	uint64_t lCount = 0;
	TVERIFYRC(lQ->count(lCount));
	return (unsigned long)lCount;
}

unsigned long TestIndexRebuild::getCountForClass(const char *pClassName)
{
	ClassID lCLSID = STORE_INVALID_CLASSID;
	TVERIFYRC(mSession->getClassID(pClassName, lCLSID)); TVERIFY(lCLSID != STORE_INVALID_CLASSID);
	return getCountForClass(lCLSID);
}

void  TestIndexRebuild::getClassName(Tstring &pClassName, int pClassType, int pClassIndex)
{
	TVERIFY(pClassType <= mClassType);
	char lB[124]; sprintf(lB, "TestIndexRebuild.%s.Type.%d.Class.%d", mClassStr.c_str(), pClassType, pClassIndex);
	pClassName = lB;	
}

void  TestIndexRebuild::getFamilyName(Tstring &pFamilyName,int pFamilyType, int pFamilyIndex)
{
	TVERIFY(pFamilyType <= mFamilyType);
	char lB[124]; sprintf(lB, "TestIndexRebuild.%s.Type.%d.Family.%d", mClassStr.c_str(), pFamilyType, pFamilyIndex);
	pFamilyName = lB;	
}

void TestIndexRebuild::rebuildDroppedClass()
{
	// Check if rebuilding index on a dropped class doesn't cause any issues
	TVERIFY(mClassType > 0);
	int i = 0;
	for(i = 0; i < mClassType; i++)
	{
		Tstring lStr; getClassName(lStr, i, 0);
		const char * lClassName = lStr.c_str();
		#if 0
			// Note (maxw, Feb2011): Doesn't seem to make sense anymore.
			dropClass(lClassName);
		#endif
		TVERIFYRC(updateClass(mSession,lClassName, NULL));
	}
}

void TestIndexRebuild::rebuildDroppedFamily()
{
	// Check if rebuilding index on a dropped family doesn't cause any issues
	TVERIFY(mFamilyType > 0);
	int i = 0;
	for(i = 0; i < mFamilyType; i++)
	{
		Tstring lStr; getFamilyName(lStr, i, 0);
		const char * lFamilyName = lStr.c_str();
		#if 0
			// Note (maxw, Feb2011): Doesn't seem to make sense anymore.
			dropClass(lFamilyName);
		#endif
		TVERIFYRC(updateClass(mSession,lFamilyName, NULL));
	}
}

void TestIndexRebuild::rebuildAllClassIndexAtOnce()
{
	mLogger.out() << "\tRebuilding all Class Index at once ..." << std::endl;
	std::vector<int> lClassCount;
	int i, j;
	for(i = 0; i < mClassType; i++)
	{
		for(j = 0; j < sNumClassesPerType; j++)
		{
			Tstring lStr; getClassName(lStr, i, j);
			const char * lClassName = lStr.c_str();
			int lCount = getCountForClass(lClassName);
			lClassCount.push_back(lCount);			
		}
	}

	TVERIFYRC(updateClass(mSession,NULL, NULL));

	int k = 0;
	for(i = 0; i < mClassType; i++)
	{
		for(j = 0; j < sNumClassesPerType; j++)
		{
			Tstring lStr; getClassName(lStr, i, j);
			const char * lClassName = lStr.c_str();
			int lBeforeRebuildCnt = lClassCount[k++];
			int lAfterRebuildCnt = getCountForClass(lClassName);
			TVERIFY(lBeforeRebuildCnt == lAfterRebuildCnt);
		}
	}
}


void TestIndexRebuild::rebuildClassIndex()
{
	mLogger.out() << "\tRebuilding Class Index ... " << std::endl;

	int i, j;
	for(i = 0; i < mClassType; i++)
	{
		for(j = 0; j < sNumClassesPerType; j++)
		{
			Tstring lStr; getClassName(lStr, i, j);
			const char * lClassName = lStr.c_str();
			int lBeforeRebuildCnt = getCountForClass(lClassName);
			TVERIFYRC(updateClass(mSession,lClassName, NULL));
			int lAfterRebuildCnt = getCountForClass(lClassName);
			TVERIFY(lBeforeRebuildCnt == lAfterRebuildCnt);
		}
	}
}

void TestIndexRebuild::rebuildFamilyIndex()
{
	mLogger.out() << "\tRebuilding Family Index ... " << std::endl;

	int i, j;
	for(i = 0; i < mFamilyType; i++)
	{
		for(j = 0; j < sNumFamiliesPerType; j++)
		{
			Tstring lStr; getFamilyName(lStr, i, j);
			const char * lFamilyName = lStr.c_str();
			int lBeforeRebuildCnt = getCountForClass(lFamilyName);
			TVERIFYRC(updateClass(mSession,lFamilyName, NULL));
			int lAfterRebuildCnt = getCountForClass(lFamilyName);
			TVERIFY(lBeforeRebuildCnt == lAfterRebuildCnt);
		}
	}
}
void TestIndexRebuild::createData()
{
	mLogger.out() << "\tCreating " << sNumPINs << " PINs ... ";
	int i, j;
	for(i = 0; i < sNumPINs; i++)
	{
		if(i % 50 == 0) mLogger.out() << ".";
		PID lPID;
		Value lV[2];
		SETVALUE(lV[0], mPropIDs[0], MVTRand::getRange(0, 100), OP_SET);
		CREATEPIN(mSession, lPID, lV, 1);
		IPIN *lPIN = mSession->getPIN(lPID);

		if(MVTRand::getBool() && mUseCollection)
		{
			int lNumElements = MVTRand::getRange(0, 10);
			for(j = 0; j < lNumElements; j++)
			{
				SETVALUE_C(lV[0], mPropIDs[0], MVTRand::getRange(0, 100), OP_ADD, STORE_LAST_ELEMENT);
				TVERIFYRC(lPIN->modify(lV, 1));
			}
		}		
		
		for(j = 1; j < sNumProps; j++)
		{
			if(MVTRand::getBool())
			{
				Value lV[1];
				Tstring lStr; MVTRand::getString(lStr, 10, 20, true, false);
				SETVALUE(lV[0], mPropIDs[j], lStr.c_str(), OP_SET);
				TVERIFYRC(lPIN->modify(lV, 1));
				if(MVTRand::getBool() && mUseCollection)
				{
					int lNumElements = MVTRand::getRange(0, 3);
					int k;
					for(k = 0; k < lNumElements; k++)
					{
						MVTRand::getString(lStr, 10, 20, true, false);
						SETVALUE_C(lV[0], mPropIDs[k], lStr.c_str(), OP_ADD, STORE_LAST_ELEMENT);
						TVERIFYRC(lPIN->modify(lV, 1));
					}
				}
			}
		}
		lPIN->destroy();
	}
	mLogger.out() << std::endl;
}
void TestIndexRebuild::createFamilies()
{
	mLogger.out() << "\tCreating Families ... " << std::endl;
	int i = 0;	
	// Create simple Families /pin[prop0 < $var] 
	for(i = 0; i < sNumFamiliesPerType; i++)
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].setParam(0);
			lET = mSession->expr(OP_LT, 2, lV);
		}
		lQ->addCondition(lVar,lET); lET->destroy();	
		char lB[124]; sprintf(lB, "TestIndexRebuild.%s.Type.%d.Family.%d", mClassStr.c_str(), mFamilyType, i);
		TVERIFYRC(defineClass(mSession,lB, lQ));
		mFamilies.push_back(lB);
	}

	mFamilyType++;
	// Create simple Families /pin[prop0 in $var] 
	for(i = 0; i < sNumFamiliesPerType; i++)
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].setParam(0);
			lET = mSession->expr(OP_IN, 2, lV);
		}
		lQ->addCondition(lVar,lET); lET->destroy();	
		char lB[124]; sprintf(lB, "TestIndexRebuild.%s.Type.%d.Family.%d", mClassStr.c_str(), mFamilyType, i);
		TVERIFYRC(defineClass(mSession,lB, lQ));
		mFamilies.push_back(lB);
	}

	mFamilyType++;
	// Create simple Families /pin[(prop0 = 10 or prop0 = 50) and prop1 in $var] 
	for(i = 0; i < sNumFamiliesPerType; i++)
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(30);
			IExprTree *lET1 = mSession->expr(OP_GT, 2, lV);
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(40);
			IExprTree *lET2 = mSession->expr(OP_LT, 2, lV);
			lV[0].set(lET1);
			lV[1].set(lET2);
			IExprTree *lET3 = mSession->expr(OP_LAND, 2, lV);
			lV[0].setVarRef(0,mPropIDs[1]);
			lV[1].setParam(0);
			IExprTree *lET4 = mSession->expr(OP_IN, 2, lV);
			lV[0].set(lET3);
			lV[1].set(lET4);
			lET = mSession->expr(OP_LAND, 2, lV);
		}
		lQ->addCondition(lVar,lET); lET->destroy();	
		char lB[124]; sprintf(lB, "TestIndexRebuild.%s.Type.%d.Family.%d", mClassStr.c_str(), mFamilyType, i);
		TVERIFYRC(defineClass(mSession,lB, lQ));
		mFamilies.push_back(lB);
	}
	mFamilyType++;

}
void TestIndexRebuild::createClasses()
{
	mLogger.out() << "\tCreating Classes ... " << std::endl;
	int i = 0;
	// Create simple classes /pin[prop0 < 50] 
	for(i = 0; i < sNumClassesPerType; i++)
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(50);
			lET = mSession->expr(OP_LT, 2, lV);
		}
		lQ->addCondition(lVar,lET); lET->destroy();	
		char lB[124]; sprintf(lB, "TestIndexRebuild.%s.Type.%d.Class.%d", mClassStr.c_str(), mClassType, i);
		TVERIFYRC(defineClass(mSession,lB, lQ));
		mClasses.push_back(lB);
	}
	mClassType++;
	// /pin[pin has propX]
	for(i = 0; i < sNumClassesPerType; i++)
	{
		IStmt *lQ = mSession->createStmt();
		lQ->addVariable();
		lQ->setPropCondition(0, &mPropIDs[i], 1);
		char lB[124]; sprintf(lB, "TestIndexRebuild.%s.Type.%d.Class.%d", mClassStr.c_str(), mClassType, i);
		TVERIFYRC(defineClass(mSession,lB, lQ));
		mClasses.push_back(lB);
	}

	mClassType++;
	// /pin[prop0 >= 50 and pin has prop1]
	for(i = 0; i < sNumClassesPerType; i++)
	{
		IStmt *lQ = mSession->createStmt();
		unsigned char lVar = lQ->addVariable();
		IExprTree *lET;
		{
			Value lV[2];
			lV[0].setVarRef(0,mPropIDs[0]);
			lV[1].set(50);
			IExprTree *lET1 = mSession->expr(OP_GE, 2, lV);
			lV[0].setVarRef(0,mPropIDs[1]);
			IExprTree *lET2 = mSession->expr(OP_EXISTS, 1, lV);
			lV[0].set(lET1);
			lV[1].set(lET2);
			lET = mSession->expr(OP_LAND, 2, lV);
		}
		lQ->addCondition(lVar,lET); lET->destroy();
		char lB[124]; sprintf(lB, "TestIndexRebuild.%s.Type.%d.Class.%d", mClassStr.c_str(), mClassType, i);
		TVERIFYRC(defineClass(mSession,lB, lQ));
		mClasses.push_back(lB);
	}
	mClassType++;
}

int TestIndexRebuild::execute()
{	
	if (MVTApp::startStore())
	{
		mSession = MVTApp::startSession();	
		mUseCollection = false;
		doTest();
		mSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
 	return 0;
}
