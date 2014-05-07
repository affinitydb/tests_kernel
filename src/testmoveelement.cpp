/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

//
// Test for moving elements of a collection (OP_MOVE, OP_MOVE_BEFORE)
//

#include "app.h"
#include "mvauto.h" // MV auto pointers
#include "collectionhelp.h"			// If reading collections
#include "teststream.h"				// If using streams
using namespace std;

class TestMoveElement : public ITest
{
	public:
		TEST_DECLARE(TestMoveElement);

		// By convention all test names start with test....
		virtual char const * getName() const { return "testmoveelement"; }
		virtual char const * getDescription() const { return "Move element"; }
		virtual char const * getHelp() const { return ""; } // Optional
		
		virtual int execute();
		virtual void destroy() { delete this; }	
	protected:
		void doTest() ;
		bool verifyExpected(IPIN* inPin, PropertyID prop, int cnt, ElementID * inExpOrdering, ElementID e0);
	private:	
		ISession * mSession ;
};
TEST_IMPLEMENT(TestMoveElement, TestLogger::kDStdOut);

void move( Value & v, PropertyID prop, ElementID which, ElementID pivot, uint8_t op=OP_MOVE )
{
	v.set((unsigned int)pivot); v.eid=which; v.op = op; v.property=prop;
}

int TestMoveElement::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }
	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;
	doTest() ;
	mSession->terminate() ;
	MVTApp::stopStore() ;
	return RC_OK ;
}

