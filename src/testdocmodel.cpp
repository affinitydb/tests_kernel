/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

// Doc Model
// 
// Demonstrate and test support for Document/Hierarchy relationships between pins
//
// Test data is a toy representation of a file system, including some text content for the files
//
// Top of each file system is a Volume Record, which also acts as the root directory, and the PROP_SPEC_DOCUMENT
//		Each directory has a collection of files and collection of subdirectories
//      Each file has size, some text content
// All elements point to their direct parent (PROP_SPEC_PARENT) and document root, so there are links in both 
// directions
//
// This test also gives a good introduction to the Full Text (FT) searching capabilities
//
// This can be extended in the future as more sophisticated concept of Documents is implemented

// See also testpropspec.cpp

#include "app.h"
#include "collectionhelp.h"
using namespace std;

#include "mvauto.h"

/* 
Corruption does not appear anymore, but the class based on PROP_SPEC_DOC returns all pins
*/
#define TEST_PROP_SPEC_DOC_CLASS  0 // See Bug #8286

#define RECURSIVE_PIN_DELETE 0 // Removing a PIN should remove all its parts
// #define TEST_IPIN_MAKEPART // Not implemented

// Publish this test.
class TestDocModel : public ITest
{
	public:
		TEST_DECLARE(TestDocModel);
		virtual char const * getName() const { return "testdocmodel"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Basic MVStore Test"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }

		virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "Deletes store file"; return false; }
		virtual bool isPerformingFullScanQueries() const { return true; }

	protected:
		void doTest() ;
		PID populateStore() ;
		ClassID CreateVolumeFamily() ;
		void DeleteVolume( const PID& inVolume ) ;
		PID AddDirectory( const PID& inVolume, const PID& inParentDir, const char * inDirName ) ;
		PID AddFile( 
				const PID& inParentDir, 
				const char * inFileName, 
				uint64_t inFileSize,
				const char * inFileText
				) ;
		void PrintFileSystem( const PID & rootPID ) ;
		void PrintFileSystem2( const PID & rootPID ) ;
		void PrintDir( const PID & inDir, long inDepth ) ;
		void GetFileSystemPids( const PID & inRoot, bool inbFiles, bool inbDirs, vector<PID>& outPids ) ;
		void GetFileSystemPids2( const PID & inRoot, bool inbFiles, bool inbDirs, vector<PID>& outPids ) ;


		ClassID CreateDocFamily() ;
	private:

		ISession * mSession ;

		// Index in mProps of the Properties used by this
		// test

		enum PropMapIndex
		{
			idxVolumnLabel = 0,// String 
			idxFileName,       // String 
			idxDirName,		   // String
			idxFiles,          // Collection of files in a directory
			idxDirs,           // Collection of sub-directories in a directory
			idxFileSize,   	   // uint64_t
			idxFileSummary,    // Potentially big string, basically imaginary text content of the file for FT search
			CountProps
		} ;

		// Contains mapping between string URI or property and
		// PropertyID in the store
		PropertyID mProps[ CountProps ] ;
		PID mUnrelatedPID ;

		string mDocClass ;
};
TEST_IMPLEMENT(TestDocModel, TestLogger::kDStdOut);

int TestDocModel::execute()
{
	doTest();
	return RC_OK;
}

