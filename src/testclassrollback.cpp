/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

 /*!
 * Contains rollback test as suggested within bug 11183
 *
 * (The first test added by Michael Andronov. ).
 */
#include "app.h"
#include <fstream>

#include <sstream>

#include <stdlib.h>
#include "serialization.h"

#include <map>
#include <set>

#include "teststream.h" 

using namespace std;

static const int sNumProps = 12;

string   class_name ("testclassrollbackclass_");
string  class1_name ("testclassrollbackclass1_");
string  class2_name ("testclassrollbackclass2_");
string  class3_name ("testclassrollbackclass3_");

class TestClassRollBack; 
/*
* The scenario classes are what its name is about: 
* It encupsulates one particular scenario for the test. 
*/ 
class scenariobase
{
	public: 
		scenariobase(TestClassRollBack * const p ): pTest(p){};
		virtual RC operator ()(){return RC_OK;};
		TestClassRollBack * pTest;
	private: 
		scenariobase(){};
		scenariobase(const scenariobase &){};
};
      
#define lAllClassNotifs (CLASS_NOTIFY_JOIN | CLASS_NOTIFY_LEAVE | CLASS_NOTIFY_CHANGE | CLASS_NOTIFY_DELETE | CLASS_NOTIFY_NEW)

// Publish this test.
class TestClassRollBack : public ITest
{
	std::vector<PID> pidMap;
	AfyKernel::StoreCtx *mStoreCtx;
	
	public:		
		PropertyID mPropIDs[sNumProps];
		
