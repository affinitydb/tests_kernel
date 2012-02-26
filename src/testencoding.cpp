/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
using namespace std;

// Publish this test.
class TestEncoding : public ITest
{
	public:
		RC mRCFinal;
		TEST_DECLARE(TestEncoding);
		virtual char const * getName() const { return "testencoding"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "testing of encoding capabilities of the AfyDB"; }
		//Enabled the test for smoketest as it passes - Rohan (2/8/2006)
		//virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "till it passes..."; return false; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testencode(ISession *session);
		void populatestore(ISession *session);
};
TEST_IMPLEMENT(TestEncoding, TestLogger::kDStdOut);

// Implement this test.
int TestEncoding::execute()
{
	mRCFinal = RC_FALSE;
	if (MVTApp::startStore())
	{
		mRCFinal = RC_OK;
		ISession * const session = MVTApp::startSession();
		populatestore(session);
		testencode(session);
		session->terminate();
		MVTApp::stopStore();
	}

 	return mRCFinal;
}

void TestEncoding::populatestore(ISession *session)
{
	Value val[3];
	PID pid;
		
	string ustr;
	std::ifstream input;
	input.open("data.txt");//UTF8 file
	getline(input, ustr);
	IPIN *pin;
	URIMap pm[2];
	memset(pm,0,2*sizeof(URIMap));

	pm[0].URI = "TestEncoding.Prop1";
	pm[1].URI = "TestEncoding.Prop2";

	session->mapURIs(2,pm);
	
	while (!input.fail())
	{
		if (!ustr.find("--",0)==0)
		{
			val[0].set(ustr.c_str());val[0].setPropID(pm[0].uid);
			TVERIFYRC(session->createPIN(pid,val,1));
			pin = session->getPIN(pid);
			assert(0 == strcmp(pin->getValue(pm[0].uid)->str,ustr.c_str()));
			pin->destroy();
		}
		ustr="";
		getline(input, ustr);
	}
	input.close();
}

void TestEncoding::testencode(ISession *session)
{
	if (MVTApp::isRunningSmokeTest())
		return;
	//1. Read the search string from a UTF-8 encoded file
	//2. Run query against the store and verify
	std::ifstream input;
	string ustr;
	input.open("keywords.txt");//UTF8 file
	getline(input, ustr);
	IStmt *query = session->createStmt();
	ICursor *result = NULL;
	unsigned char var = query->addVariable();
	int cnt=0;

	while (!input.fail())
	{
		if (!ustr.find("--",0)==0)
		{
			query->setConditionFT(var,ustr.c_str());
			TVERIFYRC(query->execute(&result));
			for (IPIN *pin; (pin=result->next())!=NULL; )
			{
				cnt++;
				pin->destroy();
			}
		}
		result->destroy();
		ustr="";
		getline(input, ustr);
		cnt=0;
	}
	query->destroy();
	input.close();
}
