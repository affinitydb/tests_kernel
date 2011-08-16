/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _CLASSHELPER_H
#define _CLASSHELPER_H
// Code to verify expected content of a class 
// and other helpers related to store classes
//
// Based on code extracted from sdrcl (CStoreClass), centralized so it can be used elsewhere

#include <vector>
#include <string>
#include <set>
#include <sstream>

#include "mvauto.h"

#include "mvstoreapi.h"
#include "serialization.h"

using namespace MVStore ;
using namespace std ; 

#define MAX_PINS_FOR_CLASS_TEST 200000
#define MAX_MISMATCH_PINS_TO_PRINT 50

class ClassHelper
{
public:
	typedef std::multiset<PID, std::less<PID> > TPidSet;


	/*! Determine whether the class has been dropped
	*/
	static bool isDropped(ISession *inSession, ClassID inClassID)
	{
		// Todo, possible via store inspector, but no ipc support
		return false;
	}

	/*! Build a query based on a class.
	For class families provide the variables.
	Orderby and ft conditions can also be added.
	Caller must call destroy() on the returned query
	*/
	static IStmt* getQueryUsingClass(
		ISession *inSession, 
		ClassID inClassID,
		STMT_OP sop=STMT_QUERY,
		unsigned int inCntParams =0,				
		const Value * inParams = NULL,						// Optional variables for Family queries
		unsigned int inCntOrderProps=0,
		const OrderSeg * inOrder=NULL,			// Optional sorting info
		const char * inFTSearch=NULL			// Optional full text search within the class		
		)
	{
		IStmt * const lQ = inSession->createStmt(sop);
		if (!lQ)
			return NULL;
		MVStore::ClassSpec lCS;
		lCS.classID = inClassID;
		lCS.nParams = inCntParams;
		lCS.params = inParams;
		unsigned char variable0=lQ->addVariable(&lCS, 1);

		if ( inFTSearch )
			lQ->setConditionFT( variable0, inFTSearch ) ;

		if ( inCntOrderProps > 0 )
		{
			lQ->setOrder( inOrder, inCntOrderProps ) ;
		}

		return lQ ;
	}


	/*! Get the query that "defines" the class, e.g. the query provided 
	    in the call to defineClass.  This is the "raw" query and its execution
		does not benefit from the index and may involve a full scan.
		
		Note: this call temporarily locks the entire store

		Caller must call destroy() on the returned query
	*/
	static IStmt* getQueryDefiningClass(ISession *inSession, const char *inClassName, ClassID& outClassid)
	{
		outClassid = STORE_INVALID_CLASSID ; IPIN *cpin=NULL;
		if (inSession->getClassID(inClassName,outClassid)!=RC_OK
			|| inSession->getClassInfo(outClassid,cpin)!=RC_OK) return NULL;

		assert(cpin!=NULL); IStmt *qry=NULL;
		const Value *cv = cpin->getValue(PROP_SPEC_PREDICATE);
		if (cv!=NULL && cv->type==VT_STMT) qry=cv->stmt->clone();
		cpin->destroy();
		return qry;
	}

	// Find the class name given a class ID

	static bool getClassNameFromID(ISession *inSession, ClassID inClassid, string &outClassName )
	{
		outClassName.clear() ; char buf[256]; size_t l=sizeof(buf)-1;
		if (inSession->getURI(inClassid,buf,l)!=RC_OK) return false;
		outClassName = buf; return true;
	}