		RC mRCFinal;
		TEST_DECLARE(TestClassRollBack);
		virtual char const * getName() const { return "testclassrollback"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "classrollbacktest - reacton for bug 11183"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual bool isLongRunningTest()const { return false; }
		
		virtual int execute();
		virtual void destroy() { delete this; }
		
		Tstring  fParam; 
		
	protected:
		void doTest();
		void destroyMap();
		void testclassrollback(ISession *session);
		
		void testclassrollbackmeta();
		void registerpropertieswithinstore();
		
		RC compareClassVersaFSQuery(); 
	  
		PID createPin();
		ISession *mSession; /* it is a constrain on this test that only one session will be used
							  so, it is easier to remember it as a class member */
		
		IPIN * createUncommittedPin(ISession *session);
	  
		/* In order to run the the same test again and again on the store, 
		* the classes names will be generated in 'random fashion'        */
		RC defineRandomClass( string s, IStmt *classQ, string *psreturn);
							  
		/* the purpose of the test dictates to remember the underlaying queries for the classes */
		/* the map serves the purpose                                                           */                        
		typedef map<ClassID, IStmt *,less<ClassID> >  clsqrymap;
		clsqrymap m_clsqryMap; 
      
		/*
		 * The following is the first scenario to test...
		 */
		class scenario_1 : public scenariobase
		{
			public:
				scenario_1(TestClassRollBack * const p ):scenariobase(p){};
				RC operator ()()
				{
					TVRC_R(pTest->mSession->startTransaction(),pTest);	
					pTest->createPin(); 
					pTest->createPin();
					pTest->createPin();
					TVRC_R(pTest->mSession->commit(),pTest);

					TVRC_R(pTest->mSession->startTransaction(),pTest);		
					pTest->createPin(); 
					pTest->createPin();
					pTest->createPin();
					TVRC_R(pTest->mSession->rollback(),pTest);
					return pTest->compareClassVersaFSQuery();
				}
		};
	
		/*
		 * Second scenario - similar to testcustom65...
		 */
		class scenario_2 : public scenariobase
		{
			public:
				scenario_2(TestClassRollBack * const p ):scenariobase(p){};
				  
				RC operator ()()
				{
					PID p0;

					/* the PIN should be added to the class... */
					p0 = pTest->createPin(); 

					TVRC_R(pTest->mSession->startTransaction(),pTest);

					Value v0; v0.set("fIGJYGluMyJvGHKtJMq"); v0.setPropID(pTest->mPropIDs[11]); 
					/* I assume that the PIN should be removed from the class... */ 
					TVRC_R(pTest->mSession->modifyPIN(p0,&v0,1),pTest);
					TVRC_R(pTest->mSession->deletePINs(&p0,1),pTest);
					TVRC_R(pTest->mSession->rollback(),pTest);

					return pTest->compareClassVersaFSQuery();
				}
		};

        /*
		 * Third scenario - as suggested by Andrew Skowronski...
		 */
		class scenario_3 : public scenariobase
		{
			public:
				scenario_3(TestClassRollBack * const p ):scenariobase(p){};
				clsqrymap m_clsScn3Map;   //class and query map for scenario 3; 
				set<PID> rsetCS, rsetFS; 
				  
				RC operator ()()
				{
					string s;
					set<PID>::iterator iter; 
					
					/* Step 1: creating simple family, storing query with classID within map... */
					createQueryPlusFamily();
					
					/* Step 2: Perform cases... */
					cout << "<scenario3_case1>" << endl;
					//case 1:
					TVRC_R(pTest->mSession->startTransaction(),pTest);
						s = createPin_scn3();
					TVRC_R(pTest->mSession->rollback(),pTest);
					TVRC_R(findPinsWithValue(s),pTest);
					if (0 != rsetCS.size()){
						cout << "ERROR: Expects 0 pins, but got " << (unsigned int) rsetCS.size()<< " " << __FILE__ << "{"<< std::dec <<__LINE__ <<"}"<< endl;
						cout << "</scenario3_case1>" << endl;
						return RC_FALSE;
					}
					cout << "</scenario3_case1>" << endl;
					cout << "<scenario3_case2>" << endl;
					
					//case 2:
					s = createPin_scn3();
					s = createPin_scn3(s);
					TVRC_R(findPinsWithValue(s),pTest);
					if (2 != rsetCS.size()){ 
						cout << "ERROR: Expects 2 pins, but got " << (unsigned int)rsetCS.size()<< " " << __FILE__ << "{"<< std::dec <<__LINE__ <<"}"<< endl;
						cout << "</scenario3_case2>" << endl;
						return RC_FALSE;
					}    
					TVRC_R(pTest->mSession->startTransaction(),pTest);
						iter = rsetCS.begin();
						TVRC_R(pTest->mSession->deletePINs(&(*iter),1),pTest);
					TVRC_R(pTest->mSession->rollback(),pTest);
					TVRC_R(findPinsWithValue(s),pTest);
					if (2 != rsetCS.size()){ 
						cout << "ERROR: Expects 2 pins, but got " << (unsigned int) rsetCS.size()<< " " << __FILE__ << "{"<< std::dec <<__LINE__ <<"}"<< endl;
						cout << "</scenario3_case2>" << endl;
						return RC_FALSE;
					}
					cout << "</scenario3_case2>" << endl;
					return RC_OK;
				}
		   
				RC findPinsWithValue(const string &s)
				{
					RC mRCFinal = RC_OK;
					clsqrymap::const_iterator iter;
					set<PID>::iterator pidIter;
				
					for(iter = m_clsScn3Map.begin(); iter!= m_clsScn3Map.end(); ++iter)
					{
						ClassSpec lCS;
						Value arg;
				  
						IStmt * queryCS(pTest->mSession->createStmt()); 
				
						arg.set(s.c_str());arg.setPropID(pTest->mPropIDs[1]);
			  
						//Check first that count numbers are equal...      
						lCS.classID = iter->first;
						lCS.nParams = 1;  
						lCS.params  = &arg; 
						queryCS->addVariable(&lCS,1);
					  
						char * strQuery = queryCS->toString() ;  //TVRC_R(( strlen( strQuery ) > 0 ),pTest) ;
						cout << strQuery << endl;
						pTest->mSession->free( strQuery ) ;
							  
						uint64_t cnt =0 ;
						queryCS->count( cnt ) ;
						cout << std::hex << cnt << " PINs belong to the class {"<< lCS.classID << "}" << std::endl ;

						char * strQueryFS = (iter->second)->toString() ; // TVERIFY( strlen( strQueryFS ) > 0 ) ;
						cout << strQueryFS << endl;
						pTest->mSession->free( strQueryFS ) ;
			
						uint64_t cntFS =0 ;
						(iter->second)->count( cntFS,&arg, 1) ;
						cout << std::hex << cntFS << " PINs returned by raw {full scan} query"<< std::endl ;       
		  
						TV_R((cntFS == cnt),pTest);
						
						//Check that PINs has the same PID(s)... 
						rsetCS.clear(); rsetFS.clear();
						
						ICursor * rQ = NULL;
						queryCS->execute(&rQ);
						
						IPIN * pPin;
						while(rQ && (NULL != (pPin = rQ->next())))
						{
							rsetCS.insert(pPin->getPID()); pPin->destroy(); 
						}   
						rQ->destroy(); 
				
						(iter->second)->execute(&rQ, &arg,1);
						
						while(rQ && (NULL != (pPin = rQ->next())))
						{
							rsetFS.insert(pPin->getPID()); pPin->destroy();
						}   
						rQ->destroy();
						
						//All the PID should be the same, consequently the SETs should be the same also!
						TV_R((rsetFS == rsetCS),pTest);
						queryCS->destroy(); 
					}
					return mRCFinal; 
				}
    
				/*
				  * Creates simple pin with propertyID=1 and random value; 
				  * The value is return as function result.
				*/
				string createPin_scn3()
				{
					Value val;
					PID   pid;
					Tstring str;
					MVTRand::getString(str,200,0,false,false);
					val.set(str.c_str());val.setPropID(pTest->mPropIDs[1]);
					TVRC_R(pTest->mSession->createPIN(pid,&val,1),pTest);
					return str;
				}

				/*
				 * Creates pin with  value, provided as parameter...
				 */
				const string & createPin_scn3(const string & str)
				{
					Value val;
					PID   pid;
					val.set(str.c_str());val.setPropID(pTest->mPropIDs[1]);
					TVRC_R(pTest->mSession->createPIN(pid,&val,1),pTest);
					return str;
				}
		  
				/*
				* Creates simple family with propID1
				*/ 
				void createQueryPlusFamily()
				{
					ClassID cls = STORE_INVALID_CLASSID;
					Value args[2];
					PropertyID pids[1];
					IStmt *query; 
					IExprTree *expr;
					unsigned char var;
					string s;

					//create a family to satisfy.
					query = pTest->mSession->createStmt();
					var = query->addVariable();

					pids[0] = pTest->mPropIDs[1];
					args[0].setVarRef(0,*pids);
					args[1].setParam(0);
					expr = pTest->mSession->expr(OP_EQ,2,args,0);

					query->addCondition(var,expr);
						
					TVRC_R(pTest->defineRandomClass(class_name,query, &s),pTest);
					TVRC_R(pTest->mSession->getClassID(s.c_str(),cls),pTest);
					TVRC_R(pTest->mSession->enableClassNotifications(cls,lAllClassNotifs),pTest);

					m_clsScn3Map.insert(clsqrymap::value_type(cls, query));

					expr->destroy();
				}
		};  
};
TEST_IMPLEMENT(TestClassRollBack,TestLogger::kDStdOut);

int TestClassRollBack::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }
	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;
	MVTApp::mapURIs(mSession,"TestClassRollBack.prop",sNumProps,mPropIDs);
	
	doTest();
	destroyMap();
	mSession->terminate();
	MVTApp::stopStore();
	return RC_OK;
}

