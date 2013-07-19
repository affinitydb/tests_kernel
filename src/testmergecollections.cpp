/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"
#include "collectionhelp.h"

#define NPINS	100
#define RANGE	200
#define MAX_ELEM_INCOLLECTION 5

// Publish this test.
class TestMergeCollections : public ITest
{
	public:
		TEST_DECLARE(TestMergeCollections);
		virtual char const * getName() const { return "testmergecollections"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "simple join query test"; }
		virtual int execute();
		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestMergeCollections, TestLogger::kDStdOut);

// Implement this test.
int TestMergeCollections::execute()
{
	bool lSuccess =	true;
	if (MVTApp::startStore())
	{
		ISession * const lSession =	MVTApp::startSession();

		// Declare properties.
		URIMap	lData[2];
		MVTApp::mapURIs(lSession,"TestMergeCollections.prop",2,lData);
		PropertyID const lPropIdVal    = lData[0].uid;
		PropertyID const lPropIdRef    = lData[1].uid;
		PropertyID const lPropIdPinId  = PROP_SPEC_PINID;

		ClassID lClassVal = STORE_INVALID_CLASSID;
		{
			char lB[100]; sprintf(lB,"TestMergeCollections.ClassVal.%d",rand());
			Value lV[2]; 
			lV[0].setVarRef(0,lPropIdVal);
			lV[1].setParam(0);
			IExprTree *expr=lSession->expr(OP_LT,2,lV);
			IStmt *lQ = lSession->createStmt();
			lQ->addVariable(NULL,0,expr);
			TVERIFYRC(defineClass(lSession,lB,lQ,&lClassVal));
			lQ->destroy();
		}

		ClassID lClassRef = STORE_INVALID_CLASSID;
		{
			char lB[100]; sprintf(lB,"TestMergeCollections.ClassRef.%d",rand());
			IStmt *lQ = lSession->createStmt();
			lQ->addVariable();
			lQ->setPropCondition(0,&lPropIdRef,1);
			TVERIFYRC(defineClass(lSession,lB,lQ,&lClassRef));
			lQ->destroy();
		}

		/**
		* The following creates the family on reference class...
		**/		
		ClassID lClassFamilyOnRef = STORE_INVALID_CLASSID;
		{
			char lB[100]; sprintf(lB,"TestMergeCollections.ClassFamilyOnRef.%d",rand());
			Value lVFoR[2];
			lVFoR[0].setVarRef(0,lPropIdRef);
			lVFoR[1].setParam(0);
			IExprTree *expr=lSession->expr(OP_IN,2,lVFoR);
			IStmt *lQ = lSession->createStmt();
			lQ->addVariable(NULL,0,expr);
			TVERIFYRC(defineClass(lSession,lB,lQ,&lClassFamilyOnRef));
			lQ->destroy();
		}

		// Create a few pins
		mLogger.out() << "Creating " << NPINS*2 << " pins... ";

		PID pins[NPINS]; bool lfMember[NPINS];
		PID refferedPins[NPINS];

		//Creating first group of pins: 
		// - each pin may have an interger value, or collection of interger values; 
		// - each pin may have the same interger value within collection; 
		// - if at least one of the values within pin < treshold, it should be picked up by the family... 		
		const int threshold = RANGE/3; int i;
		if(isVerbose()) mLogger.out() << " List of PINs with value less than threshold " << std::endl;
		for (i = 0; i < NPINS; i++)
		{
			Value lV[2]; 
			int val = MVTRand::getRange(0, RANGE);
			lfMember[i] = val < threshold;
			SETVALUE(lV[0], lPropIdVal, val, OP_SET);
			CREATEPIN(lSession, pins[i], lV, 1);
                        
			IPIN *ppin = lSession->getPIN(pins[i]);
			int elemincollection = MVTRand::getRange(1, MAX_ELEM_INCOLLECTION);
			for(int jj = 0; jj < elemincollection; jj++)
			{
				int val2 = MVTRand::getRange(1, RANGE);
				if(!lfMember[i] && (val2 < threshold)) 
					lfMember[i] = true;
				SETVALUE_C(lV[0], lPropIdVal, val2, OP_ADD, STORE_LAST_ELEMENT);
				TVERIFYRC(ppin->modify(lV,1));
			}
			if(isVerbose() && lfMember[i]) mLogger.out() << std::hex <<  pins[i].pid << std::endl;
			ppin->destroy();
		}

		// Create second group of pins:
		//  - each pin within that group has either reference or collection of references 
		//    to a random pin(s) in the set above
		//  - the reference to the pin, which value < threshold may be within any position within collection; 
		//  - it may be multiple references to the same pin, including the pin which value < threshold; 		
		if(isVerbose()) mLogger.out() << " List of PINs with refs whose value is less than threshold " << std::endl;
		int lCount = 0; 
		int lCountDiff = 0; //counts how many pins with value < threshold has been added while creating collection... 
		for (i = 0; i < NPINS; i++)
		{
			PID lReferPID;
			INITLOCALPID(lReferPID); lReferPID.pid = STORE_INVALID_PID;
			Value lV[2]; 
			int idx = MVTRand::getRange(0, NPINS-1); 
			PID id; bool willbefound = false;
			if (lfMember[idx])
			{ 
				lCount++; 
				willbefound = true; // The referenced pin will match the family query
				lReferPID = pins[idx];
			} 
			SETVALUE(lV[0], lPropIdRef, pins[idx], OP_SET); 
			refferedPins[i] = pins[idx];    // to remember reffered pins;
			CREATEPIN(lSession, id, lV, 1);

			IPIN *ppin = lSession->getPIN(id);
			int elemincollection = MVTRand::getRange(1, MAX_ELEM_INCOLLECTION);
			for(int jj = 0; jj < elemincollection; jj++)
			{
				int idx1 = MVTRand::getRange(0, NPINS-1);
				SETVALUE_C(lV[0], lPropIdRef, pins[idx1], OP_ADD, STORE_LAST_ELEMENT); 
				TVERIFYRC(ppin->modify(lV,1));

				if(!willbefound && lfMember[idx1])
				{ 
					lCount++; willbefound = true; 
					lReferPID = pins[idx1];
					lCountDiff++; 
				}  
			}
			if(isVerbose() && willbefound) mLogger.out() << std::hex << id.pid << " with ref to " << lReferPID.pid << std::endl;
			ppin->destroy();
		}
		
		mLogger.out() << std::dec << "DONE" << std::endl;
		
		// Do a simple join search.
		// " All pins from lClassFamilyOnRef Family, where any of the PINs in lPropIdRef has (lPropIdVal < threshhold)
		
		// "All pins, from lClassRef class, where the lPropIdRef property is equal 
		// to the PROP_SPEC_PINID of a PIN matching the lClassVal query of (lPropIdVal < threshhold)"
		// 
		IStmt *lQ	= lSession->createStmt();
		
		Value lV[4]; 
		lV[0].setError(lPropIdRef); lV[1].setError(lPropIdRef); lV[2].setRange(&lV[0]);
		lV[3].set(threshold); // Family query will find all pins with lPropIdVal < threshhold

		SourceSpec classSpec[2]={{lClassFamilyOnRef,1,&lV[2]},{lClassVal,1,&lV[3]}};		
		unsigned char lVar1 = lQ->addVariable(&classSpec[0],1),lVar2 = lQ->addVariable(&classSpec[1],1);

		lV[0].setVarRef(lVar1,lPropIdRef);
		lV[1].setVarRef(lVar2,lPropIdPinId);
		IExprTree *expr = lSession->expr(OP_EQ,2,lV);
		lQ->join(lVar1,lVar2,expr);
		OrderSeg ord={NULL,lPropIdRef,0,0,0};
		lQ->setOrder(&ord,1);

		uint64_t lCnt = 0;
		TVERIFYRC(lQ->count(lCnt,NULL,0,~0u));

		TVERIFY((int)lCnt == lCount);
		
		{
			mLogger.out() << "Count returned = " << lCnt << ";  Expected Count = " << lCount << "; lCountDiff = " << lCountDiff << ";" <<  std::endl;

			//The query below - attempt to find how many pins with at least one value < threshold were found by Family...
			mLogger.out() << "The query below - attempt to find how many pins with at least one value < threshold were found by Family..." << std::endl; 
			IStmt *lQFamily = lSession->createStmt(); 
			lQFamily->addVariable(&classSpec[1],1);
			uint64_t lCntFFound = 0;
			TVERIFYRC(lQFamily->count(lCntFFound));
			mLogger.out() << "lCntFFound= " << lCntFFound << ";" << std::endl;
			lQFamily->destroy();
		}  
		
		ICursor *lR = NULL;
		TVERIFYRC(lQ->execute(&lR, NULL,0,~0u,0));
		int lResultCount = 0;
		IPIN *pInResults;
		if (isVerbose()) mLogger.out() << "List of PINs returned " << std::endl;
		while((pInResults=lR->next())!=NULL)
		{
			lResultCount++;
			//if (isVerbose()) MVTApp::output(*pInResults,mLogger.out());
			if (isVerbose()) mLogger.out() << std::hex << pInResults->getPID().pid << std::endl;
			
			const Value * refVal = pInResults->getValue(lPropIdRef); 
			if ( refVal!=NULL )
			{
				MvStoreEx::CollectionIterator lCollection(refVal);
				bool lBelongs = false;
				const Value *lValue;
				for(lValue = lCollection.getFirst(); lValue!=NULL; lValue = lCollection.getNext())
				{
					TVERIFY(lValue->type == VT_REFID);
					IPIN *lPIN = lSession->getPIN(lValue->id); TVERIFY(lPIN != NULL);
					const Value *lVal = lPIN->getValue(lPropIdVal); 
					MvStoreEx::CollectionIterator lValues(lVal);
					const Value *lVal1;
					for(lVal1 = lValues.getFirst(); lVal1!=NULL; lVal1 = lValues.getNext())
					{
						TVERIFY(lVal1->type == VT_INT);
						if(lVal1->i < threshold)
						{ lBelongs = true; break; }
					}
					lPIN->destroy();
					if(lBelongs) break;
				}
				TVERIFY(lBelongs && "Unexpected PIN returned");
			}
			else
			{
				TVERIFY(false && "Failed to fetch the collection");
			}

			pInResults->destroy();
		}

		mLogger.out() << " Total PINs returned in the result set " << std::dec << lResultCount << std::endl;
		TVERIFY(lResultCount == lCount);

		lR->destroy();
		lQ->destroy();

		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }

	return lSuccess ? 0 : 1;
}