	static bool isRangeQuery(ISession *inSession, const char * inClassName)
	{

		// Determine whether the query expects a RANGE (e.g. OP_IN)
		// For example this is important if trying to fill in a parameter to a 
		// family.
		//
		ClassID cid; IPIN *cpin=NULL; bool fRange=false;
		if (inSession->getClassID(inClassName,cid)==RC_OK && inSession->getClassInfo(cid,cpin)==RC_OK) {
			assert(cpin!=NULL);
			const Value *cv = cpin->getValue(PROP_SPEC_INDEX_INFO);
			if (cv!=NULL && cv->type==VT_ARRAY) fRange=cv->varray[0].iseg.op==OP_IN;
			cpin->destroy();
		}
		return fRange;
	}
	static bool isRangeQuery(ISession *inSession, ClassID inClassID )
	{
		// Second signature, if classname not known
		IPIN *cpin=NULL; bool fRange=false;
		if (inSession->getClassInfo(inClassID,cpin)==RC_OK) {
			assert(cpin!=NULL);
			const Value *cv = cpin->getValue(PROP_SPEC_INDEX_INFO);
			if (cv!=NULL && cv->type==VT_ARRAY) fRange=cv->varray[0].iseg.op==OP_IN;
			cpin->destroy();
		}
		return fRange;
	}


	/*! Test how a class compares with its underlying query.  Useful for testing
	that the cached results in the index are accurate with the store itself.
	Details of any divergence are returned in the stringstream.

	For classes:
	   Confirms that a class lookup returns the exact same pins as an execution of
	   the underlying query

	For class families:
	   To test a single index key then pass it to inCntParams,inParams
	   Otherwise each key value is individually tested, up to inMaxKeys
	   NOTE: This scan does not catch corrupt index if the key value is missing from the
	   index and should have been indexed.  However it will catch other problems,
	   for example where the key is present but it points to pins that doesn't match the key
	   OR points to too few pins.

	Use this signature if you don't already have the "raw" query that defines the class.

	TIP: When investigating use storedr pclass to print the class, testclass=X to run this
	code, and class=Y to see info about the class
	*/
	static bool testClass(
		ISession *inSession,
		const char *inClassName,
		std::ostream & outResultInfo,
		int &outCntQ,
		unsigned int inCntParams =0,				
		Value * inParams = NULL,  // Optional variables to test specific key in class Family queries
								  // pass null to test all keys
	    int inMaxKeys=-1 )		  // Set a maximum number of keys to scan
	{
		// REVIEW: in debug two potentially distracting fullscan query printouts are logged,
		// perhaps they should be silenced during this period

		ClassID lClassID;
		outCntQ=-1;

		// TODO: If we find any of these errors can happen in practice we should build even more context information into the result string
		CmvautoPtr<IStmt> lRawQ(getQueryDefiningClass(inSession,inClassName,lClassID)) ;

		if (!lRawQ.IsValid() || lClassID == STORE_INVALID_CLASSID )
		{
			outResultInfo << "Class not found" ;
			return false ;
		}

		return testClass(inSession,lClassID,lRawQ,outResultInfo,outCntQ,inCntParams,inParams);
	}

