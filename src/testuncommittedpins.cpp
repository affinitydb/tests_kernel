/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

// Publish this test.
class TestUncommittedPins : public ITest
{
	public:
		TEST_DECLARE(TestUncommittedPins);
		virtual char const * getName() const { return "testuncommittedpins"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "bashes on uncommitted pins"; }
		virtual bool includeInPerfTest() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestUncommittedPins, TestLogger::kDStdOut);

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

struct Relationship { IPIN * mFromPtr, * mToPtr; PID mTo; PropertyID mPropID; };
struct CommittedRelationship { PID mFrom, mTo; PropertyID mPropID; };

int TestUncommittedPins::execute()
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		ISession * const lSession =	MVTApp::startSession();
		static const int sNumProps = 3;
		PropertyID lPropIDs[sNumProps];// = {1000, 1001, 1002};
		MVTApp::mapURIs(lSession,"TestUncommittedPins.prop",sNumProps, lPropIDs);
		int const lNumProps = sizeof(lPropIDs) / sizeof(lPropIDs[0]);

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
		int i, j, k, lCloudSize;
		for (i = 0, lCloudSize = 10; i < 50; i++, lCloudSize += 10)
		{
			int lNewRels = 0;
			std::vector<IPIN *> lLocalBucket;
			#if HOLD_RELATIONSHIPS_BY_PID
				std::vector<Relationship> lTmpRelationships;
			#endif

			#define TEST_FORWARD_REFERENCES 1
			#if TEST_FORWARD_REFERENCES
			for (j = 0; j < lCloudSize; j++)
			{
				IPIN * const lPIN = lSession->createUncommittedPIN();
				lLocalBucket.push_back(lPIN);
			}
			#endif

			mLogger.out() << "Creating cloud of " << lCloudSize << " uncommitted pins." << std::endl;
			for (j = 0; j < lCloudSize; j++)
			{
				// Create an uncommitted pin.
				#if TEST_FORWARD_REFERENCES
					IPIN * const lPIN = lLocalBucket[j];
				#else
					IPIN * const lPIN = lSession->createUncommittedPIN();
				#endif

				// Set a few bogus properties on it.
				for (k = 0; k < lNumProps; k++)
				{
					Value lV;
					Tstring lS;
					MVTRand::getString(lS, 100, 0, true);
					// Note: Avoid FT-indexing slowdown by using VT_BSTR...
					lV.set((unsigned char *)lS.c_str(), (uint32_t)lS.length()); lV.setPropID(lPropIDs[k]); lV.setOp(OP_ADD);
					if (RC_OK != lPIN->modify(&lV, 1))
					{
						lSuccess = false;
						assert(false);
					}

					Value const * const lVCheck = lPIN->getValue(lPropIDs[k]);
					if (!lVCheck || lVCheck->type != VT_BSTR || lVCheck->length != lS.length())
					{
						lSuccess = false;
						assert(false);
					}
				}

				// Randomly assign references to some other uncommitted/committed pins.
				int const lIndexPropRef = (int)(lNumProps * rand() / RAND_MAX);
				bool const lUncommittedRef = (100.0 * rand() / RAND_MAX) > 33.0;
				if (lIndexPropRef < lNumProps &&
					((lUncommittedRef && !lLocalBucket.empty()) ||
					(!lUncommittedRef && !lGlobalBucket.empty())))
				{
					int const lMaxElms = (int)(lUncommittedRef ? lLocalBucket.size() : lGlobalBucket.size());
					int const lIndexRef = (int)((double)lMaxElms * rand() / RAND_MAX);
					int const lNumElms = min(lMaxElms, 5);
					int m;
					for (m = 0; m < lNumElms && (lIndexRef + m < lMaxElms); m++)
					{
						Value lV; IPIN *lTo=NULL; PID lToPID;
						INITLOCALPID(lToPID); lToPID.pid = STORE_INVALID_PID;
						#if HOLD_RELATIONSHIPS_BY_PID
							if (lUncommittedRef) {
								lTo = lLocalBucket[lIndexRef + m];
								SETVALUE_C(lV, lPropIDs[lIndexPropRef], lTo, OP_ADD, STORE_LAST_ELEMENT);
							} else {
								lToPID = lGlobalBucket[lIndexRef + m];
								SETVALUE_C(lV, lPropIDs[lIndexPropRef], lToPID, OP_ADD, STORE_LAST_ELEMENT);
							}
						#else
							IPIN * const lTo = lUncommittedRef ? lLocalBucket[lIndexRef + m] : lGlobalBucket[lIndexRef + m];
							SETVALUE_C(lV, lPropIDs[lIndexPropRef], lTo, OP_ADD, STORE_LAST_ELEMENT);
						#endif
						if (RC_OK != lPIN->modify(&lV, 1))
						{
							lSuccess = false;
							assert(false);
						}
						else
						{
							#if 0
								mLogger.out() << "*";
							#endif

							#if HOLD_RELATIONSHIPS_BY_PID
								Relationship lR;
								lR.mFromPtr = lPIN;
								if (lUncommittedRef)
								{
									assert(lTo!=NULL);
									lR.mToPtr = lTo;
									INITLOCALPID(lR.mTo);
									LOCALPID(lR.mTo) = STORE_INVALID_PID;
								}
								else
								{
									lR.mToPtr = NULL;
									lR.mTo = lToPID;
								}
								lR.mPropID = lPropIDs[lIndexPropRef];
								lTmpRelationships.push_back(lR);
							#else
								assert(lTo!=NULL);
								Relationship lR;
								lR.mFromPtr = lPIN;
								lR.mToPtr = lTo;
								lR.mPropID = lPropIDs[lIndexPropRef];
								lRelationships.push_back(lR);
							#endif
							lNewRels++;
						}
					}
				}

				// Maintenance of new pin.
				#if !TEST_FORWARD_REFERENCES
					lLocalBucket.push_back(lPIN);
				#endif
			}
			mLogger.out() << " (" << lNewRels << " new relationships)" << std::endl;

			// Commit the new cloud of pins.
			assert((unsigned)lCloudSize == lLocalBucket.size());
			mLogger.out() << "  Committing the cloud." << std::endl;
			if (RC_OK != lSession->commitPINs(&lLocalBucket[0], (unsigned)lLocalBucket.size()))
			{
				lSuccess = false;
				mLogger.out() << "commitPINs failed!" << std::endl;
				assert(false);
			}
			else
			{
				// Check that all were committed properly.
				std::vector<IPIN *>::iterator lIter;
				for (lIter = lLocalBucket.begin(); lIter != lLocalBucket.end(); lIter++)
				{
					IPIN * const lPIN = *lIter;
					PID const lPID = lPIN->getPID();
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
							lGlobalBucket.push_back(lPIN);
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
						lCR.mFrom = lUR.mFromPtr->getPID();
						lCR.mTo = lUR.mToPtr ? lUR.mToPtr->getPID() : lUR.mTo;
						lCR.mPropID = lUR.mPropID;
						lRelationships.push_back(lCR);
					}

					// Cleanup.
					for (lIter = lLocalBucket.begin(); lIter != lLocalBucket.end(); lIter++)
						(*lIter)->destroy();
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
