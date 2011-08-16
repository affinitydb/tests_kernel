/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"

#include <fstream>
#include "serialization.h"
#include "mvauto.h"
using namespace std;
#define NOEDITS 100
#define NOFPINS 10

#define BASH_HUGE_STREAM_EDITS 0 // Enable very long running portion of the test

// Publish this test.
class TestOpEdit : public ITest
{
	public:
		string stream;
		
		TEST_DECLARE(TestOpEdit);
		virtual char const * getName() const { return "testopedit"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "tests working of OP_EDIT"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Seemed to be freezing smoke test and has known failures.  Needs investigation"; return false; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:
		void populateStore(ISession *session,URIMap *pm,int npm, PID *pid);	
		IPIN* createUncommittedPIN(ISession *session,URIMap *pm,int npm);
		void quickstringtest( PropertyID & inID ) ;
		void teststring(ISession *session,URIMap *pm,int npm, PID *pid);
		void testbstr(ISession *session,URIMap *pm,int npm, PID *pid);
		void teststream(ISession *session);
		RC testCheckEdit(MVStore::Value const & val,string str, int startpos,ISession *session);
		void testExpectedString( const char * inExpected, IPIN* pin, PID & pid, PropertyID & inID ) ;
		void compareStr( const char * inExpected, const char * inGot, const char * inDesc ) ;
	protected:
		ISession * mSession ;
		string mCase ; 
};
class testOpEditStr : public MVStore::IStream
{
	protected:
		string & stream;
		size_t const mLength;
		size_t len;
		ValueType const mVT;
		char const mStartChar;
		size_t mSeek;
	public:
		testOpEditStr(size_t pLength, char pStartChar , ValueType pVT, string & pStream) : stream(pStream), mLength(pLength), mVT(pVT), mStartChar(pStartChar), mSeek(0) {len = pLength;}
		virtual ValueType dataType() const { return mVT; }
		virtual	uint64_t length() const { return len; }
		virtual size_t read(void * buf, size_t maxLength) { 
			size_t const lLength = MvStoreSerialization::PrimitivesOutDbg::mymin(mLength - mSeek, maxLength); 
			for (size_t i = 0; i < lLength; i++) {((char *)buf)[i] = getCharAt(mSeek + i, mStartChar);stream.push_back(((char *)buf)[i]);}
			mSeek += lLength;
			return lLength; 
		}
		virtual size_t readChunk(uint64_t pSeek, void * buf, size_t maxLength) { mSeek = (unsigned long)pSeek; return read(buf, maxLength); }
		virtual	IStream * clone() const { return new testOpEditStr(mLength,mStartChar,mVT,stream); }
		virtual	RC reset() { mSeek = 0; return RC_OK; }
		virtual void destroy() { delete this; }
	public:
		static char getCharAt(size_t pIndex, char pStartChar = '0') { return pStartChar + (char)(pIndex % 10); }
};

TEST_IMPLEMENT(TestOpEdit, TestLogger::kDStdOut);

// Implement this test.
int TestOpEdit::execute()
{
	if (MVTApp::startStore())
	{
		ISession * const session = MVTApp::startSession();
		mSession = session ;

		URIMap pm[4];
		PID pid[3];
		populateStore(session,pm,sizeof(pm)/sizeof(pm[0]),pid);	

		quickstringtest( pm[0].uid ) ;

		teststring(session,pm,sizeof(pm)/sizeof(pm[0]),pid);
		testbstr(session,pm,sizeof(pm)/sizeof(pm[0]),pid);
		teststream(session);
		session->terminate();
		MVTApp::stopStore();
	}

	return RC_OK  ;
}

void TestOpEdit::compareStr( const char * inExpected, const char * inGot, const char * inDesc )
{
	stringstream err ;
	err << mCase << " : " << inDesc << "- Expected " << inExpected << " got " << inGot ;
	TVERIFY2( 0 == strcmp( inGot, inExpected ), err.str().c_str() ) ;
}

void TestOpEdit::testExpectedString( const char * inExpected, IPIN* pin, PID & pid, PropertyID & inID )
{
	const char * mod = pin->getValue( inID )->str ; 	
	compareStr( inExpected, mod, "Original Pin" ) ;
	
	// Reconfirm after refresh
	pin->refresh() ;
	mod = pin->getValue( inID )->str ; 
	compareStr( inExpected, mod, "After Refresh" ) ;

	// Reconfirm with fresh copy of PIN data
	IPIN * pinLookup = mSession->getPIN( pid ) ;
	mod = pinLookup->getValue( inID )->str ; 
	compareStr( inExpected, mod, "New PIN Copy" ) ;
	pinLookup->destroy() ;
}

void TestOpEdit::quickstringtest( PropertyID & inID ) 
{
	// Quick overview of the OP_EDIT functionality, see teststring for more scenarios
	PID pid ;
	TVERIFYRC( mSession->createPIN( pid, NULL, 0 ) ) ;
	CmvautoPtr<IPIN> pin( mSession->getPIN(pid)) ;
	

	// Case 1 - insert 123 at position 1, removing no characters out of existing string
	mCase = "Case 1" ;
	Value val ;
	val.set( "ABCDEF" ) ; val.property = inID ; 
	TVERIFYRC( pin->modify( &val, 1 ) ) ; // Reset

	val.setEdit( "123" /*new string*/, 
				(unsigned long)strlen( "123") /*nChars*/, // Number of characters to insert 
				1 /*shift*/,      // offset in the string
				0 /*length*/ ) ;  // Number of characters to remove from original
	val.property = inID ;
	TVERIFYRC( pin->modify( &val, 1 ) ) ; 
	testExpectedString( "A123BCDEF", pin, pid, inID ) ; // REVIEW: getting "A123EF-"

	// case 2 - replace 3 characters as position 1
	mCase = "Case 2" ;
	val.set( "ABCDEF" ) ; val.property = inID ; pin->modify( &val, 1 ) ; // Reset
	val.setEdit( "123" /*new string*/, 
				(unsigned long)strlen( "123") /*nChars*/, 
				1 /*shift*/, 
				3 /*length*/ ) ;
	val.property = inID ;
	TVERIFYRC( pin->modify( &val, 1 ) ) ;
	testExpectedString( "A123EF", pin, pid, inID ) ;	


	// case 3 - Replace first three characters
	mCase = "Case 3" ;
	val.set( "ABCDEF" ) ; val.property = inID ; pin->modify( &val, 1 ) ; // Reset
	val.setEdit( "123" /*new string*/, 
				(unsigned long)strlen( "123") /*nChars*/, 
				0 /*shift*/, 
				3 /*length*/ ) ;
	val.property = inID ;
	TVERIFYRC(pin->modify( &val, 1 )) ; 
	testExpectedString( "123DEF", pin, pid, inID ) ;

	// case 4 - insert only two characters but take out 3
	mCase = "Case 4" ;
	val.set( "ABCDEF" ) ; val.property = inID ; pin->modify( &val, 1 ) ; // Reset
	val.setEdit( "123" /*new string*/, 
				2 /*nChars*/, 
				0 /*shift*/, 
				3 /*length*/ ) ;
	val.property = inID ;
	TVERIFYRC(pin->modify( &val, 1 )) ; 
	testExpectedString( "12DEF", pin, pid, inID ) ; // REVIEW: getting 12CDE

	// case 5 - Append
	mCase = "Case 5" ;
	val.set( "ABCDEF" ) ; val.property = inID ; pin->modify( &val, 1 ) ; // Reset
	val.setEdit( "123" /*new string*/, 
				3 /*nChars*/, 
				~0ULL /*shift*/, 
				0 /*length*/ ) ;
	val.property = inID ;
	TVERIFYRC(pin->modify( &val, 1 )) ; 
	testExpectedString( "ABCDEF123", pin, pid, inID ) ;

	// case 6 - Replace last character
	mCase = "Case 6" ;
	val.set( "ABCDEF" ) ; val.property = inID ; TVERIFYRC(pin->modify( &val, 1 )) ; // Reset
	val.setEdit( "123" /*new string*/, 
				3 /*nChars*/, 
				5 /*shift*/, 
				1 /*length*/ ) ;
	val.property = inID ;
	TVERIFYRC(pin->modify( &val, 1 )) ; 
	testExpectedString( "ABCDE123", pin, pid, inID ) ; 


	// case 7 - Append directly before last character
	mCase = "Case 7" ;
	val.set( "ABCDEF" ) ; val.property = inID ; pin->modify( &val, 1 ) ; // Reset
	val.setEdit( "123" /*new string*/, 
				1 /*nChars*/, 
				5 /*shift*/, 
				0 /*length*/ ) ;
	val.property = inID ;
	TVERIFYRC(pin->modify( &val, 1 )) ; 
	testExpectedString( "ABCDE1F", pin, pid, inID ) ; // REVIEW: getting ABCDE12
}

void TestOpEdit::teststring(ISession *session,URIMap *pm,int npm, PID *pid){
	
    Value pVal[1];
	const Value *lVal;
	uint32_t shf;
	uint32_t len;
	IPIN * pin = session->getPIN(pid[0]);
	lVal = pin->getValue(pm[0].uid);
	if ( isVerbose() ) std::cout<<"\n\n\nOriginal String:: "<<lVal->str<<std::endl;
	
	// Case #str1
	const char *s = "This string has been modified now. :):)";	
	shf = 0;
	len = uint32_t(strlen(s));
	pVal[0].setEdit(s,shf,len); pVal[0].setPropID(pm[0].uid);
	TVERIFYRC(session->modifyPIN(pid[0],pVal,1));
	pin->refresh();
	lVal = pin->getValue(pm[0].uid);
	if ( isVerbose() ) std::cout<<"Case #str1:: "<<lVal->str<<std::endl;

	TVERIFY2(strcmp(s,lVal->str) == 0, "Case #str1:: OP_EDIT on VT_STRING");		

	// Case #str2
	const char *s1 = "again  ";	
	char *expStr = "This string has been modified again  :)";	
	shf = 30;
	len = 7;
	pVal[0].setEdit(s1,shf,len); pVal[0].setPropID(pm[0].uid);
	TVERIFYRC(session->modifyPIN(pid[0],pVal,1));
	pin->refresh();	
	lVal = pin->getValue(pm[0].uid);
	if ( isVerbose() ) std::cout<<"Case #str2:: "<<lVal->str<<std::endl;

	TVERIFY2(strcmp(expStr,lVal->str) == 0, "Case #str2:: OP_EDIT on VT_STRING" ) ;
	
	// Case #str3
	const char *s2 = "dummy";	
	shf = 30;
	len = 10; // Too big so this will fail
	pVal[0].setEdit(s2,shf,len); pVal[0].setPropID(pm[0].uid);
	TVERIFY2( RC_OK != session->modifyPIN(pid[0],pVal,1), "Case #str3:: OP_EDIT on VT_STRING");
	pin->refresh();
	lVal = pin->getValue(pm[0].uid);
	if ( isVerbose() ) mLogger.out()<<"Case #str3:: "<<lVal->str<<std::endl;

	// Case #str4
	const char *s3 = "thrice :). Adding more to this string.";	
	expStr = "This string has been modified thrice :). Adding more to this string.  :)";	
	shf = 30;
	len = 5;
	pVal[0].setEdit(s3,shf,len); pVal[0].setPropID(pm[0].uid);
	TVERIFYRC(session->modifyPIN(pid[0],pVal,1));
	pin->refresh();
	
	lVal = pin->getValue(pm[0].uid);
	if ( isVerbose() )	std::cout<<"Case #str4:: "<<lVal->str<<std::endl;

	TVERIFY2(strcmp(expStr,lVal->str) == 0, "Case #str4:: OP_EDIT on VT_STRING");

	// Case #str5
	const char *s4 = "for the last time.";	
	expStr = "This string has been modified for the last time.more to this string.  :)";	
	shf = 30;
	len = 18;
	pVal[0].setEdit(s4,(unsigned long)18,shf,len); pVal[0].setPropID(pm[0].uid);
	TVERIFYRC(session->modifyPIN(pid[0],pVal,1));
	pin->refresh();
	
	lVal = pin->getValue(pm[0].uid);
	if ( isVerbose() )	std::cout<<"Case #str5:: "<<lVal->str<<std::endl;

	TVERIFY2(strcmp(expStr,lVal->str) == 0, "Case #str5:: OP_EDIT on VT_STRING");

	pin->destroy();
	
	// Case #str6 for Uncommitted PINs
	Value pvs[4];
	const	char *str = "How about editing this piece of wide char?";
	const unsigned char bstr[] = {97,98,99,65,66,67};
	SETVALUE(pvs[0], pm[0].uid, "How about editing this piece of string?", OP_SET);
	SETVALUE(pvs[1], pm[1].uid, str, OP_SET);
	pvs[2].set(bstr,6); SETVATTR(pvs[2], pm[2].uid, OP_SET);
	IPIN *uncommitpin = session->createUncommittedPIN(pvs,3,MODE_COPY_VALUES);		
	lVal= uncommitpin->getValue(pm[0].uid);
	if ( isVerbose() )	std::cout<<"\n\n\nOriginal Uncommited PIN String:: "<<lVal->str<<std::endl;

	#if 1
		const char *s5 = "This string has been modified now.";	
		const char *expstr5 = "This string has been modified now.";	
		shf = 0;len = lVal->length;
	#else
		const char *s5 = " Modified";	
		const char *expstr5 = "How about editing this piece of string? Modified";	
		shf = lVal->length;len = 0;
	#endif
	
	pVal[0].setEdit(s5,shf,len); pVal[0].setPropID(pm[0].uid);
	TVERIFYRC( uncommitpin->modify(pVal,1));
	uncommitpin->refresh();
	lVal = uncommitpin->getValue(pm[0].uid);
	if ( isVerbose() )	std::cout<<"Case #str6 Uncommited PIN:: "<<lVal->str<<std::endl;

	TVERIFY2(strcmp(expstr5,lVal->str) == 0, "Case #str6:: Uncommited PIN OP_EDIT on VT_STRING") ;
	uncommitpin->destroy();

	/** To be completed..
	  * 1> Huge strings
	  * 2> Editing property values spanning across pages ???
	  * 3> ...
	 **/
}	

void TestOpEdit::testbstr(ISession *session,URIMap *pm,int npm, PID *pid){
	Value pVal[1];
	const Value *lVal;
	uint32_t shf;
	uint32_t len;
	IPIN * pin = session->getPIN(pid[0]);
	lVal = pin->getValue(pm[2].uid);
	if ( isVerbose() )
	{
		std::cout<<"\n\n\nOriginal bstr :: ";//<<lVal->val.bstr<<std::endl;
		for(int i=0;i<long(lVal->length);i++)printf("%c",lVal->bstr[i]);
	}
	
	// Case #bstr1
	const unsigned char bstr[] = {100,101,102};
	unsigned char expbstr[] = {97,98,99,100,101,102};
	shf = 3;len = 3;
	pVal[0].setEdit(bstr,3,shf,len); pVal[0].setPropID(pm[2].uid);
	TVERIFYRC(session->modifyPIN(pid[0],pVal,1));	
	pin->refresh();
	lVal = pin->getValue(pm[2].uid);
	bool failed = false;
	if ( isVerbose() )	std::cout<<"\nCase #bstr1:: ";//<<lVal->bstr<<std::endl;

	for(int i=0;i<6;i++){
		if ( isVerbose() ) printf("%c",lVal->bstr[i]);

		if(expbstr[i] != lVal->bstr[i]) failed = true;
	}
	TVERIFY2(!failed, "Case #bstr1:: OP_EDIT on VT_BSTR" ) ;
	
	// Case #bstr2
	const unsigned char bstr1[] = {65};
	unsigned char expbstr1[] = {97,98,99,65,101,102};
	shf = 3;len = 1;
	pVal[0].setEdit(bstr1,1,shf,len); pVal[0].setPropID(pm[2].uid);
	TVERIFYRC(session->modifyPIN(pid[0],pVal,1));	
	pin->refresh();
	lVal = pin->getValue(pm[2].uid);
	failed = false;
	if ( isVerbose() )	std::cout<<"\nCase #bstr2:: ";//<<lVal->bstr<<std::endl;

	for(int i=0;i<6;i++){
		if ( isVerbose() )	printf("%c",lVal->bstr[i]);

		if(expbstr1[i] != lVal->bstr[i]) failed = true;
	}
	TVERIFY2(!failed,"Case #bstr2:: OP_EDIT on VT_BSTR");
	
	// Case #bstr3
	int nCount = 0;
	const unsigned char bstr2[] = {120,121,122};
	#if 1
		unsigned char expbstr2[] = {97,98,99,120,121,122,101,102};
		shf = 3;len = 1;nCount=8;
	#else
		// Doesn't work if the length is ZERO ? Inconsistent !!!
		unsigned char expbstr2[] = {97,98,99,120,121,122,65,101,102};
		shf = 3;len = 0;nCount=9;
	#endif
	pVal[0].setEdit(bstr2,3,shf,len); pVal[0].setPropID(pm[2].uid);
	TVERIFYRC(session->modifyPIN(pid[0],pVal,1));
	pin->refresh();
	lVal = pin->getValue(pm[2].uid);
	failed = false;
	if ( isVerbose() )	std::cout<<"\nCase #bstr3:: ";

	for(int i=0;i<nCount;i++) {
		if ( isVerbose() ) printf("%c",lVal->bstr[i]);

		if(expbstr2[i] != lVal->bstr[i]) failed = true;
	}
	TVERIFY2(!failed,"Case #bstr3:: OP_EDIT on VT_BSTR");
	
	// Case #bstr4
	unsigned long count = 2000;
	/*
	int value=65;
	unsigned char *bstr3 = new unsigned char[count];
	for(int i=0;i<long(count);i++,value++){
		bstr3[i]=value;		
		if(value==91) value = 97;
		if(value==123) value = 65;
	}
	*/
	count = 52;const unsigned char bstr3[] = {65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122};		
	shf = 0;if(len == 0) len=9; else len=8;
	pVal[0].setEdit((const unsigned char *)bstr3,count,shf,len); pVal[0].setPropID(pm[2].uid);
	TVERIFYRC(session->modifyPIN(pid[0],pVal,1));	pin->refresh();
	lVal = pin->getValue(pm[2].uid);
	failed = false;
	if ( isVerbose() )	std::cout<<"\nCase #bstr4:: ";

	for(int i=0;i<long(count);i++) if(bstr3[i] != lVal->bstr[i]) failed = true;
	TVERIFY2(!failed,"Case #bstr4:: OP_EDIT on VT_BSTR");

	pin->destroy();

	// Case #bstr5 for Uncommitted PINs
	Value pvs[4];
	const	char	*str = "How about editing this piece of wide char?";
	const unsigned char bstrx[] = {97,98,99,65,66,67};
	SETVALUE(pvs[0], pm[0].uid, "How about editing this piece of string?", OP_SET);
	SETVALUE(pvs[1], pm[1].uid, str, OP_SET);
	pvs[2].set(bstrx,6); SETVATTR(pvs[2], pm[2].uid, OP_SET);
	IPIN *uncommitpin = session->createUncommittedPIN(pvs,3,MODE_COPY_VALUES);		
	lVal= uncommitpin->getValue(pm[2].uid);
	if ( isVerbose() ) 
	{
		std::cout<<"\n\n\nOriginal Uncommited PIN bstr :: ";//<<lVal->val.bstr<<std::endl;
		for(int i=0;i<long(lVal->length);i++)printf("%c",lVal->bstr[i]);
	}

	const unsigned char bstr5[] = {68,69,70};
	#if 1
		const unsigned char expbstr5[] = {97,98,99,65,66,67,68,69,70};
		shf = lVal->length;	len = 0; count = 9;
	#else
		const unsigned char expbstr5[] = {68,69,70};
		shf = 0;len = lVal->length; count = 3;
	#endif
	
	pVal[0].setEdit(bstr5,3,shf,len); pVal[0].setPropID(pm[2].uid);
	TVERIFYRC(uncommitpin->modify(pVal,1));
	uncommitpin->refresh();
	lVal = uncommitpin->getValue(pm[2].uid);
	failed = false;
	if ( isVerbose() )	std::cout<<"\nCase #bstr5 Uncommited PIN:: ";

	for(int i=0;i<long(count);i++) if(expbstr5[i] != lVal->bstr[i]) failed = true;
	TVERIFY2(!failed,"Case #bstr5:: Uncommited PIN OP_EDIT on VT_BSTR");
	uncommitpin->destroy();
	
	// Case #bstr6
	/** To be completed..
	  * 1> Huge strings
	  * 2> Editing property values spanning across pages ???
	  * 3> ...
	 **/
}

void TestOpEdit::teststream(ISession *session)
{
	//const int mode = session->getInterfaceMode();
	//session->setInterfaceMode(mode | ITF_FORCE_STR_STREAM);
	int i;	
	IStmt *query;
	unsigned char var;

	Value val[NOEDITS];
	Tstring str,strs[NOEDITS];
	PropertyID lTmpPropID[2];
	MVTApp::mapURIs(session,"TestOpEdit.teststream",2,lTmpPropID);
	PropertyID propid = lTmpPropID[0];
	stream.clear();
	
	//case 1 : Short stream VT_STRING type
	IPIN *pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(100,65,VT_STRING,stream)));val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	PID id = pin->getPID();
	
	MVTRand::getString(str,5,0,false,false);
	int startpos = 30;
	pin = session->getPIN(id);
	pin->destroy();

	val[0].setEdit(str.c_str(),startpos,(uint32_t)str.length());val[0].setPropID(propid);
	pin = session->getPIN(id);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC2( testCheckEdit(*pin->getValue(propid),str,startpos,session),"case 1: Short Stream of VT_STRING type");
	pin->destroy();

	//case 2: Short Stream of VT_BSTR type
	str="";
	stream.clear();
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(10,48,VT_BSTR,stream)));val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();

	const unsigned char bstr[] = {100,101,102,'\0'};
	for (int x =0; bstr[x]!='\0';x++)
		str.push_back((bstr)[x]);
	startpos = 2;
	pin = session->getPIN(id);
	val[0].setEdit(bstr,3,startpos,3);val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC2(testCheckEdit(*pin->getValue(propid),str,startpos,session),"case 2: Short Stream of VT_BSTR type");
    pin->destroy();

	//case 3: MULTIPLE edit of a short stream. VT_STRING
	//Fails: Unimplemented code: RC_INTERNAL ln: 398 modifypin.cpp 

	stream.clear();
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(1024,65,VT_STRING,stream)));val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();
	for (i=0; i<NOEDITS; i++){
		MVTRand::getString(strs[i],5,0,false,false);
		size_t z = rand() % (10*(i+1));	//stream.length();
		stream.replace(z,strs[i].length(),strs[i].c_str(),strs[i].length());
		val[i].setEdit(strs[i].c_str(),(uint32_t)z,(uint32_t)strs[i].length());val[i].setPropID(propid);
	}
	pin = session->getPIN(id);
	TVERIFYRC(pin->modify(val,NOEDITS));
	str="";
	startpos=0;
	TVERIFYRC2(testCheckEdit(*pin->getValue(propid),str,startpos,session),"case 3: MULTIPLE edit of a short stream. VT_STRING");
	pin->destroy();

	//case 4: MULTIPLE edit of a short stream. VT_BSTR
	//Fails: Unimplemented code: RC_INTERNAL ln: 398 modifypin.cpp
	stream.clear();
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(4096,65,VT_BSTR,stream)));val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();
	for (i=0; i<NOEDITS; i++){
		const unsigned char bst[] = {65 + rand() %(26),'\0'};
		size_t z = rand() % stream.length();
		strs[i] = (string)(char *)bst;
		stream.replace(z,1,strs[i].c_str(),1);
		val[i].setEdit((unsigned char*)strs[i].c_str(),1,(uint32_t)z,1);val[i].setPropID(propid);
	}
	pin = session->getPIN(id);
	TVERIFYRC(pin->modify(val,NOEDITS));
	str="";
	startpos=0;
	TVERIFYRC2(testCheckEdit(*pin->getValue(propid),str,startpos,session),"case 4: MULTIPLE edit of a short stream. VT_BSTR");
	pin->destroy();

	//case 5: Large streams:: VT_STRING
	//fails with an assert from Assertion failed: (esht&~0xFFFF)==0 && esht<=pv->length, modifypin.cpp, line 881
	stream.clear();
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(70000,65,VT_STRING,stream)));val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();

	MVTRand::getString(str,100,0,true,false);
	startpos = 64000 + rand() % 5001;
	pin = session->getPIN(id);
	val[0].setEdit(str.c_str(),startpos,(uint32_t)str.length());val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	
	TVERIFYRC2(testCheckEdit(*pin->getValue(propid),str,startpos,session),"case 5: Long Stream of VT_STRING type");
	pin->destroy();

	//case 6: Large streams :: VT_BSTR
	stream.clear();
	str="";
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(70000,65,VT_BSTR,stream)));val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();
	const unsigned char bbstr[] = { 100,101,102,103,104,65,66,67,68,69,70,71,74,80,'\0' };
	str="";
	for(int x =0; bbstr[x] != '\0'; x++)
		str.push_back(bbstr[x]);
	startpos = 45000 + rand() % 5001;
	pin = session->getPIN(id);
	val[0].setEdit(bbstr,14,(uint32_t)startpos,14);val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));

	TVERIFYRC2(testCheckEdit(*pin->getValue(propid),str,startpos,session),"case 6: Long Stream of VT_BSTR type");
	pin->destroy();

	//case 7: long stream multiple edits
	stream.clear();
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(80000,65,VT_STRING,stream)));val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();
	for (i=0; i<NOEDITS; i++){
		MVTRand::getString(str,5,0,false,false);
		size_t z = rand() % stream.length();
		stream.replace(z,str.length(),str.c_str(),str.length());
		val[i].setEdit(str.c_str(),(uint32_t)z,(uint32_t)str.length());val[i].setPropID(propid);
	}
	pin = session->getPIN(id);
	TVERIFYRC(pin->modify(val,NOEDITS));
	str="";
	startpos=0;
	TVERIFYRC2(testCheckEdit(*pin->getValue(propid),str,startpos,session),"case 7: MULTIPLE edit of a long stream. VT_STRING");
	pin->destroy();

	//case 8: FTIndexing and OP_EDIT
	stream.clear();
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(80000,65,VT_STRING,stream)));val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();

	MVTRand::getString(str,100,0,true,false);
	startpos = 64000 + rand() % 5001;
	pin = session->getPIN(id);
	val[0].setEdit(str.c_str(),startpos,(uint32_t)str.length());val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));

	query = session->createStmt();
	var = query->addVariable();

	// Look for the string that we added as an edit operation
	query->setConditionFT(var,str.c_str());
	uint64_t cnt;
	query->count(cnt);
	TVERIFY2( cnt == 1, "case 8: FTIndexing and edit of stream. VT_STRING" ) ;
	
	query->destroy();
	pin->destroy();

	//case 9: Multiple Modif of strings
	stream.clear();
	//PropertyID propid1 = (unsigned)17499;
	PropertyID propid1 = lTmpPropID[1];
	//create pins with streams which will match the query.
	for (i = 0; i < NOFPINS; i ++){
		pin = session->createUncommittedPIN();
		val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(80000,65,VT_STRING,stream)));val[0].setPropID(propid);
		val[1].set("testopeditstr");val[1].setPropID(propid1);
		pin->modify(val,2);
		TVERIFYRC(session->commitPINs(&pin,1));
		pin->destroy();
	}
	//create query.
	query = session->createStmt(STMT_UPDATE);
	var = query->addVariable();

	Value args[2];
	PropertyID pids[1];
	pids[0]=propid1;
	args[0].setVarRef(0,*pids);
	IExprTree *expr = session->expr(OP_EXISTS,1,args);
	query->addCondition(var,expr);
	
	MVTRand::getString(str,100,0,true,false);
	startpos = 64000 + rand() % 5001;

	val[0].setEdit(str.c_str(),startpos,(uint32_t)str.length());val[0].setPropID(propid);
	query->setValues(val,2);

	TVERIFYRC2(query->execute(),"case 9: MULTIMODIF of streams");

	expr->destroy();
	query->destroy();
    
	//case 10: edit of a stream with SSV flag set.
	stream.clear();
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(80000,65,VT_STRING,stream)));val[0].setPropID(propid);val[0].meta=META_PROP_SSTORAGE;
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();

	MVTRand::getString(str,100,0,true,false);
	startpos = 64000 + rand() % 5001;
	pin = session->getPIN(id);
	val[0].setEdit(str.c_str(),startpos,(uint32_t)str.length());val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC2(testCheckEdit(*pin->getValue(propid),str,startpos,session),"case 10: Stream with SSV flag (VT_STRING) ");
	pin->destroy();

	//case 11: edit of a VT_BSTR stream with ssv flag set.
	stream.clear();
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(70000,65,VT_BSTR,stream)));val[0].setPropID(propid);val[0].meta=META_PROP_SSTORAGE;
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();

	str="";
	for(int x =0; bbstr[x] != '\0'; x++)
		str.push_back(bbstr[x]);
	startpos = 45000 + rand() % 5001;
	pin = session->getPIN(id);
	val[0].setEdit(bbstr,14,(uint32_t)startpos,14);val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));

	TVERIFYRC2(testCheckEdit(*pin->getValue(propid),str,startpos,session),"case 11: Stream with SSV flag (VT_BSTR)");	
	pin->destroy();

	// case 12: SSV -> LOB
	stream = "abcdefg";
	val[0].set(stream.c_str(),(uint32_t)stream.size());val[0].setPropID(propid);val[0].meta=META_PROP_SSTORAGE;
	pin = session->createUncommittedPIN(val,1,MODE_COPY_VALUES);
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();

	MVTRand::getString(str,100000,100000,false,false);
	char * lStrcpy = (char *)session->alloc(1 + str.length());
	strcpy(lStrcpy, str.c_str());
	startpos = (int)stream.size();
	pin = session->getPIN(id);
	val[0].setEdit(lStrcpy,(uint32_t)startpos,0);val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	session->free(lStrcpy);

	TVERIFYRC2(testCheckEdit(*pin->getValue(propid),str,startpos,session),"case 12: Stream with SSV flag (VT_STRING)");
	pin->destroy();

	//case 13: Truncate  short stream
	stream.clear();
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(2000,65,VT_BSTR,stream)));val[0].setPropID(propid);val[0].meta=META_PROP_SSTORAGE;
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();

    pin = session->getPIN(id);
	val[0].setEdit((unsigned char*) 0, 0, 0, 2000);val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	pin->destroy();
	
	//case 14: Truncate large stream
	stream.clear();
	pin = session->createUncommittedPIN();
	val[0].set(MVTApp::wrapClientStream(session, new testOpEditStr(70000,65,VT_BSTR,stream)));val[0].setPropID(propid);val[0].meta=META_PROP_SSTORAGE;
	TVERIFYRC(pin->modify(val,1));
	TVERIFYRC(session->commitPINs(&pin,1));
	id = pin->getPID();
	pin->destroy();

    pin = session->getPIN(id);
	val[0].setEdit((unsigned char*) 0, 0, 0, 70000);val[0].setPropID(propid);
	TVERIFYRC(pin->modify(val,1));
	pin->destroy();

	//case 15: increment value size in cycle

	TVERIFYRC(session->createPIN( id, 0, 0 ));
    unsigned char buffer[1024];
    val[0].set(buffer,100); val[0].setPropID(propid);

    TVERIFYRC(session->modifyPIN(id,val,1));

    for (i=1; i < 100; i++) {
        val[0].setEdit(buffer,100,i*100,0); val[0].setPropID(propid);
        TVERIFYRC(session->modifyPIN(id,val,1));
    }

