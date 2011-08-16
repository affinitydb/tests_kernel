/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

#define PROP_BASE	156000

// Publish this test.
class TestRefVal : public ITest
{
	public:
		TEST_DECLARE(TestRefVal);
		virtual char const * getName() const { return "testrefval"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "bashes on references to properties and values"; }
		virtual bool includeInPerfTest() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestRefVal, TestLogger::kDStdOut);

// Implement this test.
struct StoreReference { RefVID mReferencing; RefVID mReferenced; };
class SortByReferenced { public: bool operator()(StoreReference const & p1, StoreReference const & p2) const {
	if (LOCALPID(p1.mReferenced.id) != LOCALPID(p2.mReferenced.id)) return (LOCALPID(p1.mReferenced.id) < LOCALPID(p2.mReferenced.id));
	if (p1.mReferenced.pid != p2.mReferenced.pid) return (p1.mReferenced.pid < p2.mReferenced.pid);
	if (p1.mReferenced.eid != p2.mReferenced.eid) return (p1.mReferenced.eid < p2.mReferenced.eid);
	if (LOCALPID(p1.mReferencing.id) != LOCALPID(p2.mReferencing.id)) return (LOCALPID(p1.mReferencing.id) < LOCALPID(p2.mReferencing.id));
	return (p1.mReferencing.pid < p2.mReferencing.pid); } };
static inline bool equalRefVIDs(RefVID const & p1, RefVID const & p2) { return p1.id == p2.id && p1.pid == p2.pid && p1.eid == p2.eid; }

