/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "teststream.h"
using namespace std;

// This multi-threaded test bashes on properties including collections
// by randoming generating and changing values on a set of pins

class TestProps; 

struct ThreadInfoProps
{	
	MVStoreKernel::StoreCtx *mStoreCtx;
	unsigned int mSeed;
	TestProps * pTest;
	MVStoreKernel::Mutex *lock;
	MVStoreKernel::Event *start;
	MVStoreKernel::Event *finish;
	bool &fStarted;
	long &counter;
	PropertyID *mPropIDs;
};

static const int sNumProps = 111;           //total number of properties;  //(REDUCE TO 2 to force overlapping properties)
static const int sNumPropInd = sNumProps-1; //max index within array, counting from 0; 

// Publish this test.
class TestProps : public ITest
{
		MVStoreKernel::StoreCtx *mStoreCtx;		
	public:
		TEST_DECLARE(TestProps);
		virtual char const * getName() const { return "testprops"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Mark's basic test for properties"; }

		virtual bool includeInPerfTest() const { return true; }
		virtual bool includeInBashTest(char const *& pReason) const { pReason = "Locks most of the resources..."; return false; }

		
		virtual int execute();
		virtual void destroy() { delete this; }
                
        RC addPINTag(IPIN *ppin);
		void genValue(const IPIN * const *PINs,int nPINs,Value& val, PropertyID pid=STORE_INVALID_PROPID, bool fCElt=false, ISession *pSession = NULL);
		virtual bool  mapURIs(MVStoreKernel::StoreCtx *);
		virtual bool  removePins(); 
	public:
		PropertyID mPropIDs[sNumProps];      //properties array, index starts with 0; 
};

TEST_IMPLEMENT(TestProps, TestLogger::kDStdOut);

// Implement this test.
#define	NTHREADS 30
#define	NOPS 1000
static THREAD_SIGNATURE threadProc(void * pParam);
enum OPS {POP_CREATE, POP_DELETE, POP_ADDPROP, POP_UPDPROP, POP_DELPROP, POP_ADDCELT, POP_UPDCELT, POP_DELCELT, POP_ALL};

/*
 * The purpose of the function below - to add a value to each pin with the known 
 * property ID. It will be used later to determine all pins added during the test 
 * in order to remove them. 
 */
RC TestProps::addPINTag(IPIN *ppin)
{
  Value val;
  RC result;
   
  val.set("Tag");  //VT_TYPE, and value length are automatically set... 
  val.setPropID(mPropIDs[sNumPropInd]); 
 
  result =  ppin->modify(&val,1);
  TVERIFY2(RC_OK == result, "Did I added my Tag Value properly?"); 
 
  const Value * pValLookup = ppin->getValue( mPropIDs[sNumPropInd]) ;
  
  TVERIFY2( NULL != pValLookup,"Did I managed to read back my tag properly?"); 

  TVERIFY(0 == strcmp( pValLookup->str, "Tag"));

  return RC_OK;
 }

/*
 * The mapURIs function is going to be called at the begining of the tests; 
 * It will register the array of properties within the store; 
 * Laterly, each thread, while creating pins, will pick up randomly a property from that array;
 * Be noted, that the last property is reserved for tagging each pin, in order to delete them all  at 
 * the end of the test...  
 */
bool TestProps::mapURIs(MVStoreKernel::StoreCtx * pctx)
{
	ISession *ses = MVTApp::startSession(pctx);
    
        TVERIFY2(ses!=NULL,"Failed to create session while mapping properties!");
	
	MVTApp::mapURIs(ses,"TestProps.prop",sNumProps,mPropIDs);

	ses->terminate();
	return true;
}

/*
 * At the end of the test, I am going to find and remove all PINS,
 * marked as "TAG"
 */
bool TestProps::removePins()
{
	ISession *ses = MVTApp::startSession(mStoreCtx);

        TVERIFY2(ses!=NULL,"Failed to create session while removing PINs!");

        IStmt * Query = ses->createStmt(STMT_DELETE);
        unsigned char v = Query->addVariable();

        Query->setPropCondition(v,&mPropIDs[sNumPropInd],1); 
        TVERIFYRC(Query->execute(NULL,NULL,0,~0,0,MODE_PURGE));
        
        Query->destroy();	
        
	ses->terminate();
        
	return true; 
}

int TestProps::execute()
{
	if (MVTApp::startStore())
	{
		mStoreCtx = MVTApp::getStoreCtx();
		mapURIs(mStoreCtx); // I am going to register the array of properties. They are good for the store life... 
		
		MVStoreKernel::Mutex lock;
		MVStoreKernel::Event start;
		MVStoreKernel::Event finish;
		bool fStarted = false;
		long counter = 0;
		ThreadInfoProps lInfo = {mStoreCtx, mRandomSeed,this, &lock, &start, &finish, fStarted, counter, mPropIDs};
        for (counter = 0; counter<NTHREADS; counter++) 
		{
			HTHREAD lThread; 
			if (createThread(threadProc, &lInfo, lThread)!=RC_OK) 
				break;
		}

		TIMESTAMP startTime; getTimestamp(startTime);

		lock.lock();
		fStarted = true;
		start.signalAll();
		while (counter!=0) finish.wait(lock,0);
		lock.unlock();


		TIMESTAMP endTime; getTimestamp(endTime);
		mLogger.out() << "Time: " << double(endTime-startTime)/1000000. << " sec" << std::endl;
                
		removePins();  
		MVTApp::stopStore();
	}
	else { TVERIFY(!"Unable to start store"); }
	return 0;
}

void TestProps::genValue(const IPIN * const *PINs,int nPINs,Value& val, PropertyID pid, bool fCElt, ISession *pSession)
{
	if (pid==STORE_INVALID_PROPID) pid = mPropIDs[rand()%sNumPropInd]; unsigned char *ubuf; char *str;
	val.type = (ValueType)(rand()%(VT_ARRAY+1)); val.eid=STORE_COLLECTION_ID;
	if (fCElt && val.type==VT_ARRAY) val.type=VT_STRING;
	char buf[200]; unsigned len = rand()%(sizeof(buf)-1),i; Value *vals;
	switch (val.type) {
	case VT_REFID: {
		PID id;
		id.pid = (uint64_t)rand()<<48|rand();
		id.ident = (IdentityID)rand();
		if (rand()<RAND_MAX/5) {val.set(id); break;}
	}
	case VT_REF:
		if (nPINs>0) {val.set(PINs[rand()%nPINs]->getPID()); break;}
	default:
	case VT_STRING:
		for (i=0; i<len; i++) buf[i] = (char)(rand()%(0x7f-' ')+' ');
		buf[len]=0; str = (char*)pSession->alloc(sizeof(char)*(len+1)); strcpy(str, buf);
		val.set(str); break;
	case VT_BSTR:
		for (i=0; i<len; i++) buf[i] = (char)rand();
		ubuf = (unsigned char*)pSession->alloc(len); memcpy(ubuf,buf,len);
		val.set(ubuf,len); break;
	case VT_URL:
		for (i=0; i<len; i++) buf[i] = (char)(rand()%(0x7f-' ')+' ');
		buf[len]=0; str = (char*)pSession->alloc(sizeof(char)*(len+1)); strcpy(str, buf); 
		val.setURL(str); break;
	case VT_INT: val.set((int)rand()); break;
	case VT_UINT: val.set((unsigned)rand()); break;
	case VT_INT64: val.setI64((int64_t)rand()); break;
	case VT_UINT64: val.setU64((uint64_t)rand()); break;
	case VT_FLOAT: val.set((float)rand()); break;
	case VT_DOUBLE: val.set((double)rand()); break;
	case VT_BOOL: val.set((rand()&1)!=0); break;
	case VT_DATETIME: val.setDateTime((uint64_t)rand()); break;
	case VT_INTERVAL: val.setInterval((int64_t)rand()); break;
	case VT_ARRAY:
		len=rand()%30+2; vals=(Value*)pSession->alloc(len*sizeof(Value));
		for (i=0; i<len; i++) {genValue(PINs,nPINs,val,pid,true,pSession); vals[i]=val;}
		val.set(vals,len);
		break;
	case VT_STREAM:
		unsigned int streamlen = rand() % 100000;
		assert(pSession != NULL);
		IStream *stream = new TestStringStream(streamlen,VT_STRING);
		val.set(MVTApp::wrapClientStream(pSession, stream));
		break;
	}
	val.setPropID(pid);
}

static void freeValue(ISession *pSes, Value& v)
{
	switch (v.type) {
	default: break;
	case VT_STRING: case VT_BSTR: case VT_URL:
		pSes->free((void*)v.str); break;
	case VT_ARRAY:
		if (v.varray!=NULL) {
			for (ulong i=0; i<v.length; i++) freeValue(pSes, const_cast<Value&>(v.varray[i]));
			pSes->free(const_cast<Value*>(v.varray));
		}
		break;
         }
}

static bool findCollection(const IPIN * const *PINs,int nPINs,int &idx,unsigned int &propIdx)
{
	for (int i=rand()%nPINs,j=0; j<nPINs; ++j,i=(i+1)%nPINs) {
		const IPIN *pin = PINs[i]; unsigned nProps = pin->getNumberOfProperties();
		if (nProps>0) for (unsigned k=rand()%nProps,l=0; l<nProps; ++l,k=(k+1)%nProps) {
			const Value *val = pin->getValueByIndex(k);
			if (val!=NULL && val->type==VT_ARRAY) {idx=i; propIdx=k; return true;}
		}
	}
	return false;
}

static unsigned int prevSeed = 0;

static THREAD_SIGNATURE threadProc(void * pInfo)
{
	#ifndef WIN32
		pthread_detach(pthread_self());
	#endif
	ThreadInfoProps *lInfo = (ThreadInfoProps*) pInfo;
	
	ISession *ses = MVTApp::startSession(lInfo->mStoreCtx);
	if (!ses)
	{
		printf("Failed to create session!\n");
		return 0;
	}
	assert(ses!=NULL);

	unsigned int seed = lInfo->mSeed;
	if (seed<=prevSeed) seed=prevSeed+1; prevSeed=seed;
	srand(seed);

	// Each thread words on its own set of PINs
	IPIN *PINs[1024]; int nPINs = 0; Value *values = (Value*)ses->alloc(20*sizeof(Value)); PID pid; const Value *val;

	lInfo->lock->lock();
	while (!lInfo->fStarted) lInfo->start->wait(*lInfo->lock,0);

	lInfo->lock->unlock();
	for (int i=0; i<NOPS; i++) {
		int op = nPINs==0? POP_CREATE : rand()%POP_ALL, npin;
		if (op==POP_CREATE && nPINs==1024) op++; else if (op==POP_DELETE && nPINs<5) op--;
		IPIN *pin = op==POP_CREATE ? NULL : PINs[npin = rand() % nPINs];
		unsigned int idx=0, j;
		if (pin!=NULL) if ((idx=pin->getNumberOfProperties())==0) op=POP_ADDPROP; else idx=rand()%idx;
		switch (op) {
		case POP_CREATE:
			idx=MVTRand::getRange(1,19);
			for (j=0; j<idx; j++) {
				lInfo->pTest->genValue(PINs,nPINs,values[j],STORE_INVALID_PROPID,false,ses);

				//Attempt to force collection case
				values[j].op = OP_ADD; values[j].eid=STORE_FIRST_ELEMENT; // In case same property was picked more than once
			}
			if (RC_OK == ses->createPIN(pid, values,idx) && (pin = ses->getPIN(pid))!=NULL){ 
			        /*
			         * I'm planning to add 'TAG' string value to each CREATED PIN
			         * Later, I will find all those PINS in order to delete them... 
			         */
			        lInfo->pTest->addPINTag(pin);  
			        PINs[nPINs++] = pin;
			}
			for (j=0; j<idx; j++) freeValue(ses,values[j]);
			break;
		case POP_DELETE:
			TVRC_R(ses->deletePINs(&pin,1),lInfo->pTest);
			if (npin<--nPINs) memmove(&PINs[npin],&PINs[npin+1],(nPINs-npin)*sizeof(IPIN*));
			break;
		case POP_ADDPROP:
			if ( (int)pin->getNumberOfProperties() < sNumPropInd )
			{
				lInfo->pTest->genValue(PINs,nPINs,values[0],STORE_INVALID_PROPID,false,ses); values[0].setOp(OP_ADD);
				while (pin->getValue(values[0].property)!=NULL) values[0].property = lInfo->pTest->mPropIDs[rand()%sNumPropInd]; 
				TVRC_R(pin->modify(&values[0],1),lInfo->pTest); freeValue(ses,values[0]);
			}
			break;
		case POP_ADDCELT:
			if (findCollection(PINs,nPINs,npin,idx) && (val=(pin=PINs[npin])->getValueByIndex(idx))!=NULL) {
				assert(val->type==VT_ARRAY);
				lInfo->pTest->genValue(PINs,nPINs,values[0],val->property,true,ses); 
				values[0].setOp(OP_ADD_BEFORE); values[0].eid=STORE_FIRST_ELEMENT;
				TVRC_R(pin->modify(&values[0],1),lInfo->pTest); freeValue(ses,values[0]); break;
			}
			if ((val=pin->getValueByIndex(idx))!=NULL) {
				lInfo->pTest->genValue(PINs,nPINs,values[0],val->property,true,ses); values[0].setOp(OP_ADD);
				TVRC_R(pin->modify(&values[0],1),lInfo->pTest); freeValue(ses,values[0]);
			}
			break;
		case POP_UPDCELT:
			if (findCollection(PINs,nPINs,npin,idx) && (val=(pin=PINs[npin])->getValueByIndex(idx))!=NULL) {
				assert(val->type==VT_ARRAY);
				lInfo->pTest->genValue(PINs,nPINs,values[0],val->property,true,ses); 
				values[0].setOp(OP_SET); values[0].eid=val->varray[rand()%val->length].eid;
				TVRC_R(pin->modify(&values[0],1),lInfo->pTest); freeValue(ses,values[0]); break;
			}
		case POP_UPDPROP:
			{
				uint32_t cprops=pin->getNumberOfProperties();
				if (cprops>1)
				{
					// Find any random property to update, but not the tag property
					for(;;) {
						idx=MVTRand::getRange(0,cprops-1);
						val=pin->getValueByIndex(idx);
						if (val->property!=lInfo->pTest->mPropIDs[sNumPropInd]) break;
					}
					lInfo->pTest->genValue(PINs,nPINs,values[0],val->property,val->type==VT_ARRAY,ses);
					values[0].setOp(OP_SET); values[0].eid=STORE_COLLECTION_ID;
					TVRC_R(pin->modify(&values[0],1),lInfo->pTest); freeValue(ses, values[0]);
				}
			}
			break;
		case POP_DELCELT:
			if (findCollection(PINs,nPINs,npin,idx) && (val=(pin=PINs[npin])->getValueByIndex(idx))!=NULL) {
				assert(val->type==VT_ARRAY); 
				values[0].setDelete(val->property,val->varray[rand()%val->length].eid);
				TVRC_R(pin->modify(&values[0],1),lInfo->pTest); break;
			}
		case POP_DELPROP:
			if (((val=pin->getValueByIndex(idx))!=NULL) && (val->property != lInfo->pTest->mPropIDs[sNumPropInd])){ 
				values[0].setDelete(val->property); TVRC_R(pin->modify(&values[0],1),lInfo->pTest);
			}
			break;
		}
		#ifdef WIN32
			SwitchToThread();
		#else
	#ifdef WIN32
		SwitchToThread();
	#elif !defined(Darwin)
		pthread_yield();
	#else
	    ::sched_yield();
	#endif
		#endif
	}
	ses->free(values);
	ses->terminate();

	lInfo->lock->lock();
	if (--lInfo->counter==0) lInfo->finish->signalAll();
	lInfo->lock->unlock();
	return 0;
}