// case 16: 
// see performance bugzilla issue #8002
// See also testlargeblob.cpp
	PID myPID;
	TVERIFYRC(session->createPIN( myPID, 0, 0 ));
	const int size = 32*1024;
	unsigned char *buf = (unsigned char *)alloca(size);
	val[0].set(buf,size); val[0].setPropID(propid);
    TVERIFYRC(session->modifyPIN(myPID,val,1));
	uint64_t n;
	//Modify the pins in a huge loop.

#if BASH_HUGE_STREAM_EDITS
	for (i=1; i < 3000; i++) { // Takes incredible amount of time
#else
	for (i=1; i < 500; i++) {
#endif
		IPIN *pin = session->getPIN(myPID);
		if (pin->getValue(propid)->type == VT_STREAM)
			n = pin->getValue(propid)->stream.is->length();
		else
            n = pin->getValue(propid)->length;
		val[0].setEdit(buf,size,n,0); val[0].setPropID(propid);
		TVERIFYRC(pin->modify(val,1));
		pin->destroy();
	}

return;
}

RC TestOpEdit::testCheckEdit(MVStore::Value const & val, string str,int startpos,ISession *session)
{
	char ch;
	if(startpos !=0 || str != "")
		stream.replace(startpos,str.length(),str.c_str(),str.length());
	if (VT_STREAM != val.type){
		if (val.length != stream.length()){
			mLogger.out()<<"Length of the streams dont match!!!"<<std::endl;
			return RC_FALSE;
		}
		for (size_t i =0; i < stream.length(); i++){
			if (val.str[i] != stream[i]){
                std::cout<<"Expected: "<<stream[i]<<" Found: "<<val.str[i]<<std::endl;
				mLogger.out()<<"Structural difference in streams!!!"<<std::endl;
				return RC_FALSE;
			}
		}
	}
	else {
		if (val.stream.is->length() != stream.length()){
			mLogger.out()<<"Length of the streams dont match!!!"<<std::endl;
			return RC_FALSE;
		}
		for (int i =0; 0 != val.stream.is->read(&ch,1); i++){
			if(ch != stream[i]){
				mLogger.out()<<"Structural difference in streams!!! Position " << i 
							 << " Expected " << stream[i] << " got " << ch << std::endl ;
				return RC_FALSE;
			}
		}
	}
	return RC_OK;
}