void TestDocModel::doTest()
{
	TVERIFY(MVTApp::deleteStore()) ; // Must start out from scratch because tested class definition can't exclude other pins

	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return ; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;

	MVTApp::mapURIs(mSession,"TestDocModel.",CountProps,mProps);

	mDocClass = "DocumentElements" ;

#if TEST_PROP_SPEC_DOC_CLASS
	CreateDocFamily() ;
#endif

	mSession->createPIN( mUnrelatedPID, NULL, 0 ) ; 

	PID rootPID = populateStore() ;
	PrintFileSystem( rootPID ) ;

	// Look up all the file system elements two different ways
	vector<PID> pids ;
	GetFileSystemPids( rootPID, true, true, pids ) ;
	if (isVerbose())
	{
		for ( size_t i = 0 ; i < pids.size() ; i++ )
		{
			//Recursive output
			//MVTUtil::output( pids[i], mLogger.out(), mSession ) ;
			mLogger.out() << "Pid: " << std::hex << pids[i] ;
		}
		mLogger.out() << "*********************************" << endl ;
	}

	vector<PID> pids2 ;
	GetFileSystemPids2( rootPID, true, true, pids2 ) ;

	TVERIFY( pids.size() == pids2.size() ) ;

	if ( pids.size() != pids2.size() )
	{
		mLogger.out() << "Method 1 found " << std::dec << (int)pids.size() << " pins. Method 2 found " << (int)pids2.size() << endl ;
	}

	for ( size_t i = 0 ; i < pids2.size() ; i++ )
	{
		// TEST_PROP_SPEC_DOC_CLASS was seeing this problem
		// and also returning the root pin
		if ( pids2[i] == mUnrelatedPID )
		{
			TVERIFY2(0,"Totally unrelated PIN found in GetFileSystemPids2" ) ;
		}
	}

	TVERIFY( pids.size() == 10 ) ; // number here needs to change if PopulateStore adds more items
	
	if ( isVerbose())
		PrintFileSystem2( rootPID ) ;

	DeleteVolume( rootPID ) ;

	mSession->terminate(); 
	MVTApp::stopStore();  
}

PID TestDocModel::AddDirectory( const PID& inVolume, const PID& inParentDir, const char * inDirName )
{
	PID dirPID ; // Newly created PID

	Value vals[4] ;
	vals[0].set( inDirName ) ;	vals[0].property = mProps[idxDirName] ;
	vals[1].set( inVolume ) ;	vals[1].property = PROP_SPEC_DOCUMENT ;
	
	// Support not completed yet for PROP_SPEC_PARENTy
	// We can manually set and use it but store doesn't currently do anything
	// with it
	vals[2].set( inParentDir ) ; vals[2].property = PROP_SPEC_PARENT ; 

	vals[3].setIdentity( STORE_OWNER ) ; vals[3].property = PROP_SPEC_CREATEDBY ; 

	TVERIFYRC( mSession->createPIN( dirPID,vals,4) ) ;

#if TEST_IPIN_MAKEPART
	// PIN::makePart seems incomplete, currently returns RC_INTERNAL
	// and has no affect.  Not sure what the intended behavior is? 
	CmvautoPtr<IPIN> dirPIN( mSession->getPIN( dirPID ) ) ;
	TVERIFYRC(dirPIN->makePart( inParentDir, mProps[idxDirs] )) ;
#endif

	// Add the new directory to the parent's collection
	Value subDirRef ;
	subDirRef.setPart( dirPID ) ; subDirRef.property = mProps[idxDirs] ; subDirRef.op = OP_ADD ;
	TVERIFY( 0 != ( subDirRef.meta & META_PROP_PART ) ) ; // This flag, set because we use Value::setPart, 
														  // should ensure that PINs are deleted when volume 
														  // is deleted
	TVERIFYRC( mSession->modifyPIN( inParentDir, &subDirRef, 1 ) );
	
	// Verify that the new directory points to its parent
	// (Currently we are setting this manually above so it definitely will pass!)
	Value valLookup ;
	TVERIFYRC( mSession->getValue( valLookup, dirPID, PROP_SPEC_PARENT ) ) ;
	TVERIFY( valLookup.type == VT_REFID ) ; 
	TVERIFY( valLookup.id == inParentDir ) ; 

	return dirPID ;
}