/*
 * The reason I moved the destroy() method out of test class definition is 
 * the freeing of memory allocated for queries during class(es) definitions. 
 */
void TestClassRollBack::destroyMap()
{
	clsqrymap::const_iterator iter; 
	for(iter = m_clsqryMap.begin(); iter!= m_clsqryMap.end(); ++iter)
	{
		(iter->second)->destroy(); 
	}
}

/*
 * The core of the test is happening within the function below... 
 */
void TestClassRollBack::doTest()
{
	mRCFinal = RC_OK; 

	/* Next step - to define a couple of classes. 
	 * Since the purposes of the test to compare the results of 
	 * different queries, special measures are taken to remember the 
	 * class underlying queries. 
	*/
	testclassrollbackmeta(); 
	
	scenario_1  scn1(this);
	scenario_2  scn2(this);
	scenario_3  scn3(this);
	
	cout << "<scenario1_case1>" << endl;
	TVERIFYRC(scn1());  //scenario 1;
	cout << "</scenario1_case1>" << endl;
	cout << "<scenario2_case1>" << endl;
	TVERIFYRC(scn2());  //scenario 2;
	cout << "</scenario2_case1>" << endl;
	TVERIFYRC(scn3());  //scenario 3;
}

/* 
 * The puporse of the function - to compare results 
 *  full scan query and corresponding class based query. 
 * 
 * The queries pronounced the same if both conditions are met: 
 * - the number of return pins are the same;
 * - the PID of each pin within the queries results are the same; 
*/ 
RC TestClassRollBack::compareClassVersaFSQuery()
{
	mRCFinal = RC_OK; 
	clsqrymap::const_iterator iter; 
	set<PID> rsetCS, rsetFS; 
	set<PID>::iterator pidIter;
		    
	for(iter = m_clsqryMap.begin(); iter!= m_clsqryMap.end(); ++iter)
	{
		ClassSpec lCS;
		  
		IStmt * queryCS(mSession->createStmt()); 
	  
		//Check first that count numbers are equal...      
		lCS.classID = iter->first;
		lCS.nParams = 0;  
		queryCS->addVariable(&lCS,1);
			  
		char * strQuery = queryCS->toString() ;  TVERIFY( strlen( strQuery ) > 0 ) ;
		cout << strQuery << endl;
		mSession->free( strQuery ) ;
			  
		uint64_t cnt =0 ;
		queryCS->count( cnt ) ;
		cout << std::hex << cnt << " PINs belong to the class {"<< lCS.classID << "}" << std::endl ;

		char * strQueryFS = (iter->second)->toString() ;  TVERIFY( strlen( strQueryFS ) > 0 ) ;
		cout << strQueryFS << endl;
		mSession->free( strQueryFS ) ;
	
		uint64_t cntFS =0 ;
		(iter->second)->count( cntFS) ;
		cout << std::hex << cntFS << " PINs returned by raw {full scan} query"<< std::endl ;       

		TVERIFY( cntFS == cnt);
		
		//Check that PINs has the same PID(s)... 
		rsetCS.clear(); rsetFS.clear();

		ICursor * rQ= NULL;
		TVERIFYRC(queryCS->execute(&rQ));
		
		IPIN * pPin;
		while(rQ && (NULL != (pPin = rQ->next())))
		{
			rsetCS.insert(pPin->getPID()); pPin->destroy(); 
		}   
		rQ->destroy(); 
		
		TVERIFYRC((iter->second)->execute(&rQ));		
		while(rQ && (NULL != (pPin = rQ->next())))
		{
			rsetFS.insert(pPin->getPID()); pPin->destroy();
		}   
		rQ->destroy();
		
		//All the PID should be the same, consequently the SETs should be the same also!
		TVERIFY(rsetFS == rsetCS);	
		queryCS->destroy(); 
	}
	return mRCFinal; 
}

