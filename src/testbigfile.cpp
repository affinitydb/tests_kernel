/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

// Publish this test.
class TestBigFile : public ITest
{
	public:
		RC mRCFinal;
		TEST_DECLARE(TestBigFile);
		virtual char const * getName() const { return "testbigfile"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of big (>-4GB) data files"; }
		virtual bool isLongRunningTest()const {return true;}
		virtual bool includeInLongRunningSmoke(char const *& pReason) const { pReason = "Big disk space consumption..."; return false; }
		virtual bool includeInBashTest(char const *& pReason) const { pReason = "Big disk space consumption..."; return false; }
		virtual bool excludeInIPCSmokeTest(char const *& pReason) const { pReason = "Huge stream added. Not required!"; return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestBigFile, TestLogger::kDStdOut);

// Implement this test.
#define	HUGE_STREAM_SIZE	10000000000LL

class HugeStream : public AfyDB::IStream
{
	protected:
		uint64_t mLength;
	public:
		HugeStream() : mLength(0) {}
		virtual ValueType dataType() const { return VT_BSTR; }
		virtual	uint64_t length() const { return HUGE_STREAM_SIZE; }
		virtual size_t read(void * buf, size_t maxLength) {uint64_t lLength = min(uint64_t(HUGE_STREAM_SIZE - mLength),uint64_t(maxLength)); for (uint64_t i=0; i<lLength; i++) ((char *)buf)[i]=(char)i; mLength+=lLength; return (unsigned long)lLength;}
		virtual size_t readChunk(uint64_t pSeek, void * buf, size_t maxLength) {return 0;}
		virtual	IStream * clone() const { return new HugeStream(); }
		virtual	RC reset() {mLength=0; return RC_OK;}
		virtual void destroy() {delete this;}
};

int TestBigFile::execute()
{
	mRCFinal = RC_FALSE;
	if (MVTApp::startStore())
	{
		mRCFinal=RC_OK;
		ISession * const session = MVTApp::startSession();
		HugeStream hs; PID id;		
		PropertyID lPropIDs[1];	
		MVTApp::mapURIs(session,"TestBigFile.execute",1,lPropIDs);
		Value v; v.set((AfyDB::IStream*)&hs); v.setPropID(lPropIDs[0]);
		session->createPIN(id,&v,1,MODE_COPY_VALUES);
		session->terminate();
		MVTApp::stopStore();
	}

 	return mRCFinal;
}