PID TestDocModel::AddFile( 
		const PID& inParentDir, 
		const char * inFileName, 
		uint64_t inFileSize,
		const char * inFileText
)
{
	// We can retrieve the volume from the parent directory
	Value valVolume ;
	if ( RC_OK != mSession->getValue( valVolume, inParentDir, PROP_SPEC_DOCUMENT ) )
	{
		// If PROP_SPEC_DOCUMENT not set then the parent must be the document itself,
		// e.g. this file is being added to the document directly
		valVolume.set( inParentDir );
	}

	PID filePID ; // Newly created PID

	Value vals[5] ;
	vals[0].set( inFileName ) ;	vals[0].property = mProps[idxFileName] ;
	vals[1].set( valVolume.id ) ;	vals[1].property = PROP_SPEC_DOCUMENT ;
	vals[2].set( inParentDir ) ; vals[2].property = PROP_SPEC_PARENT ;  // See comments elsewhere about this prop
	vals[3].set( inFileText ) ; vals[3].property = mProps[idxFileSummary] ;  vals[3].meta = META_PROP_STOPWORDS ;
	vals[4].setU64(inFileSize ) ; vals[4].property = mProps[idxFileSize] ; 
	TVERIFYRC( mSession->createPIN( filePID,vals,5) ) ;

	// Add the new file to the parent's collection
	Value fileRef ;
	fileRef.setPart( filePID ) ; fileRef.property = mProps[idxFiles] ; fileRef.op = OP_ADD ;
	TVERIFYRC( mSession->modifyPIN( inParentDir, &fileRef, 1 ) );

	return filePID ;
}

PID TestDocModel::populateStore()
{
	PID rootPID ;

	// Root is the Volume (e.g. disk drive)	
	Value vals[2] ;
	vals[0].set( "HD1" ) ; vals[0].property = mProps[idxVolumnLabel] ;
	vals[1].set( "" ) ; vals[1].property = mProps[idxDirName] ;  // Volume acts as the root directory also
	TVERIFYRC( mSession->createPIN( rootPID,vals,2) );

	// Top level directories have the volume as their parent
	PID tempDir = AddDirectory( rootPID, rootPID, "temp" ) ;
	PID srcDir = AddDirectory( rootPID, rootPID, "src" ) ;

	// Add a subdirectories, e.g. \temp\testdata 
	PID testDataDir = AddDirectory( rootPID, tempDir, "testdata" ) ;

	// Other subdirectories
	PID caneraseDir = AddDirectory( rootPID, tempDir, "can_erase" ) ;	
	PID piDir = AddDirectory( rootPID, srcDir, "mv" ) ;	

	// Create some files
	PID file1 = AddFile( piDir, "readme.txt", 1000, "Lots of interesting info\nabout stuff that you should know" ) ;
	PID file2 = AddFile( tempDir, "notes.txt", 100000, "\tNotes from last years meeting\t1. Meeting minutes\t2. Lots of talk. \"blah, blah, blah\"\t3. End of meeting\n" ) ;
	PID file3 = AddFile( piDir, "mv.log", 9999, "Event 1000\nEvent 1001\nEvent 1002\nEvent 1003\nWordNumberCombo789" ) ;
	PID file4 = AddFile( rootPID, "root.info", 100000000, "File stored at the root directory, because very important\nIt must be easy to find" ) ;
	PID file5 = AddFile( piDir, "repeats.info", 1000, "words another words words words" ) ;

	return rootPID ;
}

string GetDepthPrefix( long depth )
{
	// Build a string with spaces according to a given depth
	string indent ;
	for ( long i = 0 ; i < depth ; i++ ) 
		indent += ' ' ;
	return indent ;
}

void TestDocModel::PrintDir( const PID & inDir, long inDepth )
{
	// Recursive

	CmvautoPtr<IPIN> dir( mSession->getPIN( inDir ) );
	TVERIFY( dir.IsValid() ) ;

	mLogger.out() << GetDepthPrefix(inDepth).c_str() << "Dir: " << dir->getValue( mProps[idxDirName] )->str << endl ;

	// Print Files
	MvStoreEx::CollectionIterator files(dir, mProps[idxFiles]);

	for( const Value * lVal=files.getFirst(); lVal != NULL;lVal=files.getNext())
	{
		TVERIFY( lVal->type == VT_REFID ) ;
		CmvautoPtr<IPIN> file( mSession->getPIN( lVal->id )) ;
		mLogger.out() << GetDepthPrefix(inDepth+1).c_str() << "File: " << file->getValue( mProps[idxFileName] )->str << endl ;
	}	

	// Recurse to subdirectories
	MvStoreEx::CollectionIterator subdirs(dir, mProps[idxDirs]);

	for( const Value * dir=subdirs.getFirst(); dir != NULL; dir=subdirs.getNext())
	{
		TVERIFY( dir->type == VT_REFID ) ;
		PrintDir( dir->id, inDepth+1 ) ;
	}	
}