/* 
 * Function below creates a pin. 
 * Each function call will return a different pin with the same 
 * properties and different values. 
 */
PID TestClassRollBack::createPin()
{
	Value val[11];
	PID pid;
	Tstring str,str1,str2;

	MVTRand::getString(str,200,0,false,false);
	MVTRand::getString(str1,175,200,false,false);

	val[0].set(str.c_str());val[0].setPropID(mPropIDs[0]);
	str1.insert(0,"path/of/the/file");
	str1+=1;
	str1+="/jpg";
	val[1].set(str1.c_str());val[1].setPropID(mPropIDs[1]);
	val[2].set(str.c_str());val[2].setPropID(mPropIDs[2]);
	val[3].set(str.c_str());val[3].setPropID(mPropIDs[3]);
	val[4].set(str.c_str());val[4].setPropID(mPropIDs[4]);
	val[5].set(str.c_str());val[5].setPropID(mPropIDs[5]);
	str2 = "image/jpg";
	val[6].set(str2.c_str());val[6].setPropID(mPropIDs[6]);
    val[7].set(str.c_str());val[7].setPropID(mPropIDs[7]);
	val[8].set(str.c_str());val[8].setPropID(mPropIDs[8]);
	val[9].set(str.c_str());val[9].setPropID(mPropIDs[9]);

	//stream not being allocated on the heap in our import (?????)
	unsigned long size = 30000 + rand() % 5000;
	MyStream myTstStream(size); 
	
	//val[10].set(MVTApp::wrapClientStream(mSession, new MyStream(size))); val[10].setPropID(mPropIDs[10]);
	val[10].set(MVTApp::wrapClientStream(mSession, &myTstStream)); val[10].setPropID(mPropIDs[10]);

	TVERIFYRC(mSession->createPIN(pid,val,11));
	return pid;
}

