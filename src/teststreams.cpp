/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h" // MyStream
//#include <commons/mvcore/sync.h>

// Publish this test.
class TestStreams : public ITest
{
	public:
		TEST_DECLARE(TestStreams);
		virtual char const * getName() const { return "teststreams"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "bashes on streams (blobs) - to be completed"; }
		virtual bool includeInPerfTest() const { return false; /* would be nice, but fails in release! must investigate... */ }
		
		virtual int execute();
		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestStreams, TestLogger::kDStdOut);

static inline bool testResultingStreams(TestLogger & pLogger, Value const * pVal, unsigned long pLen, char pStartChar, ValueType pVT, int pCollectionSize)
{
	bool lSuccess = true;
	size_t i;
	if (!pVal)
	{
		lSuccess = false;
		pLogger.out() << "Error: Could not retrieve property!" << std::endl;
	}
	else if (pVal->type == VT_COLLECTION)
	{
		if (!pVal->isNav()) {
			if (uint32_t(pCollectionSize) != pVal->length)
			{
				lSuccess = false;
				pLogger.out() << "Error: Found " << pVal->length << " elements in a collection where " << pCollectionSize << " were expected!" << std::endl;
			}
			pLogger.out() << "eids: ";
			for (i = 0; i < pVal->length; i++)
			{
				pLogger.out() << std::hex << pVal->varray[i].eid << " ";
				if (!MyStream::checkStream(pLogger, pVal->varray[i], pLen, pStartChar, pVT))
					lSuccess = false;
			}
			pLogger.out() << std::endl;
		}
		else
		{
			i = 0;
			pLogger.out() << "eids: ";
			Value const * lNext = pVal->nav->navigate(GO_FIRST);
			while (lNext)
			{
				i++;
				pLogger.out() << std::hex << lNext->eid << " ";
				if (!MyStream::checkStream(pLogger, *lNext, pLen, pStartChar, pVT))
					lSuccess = false;
				lNext = pVal->nav->navigate(GO_NEXT);
			}
			pLogger.out() << std::endl;
			if (size_t(pCollectionSize) != i)
			{
				lSuccess = false;
				pLogger.out() << "Error: Found " << (unsigned int)i << " elements in a collection where " << pCollectionSize << " were expected!" << std::endl;
			}
		}
	}
	else if (pVal->type == VT_STREAM || pVal->type == pVT)
	{
		if (pCollectionSize != 1)
		{
			lSuccess = false;
			pLogger.out() << "Error: Found stream but expected collection!" << std::endl;
		}
		if (!MyStream::checkStream(pLogger, *pVal, pLen, pStartChar, pVT))
			lSuccess = false;
	}
	else
	{
		lSuccess = false;
		pLogger.out() << "Error: Unexpected property type!" << std::endl;
	}
	return lSuccess;
}

#define TESTSTREAMS_BASHQUERY 0
#if TESTSTREAMS_BASHQUERY
	// Attempt to repro an issue in the app with IPC - no success yet
	// (even running the test multiple times concurrently).
	THREAD_SIGNATURE threadTestStreamsBashQuery(void * pStop)
	{
		ISession * const lSession =	MVTApp::startSession();

		// Define a class of all pins created by this test.
		DataEventID lClsid;
		if (RC_OK != lSession->getDataEventID("teststreams.bashquery", lClsid))
		{
			IStmt * const lQClass = lSession->createStmt();
			unsigned char const lVarClass = lQClass->addVariable();
			PropertyID const lPropIDsClass[] = {1000};
			lQClass->setPropCondition(lVarClass, lPropIDsClass, 1);
			defineClass(lSession,"teststreams.bashquery", lQClass, &lClsid);
			lQClass->destroy();
		}

		// Define a query for this class.
		IStmt * const lQ = lSession->createStmt();
		SourceSpec lCS;
		lCS.objectID = lClsid;
		lCS.nParams = 0;
		lCS.params = NULL;
		lQ->addVariable(&lCS, 1);

		// Periodically query the pins, while the test creates them.
		int j;
		long volatile & lStop = *(long volatile *)pStop;
		for (j = 0; j < 5000 && 0 == lStop; j++)
		{
			ICursor * lR = lQ->execute();
			IPIN * lP;
			for (lP = lR ? lR->next() : NULL; lP; lP = lR->next()) { lP->destroy(); }
			lR->destroy();

			Sleep(100);
		}
		lQ->destroy();
		lSession->terminate();
		return 0;
	}
