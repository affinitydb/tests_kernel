/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <fstream>

#define	NPROPS	7
#define	NELTS	20
#define	NPINS	100

class TestProtoBuf : public ITest
{
	public:
		TEST_DECLARE(TestProtoBuf);
		virtual char const * getName() const { return "testprotobuf"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Basic test of Google Protocol Buffers streaming interface"; }
		virtual bool isPerformingFullScanQueries() const { return true; }
		virtual int execute();
		virtual void destroy() { delete this; }

	private:
		bool createPINs();

		ISession * mSession ;

		// Contains mapping between string URI or property and
		// PropertyID in the store
		PropertyID mPropIDs[NPROPS];
		PID mPID[NPINS];
		uint64_t mUI64a; 
		Value varr[NELTS];
};

TEST_IMPLEMENT(TestProtoBuf, TestLogger::kDStdOut);

int TestProtoBuf::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store") ; return -1; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	MVTApp::mapURIs(mSession,"TestProtoBuf",NPROPS,mPropIDs);
	TVERIFY(createPINs());

//	IStmt *q=mSession->createStmt("SELECT * WHERE $0 < :0",mPropIDs,NPROPS);
	IStmt *q=mSession->createStmt("SELECT * WHERE EXISTS($0)",mPropIDs,NPROPS);
	TVERIFY(q!=NULL);

	if (q!=NULL) {
		IStreamOut *out=NULL;
		Value params[3]; params[0].set(10);
		TVERIFYRC(q->execute(out,params,1));
		TVERIFY(out!=NULL);
		if (out!=NULL) {
			ofstream lFile;
			lFile.open("protobuf.out",ios::binary );
			byte buf[1]; size_t l=sizeof(buf);
			while (out->next(buf,l)==RC_OK)
				lFile.write((char*)buf,l);
			lFile.close();
			out->destroy();
		}
		q->destroy();
	}

	mSession->terminate(); // No return code to test
	MVTApp::stopStore();  // No return code to test
	return 0;
}

bool TestProtoBuf::createPINs()
{
	bool lSuccess = true;
	IPIN * lPIN = NULL;
	static int const sMaxVals = 10;
	Value lPVs[sMaxVals * NPROPS];
	string lStr[6]= {"Asia", "Africa", "Europe", "America", "Australia", "Antartica"};
	int i, j, iV; string str;
	mLogger.out() << "Creating pins..." << std::endl;
	for (i = 0; i < NPINS; i++)
	{
		//if (0 == i % 100) mLogger.out() << ".";

		// Remember time at which 1/3rd of pins were created
		if(i == NPINS/3) {TIMESTAMP dt; getTimestamp(dt); mUI64a = dt;}

		int lNumProps = rand() % NPROPS + 1; // 1 or more properties.
		for (j = 0, iV = 0; j < lNumProps; j++)
		{
			switch(j){
				case 0:// VT_STRING
					{
#if 0
						if (rand()%10==0) {
							MVTRand::getString(str,40000);
							SETVALUE(lPVs[iV], mPropIDs[j], str.c_str(), OP_SET);iV++;						
						} else
#endif
						{
							int lIndex = rand()%6; 
							SETVALUE(lPVs[iV], mPropIDs[j], lStr[lIndex].c_str(), OP_SET);iV++;						
						}
					}
					break;
				case 1: // VT_INT
					{
						int lRand = (int)((float)NPINS * rand() / RAND_MAX); 
						SETVALUE(lPVs[iV], mPropIDs[j], lRand, OP_SET);iV++;
					}
					break;
				case 2: // VT_REF
					{
						bool lAdd = (int)((float)NPINS * rand()/RAND_MAX) > NPINS/2;
						if(i > 0 && lAdd){
							// Set reference to the previously created PIN
							lPVs[iV].set(mPID[i-1]);lPVs[iV].setPropID(mPropIDs[j]);iV++;
						}
					}					
					break;
				case 3: // VT_REFIDPROP
					{
						bool lAdd = (int)((float)NPINS * rand()/RAND_MAX) > NPINS/2;
						if(i > 0 && lAdd){
							// Set reference to the VT_STRING property of the previously created PIN
							RefVID lRef = {mPID[i-1],mPropIDs[0],STORE_COLLECTION_ID, 0};
							lPVs[iV].set(lRef);lPVs[iV].setPropID(mPropIDs[j]);iV++;
						}
					}
					break;
				case 4: // VT_DOUBLE
					{
						int lRand = (int)((float)NPINS * rand() / RAND_MAX); 
						SETVALUE(lPVs[iV], mPropIDs[j], (double)lRand, OP_SET);iV++;
					}
					break;
				case 5: // PROP_SPEC_UPDATED
					{
						TIMESTAMP dt; getTimestamp(dt); lPVs[iV].setDateTime(dt);lPVs[iV].setPropID(PROP_SPEC_UPDATED);iV++;						
					}
					break;
				case 6: // VT_ARRAY
					{
						const unsigned nElts = rand()%NELTS + 1;
						for (unsigned i=0; i<nElts; i++ ) {
							varr[i].set(i); varr[i].eid=i+1000;
						}
						lPVs[iV].set(varr,nElts); lPVs[iV].setPropID(mPropIDs[j]); lPVs[iV].meta=META_PROP_SSTORAGE; iV++;
					}
					break;
			}
		}	
		if(NULL == (lPIN=mSession->createUncommittedPIN(lPVs,iV,MODE_COPY_VALUES))){
			mLogger.out() << " Failed to create uncommitted pin " << std::endl;
			lSuccess = false;
		} else if (RC_OK != mSession->commitPINs(&lPIN,1)){
			mLogger.out() << " Failed to commit the pin " << std::endl;
			lSuccess = false;
		}else{
			mPID[i] = lPIN->getPID();
			/*MVTApp::output(*lPIN,mLogger.out(),pSession);
			mLogger.out() << "  ------------------------------ " << std::endl;*/
			lPIN->destroy();
		}
	}
	mLogger.out() << std::endl;
	return lSuccess;
}
