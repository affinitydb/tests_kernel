/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

#define	DEFAULT_FILENAME    "protobuf.out"
#define	DEFAULT_BUFSIZE      4096

class TestReadProto : public ITest
{
    public:
        TEST_DECLARE(TestReadProto);
        virtual char const * getName() const { return "testreadproto"; }
        virtual char const * getHelp() const { return "args(optional): --fname={name of input file}, --inbufsize={streaming buffer size}"; }
        virtual char const * getDescription() const { return "Basic test of Google Protocol Buffers streaming-in interface"; }
        virtual int execute();
        virtual void destroy() { delete this; }
    private:
        ISession * mSession ;
        void runCase1();
        void runCase2();
};
typedef std::vector<unsigned char> TStreamBuf;

TEST_IMPLEMENT(TestReadProto, TestLogger::kDStdOut);

class ResultStream : public IStreamIn
{
    public:
        ResultStream() : mIncoming(NULL) {}
        virtual ~ResultStream() { if (mIncoming) { mIncoming->destroy(); mIncoming = NULL; } }
        virtual RC next(const unsigned char *buf,size_t lBuf)
        {
            for (size_t i = 0; i < lBuf; i++)
                mResult.push_back(buf[i]);
            return RC_OK;
        }
        virtual void destroy() {}
    public:
        void getResult(TStreamBuf & pResult) { pResult = mResult; mResult.clear(); }
        void setIncoming(IStreamIn * pIncoming) { mIncoming = pIncoming; }
        IStreamIn * getIncoming() const { return mIncoming; }
    protected:
        TStreamBuf mResult;
        IStreamIn * mIncoming;
};

int TestReadProto::execute()
{
    runCase1();
    runCase2();
    return 0;
}

void TestReadProto::runCase1()
{
    // 1) Generate the input file: protobuf.out: calling testprotobuf to generate.
    mLogger.out() << endl << endl << "Beginning generate protobuf.out file." << endl << endl ;

    mLogger.out() << "Generating..." << std::endl;

    string cmd =string("testprotobuf ");

    int lResult = MVTUtil::executeProcess(MVTApp::mAppName.c_str(),cmd.c_str(),NULL,NULL,false,true );
    if (0 != lResult)
    {
        TVERIFY(!"Generation phase failed");
        return;
    }

    // 2) Begin to test read protobuf file.

    string fileName(DEFAULT_FILENAME);

    mpArgs->get_param("fname",fileName);

    size_t bufSize=DEFAULT_BUFSIZE;

    mpArgs->get_param("inbufsize",bufSize);
    
    bool bStarted = MVTApp::startStore() ;
    if ( !bStarted ) { TVERIFY2(0,"Could not start store") ; return; }

    mSession = MVTApp::startSession();
    TVERIFY( mSession != NULL );
    unsigned char *buf=(unsigned char*)mSession->malloc(bufSize);
    TVERIFY(buf!=NULL);
    ResultStream lOutput;
    if (buf!=NULL) {
        IStreamIn *istr=NULL;
        TVERIFYRC(mSession->createInputStream(istr, &lOutput, 1024));
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
        TStreamBuf result;
        lOutput.getResult(result);
        TVERIFY(result.size() > 0);
        mSession->free(buf);
    }
    mSession->terminate(); // No return code to test
    MVTApp::stopStore();  // No return code to test
}

void TestReadProto::runCase2()
{
    unsigned char buf[0x1000];
    size_t read = 0x1000;
    RC rc = RC_OK;
    TStreamBuf result;
    
    bool bStarted = MVTApp::startStore() ;
    if ( !bStarted ) { TVERIFY2(0,"Could not start store") ; return; }

    mSession = MVTApp::startSession();
    TVERIFY( mSession != NULL );
    TVERIFYRC(MVTApp::execStmt(mSession, "insert testreadproto_c2_age=18;"));
    IStmt * const stmt = mSession->createStmt("SELECT * WHERE EXISTS(testreadproto_c2_age)", NULL, 0, 0);
    TVERIFY(stmt!=NULL);

    IStreamOut *out = NULL;
    TVERIFYRC(stmt->execute(out, NULL, 0, ~0u, 0));
    while (RC_OK == (rc = out->next(buf, read))){
        for (size_t i = 0; i < read; i++)
            result.push_back(buf[i]);
        read = 0x1000;
    }
    TVERIFY (RC_OK == rc || RC_EOF == rc);
    TVERIFY(result.size() > 0);
    out->destroy();
    stmt->destroy();
    
    mSession->terminate();
    MVTApp::stopStore(); 
}