	/*! Direct test of class if the Raw Query already available, see documentation above
	*/
	static bool testClass(
		ISession *inSession,
		ClassID inClassID,
		IStmt* inRawQ,
		std::ostream & outResultInfo,
		int &outCntQ,
		unsigned int inCntParams =0,				
		const Value * inParams = NULL,
		int inMaxKeys=-1 )	// Set a maximum number of keys to scan
							// this is necessary because each family key is a full scan query
	{
		bool result=true;
		outCntQ=0;

		unsigned int cntParams = 0; IPIN *cpin=NULL;
		if (inSession->getClassInfo(inClassID,cpin)==RC_OK) {
			assert(cpin!=NULL);
			const Value *cv=cpin->getValue(PROP_SPEC_INDEX_INFO);
			if (cv!=NULL && cv->type==VT_ARRAY) cntParams = cv->length;
			cpin->destroy();
		}

		if ( cntParams == inCntParams )
		{
			// Normal case - class or a class family with specific key value
			result=testClassOrParameterizedFamily(inSession,inClassID,inRawQ,outResultInfo,outCntQ,inCntParams,inParams);
		}
		else if ( cntParams == 1 && inCntParams == 0 )
		{
			// Class Family with no parameter specified - need to test all keys
			// Use listValues to enumerate each unique value in the index and
			// test each individually.
			CmvautoPtr<IndexNav> lValEnum ; 
			RC rc=inSession->listValues(inClassID,STORE_INVALID_PROPID,VT_ANY, lValEnum.Get());
			if ( rc!= RC_OK ) 
			{ 
				outResultInfo << "Problem with listValues" << std::endl;
				return false; 
			}
		
			bool bIsRange=isRangeQuery(inSession,inClassID);
			int keycnt;
			for(keycnt=0;inMaxKeys==-1 || keycnt<inMaxKeys;keycnt++)
			{
				const Value *lVal = lValEnum->next();
				if(lVal==NULL) break; // Normal end of enumeration

				Value twoVals[2]; twoVals[0]=*lVal; twoVals[1]=*lVal;
				Value asRange; asRange.setRange(twoVals);
				const Value * param=bIsRange?&asRange:lVal;

				int cntQ;
				if (!testClassOrParameterizedFamily(inSession,inClassID,inRawQ,outResultInfo,cntQ,1,param))
				{
					result=false;
				}
				outCntQ+=cntQ;
			}
			if (keycnt==inMaxKeys)
			{
				// Not a failure, but need to warn that we did only a partial test
				outResultInfo << "Tested only the first " << inMaxKeys << " keys" << std::endl;
			}
		}
		else
		{
			outResultInfo << "Unsupported multi-param" << std::endl;
		}
		
		if (result)
		{
			outResultInfo << "Test Passed (" << outCntQ << " results)" << std::endl;
		}
		return result ;
	}

	/*! Print pids in the query, sorted by PID id.  (e.g. for debugging of small class or query scenarios)
	*/
	static bool printClass(
		IStmt* inQuery, 
		std::ostream & outResultInfo, 
		int maxPins=500,
		unsigned int inCntParams =0,				
		Value * inParams = NULL)
	{
		stringstream osConclude ; // so errors appear at the end

		// Use a map so results are sorted by PID
		TPidSet lFoundPIDs;
		populateSet(inQuery,lFoundPIDs,osConclude,maxPins,inCntParams,inParams);

		for( TPidSet::iterator it = lFoundPIDs.begin() ; it != lFoundPIDs.end() ; it++ )
		{
			outResultInfo << std::hex << (*it).pid << std::dec << std::endl;
		}

		outResultInfo << osConclude.str() << std::endl 
			<< "Printed " << (int)lFoundPIDs.size() << " pins" ;

		return true ;
	}

	static bool getClassFamilyValues(
		ClassID inClass, 
		ISession * inSession,
		vector<Value> & vals)
	{
		// Caller will need to freeValue on each returned value
		// Normally you would use listValues directly to avoid building full list
		// this can be a workaround to 14804

		vals.clear();

		CmvautoPtr<IndexNav> lValEnum ; 
		RC rc=inSession->listValues(inClass,STORE_INVALID_PROPID,VT_ANY, lValEnum.Get());
		if ( rc!= RC_OK ) return false;
		
		for(;;)
		{
			const Value *lVal = lValEnum->next();
			if(lVal==NULL) break;
			
			Value copy;
			rc = inSession->copyValue(*lVal,copy);
			if ( rc == RC_OK ) 
				vals.push_back(copy);
			else
				return false;
		}

		return true;
	}