int TestRefVal::execute()
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		ISession * const lSession =	MVTApp::startSession();
		const static int lNumProps = 25;
		PropertyID lPropIDs[lNumProps];
		MVTApp::mapURIs(lSession,"TestRefVal.prop",lNumProps,lPropIDs);

		// Create a few pins with properties, collections,
		// and references to other properties or values.
		typedef std::set<StoreReference, SortByReferenced> TReferences;
		TReferences lReferences;
		int i, j, k;
		PID lPIDs[100];
		for (i = 0; i < 100; i++)
		{
			CREATEPIN(lSession, lPIDs[i], NULL, 0);
			IPIN * const lPIN = lSession->getPIN(lPIDs[i]);
			int const lNumProps = MVTRand::getRange(1, 20);
			for (j = 0; j < lNumProps; j++)
			{
				int const lNumVals = MVTRand::getRange(1, 5);
				for (k = 0; k < lNumVals; k++)
				{
					bool lDoReference = i > 0 && MVTRand::getRange(0, 10) > 3;

					// Either create a reference to some other property/value created earlier...
					if (lDoReference)
					{
						StoreReference lReference;
						lReference.mReferencing.id = lPIDs[i];
						lReference.mReferencing.pid = lPropIDs[j];
						lReference.mReferencing.eid = 0;

						// Choose a PIN to reference.
						int const lPINReferencedIndex = MVTRand::getRange(0, i-1);
						lReference.mReferenced.id = lPIDs[lPINReferencedIndex];

						// Choose a property to reference.
						IPIN * lPINReferenced;
						bool lOLMethod = MVTRand::getRange(0, 100) > 33;
						if(lOLMethod){
							lPINReferenced = lSession->getPIN(lReference.mReferenced.id);
						}
						else{
							Value lVal;
							lVal.set(lReference.mReferenced.id);
							lPINReferenced = lSession->getPIN(lVal);	
						}
						int lNumPINProps = (int)lPINReferenced->getNumberOfProperties();
						unsigned int const lPropReferencedIndex = (unsigned int)MVTRand::getRange(0, lNumPINProps-1);
						Value const * const lPropReferenced = lPINReferenced->getValue(lPropIDs[lPropReferencedIndex]);
						lReference.mReferenced.pid = lPropReferenced->property;

						// Choose a value to reference, if relevant.
						if (lPropReferenced->type == VT_ARRAY)
						{
							int const lValueReferencedIndex = MVTRand::getRange(0, (int)lPropReferenced->length-1);
							lReference.mReferenced.eid = lPropReferenced->varray[lValueReferencedIndex].eid;
						}
						else
						{
							lReference.mReferenced.eid = STORE_COLLECTION_ID;
							if(lPropReferenced->type == VT_COLLECTION) 
								TVERIFY(false && "navigator returned even after setting session interface mode to ITF_COLLECTIONS_AS_ARRAYS");
						}

						// Unless this exact same reference already exists, create it.
						if (lReferences.end() != lReferences.find(lReference))
							lDoReference = false;
						else
						{
							Value lPV;
							SETVALUE(lPV, lPropIDs[j], lReference.mReferenced, OP_ADD);
							if (RC_OK != lPIN->modify(&lPV, 1))
							{
								lSuccess = false;
								assert(false);
							}
							else
								lReferences.insert(lReference);
						}
					}

					// Or create a new property.
					if (!lDoReference)
					{
						Value lPV;
						Tstring lString;
						MVTRand::getString(lString, 20, 0, false);
						SETVALUE_C(lPV, lPropIDs[j], lString.c_str(), OP_ADD, STORE_LAST_ELEMENT);
						if (RC_OK != lPIN->modify(&lPV, 1))
						{
							#if 0
								lSuccess = false;
								assert(false);
							#endif
						}
					}
				}
			}
			lPIN->destroy();
		}

		#if 0 // Note: Queries for VT_REFIDELT not supported yet...
		// Query to see if all relationships are found.
		TReferences::iterator lItRef;
		for (lItRef = lReferences.begin(); lReferences.end() != lItRef; lItRef++)
		{
			// Execute the mvstore query.
			IStmt * const lQ = lSession->createStmt();
			unsigned char const lVar = lQ->addVariable();
			Value lV[2];
			PropertyID lPropIds[] = {(*lItRef).mReferencing.pid};
			lV[0].setVarRef(0, *lPropIds);
			lV[1].set((*lItRef).mReferenced);
			TExprTreePtr lExprTree = EXPRTREEGEN(lSession)(OP_EQ, 2, lV, 0);
			lQ->addCondition(lExprTree);
			ICursor * const lQR = lQ->execute();

			// Check that all objects found by the mvstore query are correct.
			IPIN * lQRIter;
			int lNumFound = 0;
			for (lQRIter = lQR->next(); NULL != lQRIter; lQRIter = lQR->next(), lNumFound++)
			{
				StoreReference lRef;
				lRef.mReferencing.id = lQRIter->getPID();
				lRef.mReferencing.pid = (*lItRef).mReferencing.pid;
				lRef.mReferenced = (*lItRef).mReferenced;
				if (lReferences.end() == lReferences.find(lRef))
				{
					lSuccess = false;
					mLogger.out() << "Query found a reference that did not exist!" << std::endl;
				}
			}

			// Check that the mvstore query found exactly the right number of objects.
			int lNumExpected = 0;
			TReferences::iterator lItRef2;
			StoreReference lRef;
			lRef.mReferenced = (*lItRef).mReferenced;
			INITLOCALPID(lRef.mReferencing.id);
			LOCALPID(lRef.mReferencing) = 0;
			lRef.mReferencing.pid = 0;
			lRef.mReferencing.eid = 0;
			for (lItRef2 = lReferences.lower_bound(lRef); lReferences.end() != lItRef2 && equalRefVIDs((*lItRef2).mReferenced, lRef.mReferenced); lItRef2++)
				if ((*lItRef2).mReferencing.pid == (*lItRef).mReferencing.pid)
					lNumExpected++;
			if (lNumFound != lNumExpected)
			{
				lSuccess = false;
				mLogger.out() << "Query found " << lNumFound << " referencing objects instead of " << lNumExpected << std::endl;
			}

			// Cleanup.
			lQR->destroy();
			lQ->destroy();
		}
		#endif

		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	return lSuccess	? 0	: 1;
}
