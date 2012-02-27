/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

#define	DEFAULT_FILENAME	"protobuf.out"
#define	DEFAULT_BUFSIZE		4096

class TestReadProto : public ITest
{
	public:
		TEST_DECLARE(TestReadProto);
		virtual char const * getName() const { return "testreadproto"; }
		virtual char const * getHelp() const { return "args(optional): --fname={name of input file}, --inbufsize={streaming buffer size}"; }
		virtual char const * getDescription() const { return "Basic test of Google Protocol Buffers streaming-in interface"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { return false; }
		virtual int execute();
		virtual void destroy() { delete this; }

	private:

		ISession * mSession ;
};

TEST_IMPLEMENT(TestReadProto, TestLogger::kDStdOut);

int TestReadProto::execute()
{
	// 1) Generate the input file: protobuf.out: calling testprotobuf to generate.

	mLogger.out() << endl << endl << "Beginning generate protobuf.out file." << endl << endl ;
	
	mLogger.out() << "Generating..." << std::endl;
	
	string cmd =string("testprotobuf ");
	
	int lResult = MVTUtil::executeProcess(MVTApp::mAppName.c_str(),cmd.c_str(),NULL,NULL,false,true );
	if (0 != lResult)
	{
		TVERIFY(!"Generation phase failed");
		return -1;
	}

	// 2) Begin to test read protobuf file.

	string fileName(DEFAULT_FILENAME);
	
	mpArgs->get_param("fname",fileName);

	size_t bufSize=DEFAULT_BUFSIZE;
	
	mpArgs->get_param("inbufsize",bufSize);

	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store") ; return -1; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL );

	unsigned char *buf=(unsigned char*)mSession->alloc(bufSize);
	TVERIFY(buf!=NULL);

	if (buf!=NULL) {
		IStreamIn *istr=NULL;
		TVERIFYRC(mSession->createInputStream(istr, NULL, 0));
		if (istr!=NULL) {
			FILE *pf=fopen(fileName.c_str(),"rb"); TVERIFY(pf!=NULL);
			if (pf!=NULL) {
				RC rc=RC_OK; size_t lRead=0;
				while ((lRead=fread(buf,1,bufSize,pf))!=0 && (rc=istr->next(buf,lRead))==RC_OK);
				TVERIFYRC(rc);
				TVERIFYRC(istr->next(NULL,0));
			}
			istr->destroy();
		}
		mSession->free(buf);
	}

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
	return 0;
}
