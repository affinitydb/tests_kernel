/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

/*Massive classification
Verify that nothing wrong happens when say 5000 (COUNT_CLASS) classes are present. 
Verify that nothing wrong happens when large quantities of pins are classified by say 200 (run test with -largeclass=200) classes. 
N.b. this kind of test can also be repurposed for the benchmarks, but here the focus is to make sure that it works. */
//tests accepts a parameter -largeclass which is the number of massive classes that will have all the pins. Default is zero

#include "app.h"
#include "mvauto.h"
#define COUNT_PROP 5000
#define COUNT_CLASS 5000
#define COUNT_PIN 10000

class testmassiveclassification : public ITest
{
public:
	TEST_DECLARE(testmassiveclassification);
	virtual char const * getName() const { return "testmassiveclassification"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "Testing performance when huge number of classes defined: use -largeclass parameter to define the numeber of huge classes"; }
	virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "More of a perf test..."; return false; }		
	virtual bool includeInPerfTest() const { return true; }
	virtual int execute();
	virtual void destroy() { delete this; }
	void createProps(int PropCount);
	void defineClasses(int ClassCount);
	void createPins(int PinCount);
	void printResults(void);
	public:
	PropertyID	mProp[COUNT_PROP] ;
	ISession * mSession ;
	int mLargeClass;
	long  mDefineTime,mPinTimeTotal;
	long  mPinTime [(COUNT_PIN/1000)+1];
};
TEST_IMPLEMENT(testmassiveclassification, TestLogger::kDStdOut);

int testmassiveclassification::execute()
{
	if (!MVTApp::startStore()) {mLogger.print("Failed to start store\n"); return RC_NOACCESS;}
	mSession = MVTApp::startSession();
	mLargeClass=0;

	if(mpArgs->get_param("largeclass",mLargeClass))
	{
		if(mLargeClass>COUNT_CLASS)
		{
			mLogger.out()<<"Invalid parameter value. -largeclass should not be greater than "<<COUNT_CLASS<<"running with default values";
			mLargeClass=0;
		}
		mLogger.out()<<endl<<mLargeClass<<" classes will have all the pins";
	}
	
	createProps(COUNT_PROP);
	defineClasses(COUNT_CLASS);
	createPins(COUNT_PIN);
	printResults();

	mSession->terminate();
	MVTApp::stopStore();
	return RC_OK;
}

void testmassiveclassification::createProps(int PropCount)
{
	int i;
	std::cout<<endl<<"Mapping "<<COUNT_PROP<<" properties."<<std::flush;
	for(i=0;i<PropCount;i++)		
	{	
		char lB[64]; sprintf(lB, "%d.massive.property", i);
		mProp[i]=MVTUtil::getPropRand(mSession,lB);
		if(!(i%100))
			std::cout<<"."<<std::flush;
	}
}

void testmassiveclassification::defineClasses(int ClassCount)
{
	string strRand ; 
	char * className;
	int i;
	mLogger.out()<<endl<<"Defining "<<COUNT_CLASS<< " classes."<<std::flush;
	long const lBef = getTimeInMs();
	for(i=0;i<ClassCount;i++)
	{
		MVTRand::getString( strRand, 20, 0 ) ;
		char lB[64]; sprintf(lB, "%d.massive.class.%s", i,strRand.c_str());
		className=lB;
		
		IStmt *lClassQ=mSession->createStmt() ;
		unsigned char lVar = lClassQ->addVariable() ;
		Value ops[1] ; 
		ops[0].setVarRef(0, mProp[i] ) ;
		IExprNode *lE=mSession->expr(OP_EXISTS, 1, ops ) ;
		lClassQ->addCondition( lVar, lE ) ;

		TVERIFYRC(defineClass(mSession, lB, lClassQ ));
		lClassQ->destroy();
		lE->destroy();
		if(!(i%100))
			std::cout<<"."<<std::flush;
	}
	mDefineTime = getTimeInMs()-lBef ;
	mLogger.out()<<endl<<"Defining "<<COUNT_CLASS<< " classes done!";
}

void testmassiveclassification::createPins(int PinCount)
{
	int i,j;
	//Value tValue[mLargeClass+1];
	Value* tValue = NULL; 
	tValue = (Value*) mSession->malloc(sizeof(Value)*(mLargeClass+1));
	
	std::cout<<endl<<"Creating "<<PinCount<< " pins."<<std::flush;
	long lBef = getTimeInMs();
	for(i=0;i<PinCount;i++)
	{
		for(j=0;j<mLargeClass;j++)
		{
			tValue[j].set(j);
			tValue[j].property=mProp[j];
		}
		tValue[j].set(j);
		tValue[j].property=mProp[j+(i%COUNT_PROP-mLargeClass)];

		TVERIFYRC(mSession->createPIN(tValue,j+1,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));	
		if(!(i%100))
			std::cout<<"."<<std::flush;
		if(!((i+1)%1000))
			mPinTime[(i+1)/1000] = getTimeInMs()-lBef ; //Calculating time taken for each 1000 pins
	}
	mPinTimeTotal = getTimeInMs()-lBef;
	std::cout<<endl<<"Creating "<<PinCount<< " pins : done!";
	mSession->free(tValue); 
}

void testmassiveclassification::printResults(void)
{
	int i;
	mLogger.out()<<endl<<"Time taken for defining "<<COUNT_CLASS<<" classes (in ms) :"<<mDefineTime;
	mLogger.out()<<endl<<"Time taken for creating pins"<<endl;
	for(i=1;i<(COUNT_PIN/1000)+1;i++)
	{
		if (i==1)
			mLogger.out()<<endl<<"Time taken for "<<(i-1)*1000<<" - "<<i*1000<<" pins (in ms) :"<<mPinTime[i];
		else
			mLogger.out()<<endl<<"Time taken for "<<(i-1)*1000<<" - "<<i*1000<<" pins (in ms) :"<<mPinTime[i]-mPinTime[i-1];
	}
	mLogger.out()<<endl<<"----------------------------------------------";
	mLogger.out()<<endl<<"Total Time taken for "<<COUNT_PIN<<" pins (in ms) :"<<mPinTimeTotal;

	mLogger.out()<<endl;
}
