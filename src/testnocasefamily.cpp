/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#define NOCASEPINS 100
// Publish this test.
class TestNoCaseFamily : public ITest
{
	private:
		std::vector<Tstring> vOpEqStr,vOpBegStr,vOpConStr,vOpEndStr;
	public:
		std::vector<Tstring> mFamilyNames;
		static const int sNumProps = 4;
		PropertyID mPropIDs[sNumProps];
		TEST_DECLARE(TestNoCaseFamily);
		virtual char const * getName() const { return "testnocasefamily"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Basic test for CASE_INSENSITIVE_OP family queries"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void testNoCaseMeta(ISession *session);
		void testNoCasePins(ISession *session);
		void testNoCaseQueries(ISession *session);
		Afy::Value testNoCaseString(Tstring &str,int x, int op);
};
TEST_IMPLEMENT(TestNoCaseFamily, TestLogger::kDStdOut);

int TestNoCaseFamily::execute()
{
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		MVTApp::mapURIs(session,"TestNoCaseFamily.prop",sNumProps,mPropIDs);
		testNoCaseMeta(session);
		testNoCasePins(session);
		testNoCaseQueries(session);
		session->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	return RC_OK;
}

void TestNoCaseFamily::testNoCaseMeta(ISession *session)
{
	/*create family for VT_STRING types
	1. OP_EQ
	2. OP_BEGINS
	*/
	ClassID cls = STORE_INVALID_CLASSID;	
	Tstring lName; MVTRand::getString(lName,10,10,false,false);
	char lB[100];
	sprintf(lB,"testNoCaseFamily.OP_EQ%s.%d", lName.c_str(), 1); mFamilyNames.push_back(lB);
	sprintf(lB,"testNoCaseFamily.OP_BEGINS%s.%d", lName.c_str(), 1); mFamilyNames.push_back(lB);
	
	//OP_EQ class
	if (RC_NOTFOUND == session->getClassID(mFamilyNames[0].c_str(),cls)){
		IStmt *query = session->createStmt();
		unsigned char var = query->addVariable();
		Value args[2];
		args[0].setVarRef(0,(mPropIDs[0]));
		args[1].setParam(0);
		IExprTree *expr = session->expr(OP_EQ,2,args,CASE_INSENSITIVE_OP);
		query->addCondition(var,expr);		
		TVERIFYRC(defineClass(session,mFamilyNames[0].c_str(), query, &cls));
		query->destroy();
		expr->destroy();
	}
	//OP_BEGINS
	if (RC_NOTFOUND == session->getClassID(mFamilyNames[1].c_str(),cls)){
		IStmt *query = session->createStmt();
		unsigned char var = query->addVariable();
		Value args[2];
		args[0].setVarRef(0,(mPropIDs[1]));
		args[1].setParam(0);
		IExprTree *expr = session->expr(OP_BEGINS,2,args,CASE_INSENSITIVE_OP);
		query->addCondition(var,expr);		
		TVERIFYRC(defineClass(session,mFamilyNames[1].c_str(),query,&cls));
		query->destroy();
		expr->destroy();
	}
}

void TestNoCaseFamily::testNoCasePins(ISession *session)
{
	Value *val = NULL;
	Tstring str;
	char *tmp = NULL;
	int x=0;
	mLogger.out()<<"Creating "<<NOCASEPINS<<" pins...";
	for (int i=0; i<NOCASEPINS; i++){
		if (i%100==0)
			mLogger.out()<<".";
		val = (Value *)session->malloc(sNumProps*sizeof(Value));

		x = 20 + rand() % 235; 	
		MVTRand::getString(str,x,0,false);
		vOpEqStr.push_back(str);
		tmp = (char *)session->malloc((str.length() + 1)*sizeof(char));
		strcpy(tmp,str.c_str());
		val[0].set(tmp);val[0].setPropID(mPropIDs[0]);

		x = 20 + rand() % 235; 	
		MVTRand::getString(str,x,0,false);
		vOpBegStr.push_back(str);
		tmp = (char *)session->malloc((str.length() + 1)*sizeof(char));
		strcpy(tmp,str.c_str());
		val[1].set(tmp);val[1].setPropID(mPropIDs[1]);

		x = 20 + rand() % 235; 	
		MVTRand::getString(str,x,0,false);
		vOpConStr.push_back(str);
		tmp = (char *)session->malloc((str.length() + 1)*sizeof(char));
		strcpy(tmp,str.c_str());
		val[2].set(tmp);val[2].setPropID(mPropIDs[2]);

		x = 20 + rand() % 235; 	
		MVTRand::getString(str,x,0,false);
		vOpEndStr.push_back(str);
		tmp = (char *)session->malloc((str.length() + 1)*sizeof(char));
		strcpy(tmp,str.c_str());
		val[3].set(tmp);val[3].setPropID(mPropIDs[3]);

		TVERIFYRC(session->createPIN(val,4,NULL,MODE_PERSISTENT));;
	}
}

void TestNoCaseFamily::testNoCaseQueries(ISession *session)
{
	vector<Tstring>::iterator it;
	Tstring str;
	ClassID cls;
	int x =0;
	mLogger.out()<<"\nRunning Family queries for OP_EQ...";
	for (it=vOpEqStr.begin();vOpEqStr.end() != it; it++){
		mLogger.out()<<".";
		IStmt *query;
		uint64_t cnt=0;
		SourceSpec csp;
		Value args[1];
		unsigned char var;
		x = rand() % 4;
		args[0] = testNoCaseString(*it,x,0);
		query = session->createStmt();
		cls = STORE_INVALID_CLASSID;
		session->getClassID(mFamilyNames[0].c_str(),cls);
		csp.objectID = cls;
		csp.nParams = 1;
		csp.params = args;
		var = query->addVariable(&csp,1);
		query->count(cnt);
		TVERIFY(1 == cnt);
		query->destroy();
	}
	vOpEqStr.clear();
	mLogger.out()<<"Done"<<std::endl;

	//OP_BEGINS
	mLogger.out()<<"\nRunning Family queries for OP_BEGINS...";
	for (it=vOpBegStr.begin();vOpBegStr.end() != it; it++){
		mLogger.out()<<".";
		IStmt *query;
		uint64_t cnt=0;
		SourceSpec csp;
		Value args[1];
		unsigned char var;
		x = rand() % 4;
		args[0] = testNoCaseString(*it,x,1);
		query = session->createStmt();
		session->getClassID(mFamilyNames[1].c_str(),cls);
		csp.objectID = cls;
		csp.nParams = 1;
		csp.params = args;
		var = query->addVariable(&csp,1);
		query->count(cnt);
		TVERIFY(1 == cnt);
		query->destroy();
	}
	vOpBegStr.clear();
	mLogger.out()<<"Done"<<std::endl;
}

Afy::Value TestNoCaseFamily::testNoCaseString(Tstring &str,int x,int op)
{
	Value args;
	size_t i,y;
	switch (x){
			case(0): //all to lower case
				for(i=0;i<str.length();i++)
					str[i]=tolower(str[i]);
				break ;
			case(1): //all to upper case
				for(i=0;i<str.length();i++)
					str[i]=toupper(str[i]);
				break;
			case(2)://mixed case
				for(i=0;i<str.length();i++){
					if (i % 2==0)
						str[i]=toupper(str[i]);
					else
						str[i]=tolower(str[i]);
				}
				break;
			case(3)://normal case
				break;
			default:
				break;
		}
	switch (op){
		case (0): //OP_EQ
			args.set(str.c_str());
			break;
		case(1)://OP_BEGINS
			y = MVTRand::getRange(10,(int)(str.length()-1));
			str = str.substr(0,y);
			args.set(str.c_str());
			break;
		default:
			break;
	}
	return args;
}