void TestOpEdit::populateStore(ISession *session,URIMap *pm,int npm, PID *pid)
{
	const	char	*str = "How about editing this piece of wide char?";
	const unsigned char bstr[] = {97,98,99,65,66,67};
	/*
	memset(pm,0,npm*sizeof(URIMap));	
	
	pm[0].URI="string";
	pm[1].URI="ustr";
	pm[2].URI="bstr";
	pm[3].URI="stream";	
	session->mapURIs(npm,pm);
	*/
	MVTApp::mapURIs(session,"TestOpEdit.prop",npm,pm);

	Value pvs[4];
	SETVALUE(pvs[0], pm[0].uid, "How about editing this piece of string?", OP_SET);
	//SETVALUE(pvs[1], pm[1].uid, ustr, OP_SET);
	pvs[1].set(str,39);pvs[1].setPropID(pm[1].uid);
	pvs[2].set(bstr,6); SETVATTR(pvs[2], pm[2].uid, OP_SET);

	session->createPIN(pid[0],pvs,3);	

	/*
	pvs[0].set(pm[0].uid,"Delhi");
	pvs[1].set(pm[1].uid,"Harsh");
	pvs[2].set(pm[2].uid,"Raju");
	pvs[3].set(pm[3].uid,"100011");
	
	session->createPIN(pid[2],pvs,6);
	*/
}

IPIN* TestOpEdit::createUncommittedPIN(ISession *session,URIMap *pm,int npm){
	const	char	*str = "How about editing this piece of wide char?";
	const unsigned char bstr[] = {97,98,99,65,66,67};
	memset(pm,0,npm*sizeof(URIMap));
	
	pm[0].URI="string";
	pm[1].URI="ustr";
	pm[2].URI="bstr";
	pm[3].URI="stream";	
	session->mapURIs(npm,pm);

	Value pvs[4];
	SETVALUE(pvs[0], pm[0].uid, "How about editing this piece of string?", OP_SET);
	SETVALUE(pvs[1], pm[1].uid, str, OP_SET);
	pvs[2].set(bstr,6); SETVATTR(pvs[2], pm[2].uid, OP_SET);
	return session->createUncommittedPIN(pvs,3);
}