	static bool printClassFamily(
		ClassID inClass, 
		ISession * inSession,
		std::ostream & outResultInfo, 
		bool printPIDs=true,
		int maxLines=500)
	{
		stringstream osConclude ; // so errors appear at the end

		CmvautoPtr<IndexNav> lValEnum ; 
		RC rc=inSession->listValues(inClass,STORE_INVALID_PROPID,VT_ANY, lValEnum.Get());
		if ( rc!= RC_OK ) 
		{ 
			outResultInfo << "listValues value traversal failed" << endl ; return false; 
		}
		
		int cntKeys=0,cntMatches=0;
		bool bNeedRange=isRangeQuery(inSession,inClass);

		int cntLine;
		for(cntLine=0;cntLine<maxLines;)
		{
			const Value *lVal = lValEnum->next();
			if(lVal==NULL) break; // Normal end of enumeration

			cntKeys++;

			printVal(inSession,lVal,outResultInfo,printPIDs);
			cntLine++;

			// Run family query based on this value
			CmvautoPtr<IStmt> q( inSession->createStmt() );
			ClassSpec r; 
			r.classID=inClass ;
			r.nParams=1 ;

			// OP_IN must have a VT_RANGE, so pass same value as start/end
			Value twoVals[2]; twoVals[0]=*lVal; twoVals[1]=*lVal;
			Value asRange; asRange.setRange(twoVals);

			if ( bNeedRange )
				r.params=&asRange;
			else
				r.params=lVal;

			q->addVariable(&r,1);

			// Short version
			if ( !printPIDs )
			{
				uint64_t cnt = 0 ;
				RC rc = q->count(cnt);
				if ( rc!=RC_OK )
				{
					outResultInfo << ",ERROR: QUERY FAILURE" << endl ; 
				}
				else
				{
					//comma sep to help get into excel
					//todo: for complex text e.g. with commas would have to sanitize with csv helper
					outResultInfo << "," << cnt << endl ; 
					cntMatches+=(int)cnt;
				}
			}
			else
			{
				// Use a map so results are sorted by PID
				TPidSet lFoundPIDs;
				populateSet(q,lFoundPIDs,osConclude,maxLines-cntLine,0,NULL);

				for( TPidSet::iterator it = lFoundPIDs.begin() ; it != lFoundPIDs.end() ; it++ )
				{
					outResultInfo << "\t" << std::hex << (*it).pid << std::dec << std::endl;
					cntLine++;
					cntMatches++;
				}
			}

		
			// Cleanup
			/*
			//????
			*/
		}

		if (cntLine<maxLines) 
		{
			outResultInfo << "Total: " << cntKeys << " keys, " << cntMatches << " pins" << endl;
		}
		
		outResultInfo << osConclude.str() << std::endl ;
		return true ;
	}

	static bool familyInfo(
		ClassID inClass, 
		ISession * inSession,
		PropertyID & outIndexedProp, // Which index it is based on
		uint8_t & outType,
		int & outKeyCnt,
		int & outPinRefCnt,
		bool & outisRange )
	{
		// Discover information about an family index

		CmvautoPtr<IndexNav> lValEnum ; 
		RC rc=inSession->listValues(inClass,STORE_INVALID_PROPID,VT_ANY, lValEnum.Get());
		if ( rc!= RC_OK ) 
		{ 
			return false; 
		}
		
		outType=VT_ANY; outIndexedProp=STORE_INVALID_PROPID;
		outKeyCnt=0; outPinRefCnt=0;
		outisRange=isRangeQuery(inSession,inClass);

		for(;;)
		{
			const Value *lVal = lValEnum->next();
			if(lVal==NULL) break; // Normal end of enumeration
			outKeyCnt++;

			if ( outKeyCnt==1 )
			{
				// Should be the same for ALL properties
				outIndexedProp=lVal->property;
				outType=lVal->type; // Note; also available via IStoreInspector::classInfo 
			}
		}

		{
			// Run family query based on this value
			CmvautoPtr<IStmt> q( inSession->createStmt() );
			ClassSpec r; 
			r.classID=inClass ;
			r.nParams=0 ;
			r.params=NULL;

			/*
			// Bug # 21045 : Index + all values would be returned if no param was passed to family query
			// OP_IN must have a VT_RANGE, so pass same value as start/end
			Value twoVals[2]; twoVals[0]=*lVal; twoVals[1]=*lVal;
			Value asRange; asRange.setRange(twoVals);

			if ( outisRange )
				r.params=&asRange;
			else
				r.params=lVal;
			*/
			q->addVariable(&r,1);

			// Short version
			uint64_t cnt = 0 ;
			RC rc = q->count(cnt);
			if ( rc!=RC_OK )
			{
				return false;
			}
			outPinRefCnt=(int)cnt;
		}	

		return true ;
	}