void TestMoveElement::doTest()
{
	PropertyID prop=MVTUtil::getProp(mSession,"testmoveelement");

	// Create original pin, which has a collection with 9 elements
	static const int cntElements=9;

	Afy::Value lVal[cntElements];
	for(int idx= 0 ; idx < cntElements ; idx++)
	{
		lVal[idx].set(idx); // Value matches the original position in the array
		lVal[idx].op = OP_ADD;
		lVal[idx].setPropID(prop);
	}

	IPIN *lNewPIN;
	TVERIFYRC(mSession->createPIN(lVal, cntElements,&lNewPIN,MODE_COPY_VALUES|MODE_PERSISTENT));
	lNewPIN->refresh();

	ElementID e[cntElements]; // Eids in original ordering
	MvStoreEx::CollectionIterator it(lNewPIN,prop);

	// Remember the eids
	const Value * v; int pos=0;
	for ( v = it.getFirst(); v!=NULL ; v = it.getNext() )
	{
		e[pos] = v->eid;
		TVERIFY(e[pos]==e[0]+pos); // Verify an assumption of the tests that 
									  // eids are sequentially assigned
		pos++;
	}

	// Sanity check of helper function
	TVERIFY(verifyExpected(lNewPIN,prop,cntElements,e,e[0]));

	//
	// Case 1 - single move
	// 

	{
		mLogger.out() << "Case 1" << endl;
		CmvautoPtr<IPIN> clone(lNewPIN->clone(0,0,MODE_PERSISTENT));

		// Move e[4] BEFORE STORE_FIRST_ELEMENT
		move(lVal[0],prop,e[4],STORE_FIRST_ELEMENT,OP_MOVE_BEFORE);
		TVERIFYRC(clone->modify(lVal,1));
		clone->refresh();
		
		ElementID expect[] = { e[4], e[0], e[1], e[2], e[3], e[5], e[6], e[7], e[8]};
		TVERIFY(verifyExpected(clone,prop,cntElements,expect,e[0]));
	}

	//
	// Case 2 - swap first elements
	// 

	{
		mLogger.out() << "Case 2" << endl;
		CmvautoPtr<IPIN> clone(lNewPIN->clone(0,0,MODE_PERSISTENT));

		// Move e[0] AFTER e[1]
		move(lVal[0],prop,e[0],e[1],OP_MOVE);
		TVERIFYRC(clone->modify(lVal,1));
		clone->refresh();		
		ElementID expect[] = { e[1], e[0], e[2], e[3], e[4], e[5], e[6], e[7], e[8]};
		TVERIFY(verifyExpected(clone,prop,cntElements,expect,e[0]));
	}

	//
	// Case 3 - 2 step, returns back 
	// 

	{
		mLogger.out() << "Case 3" << endl;
		CmvautoPtr<IPIN> clone(lNewPIN->clone(0,0,MODE_PERSISTENT));

		// Move e[0] AFTER e[1] and back again!
		move(lVal[0],prop,e[0],e[1],OP_MOVE);		
		move(lVal[0],prop,e[1],e[0],OP_MOVE);
		
		TVERIFYRC(clone->modify(lVal,1));

		clone->refresh();		
		TVERIFY(verifyExpected(clone,prop,cntElements,e,e[0]));
	}

	//
	// Case 4 - 2 step, returns back, variation 2
	// 

	{
		mLogger.out() << "Case 4" << endl;
		CmvautoPtr<IPIN> clone(lNewPIN->clone(0,0,MODE_PERSISTENT));

		// Move e[0] AFTER e[1] and back again!
		move(lVal[0],prop,e[0],e[1],OP_MOVE);		
		move(lVal[0],prop,e[0],e[1],OP_MOVE_BEFORE);
		
		TVERIFYRC(clone->modify(lVal,1));

		clone->refresh();		
		TVERIFY(verifyExpected(clone,prop,cntElements,e,e[0]));
	}

	//
	// Case 5 Yasir scenario (05, 01 , 09, 04,  03 , 02, 08, 07, 06)
	// which I've reworked to be base 00 (04, 00 , 08, 03,  02 , 01, 07, 06, 05)
	//
	{
		mLogger.out() << "Case 5" << endl;
		CmvautoPtr<IPIN> clone(lNewPIN->clone(0,0,MODE_PERSISTENT));

		move(lVal[0],prop,e[4],STORE_FIRST_ELEMENT,OP_MOVE_BEFORE); // 4 before 0
		move(lVal[1],prop,e[3],e[0]);	// 3 after 0
		move(lVal[2],prop,e[8],e[0]);	// 8 after 0 (e.g. between 0 and 3)
		move(lVal[3],prop,e[5],e[2]);	
		move(lVal[4],prop,e[6],e[2]);	
		move(lVal[5],prop,e[7],e[2]);	
		move(lVal[6],prop,e[1],e[2]);	

		TVERIFYRC(clone->modify(lVal,7));
		clone->refresh();		
		ElementID expect[] = { e[4], e[0], e[8], e[3], e[2], e[1], e[7], e[6], e[5]};
		TVERIFY(verifyExpected(clone,prop,cntElements,expect,e[0]));
	}
}

bool TestMoveElement::verifyExpected(IPIN* inPin, PropertyID prop, int cnt, ElementID * inExpOrdering, ElementID e0)
{
	// Verify that the collection has size cnt and elements in ordering as specified
	// (Assumes that the value of each item also matches the ElementID + base element ID e0)

	bool bRet = true;

	stringstream str;
	str << "Expected\t\tGot" << endl;

	MvStoreEx::CollectionIterator it(inPin,prop);

	const Value * v; int pos=0;
	for ( v = it.getFirst(); v!=NULL ; v = it.getNext() )
	{
		str << hex << inExpOrdering[pos] << "\t\t" << v->eid << dec ;
		if ( v->eid != inExpOrdering[pos])
		{
			bRet = false;
			str << "\tMISMATCH";
		}

		TVERIFY( v->type == VT_INT );

		if ( ElementID(v->i) != v->eid - e0 )
		{
			bRet=false;
			str << " Bad value" << v->i ;
		}

		str << endl;

		pos++;
		if (pos > cnt)
		{
			str<<"Too many elements on pin"<<endl;
			bRet=false;
			break;
		}
	}

	if ( !bRet || isVerbose() )
	{
		mLogger.out() << str.str() ;
	}

	return bRet;
}
