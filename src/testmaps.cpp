/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

// WARNING: This test adds 3000 properties with random names to the store

// Publish this test.
class TestMaps : public ITest
{
	public:
		TEST_DECLARE(TestMaps);
		virtual char const * getName() const { return "testmaps"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "bashes on mvstore's property maps"; }
		virtual bool includeInPerfTest() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestMaps, TestLogger::kDStdOut);

// Implement this test.
class PITPropertyMap
{
	public:
		Tstring mURI;
		PropertyID mPropID;
		PITPropertyMap(PropertyID pPropID = 0) : mPropID(pPropID) {}
		PITPropertyMap(URIMap const & pFrom) : mURI(pFrom.URI), mPropID(pFrom.uid) {}
		bool operator <(PITPropertyMap const & pO) const { return mPropID < pO.mPropID; }
};

int TestMaps::execute()
{
	// Open the store.
	bool lSuccess = false;
	if (MVTApp::startStore())
	{
		lSuccess = true;
		// Fill the store's map and an in-memory clone.
		TestLogger lOutV(TestLogger::kDStdOutVerbose);
		lOutV.out() << "{" << getName() << "} Filling the maps..." << std::endl;
		ISession * const lSession =	MVTApp::startSession();
		typedef std::set<PITPropertyMap> TPropertyMap;
		std::vector<PropertyID> lPropIDs;
		TPropertyMap lMap;
		int i, j;
		for (i = 0; i < 3000; i++)
		{
			lOutV.out() << ".";
			Tstring lS2[5];
			URIMap lData[5];
			for (j = 0; j < 5; j++)
			{
				Tstring lS;
				MVTRand::getString(lS2[j], 10, 256, false);
				lData[j].URI = lS2[j].c_str();
				lData[j].uid = STORE_INVALID_URIID;
			}
			lSession->mapURIs(5, lData);
			for (j = 0; j < 5; j++)
			{
				lMap.insert(TPropertyMap::value_type(PITPropertyMap(lData[j])));
				lPropIDs.push_back(lData[j].uid);
			}
		}

		// Compare.
		lOutV.out() << "{" << getName() << "} Comparing..." << std::endl;
		for (i = 0; i < 10000 && lSuccess; i++)
		{
			j = rand() * (int)(lPropIDs.size()-1) / RAND_MAX;
			char lS[1024];
			TPropertyMap::iterator p = lMap.find(PITPropertyMap(lPropIDs[j]));
			if (p == lMap.end())
			{
				assert(false);
				continue;
			}
			PropertyID const lPropID = lPropIDs[j];
			size_t lPropURI = 1024;
			lSession->getURI(lPropID, lS, lPropURI);
			lSuccess = (0 == strcmp(lS, (*p).mURI.c_str()));
			if (!lSuccess)
			{
				mLogger.out() << "Failed at iteration " << i << " for PropertyID=" << lPropID;
			}
		}

		lSession->terminate();
		MVTApp::stopStore();
	}

	return lSuccess ? 0 : 1;
}
