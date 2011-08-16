/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "emailscenario.h"
#include "mvauto.h"


/*
TODO:
From Shivam's mail:
-updation of its ref into the folder pin, creation of attachment object (once every 20 message pins) and updating that reference into the pin object.
-In the parallel as the pin creation is happening at regular intervals query for pins with mime = 'message/xx' (The exact query can be got from the app team, they already have it) and iterate through those pins.
*/

using namespace std;

// Publish this test.
class TestEmailImport : public ITest
{
	public:
		TEST_DECLARE(TestEmailImport);
		virtual char const * getName() const { return "testemailimport"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Email Import test.  Args: [--nmaxpins] [--batchsize] [--sourcedir=random/EmailDir]"; }
		
		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "need parameters, see help... "; return false; }
		virtual int execute();
		virtual void destroy() { delete this; }
	protected:

	private:
		ISession * mSession ;
};
TEST_IMPLEMENT(TestEmailImport, TestLogger::kDStdOut);

int TestEmailImport::execute()
{
	// Start with a fresh store
//	TVERIFY(MVTApp::deleteStoreFiles()) ;
	int batchSize=1,maxPins=10000; string inSourceDir("random"); bool pparsing = true;
	
	if(!mpArgs->get_param("nmaxpins",maxPins))
	{
		mLogger.out() << "No --nmaxpins arg, defaulting to 10000" << endl;
	}
	
	if(!mpArgs->get_param("batchsize",batchSize))
	{
		mLogger.out() << "No --batchsize arg, defaulting to 1" << endl;
	}

	if(!mpArgs->get_param("sourcedir",inSourceDir))
	{
		mLogger.out() << "No --sourcedir parameter. Generating emails randomly" << endl;
	}
	
	if(!pparsing){
	   mLogger.out() << "Parameter initialization problems! " << endl; 
	   mLogger.out() << "Test name: testemailimport" << endl; 	
	   mLogger.out() << getDescription() << endl;	
	   mLogger.out() << "Example: ./tests testemailimport --nmaxpins={...} --batchsize={...} --sourcedir={...}" << endl; 
			
	   return RC_FALSE;
	}

	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE; }

	// Importer may stop if it runs out of input, but
	// we may want it to loop around?
	if ( batchSize <= 1 ) batchSize = 1 ;

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	mSession->setURIBase( "http://vmware.com/core" ) ;

	EmailImporter gen(mSession,this, batchSize) ;

	//
	// Todo this can evolve to take more
	// arguments, support multi-threads etc
	//

	EmailSource * emailReader = NULL ;
	if ( 0 == strncasecmp( inSourceDir.c_str(), "random", 6 ) ||		
		strlen( inSourceDir.c_str() ) < 2 )
	{
		mLogger.out() << "Generating random email content" << endl ;

		emailReader = EmailSourceFactory::getRandomEmailGenerator(mSession,this) ;
		TVERIFY(gen.doIt( emailReader, maxPins )) ;
	}
	else
	{
		// Todo: clear error if directory doesn't exist cross platform?
#ifdef WIN32
		DWORD dw =::GetFileAttributes( inSourceDir.c_str()) ;
		if (( dw ==  INVALID_FILE_ATTRIBUTES ) ||
			0 == ( dw & FILE_ATTRIBUTE_DIRECTORY ) )
		{
			mLogger.out() << "Invalid directory argument " << inSourceDir << endl ;
			return S_FALSE ;
		}
#endif

		mLogger.out() << "Importing real email content from " << inSourceDir << endl ;

		emailReader = EmailSourceFactory::getTextFileReader(mSession,this,inSourceDir.c_str()) ;
		TVERIFY(gen.doIt( emailReader, maxPins )) ;
	}

	delete emailReader ;
	mSession->terminate(); 
	MVTApp::stopStore();  

	return RC_OK;
}