	/*! Fill a set with the pids returned by a query execution.  
	The set permits fast lookup of any particular pin by id, and enumeration in sorted order.
	*/
	static bool populateSet( 
		IStmt * q, 
		TPidSet & s, 
		std::ostream & err, 
		int maxPins, 
		unsigned int inCntParams =0,				
		const Value * inParams = NULL)
	{
		if (q==NULL) return false;

		s.clear();

		TPidSet::const_iterator result; 
		ICursor* lC = NULL;
		q->execute(&lC, inParams,inCntParams);
		CmvautoPtr<ICursor> lR(lC);

		if (!lR.IsValid())
		{
			err << "IStmt::Execute failed" << std::endl;
			return false ;
		}

		bool bRes=true;
		int pos=0;
		while (true)
		{
			PID p ;
			if ( RC_OK != lR->next(p) )
			{
				break;
			}
			result = s.find(p);
			if( result != s.end())
			{
				err << "Duplicate pin appeared " << std::hex << p.pid << std::dec 
					<< " (position " << pos << ")" << std::endl;
				bRes=false;
			}
			else
			{
				s.insert(p);			
			}
			pos++;
			if ( pos >= maxPins )
			{
				// Not really error
				err << "Reached maximum pids " << maxPins << std::endl;
				break ;
			}
		}
		return bRes;
	}

	static void printVal(ISession* inSession, const Value * v, std::ostream & outStream, bool endline=false )
	{
		MvStoreSerialization::ContextOutDbg lSerCtx(outStream, inSession, 100);
		MvStoreSerialization::OutDbg::valueContent(lSerCtx, *v, v->length);		
		if ( endline ) outStream<<endl;
	}

private:
	/*!  Compare two sets of pins.  
	Both sets are expected to be equal and belong to class "inClassID"
	The operation should be described in msg.
    If any divergence found the function returns false and the problem is described in the stringstream.
	*/
	static bool compareSet( 
		const TPidSet & A, 
		const TPidSet & B, 
		const char * msg,
		ClassID inClassID,
		ISession* inSession,
		bool bFamilyPins,
		std::ostream & outResultInfo )
	{
		// Find any pins in set A that are missing from set B
		// msg contains error to describe the situation if such a pin is found
		// outResultInfo concatenated with details about such missing PIDs

		// REVIEW: a single pass (or calling == operator) would be faster 
		// but we want full details

		bool bRes=true;
		int cntBadPins=0;

		for( TPidSet::const_iterator it = A.begin() ; it != A.end() ; it++ )
		{
			TPidSet::const_iterator lookup=B.find(*it);
			if (lookup!=B.end())
			{
				continue ;
			}

			// This PID is in A but not B
			outResultInfo << std::hex << (*it).pid << std::dec << msg ;

			// Get info about what might be wrong
			CmvautoPtr<IPIN> missingPIN(inSession->getPIN(*it));
			if ( !missingPIN.IsValid() )
			{
				outResultInfo << " Load: FAIL";
			}
			else
			{
				if (bFamilyPins)
				{
					// testClassMembership only works for class
					outResultInfo << " (Load: OK)";
				}
				else if (!missingPIN->testClassMembership(inClassID))
				{
					outResultInfo << " testClassMembership: FAIL";
				}
				else
				{
					outResultInfo << " (Load: OK, testClassMembership: OK)";
				}
			}
			outResultInfo << std::endl;
			bRes=false;
			cntBadPins++;
			if(cntBadPins>MAX_MISMATCH_PINS_TO_PRINT)
			{
				outResultInfo << "Only showing first " << MAX_MISMATCH_PINS_TO_PRINT << " problem pids..." << endl;
				break ;
			}
		}
		return bRes ;
	}