void TestDocModel::PrintFileSystem( const PID & rootPID )
{
	// Traverse the hierarchy

	CmvautoPtr<IPIN> root(mSession->getPIN( rootPID )) ;

	mLogger.out() << endl << "---------------VOLUME CONTENT------------" << endl ;
	mLogger.out() << "Volume: " << root->getValue( mProps[idxVolumnLabel] )->str << endl ;
	PrintDir( rootPID, 0 ) ;
	mLogger.out() << endl << "-----------------------------------------" << endl ;
}

void TestDocModel::PrintFileSystem2( const PID & rootPID )
{
	// This output lacks the knowledge of the structure of the filesystem
	// because it uses a query to get all the elements directly

	mLogger.out() << endl << "-----------PrintFileSystem2--------------" << endl ;

	CmvautoPtr<IPIN> root(mSession->getPIN( rootPID )) ;
	mLogger.out() << "Volume: " << root->getValue( mProps[idxVolumnLabel] )->str << endl ;

	vector<PID> pids ;
	GetFileSystemPids2( rootPID, true, true, pids ) ;

	for ( size_t i = 0 ; i < pids.size() ; i++ )
	{
		CmvautoPtr<IPIN> pin( mSession->getPIN( pids[i] ) ) ;
		MVTApp::output( *(pin.Get()), mLogger.out() ) ;
	}
	mLogger.out() << "-------------------------" << endl << endl ;
}

void TestDocModel::GetFileSystemPids( const PID & inRoot, bool inbFiles, bool inbDirs, vector<PID>& outPids )
{
	// Recursive scan to get PIDs of all files or directory (or both)
	// Top most PID is not included in result

	CmvautoPtr<IPIN> root(mSession->getPIN( inRoot )) ;
	TVERIFY( root.IsValid() ) ;
	if ( inbFiles )
	{
		MvStoreEx::CollectionIterator files(root, mProps[idxFiles]);
		for( const Value * lVal=files.getFirst(); lVal != NULL; lVal=files.getNext())
		{
			outPids.push_back( lVal->id ) ;
		}
	}

	MvStoreEx::CollectionIterator subdirs(root, mProps[idxDirs]);
	for( const Value * lVal=subdirs.getFirst(); lVal != NULL; lVal=subdirs.getNext())
	{
		if ( inbDirs )
			outPids.push_back( lVal->id ) ;

		GetFileSystemPids( lVal->id, inbFiles, inbDirs, outPids ) ;
	}
}

void TestDocModel::DeleteVolume( const PID & rootPID )
{
	// For testing purpose get all the File system PIDs
	vector<PID> allElements ;
	GetFileSystemPids( rootPID, true, true, allElements ) ;
	TVERIFY( !allElements.empty() ) ;

	// Should to a recursive deletion of all "parts" of the document
	// (Based on META_PROP_PART)
	TVERIFYRC( mSession->deletePINs( &rootPID, 1 ) );

	// Volume itself should be gone
	TVERIFY( NULL == mSession->getPIN( rootPID ) );

	// (it is actually in deleted state)
	IPIN * p = mSession->getPIN( rootPID, MODE_DELETED ) ; TVERIFY( NULL != p ); 
	if(p) { p->destroy() ; }

	// Files and directories should also be gone
	for ( size_t i = 0 ; i < allElements.size() ; i++ )
	{
		CmvautoPtr<IPIN> pin( mSession->getPIN( allElements[i] ) ) ;

#if RECURSIVE_PIN_DELETE 
		// REVIEW: currently no refs to pins that are in a collection are deleted.
		// Only child pin deleted in current scenario is root.info because there is
		// only one file at the root (no collection)

		TVERIFY( !pin.IsValid() ); // Should be gone!

		// More detailed error message if something hasn't been deleted
		if ( pin.IsValid() )
		{
			mLogger.out() << "Pin is still around----" ; 
			MVTApp::output( *(pin.Get()), mLogger.out() ) ;
		}
#endif
	}
}