/*
 * The purpose of the function to 'randomize' the name of the class: 
 * everytime we run the test, we would create a new class within the store. 
 * @param string s       - prefix of the class name, stays the same from test run to test run... 
 * @param IStmt *classq - query for defining the class within the store; 
 * 
 * function returns the result of internal definClass call.  
 */
RC TestClassRollBack::defineRandomClass(string s, IStmt *classQ, string *psreturn)
{
	RC rc;
	string randClassName;

	do 
	{
		randClassName = MVTRand::getString2(20, 20, false);
		s += randClassName;
		rc = defineClass(mSession, s.c_str(), classQ);
	}
	while(rc == RC_ALREADYEXISTS); // Problem when seed reused

	(*psreturn) = s;
	return rc;
}

/*
 * I am going to define several classes for the test purposes. 
 */
void TestClassRollBack::testclassrollbackmeta()
{
	ClassID cls = STORE_INVALID_CLASSID;
	Value args[2];
	PropertyID pids[1];
	IStmt *query; 
	IExprTree *expr;
	unsigned char var;
	string s;

	query = mSession->createStmt();
	var = query->addVariable();      //QueryVariable was allocated, its ID was return;

	pids[0] = mPropIDs[0];
	args[0].setVarRef(0,*pids);
	expr = mSession->expr(OP_EXISTS,1,args,0);
	query->addCondition(var,expr);
	
	TVERIFYRC(defineRandomClass(class1_name,query, &s));
	TVERIFYRC(mSession->getClassID(s.c_str(),cls));
	TVERIFYRC(mSession->enableClassNotifications(cls,lAllClassNotifs));
	
	m_clsqryMap.insert(clsqrymap::value_type(cls, query));
	expr->destroy();
	
	query = mSession->createStmt();
	var = query->addVariable();

	pids[0] = mPropIDs[6];
	args[0].setVarRef(0,*pids);
	args[1].set("image");
	expr = mSession->expr(OP_CONTAINS,2,args,0);

	query->addCondition(var,expr);
	TVERIFYRC(defineRandomClass(class2_name,query, &s));
	TVERIFYRC(mSession->getClassID(s.c_str(),cls));
	TVERIFYRC(mSession->enableClassNotifications(cls,lAllClassNotifs));
	
	m_clsqryMap.insert(clsqrymap::value_type(cls, query));
	
	expr->destroy();
	
	query = mSession->createStmt();
	var = query->addVariable();

	pids[0] = mPropIDs[5];
	args[0].setVarRef(0,*pids);
	expr = mSession->expr(OP_EXISTS,1,args,0);

	query->addCondition(var,expr);
	TVERIFYRC(defineRandomClass(class3_name,query, &s));
	TVERIFYRC(mSession->getClassID(s.c_str(),cls));
	TVERIFYRC(mSession->enableClassNotifications(cls,lAllClassNotifs));
	
	m_clsqryMap.insert(clsqrymap::value_type(cls, query));
	
	expr->destroy();
}
