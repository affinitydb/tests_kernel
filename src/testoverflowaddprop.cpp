/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h"
#include "mvauto.h"

class TestOverflowAddProp:  public ITest
{
	static const int sNumProps = 100;
	PropertyID mPropIds[sNumProps];
	std::vector<PID> pids;
	public:
		TEST_DECLARE(TestOverflowAddProp);
		virtual char const * getName() const { return "testoverflowaddprop"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "repro for https://tech.vmware.com/bugzilla/show_bug.cgi?id=18760"; }
		virtual void destroy() { delete this; }
		virtual int execute();			
	private:
};
TEST_IMPLEMENT(TestOverflowAddProp, TestLogger::kDStdOut);

int TestOverflowAddProp::execute() 
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		ISession * session = MVTApp::startSession();
		MVTApp::mapURIs(session, "TestOverflowAddProp.prop.", sNumProps, mPropIds);
		for (int x=0; x < 1000; x ++)
		{
			IPIN *pin;
			Value val[30];int i=0;
			for (i=0; i < 10; i ++)
			{
				Tstring str;
				MVTApp::randomString(str,7,10);
				val[i].set(str.c_str());val[i].setPropID(mPropIds[i]);
			}
			for (i =10; i < 20; i ++)
			{
				val[i].setNow();val[i].setPropID(mPropIds[i]);
			}
			TVERIFYRC(session->createPIN(val,i,&pin,MODE_PERSISTENT|MODE_COPY_VALUES));
			pids.push_back(pin->getPID());
			pin->destroy();
		}
		session->terminate();
		session = MVTApp::startSession();
		session->setInterfaceMode(ITF_REPLICATION);			
		vector<PID>::iterator it; 
		for (it=pids.begin();pids.end() != it; it++)
		{
			IPIN *pin = session->getPIN(*it);
			Value val[40]; ElementID eids[40];unsigned short storeID_A = 0x1a00;
			Tstring str;int i=0,j=10;ElementID baseEID = BUILDEID(storeID_A,0);
			MVTApp::randomString(str,50,100);
			for (i=0; i < 20; i ++,j++)
			{
				val[i].set(str.c_str());val[i].setPropID(mPropIds[i]);eids[i] = baseEID;
			}
			TVERIFYRC(pin->modify(val,i,MODE_FORCE_EIDS,eids));
			pin->destroy();
		}
		for (it=pids.begin();pids.end() != it; it++)
		{
			IPIN *pin = session->getPIN(*it);
			Value val[5]; ElementID eids[5];unsigned short storeID_A = 0x1a00;
			ElementID baseEID = BUILDEID(storeID_A,0);
			Tstring buf; MVTApp::randomString(buf,40,60);
			val[0].set(buf.c_str()); val[0].setPropID(mPropIds[60]);eids[0] = baseEID;
			val[1].setU64(1000); val[1].setPropID(mPropIds[61]);eids[1] = baseEID;
			val[2].setU64(2000); val[2].setPropID(mPropIds[62]);eids[2] = baseEID;
			val[3].setNow(); val[3].setPropID(mPropIds[63]);eids[3] = baseEID;
			val[4].setDelete(mPropIds[1]);val[4].setPropID(mPropIds[1]);	//val[4].setMeta(META_PROP_IFEXIST);
			TVERIFYRC(pin->modify(val,5,MODE_FORCE_EIDS,eids));
			pin->destroy();
			
		}
		session->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
	return lSuccess ? 0 : 1;
}
