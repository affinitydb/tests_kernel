/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

// Publish this test.
class TestBatch : public ITest
{
	public:
		TEST_DECLARE(TestBatch);
		virtual char const * getName() const { return "testbatch"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "bashes on pin batch insert"; }
		virtual bool includeInPerfTest() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void createBatchPIN(IBatch & pBatch, PropertyID const * pPropIDs, int pNumProps);
};
TEST_IMPLEMENT(TestBatch, TestLogger::kDStdOut);

// Implement this test.
static bool isExpectedRelationship(Value const & pV, PID const & pPIDTo)
{
	if (VT_REF == pV.type && pV.pin->getPID() == pPIDTo)
		return true;
	else if (VT_REFID == pV.type && pV.id == pPIDTo)
		return true;
	else if (VT_ARRAY == pV.type)
	{
		bool lFound = false;
		size_t i;
		for (i = 0; i < pV.length && !lFound; i++)
			lFound = isExpectedRelationship(pV.varray[i], pPIDTo);
		return lFound;
	}
	else if (VT_COLLECTION == pV.type)
	{
		bool lFound = false;
		Value const * lNext = pV.nav->navigate(GO_FIRST);
		while (lNext && !lFound)
		{
			lFound = isExpectedRelationship(*lNext, pPIDTo);
			lNext = pV.nav->navigate(GO_NEXT);
		}
		return lFound;
	}
	return false;
}

struct Relationship { int mFromIdx, mToIdx; IPIN * mToPtr; PID mTo; PropertyID mPropID; Relationship() : mFromIdx(-1), mToIdx(-1), mToPtr(NULL), mPropID(STORE_INVALID_URIID) { INITLOCALPID(mTo); LOCALPID(mTo) = STORE_INVALID_PID; } };
struct CommittedRelationship { PID mFrom, mTo; PropertyID mPropID; };