#endif

class TestStreamsReplCheck : public IStoreNotification
{
	protected:
		ITest * mTest;
		Afy::IStream * mCurInputStream;
	public:
		TestStreamsReplCheck(ITest * pTest) : mTest(pTest), mCurInputStream(NULL) {}
		void setCurInputStream(Afy::IStream * pStream) { mCurInputStream = pStream; }
		virtual	void notify(NotificationEvent *events,unsigned nEvents,uint64_t txid) {}
		virtual	void replicationNotify(NotificationEvent *events,unsigned nEvents,uint64_t txid)
		{
			if (!mCurInputStream)
				return;
			unsigned i, j;
			for (i = 0; i < nEvents; i++)
			{
				for (j = 0; j < events[i].nData; j++)
				{
					if (!events[i].data[j].newValue)
						continue;
					if (VT_STREAM != events[i].data[j].newValue->type)
						continue;
					if (events[i].data[j].newValue->stream.is == mCurInputStream)
					{
						std::cout << "DANGER: Client stream passed directly to replication notification!" << std::endl;
						TV_R(!"DANGER: Client stream passed directly to replication notification!", mTest);
					}
				}
			}
		}
		virtual	void txNotify(TxEventType,uint64_t txid) {}
};

int TestStreams::execute()
{
	bool lSuccess = true;
	TestStreamsReplCheck lReplCallback(this);
 	if (MVTApp::startStore(NULL, &lReplCallback))
	{
		ISession * const lSession =	MVTApp::startSession();
		static const int sNumProps = 2;
		PropertyID lPropIDs[sNumProps];
		int k = 0;
		for(k = 0; k < sNumProps; k ++)
		{
			char lB[64];
			sprintf(lB,"TestStreams.prop%d",k);
			MVTApp::mapStaticProperty(lSession,lB,lPropIDs[k]);
		}
		//PropertyID const lPropIDs[] = {1000, 1001};
		PID lPID, lPID2;
		Value lV;

		// Basic test.
		#if 1
		{
			CREATEPIN(lSession, &lPID, NULL, 0);
			IPIN * const lPIN = lSession->getPIN(lPID);
			unsigned long lStreamLengths[] = {150, 6194304};

			mLogger.out() << "Creating short stream" << std::endl;
			MyStream* mystream1 = new MyStream(lStreamLengths[0]);
			SETVALUE(lV, lPropIDs[0], MVTApp::wrapClientStream(lSession, mystream1), OP_ADD);
			{
				lReplCallback.setCurInputStream(lV.stream.is);
				if (RC_OK != lPIN->modify(&lV, 1))
				{
					lSuccess = false;
					assert(false);
				}
				lReplCallback.setCurInputStream(NULL);
			}
			
			lV.stream.is->reset();
			mLogger.out() << "Creating pin from short stream" << std::endl;
			{
				lReplCallback.setCurInputStream(lV.stream.is);
				CREATEPIN(lSession, &lPID2, &lV, 1);
				lReplCallback.setCurInputStream(NULL);
			}

			mLogger.out() << "Creating long stream" << std::endl;
			MyStream* mystream2 = new MyStream(lStreamLengths[1]);
			SETVALUE(lV, lPropIDs[1], MVTApp::wrapClientStream(lSession, mystream2), OP_ADD);
			{
				lReplCallback.setCurInputStream(lV.stream.is);
				if (RC_OK != lPIN->modify(&lV, 1))
				{
					lSuccess = false;
					assert(false);
				}
				lReplCallback.setCurInputStream(NULL);
			}
			
			int p;
			for (p = 0; p < 2; p++)
			{
				Value const * lVal = lPIN->getValue(lPropIDs[p]);
				if (!lVal)
				{
					lSuccess = false;
					mLogger.out() << "Error: Could not retrieve property " << lPropIDs[p] << "!" << std::endl;
				}
				else if (!MyStream::checkStream(mLogger, *lVal, lStreamLengths[p]))
					lSuccess = false;
			}
			delete mystream1;
			delete mystream2;
			lPIN->destroy();
		}
		#endif

		// Trying to repro 757.
#if 0
		if (!MVTApp::isRunningSmokeTest())
		{
			unsigned long lStreamLengths1[] = {971520, 20971520};
			mLogger.out()<<"Trying to repro bug 757"<<std::endl;
			SETVALUE(lV, lPropIDs[1], MVTApp::wrapClientStream(lSession, new MyStream(lStreamLengths1[1])), OP_SET);
			{
				lReplCallback.setCurInputStream(lV.stream.is);
				CREATEPIN(lSession, lPID2, &lV, 1);
				lReplCallback.setCurInputStream(NULL);
			}
			SETVALUE(lV, lPropIDs[1], MVTApp::wrapClientStream(lSession, new MyStream(lStreamLengths1[0])), OP_SET);
			{
				lReplCallback.setCurInputStream(lV.stream.is);
				CREATEPIN(lSession, lPID2, &lV, 1);
				lReplCallback.setCurInputStream(NULL);
			}
			#ifdef WIN32
				::DebugBreak();
			#endif
		}

		// Log file issue.
		if (!MVTApp::isRunningSmokeTest())
		{
			int x;
			IPIN *mp3pin;
			Value val[3];
			PropertyID lPropIDs[3];
			MVTApp::mapURIs(lSession,"TestStreams.prop",3,lPropIDs);
			for (x =0; x<20; x++)
			{
				mLogger.out()<<"Creating streams for Ajay's Scneario"<<std::endl;
				SETVALUE(lV, lPropIDs[1], MVTApp::wrapClientStream(lSession, new MyStream(lStreamLengths[1])), OP_SET);
				{
					lReplCallback.setCurInputStream(lV.stream.is);
					CREATEPIN(lSession, lPID2, &lV, 1);
					lReplCallback.setCurInputStream(NULL);
				}
				mp3pin = lSession->getPIN(lPID2);
				val[0].set("Test prop1");val[0].setPropID(lPropIDs[0]);
				val[1].set("Test prop2");val[0].setPropID(lPropIDs[1]);
				val[2].set("Test prop3");val[0].setPropID(lPropIDs[2]);
				mp3pin->modify(val,3);
				mp3pin->destroy();
			}
		}
#endif
		//returning RC_CORRUPTED
		if (!MVTApp::isRunningSmokeTest())
		{
			unsigned long lStreamLengths1[] = {71520, 971520};
			MyStream* mystream1 = new MyStream(lStreamLengths1[0]);  
			SETVALUE(lV, lPropIDs[1], MVTApp::wrapClientStream(lSession, mystream1), OP_SET);
			{
				lReplCallback.setCurInputStream(lV.stream.is);
				CREATEPIN(lSession, &lPID2, &lV, 1);
				lReplCallback.setCurInputStream(NULL);
			}
                      
			IPIN *pin = lSession->getPIN(lPID2);
			MyStream* mystream2 = new MyStream(lStreamLengths1[1]); 
			SETVALUE(lV, lPropIDs[1], MVTApp::wrapClientStream(lSession, mystream2), OP_SET);
			{
				lReplCallback.setCurInputStream(lV.stream.is);
				TVERIFYRC(pin->modify(&lV,1));
				lReplCallback.setCurInputStream(NULL);
			}
			delete mystream1;
			delete mystream2;
			pin->destroy();
			
		}
		// More aggressive test.
		#if 1
			#if TESTSTREAMS_BASHQUERY
				long volatile lStop = 0;
				HTHREAD lThreads[2];
				createThread(&threadTestStreamsBashQuery, (void *)&lStop, lThreads[0]);
				createThread(&threadTestStreamsBashQuery, (void *)&lStop, lThreads[1]);
				static int const sNumTests = 500;
				static int const sMinSize = 5000;
				static double const sMaxSize = 5000.0;
			#else
				static int const sNumTests = 20;
				static int const sMinSize = 1;
				static double const sMaxSize = 200000.0;
			#endif

			PID lPIDs[sNumTests];
			unsigned long lLens[sNumTests];
			char lStartChars[sNumTests];
			ValueType lVTs[sNumTests];
			int lCollectionSizes[sNumTests];
			int i, j;
			mLogger.out() << "Creating lots of streams..." << std::endl;
			for (i = 0; i < sNumTests; i++)
			{
				mLogger.out() << ".";
				lLens[i] = sMinSize + (int)(sMaxSize * rand() / RAND_MAX);
				lStartChars[i] = (char)(100.0 * rand() / RAND_MAX);
				lVTs[i] = ((100.0 * rand() / RAND_MAX) > 50.0) ? VT_STRING : VT_BSTR;
				#if 0
					lCollectionSizes[i] = 1;
				#else
					lCollectionSizes[i] = 1 + (int)(5.0 * rand() / RAND_MAX);
				#endif

				Value lV;
				MyStream* mystream1 = new MyStream(lLens[i], lStartChars[i], lVTs[i]); 
				SETVALUE(lV, lPropIDs[0], MVTApp::wrapClientStream(lSession, mystream1), OP_ADD);
				{
					lReplCallback.setCurInputStream(lV.stream.is);
					CREATEPIN(lSession, &lPIDs[i], &lV, 1);
					lReplCallback.setCurInputStream(NULL);
				}

				IPIN * const lPIN = lSession->getPIN(lPIDs[i]);
				for (j = 1; j < lCollectionSizes[i]; j++)
				{
					MyStream* mystream2 = new MyStream(lLens[i], lStartChars[i], lVTs[i]);
					SETVALUE_C(lV, lPropIDs[0], MVTApp::wrapClientStream(lSession, mystream2), OP_ADD, STORE_LAST_ELEMENT);
					{
						lReplCallback.setCurInputStream(lV.stream.is);
						if (RC_OK != lPIN->modify(&lV, 1))
						{
							lSuccess = false;
							mLogger.out() << "Error: Could not add stream to collection!" << std::endl;
						}
						lReplCallback.setCurInputStream(NULL);
					}
					delete mystream2;
				}

				Value const * lVal = lPIN->getValue(lPropIDs[0]);
				if (!testResultingStreams(mLogger, lVal, lLens[i], lStartChars[i], lVTs[i], lCollectionSizes[i]))
					lSuccess = false;
				delete mystream1;
				lPIN->destroy();
			}
			mLogger.out() << std::endl << "Retesting them..." << std::endl;
			for (i = 0; i < sNumTests; i++)
			{
				mLogger.out() << ".";
				IPIN * const lPIN = lSession->getPIN(lPIDs[i]);
				Value const * lVal = lPIN->getValue(lPropIDs[0]);
				if (!testResultingStreams(mLogger, lVal, lLens[i], lStartChars[i], lVTs[i], lCollectionSizes[i]))
					lSuccess = false;
				lPIN->destroy();
			}
		#endif

		#if TESTSTREAMS_BASHQUERY
			lStop = 1;
			MVTestsPortability::threadsWaitFor(2, lThreads);
		#endif
		lSession->terminate();
		MVTApp::stopStore();
	}

	else { TVERIFY(!"Unable to start store"); }
	return lSuccess ? 0 : 1;
}
