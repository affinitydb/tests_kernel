/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"
#include <stdlib.h>
#include <string.h>

class testlistwords : public ITest
{
public:
	
	TEST_DECLARE(testlistwords);
	virtual char const * getName() const { return "testlistwords"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "Test for Isession::listwords()"; }
	virtual int execute();
	virtual void destroy() { delete this; }
	void createWords(const char* strList);
	void verifyListWords(const char* q,const char* rstr,bool fExpected=false);
	
public:
	ISession * mSession ;
	PropertyID	mProp[1] ;
	IPIN *mIPIN;	
};
TEST_IMPLEMENT(testlistwords, TestLogger::kDStdOut);

int testlistwords::execute()
{
	if (!MVTApp::startStore()) {mLogger.print("Failed to start store\n"); return RC_NOACCESS;}
	mSession = MVTApp::startSession();

	char *propName2;char lB[64]; sprintf(lB, "testlistwordsProp.");propName2=lB;
	mProp[0]=MVTUtil::getPropRand(mSession,propName2);
	
	createWords("abcd"); //single word scenario
	verifyListWords("a","abcd");
	verifyListWords("ab","abcd");
	verifyListWords("abc","abcd");
	verifyListWords("abcd","abcd");
	verifyListWords("abcde","abcd",true);

	createWords("abcd abeq bcfg"); //multiple words scenario
	verifyListWords("ab","abcd abeq");
	verifyListWords("ab bc","abcd abeq bcfg");
	
	createWords("qtvy dwqa iopl"); //multiple words scenario one of the items not found
	verifyListWords("dw hyretpo","dwqa");
	verifyListWords("hyretpo dw","dwqa");

	createWords("zio38"); //alpha numeric
	verifyListWords("zi","zio38");
	verifyListWords("zio3","zio38");
	verifyListWords("zio38","zio38");
	
	createWords("786blunders"); //numeric alpha
	verifyListWords("7","786");
	verifyListWords("786b","786blunders",true);
	verifyListWords("blu","blunders");

	createWords("impgo_kotre imhr-ghet grewqq#thge"); //special chars and delimiters
	verifyListWords("impg","impgo_kotre");
	verifyListWords("impg","impgo_kotre");
	verifyListWords("impgo_","impgo_kotre");
	verifyListWords("impgo_k","impgo_kotre");
	verifyListWords("imhr","imhr");
	verifyListWords("imhr-ghet","imhr ghet");
	verifyListWords("imhr-gh","imhr-ghet",true);

	mSession->terminate();
	MVTApp::stopStore();
	return RC_OK;
}
void testlistwords::createWords(const char* strList)
{
	Value vl ; 
	vl.set(strList) ; 
	vl.property = mProp[0] ;
	vl.meta = META_PROP_FTINDEX;
	mIPIN = mSession->createPIN(&vl,1,MODE_COPY_VALUES);	
	TVERIFYRC(mSession->commitPINs(&mIPIN,1)); 
	mIPIN->destroy();
}

void testlistwords::verifyListWords(const char* q,const char* rstr,bool fExpected)
{
	int i=0;int j=0;int k=0;
	string tstring;	string substring[10];
	tstring=rstr;
	while(rstr[i]!='\0')
	{
		if(rstr[i]==' ')
			{
				substring[k]=tstring.substr(j,i-j);
				j=i+1;k++;
			}
		i++;
	}
	substring[k]=tstring.substr(j);
	
	StringEnum *strEnm;
	const char * strE;
	TVERIFYRC(mSession->listWords(q,strEnm));
	int enmCnt=0;
	for(;;)
	{
		strE=strEnm->next();
		if(strE==NULL) 
		{
			TVERIFY((enmCnt>=k)||fExpected);break;
		}
		else
		{
			for(i=0;i<=k;i++)
			{
				if(!(strcmp(strE,substring[i].c_str())))
				{substring[i].clear();}
			}

		}
		enmCnt++;
	} 
	strEnm->destroy();

	for(i=0;i<=k;i++)
	{
		TVERIFY(((strcmp(substring[i].c_str(),""))==0)||fExpected);
	}
}
