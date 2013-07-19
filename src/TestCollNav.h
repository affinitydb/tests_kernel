/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#ifndef __TEST_COLLNAV_H
#define __TEST_COLLNAV_H

#include "mvstoreapi.h"

class CTestCollNav:
	public INav
{
public:
	//constructor
	CTestCollNav(Value *list,unsigned int num)
		:valList(list),iNumVals(num),iCurrIdx(0)
	{};


	/*overloaded method*/
	virtual	const Value	*navigate(GO_DIR op,ElementID eid=STORE_COLLECTION_ID)
	{
		const Value *retVal = NULL;
		if(iNumVals > 0)
		{
			switch(op)
			{
			case GO_FIRST:
				iCurrIdx = 0;
				break;
			case GO_LAST:
				iCurrIdx = (iNumVals - 1);
				break;
			case GO_NEXT:
				if(iNumVals > iCurrIdx + 1)
				{
					iCurrIdx++;
				}
				else return NULL;
				break;
			case GO_PREVIOUS:
				if(iCurrIdx > 0)
				{
					iCurrIdx++;
				}
				else return NULL;
				break;
			case GO_FINDBYID:
				if(iNumVals > eid)
				{
					iCurrIdx = eid;
				}
				else return NULL;
				break;
			default:
				return NULL;
			};
			retVal = &(valList[iCurrIdx]);
		}
		return retVal;
	};
	virtual	ElementID getCurrentID()
	{
		return iCurrIdx;
	};
	virtual	const Value	*getCurrentValue()
	{
		const Value	*retVal = NULL;
		if(iNumVals > 0 && iNumVals >iCurrIdx)
		{
			retVal = &(valList[iCurrIdx]);
		}
		return retVal;
	};
	virtual	RC	getElementByID(ElementID eid,Value& val)
	{
		RC retVal = RC_FALSE;
		if(iNumVals > 0 && iNumVals >eid)
		{
			val = (valList[eid]);
			retVal = RC_OK;
		}
		return retVal;
	};
	virtual	INav *clone() const
	{
		INav *retVal = NULL;
		Value *tmpVal = new Value[iNumVals];
		for(unsigned int idx = 0;idx < iNumVals;idx++)
		{
			tmpVal[idx] = valList[idx];
		}
		retVal = new CTestCollNav(valList,iNumVals);
		return retVal;
	};
	virtual unsigned count() const
	{
		return iNumVals;
	};
	virtual	void destroy()
	{
		delete this;
	};
private:
	//destructor
	virtual ~CTestCollNav()
	{		
	};

	Value *valList;
	unsigned int iNumVals;
	unsigned int iCurrIdx;
};
#endif