ClassID TestDocModel::CreateDocFamily()
{
	// Query to find all PIDs associated with a particular document
	// (e.g. PROP_SPEC_DOCUMENT=param0)
	// registered as a Class query so that it is indexed
	ClassID cls = STORE_INVALID_CLASSID ;
	RC rc = mSession->getClassID(mDocClass.c_str(),cls) ;
	if ( rc == RC_OK ) 
	{
		return cls ; // Class already exists in this store
	}

	IStmt * classQ = mSession->createStmt() ;
	unsigned char v = classQ->addVariable() ;

	Value exprNodes[2] ;
	PropertyID docId = PROP_SPEC_DOCUMENT ; 
							// Also breaks if we specify PROP_SPEC_PARENT
							// But to specify normal property there is no problem.
							// e.g. PropertyID docId = mProps[idxFileName] ; 
							// also no trouble for PROP_SPEC_CREATEDBY

	exprNodes[0].setVarRef(0, docId ) ;	
	exprNodes[1].setParam(0);
	IExprTree *expr1 = mSession->expr(OP_EQ,2,exprNodes);
	TVERIFYRC( classQ->addCondition( v, expr1 ) ) ;

	/*	
	QUERY.100(0) {
			Var:0 {
					CondIdx:        /FFFFFFFD =(8) $0
					CondProps:              FFFFFFFD
			}
	}
	*/
	char * classQAsText = classQ->toString() ;
	mLogger.out() << classQAsText << endl ;
	mSession->free( classQAsText ) ;

	TVERIFYRC( defineClass(mSession,mDocClass.c_str(), classQ, &cls));
	TVERIFY( cls != STORE_INVALID_CLASSID ) ;

	expr1->destroy() ;
	classQ->destroy() ;

	return cls ;
}

void TestDocModel::GetFileSystemPids2( const PID & inRoot, bool /*inbFiles*/, bool /*inbDirs*/, vector<PID>& outPids )
{
	IStmt * elemsQ = mSession->createStmt() ;

#if TEST_PROP_SPEC_DOC_CLASS
	ClassID cls = STORE_INVALID_CLASSID;
	TVERIFYRC( mSession->getClassID(mDocClass.c_str(),cls) ); // Lookup the ClassID of existing class query

	Value refRoot ;
	refRoot.set( inRoot ) ;

	ClassSpec spec ;
	spec.classID = cls ;
	spec.nParams = 1 ;
	spec.params = &refRoot ; // Specify the specific document we want to search for

	unsigned char v = elemsQ->addVariable( &spec ) ;
#else
	// Not using the class query, seems to have no problem
	unsigned char v = elemsQ->addVariable() ;

	Value exprNodes[2] ;	
	PropertyID docId = PROP_SPEC_DOCUMENT ; 
	exprNodes[0].setVarRef(0, docId ) ;	
	exprNodes[1].set(inRoot);
	IExprTree *expr1 = mSession->expr(OP_EQ,2,exprNodes);
	TVERIFYRC( elemsQ->addCondition( v, expr1 ) ) ;

	if (isVerbose())
	{
		char * qAsText = elemsQ->toString() ;
		mLogger.out() << qAsText << endl ;
		mSession->free( qAsText ) ;
	}
#endif

	// TODO: also add expressions to filter out the files or directories if they aren't requested

	// Performance Review - Query API forces us to retrieve the entire PIN into memory just to get at its PID
	ICursor * result = NULL;
	TVERIFYRC(elemsQ->execute(&result));
	IPIN* pin ;
	while( pin = result->next() )
	{
		outPids.push_back( pin->getPID() ) ;

		if (isVerbose())
			//MVTUtil::output( *pin, mLogger.out(), mSession ) ;// infinite output
			mLogger.out() << "Pins2: " << std::hex << pin->getPID() << endl ;
		
		pin->destroy() ;
	}

	result->destroy() ;
	elemsQ->destroy() ;
}
