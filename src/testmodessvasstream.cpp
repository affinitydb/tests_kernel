/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h"
using namespace std;

class TestModeSSVAsStream : public ITest
{
		static const int sNumProps = 10;
		PropertyID mPropIds[sNumProps];
		unsigned int mode;
	public:
		TEST_DECLARE(TestModeSSVAsStream);
		virtual char const * getName() const { return "testmodessvasstream"; }
		virtual char const * getHelp() const { return ""; }
		
		virtual char const * getDescription() const { return "test for MODE_SSV_AS_STREAM / MODE_FORCED_SSV_AS_STREAM"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual void destroy() { delete this; }		
		virtual int execute();
	private:
		void testMODEFORCEDSSV(ISession *session);
		void testMODESSV(ISession *session);
		template <class T> RC testStreamInt(PID pid,basic_string<T> &str,PropertyID &mPropId,ISession *session);
};
TEST_IMPLEMENT(TestModeSSVAsStream, TestLogger::kDStdOut);

int TestModeSSVAsStream::execute()
{	
	bool lSuccess = false;	
	if (MVTApp::startStore())
	{	
		lSuccess=true;
		ISession *session = MVTApp::startSession();
		MVTApp::mapURIs(session,"testModeSSVAsStream",sNumProps,mPropIds);
		mode = session->getInterfaceMode();
		mode |= MODE_FORCED_SSV_AS_STREAM;
		testMODEFORCEDSSV(session);
		mode=0;
		mode |= MODE_SSV_AS_STREAM;
		testMODEFORCEDSSV(session);
		session->terminate();
		MVTApp::stopStore();
	}
	return lSuccess?0:1;
}

void TestModeSSVAsStream::testMODEFORCEDSSV(ISession *session)
{
	Value val[5];Tstring str;
	//case 1: small VT_STRING
	IPIN *pin = session->createUncommittedPIN();
	MVTRand::getString(str,10,0,false);
	val[0].set(str.c_str());val[0].setPropID(mPropIds[0]);val[0].setMeta(META_PROP_SSTORAGE);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	PID pid = pin->getPID();
	pin->destroy();
	TVERIFYRC(testStreamInt(pid,str,mPropIds[0],session));


	//case 2: small VT_BSTR
	str="";
	const unsigned char bbstr[] = { 100,101,102,103,104,65,66,67,68,69,70,71,74,80,'\0' };
	for(int x =0; bbstr[x] != '\0'; x++)
		str.push_back(bbstr[x]);
	pin = session->createUncommittedPIN();
	val[0].set(bbstr,14);val[0].setPropID(mPropIds[0]);val[0].setMeta(META_PROP_SSTORAGE);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	pid = pin->getPID();
	pin->destroy();
	TVERIFYRC(testStreamInt(pid,str,mPropIds[0],session));


	//case 3:< pagesize streams (of all types)
	//unsigned int size = MVTApp::getPageSize();
	val[0].set("");val[0].setPropID(mPropIds[0]);val[0].setMeta(META_PROP_SSTORAGE);
	val[1].set("");val[1].setPropID(mPropIds[1]);val[1].setMeta(META_PROP_SSTORAGE);

	TVERIFYRC(session->createPIN(pid,val,2,MODE_COPY_VALUES));
	IStream *streamVTSTR = MVTApp::wrapClientStream(session,new TestStringStream(20000,VT_STRING));
	IStream *streamVTBSTR = MVTApp::wrapClientStream(session,new TestStringStream(20000,VT_BSTR));	

	char ch;Tstring chkVTSTR,chkVTBSTR;Wstring chkVTSUTR;
	for (int i =0; 0 != streamVTSTR->read(&ch,sizeof(ch)); i++)
		chkVTSTR.push_back(ch);
	streamVTSTR->reset();
	for (int i =0; 0 != streamVTBSTR->read(&ch,sizeof(ch)); i++)
		chkVTBSTR.push_back(ch);
	streamVTBSTR->reset();
	
	
	pin = session->getPIN(pid,mode); //no effect it seems with this mode here
	val[0].set(streamVTSTR);val[0].setPropID(mPropIds[0]);val[0].setMeta(META_PROP_SSTORAGE);
	val[1].set(streamVTBSTR);val[1].setPropID(mPropIds[1]);val[1].setMeta(META_PROP_SSTORAGE);
	TVERIFYRC(pin->modify(val,2));
	pin->destroy();
	streamVTSTR->destroy();
	streamVTBSTR->destroy();
	
	TVERIFYRC(testStreamInt(pid,chkVTSTR,mPropIds[0],session));
	TVERIFYRC(testStreamInt(pid,chkVTBSTR,mPropIds[1],session));

	//case 4: IStmt execute 
	IStmt *query = session->createStmt();
	unsigned char var = query->addVariable();
	Value args[2];
	args[0].setVarRef(0,mPropIds[0]);
	IExprTree *expr = session->expr(OP_EXISTS,1,args);
	query->addCondition(var,expr);
	ICursor *result = NULL;
	TVERIFYRC(query->execute(&result,0,0,~0,0,mode));
	for (IPIN *rpin=result->next(); rpin!=NULL; rpin=result->next() ){
		if (NULL != rpin){
			pid = rpin->getPID();
			for (uint32_t i=0; i < rpin->getNumberOfProperties(); i ++)
			{
				const Value *val;
				val = rpin->getValueByIndex(i);
				if (!(val) || VT_STREAM != val->type)
				{
					mLogger.out()<<"NULL Value or VT_STREAM not returned for pin "<<std::cout<<pid.pid
						<<"at value index "<<i<<std::endl;
					TVERIFY(false);
				}

			}
			rpin->destroy();
		}
	}
}
template <class T>
RC TestModeSSVAsStream::testStreamInt(PID pid,basic_string<T> &str,PropertyID &mPropId,ISession *session)
{

	IPIN *pin = session->getPIN(pid,mode);
	if (NULL == pin || pin->getNumberOfProperties() == 0)
	{
		mLogger.out()<<"No PIN returned by get pin or properties returned were zero!!."<<std::endl;
		return RC_FALSE;
	}
	const Value *val = pin->getValue(mPropId);
	if (!(val) || VT_STREAM != val->type)
	{
		mLogger.out()<<"NULL Value or VT_STREAM not returned for property id: "<<mPropId<<std::endl;
		pin->destroy();
		return RC_FALSE;
	}
	else
	{
		//currently the length of the stream is faling. So this check is disbaled.
		if (val->stream.is->length() != str.length() * sizeof(T))
		{
			mLogger.out()<<"Expected len: "<<(unsigned int) str.length()<<"got :"<<(unsigned int)val->stream.is->length()<<std::endl;
			pin->destroy();
			return RC_FALSE;
		}
		else 
		{
			if ((val->stream.is->dataType() == VT_STRING) || (val->stream.is->dataType() == VT_BSTR))
			{
				T ch;
				for (int i =0; 0 != val->stream.is->read(&ch,sizeof(T)); i++)
				{
					if(ch != str[i])
					{
						mLogger.out()<<"Structural difference in streams!!! Position " << i 
								<< " Expected " << str[i] << " got " << ch << std::endl ;
						return RC_FALSE;
					}
				}
			}
			else
			{
				mLogger.out()<<"Unsupported data type returned"<<std::endl;
			}
		}
	}
	return RC_OK;
}