	/*! Direct test of class if the Raw Query already available
	*/
	static bool testClassOrParameterizedFamily(
		ISession *inSession,
		ClassID inClassID,
		IStmt* inRawQ,
		std::ostream & outResultInfo,
		int &outCntQ,
		unsigned int inCntParams =0,				
		const Value * inParams = NULL)
	{
		bool result=true;
		outCntQ=0;

		// Normal case - class or class family with specific key value

		CmvautoPtr<IStmt> lClassQ(getQueryUsingClass(inSession,inClassID,STMT_QUERY,inCntParams,inParams)) ;

		uint64_t lCntRaw = 0 ; 
		uint64_t lCntQ = 0 ;
		RC rc = inRawQ->count(lCntRaw, inParams, inCntParams) ;
		if ( rc != RC_OK )
		{
			outResultInfo << "IStmt::count failed on class definition query" << endl ;
			char * lQueryAsString = inRawQ->toString() ;
			if ( lQueryAsString != NULL ) 
			{
				outResultInfo << lQueryAsString << endl;
				inSession->free(lQueryAsString);
			}

			result =false ;
		}

		rc = lClassQ->count( lCntQ ) ;
		if ( rc != RC_OK )
		{
			outResultInfo << "IStmt::count failed on class query " << endl ;
			char * lQueryAsString = lClassQ->toString() ;
			if ( lQueryAsString != NULL ) 
			{
				outResultInfo << lQueryAsString << endl;
				inSession->free(lQueryAsString);
			}

			result = false ;
		}
		else
		{
			outCntQ = (int)lCntQ ; 
		}

		if ( lCntQ != lCntRaw )
		{
			outResultInfo << "IStmt::count returned different results: Raw " << lCntRaw << " Class: " << lCntQ << std::endl;
			result = false ; // Keep going to find difference
		}

		// Use these sets to sort the PIDs and catch duplicates
		TPidSet classPIDs;
		TPidSet rawPIDs;

		stringstream errClass;
		if (!populateSet( lClassQ, classPIDs, errClass, MAX_PINS_FOR_CLASS_TEST))
		{
			outResultInfo << "Problem enumerating class pins..." << endl ;	
			outResultInfo << errClass.str();
			result=false;
		}

		stringstream errRaw;
		if (!populateSet( inRawQ, rawPIDs, errRaw, MAX_PINS_FOR_CLASS_TEST,inCntParams, inParams))
		{
			outResultInfo << "Problem enumerating raw pins..." << endl ;	
			outResultInfo << errClass.str();
			result=false;
		}

		if (classPIDs.size() != rawPIDs.size() )
		{
			outResultInfo << "IStmt::execute mismatch: Class has " << (int) classPIDs.size()
				<< " pins, Raw query: " << (int)rawPIDs.size() << std::endl;
			result=false;
		}

		if ( classPIDs.size() != MAX_PINS_FOR_CLASS_TEST && classPIDs != rawPIDs)
		{
			// look for any mismatching pins
			if (!compareSet( classPIDs, rawPIDs, " Class:OK Raw:MISSING",inClassID, inSession,inCntParams>0,outResultInfo ))
				result=false;
			if (!compareSet( rawPIDs, classPIDs, " Raw:OK Class:MISSING",inClassID, inSession,inCntParams>0,outResultInfo ))
				result=false;
		}

		if ( !result && inCntParams == 1 && inParams != NULL )
		{
			outResultInfo << "Query Parameter ";
			printVal(inSession,inParams,outResultInfo,true);
		}

		return result ;
	}


} ;


#endif
