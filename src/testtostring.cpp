/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <fstream>
#include "serialization.h"

using namespace std;

class TestToString : public ITest{
		PropertyID mPropIds[5];	
	public:
		TEST_DECLARE(TestToString);
		virtual char const * getName() const { return "testtostring"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "tests IExpr/IStmt toString() methods - 5936"; }
		virtual bool isPerformingFullScanQueries() const { return true; } // Note: countPinsFullScan...
		virtual bool includeInPerfTest() const { return false; }
		
		virtual int execute();
		virtual void destroy() { delete this; }		
	protected:
		void createPINs(ISession *pSession);
		bool testIExpr(ISession *pSession);
		bool testIQuery(ISession *pSession);		
};
TEST_IMPLEMENT(TestToString, TestLogger::kDStdOut);

int	TestToString::execute()
{
	bool lSuccess =	true;
	//RC rc;
	if (MVTApp::startStore()){		
		ISession * const lSession =	MVTApp::startSession();
		const int lMaxNumProps = sizeof(mPropIds) / sizeof(mPropIds[0]);
		int i;
		MVTApp::mapURIs(lSession,"TestToString.prop",lMaxNumProps,mPropIds);
		//Create few pins
		const int lNumPINs = 20;		
		{
			Value lPVs[lMaxNumProps];
			for(i = 0; i < lNumPINs; i++)
			{
				const int lNumProps = (int)(lMaxNumProps * rand()/RAND_MAX); 
				int j = 0;
				for(j = 0; j < lNumProps; j++){
					Tstring * lS = new Tstring;
					MVTRand::getString(*lS, 50, 0, true);
					SETVALUE(lPVs[j],mPropIds[j],lS->c_str(),OP_SET);
				}
				TVERIFYRC(lSession->createPIN(lPVs,lNumProps,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));
			}
		}
		// Case #1 : IExpr::toString()
		{
			TExprTreePtr lET;
			{		        
				Value val[2];
				// 1: 100 + 100 = 200
				val[0].set(100);
				val[1].set(100);
				TExprTreePtr lE1 = EXPRTREEGEN(lSession)(OP_PLUS, 2, val, 0);

				// 2: 20 * 25 = 500
				val[0].set(20);
				val[1].set(25);
				TExprTreePtr lE2 = EXPRTREEGEN(lSession)(OP_MUL, 2, val, 0);

				// 3: 2	- 1
				val[0].set(lE2);
				val[1].set(lE1);
				TExprTreePtr lE3 = EXPRTREEGEN(lSession)(OP_MINUS, 2, val, 0);

				// 4: lE3/30
				val[0].set(lE3);
				val[1].set(30);
				TExprTreePtr lE4 = EXPRTREEGEN(lSession)(OP_DIV, 2, val, 0);

				// 5: lE4 = 10  ((20 * 25) - (100 + 100)) / 30 = 10
				val[0].set(lE4);
				val[1].set(10);
				lET = EXPRTREEGEN(lSession)(OP_EQ, 2, val, 0);			
			}		
			IExpr *lE = lET->compile();
			
			Value lRetVal1; TVERIFYRC(lE->execute(lRetVal1));
			int lRetValBef = lRetVal1.b?0:1;

			lET->destroy();			

			//Serialize the IExpr	
			{
				Value lVal;
				lVal.set(lE);
				ofstream lFile;lFile.open("IExpr.log",ios::out);
				MvStoreSerialization::ContextOut lContextOut(lFile,lSession,false,false);
				MvStoreSerialization::PrimitivesOutRaw::outExpr(lContextOut,lVal);lFile.close();
			}

			// Deserialize the IExpr
			IExpr *lEIn;
			{
				Value lVal;
				lVal.length = 1;
				ifstream lFile; lFile.open("IExpr.log",ios::in);
				MvStoreSerialization::ContextIn lContextIn(lFile,lSession);
				MvStoreSerialization::PrimitivesInRaw::inExpr(lContextIn,lVal);lFile.close();
				lEIn = lVal.expr;			
			}		
			
			Value lRetVal2;TVERIFYRC(lEIn->execute(lRetVal2));
			int lRetValAft = lRetVal2.b?0:1;
			TVERIFY(lRetValAft == lRetValBef);
			if(lRetValAft!= lRetValBef){
				mLogger.out() << "ERROR : The IExpr got after de-serializing, resulted in a diff value " <<std::endl;
				lSuccess = false;
			}
		}

		// Case #2 : IStmt::toString()
		{
#if 1
			CompilationError ce;
			IStmt *lQ = lSession->createStmt("SELECT * WHERE (EXISTS($0) OR CONTAINS($0,'a')) AND EXISTS($1)",mPropIds,2,&ce);
#else
			IStmt *lQ = lSession->createStmt();
			unsigned int lVar = lQ->addVariable();		
			TExprTreePtr lET;
			{		        
				Value val[2];
				// 1: mPropIds[0] Exists
				val[0].setVarRef(0,mPropIds[0]);
				TExprTreePtr lE1 = EXPRTREEGEN(lSession)(OP_EXISTS, 1, val, 0);

				// 2: mPropIds[0] Contains "a"
				val[0].setVarRef(0,mPropIds[0]);
				val[1].set("a");
				TExprTreePtr lE2 = EXPRTREEGEN(lSession)(OP_CONTAINS, 2, val, 0);

				// 3: 1	|| 2
				val[0].set(lE1);
				val[1].set(lE2);
				TExprTreePtr lE3 = EXPRTREEGEN(lSession)(OP_LOR, 2, val, 0);

				// 4: mPropIds[1] Exists
				val[0].setVarRef(0,mPropIds[1]);
				TExprTreePtr lE4 = EXPRTREEGEN(lSession)(OP_EXISTS, 1, val, 0);

				// 5: 3	&& 4
				val[0].set(lE3);
				val[1].set(lE4);
				lET = EXPRTREEGEN(lSession)(OP_LAND, 2, val, 0);			
			}		
			lQ->addCondition(lVar,lET);
			lET->destroy();
#endif
			
			//Serialize the IStmt	
			{
				Value lVal;
				lVal.set(lQ);
				ofstream lFile;lFile.open("IStmt.log",ios::out);
				MvStoreSerialization::ContextOut lContextOut(lFile,lSession,false,false);
				MvStoreSerialization::PrimitivesOutRaw::outQuery(lContextOut,lVal);lFile.close();
			}
			mLogger.out() << lQ->toString() << std::endl;
			ICursor * lR = NULL;
			TVERIFYRC(lQ->execute(&lR));
			mLogger.out() << " List of PINs " << std::endl;
			const int lExpCnt = MVTApp::countPinsFullScan(lR,lSession);

			lR->destroy();
			lQ->destroy();	

			// Deserialize the IStmt
			IStmt *lQIn;
			int lQInCnt = 0;
			{
				Value lVal;
				lVal.length = 1;
				ifstream lFile; lFile.open("IStmt.log",ios::in);
				MvStoreSerialization::ContextIn lContextIn(lFile,lSession);
				MvStoreSerialization::PrimitivesInRaw::inQuery(lContextIn,lVal);lFile.close();
				lQIn = lVal.stmt; TVERIFY(lQIn!=NULL);
				if (lQIn!=NULL)
				{
					mLogger.out() << lQIn->toString() << std::endl;
					ICursor * lR = NULL;
					TVERIFYRC(lQIn->execute(&lR));
					mLogger.out() << " List of PINs " << std::endl;
					lQInCnt = MVTApp::countPinsFullScan(lR,lSession);
					lR->destroy();
					lQIn->destroy();
				}
			}
			TVERIFY(lQInCnt == lExpCnt);
			if(lQInCnt!= lExpCnt)
			{
				mLogger.out() << "ERROR : The IStmt got after de-serializing, resulted in a diff result set " <<std::endl;
				mLogger.out() << "Expected =  " << lExpCnt << " ; Returned = " << lQInCnt << std::endl;
				lSuccess = false;
			}
		}

		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	return lSuccess	? 0	: 1;
}