int TestBatch::execute()
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		ISession * const lSession =	MVTApp::startSession();
		static const int sNumProps = 3;
		PropertyID lPropIDs[sNumProps];// = {1000, 1001, 1002};
		MVTApp::mapURIs(lSession,"TestBatch.prop",sNumProps, lPropIDs);
		int const lNumProps = sizeof(lPropIDs) / sizeof(lPropIDs[0]);
		RC lRC;

		#define HOLD_RELATIONSHIPS_BY_PID 1
		#if HOLD_RELATIONSHIPS_BY_PID
			std::vector<PID> lGlobalBucket;
			std::vector<CommittedRelationship> lRelationships;
		#else
			std::vector<IPIN *> lGlobalBucket;
			std::vector<Relationship> lRelationships;
		#endif
		std::set<uint64_t> lGlobalBucketS;
		
		// Create increasingly large clouds of inter-related pins.
		int i, j, lCloudSize;
		for (i = 0, lCloudSize = 10; i < 50; i++, lCloudSize += 10)
		{
			int lNewRels = 0;
			int lLocalBucketSize = 0;
			IBatch * lBatch = lSession->createBatch();
			#if HOLD_RELATIONSHIPS_BY_PID
				std::vector<Relationship> lTmpRelationships;
			#endif

			#define TEST_FORWARD_REFERENCES 0
			#if TEST_FORWARD_REFERENCES
			for (j = 0; j < lCloudSize; j++)
			{
				createBatchPIN(*lBatch, lPropIDs, lNumProps);
				lLocalBucketSize++;
			}
			#endif

			mLogger.out() << "Creating cloud of " << lCloudSize << " uncommitted pins." << std::endl;
			for (j = 0; j < lCloudSize; j++)
			{
				// Create an uncommitted pin.
				#if !TEST_FORWARD_REFERENCES
					createBatchPIN(*lBatch, lPropIDs, lNumProps);
					lLocalBucketSize++;
				#endif

				// Randomly assign references to some other uncommitted/committed pins.
				int const lIndexPropRef = (int)(lNumProps * rand() / RAND_MAX);
				bool const lUncommittedRef = (100.0 * rand() / RAND_MAX) > 33.0;
				if (lIndexPropRef < lNumProps &&
					((lUncommittedRef && lLocalBucketSize > 0) ||
					(!lUncommittedRef && !lGlobalBucket.empty())))
				{
					int const lMaxElms = (int)(lUncommittedRef ? lLocalBucketSize : lGlobalBucket.size());
					int const lIndexRef = (int)((double)lMaxElms * rand() / RAND_MAX);
					int const lNumElms = min(lMaxElms, 5);
					int m;
					for (m = 0; m < lNumElms && (lIndexRef + m < lMaxElms); m++)
					{
						Value lV; int lToIdx = -1; PID lToPID;
                        INITLOCALPID(lToPID); lToPID.pid = STORE_INVALID_PID;
						if (lUncommittedRef)
						{
							lToIdx = lIndexRef + m;
							SETVALUE_C(lV, lPropIDs[lIndexPropRef], lToIdx, OP_ADD, STORE_LAST_ELEMENT);
						}
						else
						{
							#if HOLD_RELATIONSHIPS_BY_PID
								lToPID = lGlobalBucket[lIndexRef + m];
								SETVALUE_C(lV, lPropIDs[lIndexPropRef], lToPID, OP_ADD, STORE_LAST_ELEMENT);
							#else
								lTo = lGlobalBucket[lIndexRef + m];
								SETVALUE_C(lV, lPropIDs[lIndexPropRef], lTo, OP_ADD, STORE_LAST_ELEMENT);
							#endif
						}
						if (RC_OK != (lRC = lBatch->addRef(unsigned(j), lV)))
						{
							mLogger.out() << "Failed to addRef from batch element #" << j << " to "; MVTApp::output(lV, mLogger.out(), lSession); mLogger.out() << " with RC=" << lRC << std::endl;
							lSuccess = false;
							TVERIFY(false);
						}
						else
						{
							#if 0
								mLogger.out() << "*";
							#endif

							Relationship lR;
							lR.mFromIdx = j;
							lR.mPropID = lPropIDs[lIndexPropRef];
							if (lUncommittedRef)
							{
								TVERIFY(lToIdx >= 0);
								lR.mToIdx = lToIdx;
							}
							else
							{
								#if HOLD_RELATIONSHIPS_BY_PID
									TVERIFY(STORE_INVALID_PID != LOCALPID(lToPID));
									lR.mTo = lToPID;
									lTmpRelationships.push_back(lR);
								#else
									TVERIFY(lTo);
									lR.mToPtr = lTo;
									lRelationships.push_back(lR);
								#endif
							}
							lNewRels++;
						}
					}
				}
			}
			mLogger.out() << " (" << lNewRels << " new relationships)" << std::endl;

			// Commit the new cloud of pins.
			TVERIFY(lBatch->getNumberOfPINs() == size_t(lLocalBucketSize));
			mLogger.out() << "  Committing the cloud." << std::endl;
			if (RC_OK != lBatch->process(false))
			{
				lSuccess = false;
				mLogger.out() << "IBatch::process failed!" << std::endl;
				TVERIFY(false);
			}
			else
			{
				// Check that all were committed properly.
				// Note: invoking IBatch::getPIDs so many times like this is a bit funny,
				//       but this is a test after all :)
				for (j = 0; j < lLocalBucketSize; j++)
				{
					PID lPID; unsigned nP = 1;
					TVERIFYRC(lBatch->getPIDs(&lPID, nP, j));
					TVERIFY(1 == nP);
					if (LOCALPID(lPID) == STORE_INVALID_PID)
					{
						lSuccess = false;
						mLogger.out() << "Error! PIN was not really committed?" << std::endl;
						assert(false);
					}
					else if (lGlobalBucketS.end() != lGlobalBucketS.find(LOCALPID(lPID)))
					{
						lSuccess = false;
						mLogger.out() << "Error! commitPINs attributed to a new PIN the PID of an already existing PIN!" << std::endl;
						assert(false);
					}
					else
					{
						#if HOLD_RELATIONSHIPS_BY_PID
							lGlobalBucket.push_back(lPID);
						#else
							lGlobalBucket.push_back(lSession->getPIN(lPID));
						#endif
						lGlobalBucketS.insert(LOCALPID(lPID));
					}
				}

				#if HOLD_RELATIONSHIPS_BY_PID
					// Transfer tmp relationships (*-based) to committed ones (PID-based).
					std::vector<Relationship>::iterator lIterUR;
					for (lIterUR = lTmpRelationships.begin(); lIterUR != lTmpRelationships.end(); lIterUR++)
					{
						Relationship const & lUR = *lIterUR;
						CommittedRelationship lCR;
						PID lPID; unsigned nP = 1;
						TVERIFYRC(lBatch->getPIDs(&lPID, nP, lUR.mFromIdx));
						TVERIFY(1 == nP);
						lCR.mFrom = lPID;
						lCR.mTo = lUR.mToPtr ? lUR.mToPtr->getPID() : lUR.mTo;
						lCR.mPropID = lUR.mPropID;
						lRelationships.push_back(lCR);
					}
				#endif
			}
		}

		// Verify that all relationships established during the previous pass are still there.
		mLogger.out() << "Testing the " << (unsigned int)lRelationships.size() << " relationships." << std::endl;
		#if HOLD_RELATIONSHIPS_BY_PID
			std::vector<CommittedRelationship>::iterator lIter;
			for (lIter = lRelationships.begin(); lRelationships.end() != lIter; lIter++)
			{
				#if 0
					mLogger.out() << "*";
				#endif
				CommittedRelationship const & lR = (*lIter);
				IPIN * const lPIN = lSession->getPIN(lR.mFrom);
				Value const * const lV = lPIN->getValue(lR.mPropID);
				PID const lPIDTo = lR.mTo;
				assert(LOCALPID(lPIDTo) != STORE_INVALID_PID);
				if (!isExpectedRelationship(*lV, lPIDTo))
				{
					lSuccess = false;
					mLogger.out() << "Did not find expected relationship!" << std::endl;
				}
				lPIN->destroy();
			}
		#else
			std::vector<Relationship>::iterator lIter;
			for (lIter = lRelationships.begin(); lRelationships.end() != lIter; lIter++)
			{
				#if 0
					mLogger.out() << "*";
				#endif
				Relationship const & lR = (*lIter);
				Value const * const lV = lR.mFromPtr->getValue(lR.mPropID);
				PID const lPIDTo = lR.mToPtr->getPID();
				assert(LOCALPID(lPIDTo) != STORE_INVALID_PID);
				if (!isExpectedRelationship(lV->val, lPIDTo))
				{
					lSuccess = false;
					mLogger.out() << "Did not find expected relationship!" << std::endl;
				}

				#if 1
					lR.mFromPtr->refresh();
					Value const * const lV2 = lR.mFromPtr->getValue(lR.mPropID);
					if (!isExpectedRelationship(lV2->val, lPIDTo))
					{
						lSuccess = false;
						mLogger.out() << "Did not find expected relationship in reloaded pin!" << std::endl;
					}
				#endif
			}
		#endif

		lSession->terminate();
		MVTApp::stopStore();
	}
	
	else { TVERIFY(!"Unable to start store"); }
	return lSuccess ? 0 : 1;
}

void TestBatch::createBatchPIN(IBatch & pBatch, PropertyID const * pPropIDs, int pNumProps)
{
	// Setup a few bogus properties.
	// Note: Avoid FT-indexing slowdown by using VT_BSTR...
	Value * const lNewVs = pBatch.createValues(pNumProps);
	int k;
	for (k = 0; k < pNumProps; k++)
	{
		Tstring tstr;
		MVTRand::getString(tstr, 100, 0, true);
		size_t l=tstr.length();
		unsigned char *str=(unsigned char*)pBatch.malloc(l+1);
		memcpy (str,tstr.c_str(),l); str[l]=0;
		lNewVs[k].set(str, (uint32_t)l); lNewVs[k].setPropID(pPropIDs[k]); lNewVs[k].setOp(OP_ADD);
	}

	// Create the in-memory PIN.
	TVERIFYRC(pBatch.createPIN(lNewVs, pNumProps));
}


