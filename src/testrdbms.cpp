/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

// test RDBMS
//
// Create typical relational database scenario for
// demonstration of how common table/row/query 
// concepts can be represented in the mvstore.
//
// This is basically a test of the usability and feature set
// rather than of the details of the API, which are covered in more
// specific tests.
// 
// Test simulates a library:
//
// Table 1: "Clients" - LibraryCardNumber (unique), Name, Age
// Table 2: "Books" - ISBN (unique), Author, Title
// Table 3: "Loans" - Book ISBN, Client LibraryCardNumber, BorrowedData, DueDate
//
// It demonstrates typical operations like adding a new library client or new book.
// 
// And it performs typical queries such as finding what books a client has borrowed, finding overdue books etc
//
// NOTES: The code ended up based on objects, which of course hints at a more 
// object-oriented database model than normally associated with Relational Databases.  
// However the classes only provide helpful operations on the 
// different tables, not a representation of each individual PIN in the database.  Coverage
// of a typical OO database usage should be covered in another test.
// 
// One interesting question here is when to use PID references instead of keys.  Relational databases 
// don't directly support references from one row to another, whereas the MV store supports PIN references.
// On the other hand relational databases have explicit support for unique, primary keys in a table,
// data integrity etc, whereas the MVStore does not.  This test demonstrates both key and reference approach
// based on the following #ifdef
#define USE_PIN_REF 1

#include "app.h"
using namespace std;

#include "mvauto.h"

//#define BIG_DATA  // Turn off for easier debugging
#define SUPPORT_NOW_IN_DATE_QUERY  // Now supported

class BaseTable
{
public:
	BaseTable() 
		: mClass(STORE_INVALID_CLASSID)
		, mIndexFamily(STORE_INVALID_CLASSID)
	{

	}

	void Init(ITest* inParent, ISession* inSession, const string & inTableName)
	{
		mTest = inParent ;
		mSession = inSession ;

		// Table name should be unique in the store
		// Because tests re-use the same store over an over again we mix in a random component
		// So the key name will also be unique in the store
		string randPortion ;
		MVTRand::getString( randPortion, 10,0,false ) ;
		mTableName = inTableName + string(".") + randPortion ;
	}

	void CreateClass( const PropertyID *inProps,unsigned nProps )
	{
		// Any pin with these properties should be considered part of the table
		// In fact only a single property is needed if we use unique names for our properties
		// based on the table name (as is the case in the example).  For looser tables
		// we might want to define all the properties that we consider critical to the table
		// 

		string className = mTableName + ".class" ;

		if ( RC_OK == mSession->getClassID(className.c_str(),mClass))
			return ; // Already defined

		CmvautoPtr<IStmt> lQ(mSession->createStmt());
		unsigned const char lVar = lQ->addVariable();

		assert( nProps == 1 ) ;

		Value lV[1];
		lV[0].setVarRef(0,*inProps);
		CmvautoPtr<IExprTree> lPropExist( mSession->expr(OP_EXISTS, 1, lV, 0));
		TVRC_R(lQ->addCondition(lVar,lPropExist),mTest);

		/*

		// setPropCondition is more convenient
//		lQ->setPropCondition( lVar, inProps, nProps ) ;

		*/

		TVRC_R(ITest::defineClass(mSession,className.c_str(), lQ, &mClass),mTest);
	}

	void CreateKeyLookupFamily( PropertyID inKey )
	{
		// Register a class for looking up PINs by the "key" property
		TV_R( mClass != STORE_INVALID_CLASSID, mTest ) ; // call CreateClass first
		string className = mTableName + ".keylookup" ;

		if ( RC_OK == mSession->getClassID(className.c_str(),mIndexFamily))
			return ; // Already defined

		CmvautoPtr<IStmt> lQ(mSession->createStmt());

		ClassSpec spec ;
		spec.classID = mClass ;
		spec.nParams = 0 ; 
		spec.params = NULL ;			
		unsigned char lVar = lQ->addVariable( &spec,1 ) ;

		Value args[2] ;
		args[0].setVarRef(0,inKey );
		args[1].setParam(0);
		CmvautoPtr<IExprTree> expr(mSession->expr(OP_EQ,2,args));

		TVRC_R(lQ->addCondition( lVar, expr ),mTest) ;

		TVRC_R(ITest::defineClass(mSession,className.c_str(), lQ, &mIndexFamily),mTest);
	}

	void Flush()
	{
		CmvautoPtr<IStmt> lQ(GetClassQuery(STMT_DELETE));
		lQ->execute() ;
	}
	
	IStmt * GetClassQuery(STMT_OP sop=STMT_QUERY) const 
	{	
		TV_R( mClass != STORE_INVALID_CLASSID, mTest ) ; // call CreateClass first

		IStmt * lQ = mSession->createStmt(sop) ;
		ClassSpec spec ;
		spec.classID = mClass ;
		spec.nParams = 0 ; // Class with no variables
		spec.params = NULL ;			
		lQ->addVariable( &spec,1 ) ;
		return lQ ;
	}

	IPIN * LookupByKey(const Value & inKeyValue) const 
	{
		CmvautoPtr<IStmt> lQ( mSession->createStmt() ) ;
		ClassSpec spec ;
		spec.classID = mIndexFamily ;
		spec.nParams = 1 ; 
		spec.params = &inKeyValue ;
		lQ->addVariable( &spec,1 ) ;

		ICursor* lC = NULL;
		lQ->execute(&lC);
		CmvautoPtr<ICursor> res(lC);
		TV_R(res.Get()!=NULL,mTest) ;
		IPIN* retVal = res->next() ;
		TV_R( retVal != NULL, mTest ) ;

		IPIN* nextVal = res->next() ;
		if ( nextVal != NULL )
		{
			// Should be only one pin with this key value, probably flaw in the
			// program logic
			TV_R( !"More than one results", mTest ) ; 
			mTest->getLogger().out() << "Matching pins in primary index!" << endl;
			MVTApp::output( *retVal, mTest->getLogger().out()) ;
			MVTApp::output( *nextVal, mTest->getLogger().out()) ;

			mTest->getLogger().out() << "Key Lookup" << endl;
			MVTApp::output( inKeyValue, mTest->getLogger().out(), mSession);

			nextVal->destroy() ;
		}
		return retVal ;
	}

	ClassID mClass ;		// Class that defines membership in the table
	ClassID mIndexFamily ;   // Family, based on mClass, for lookup by the primary key

	string mTableName ;
	ISession * mSession ;
	ITest * mTest ; // For running TVERIFY/TV_R statements
} ;

class ClientTable : public BaseTable
{
	// Client of the library
public:
	void Init(ITest* inTest,ISession* inSession)
	{
		mNextKey = 0 ; // If reloading existing store this should be calculated based on maximum
					   // key so far assigned

		BaseTable::Init( inTest, inSession, "Clients" ) ;
		mLibraryCardNumber_id = MVTApp::getProp( inSession, string( mTableName + ".card" ).c_str() ) ;
		mFirstName_id = MVTApp::getProp( inSession, string( mTableName + ".first" ).c_str() ) ;
		mLastName_id = MVTApp::getProp( inSession, string( mTableName + ".last" ).c_str() ) ;
		mAge_id = MVTApp::getProp( inSession, string( mTableName + ".age" ).c_str() ) ;

		CreateClass( &mLibraryCardNumber_id, 1 ) ;

		// Permit fast and easy lookup by library card number
		// (In USE_PIN_REF case this is not needed to resolving references from 
		// other pins, but it is still useful/needed as an "entry point" to find
		// clients based on their number)
		CreateKeyLookupFamily( mLibraryCardNumber_id ) ;
	}

	void GenerateData()
	{
		mTest->getLogger().out() << "Creating " << mCntClients << " library clients" << endl ;

		for ( int i = 0 ; i < mCntClients ; i++ )
		{
			string strFirstName, strLastName ;
			MVTRand::getString( strFirstName, 5, 10, false,true ) ;
			MVTRand::getString( strLastName, 5, 10, false,true ) ;

			int Age = MVTRand::getRange( 8,80 ) ;

			AddClient( strFirstName.c_str(), strLastName.c_str(), Age ) ;
		}

		// Sanity check - we use MODE_SYNC_CLASSIFY so that we know index 
		// is immediately updated
		CmvautoPtr<IStmt> qClients(GetClassQuery()) ;
		uint64_t cnt ; 
		qClients->count( cnt ) ; 
		TV_R( (int)cnt == mCntClients, mTest ) ;
	}

	void AddClient( const char * inFirstName, const char * inLastName, int inAge )
	{
		PID pid ;
		Value vals[4] ;

		vals[0].set( mNextKey ) ; vals[0].property = mLibraryCardNumber_id ;
		vals[1].set( inFirstName ) ; vals[1].property = mFirstName_id ;
		vals[2].set( inLastName ) ; vals[2].property = mLastName_id ;
		vals[3].set( inAge ) ; vals[3].property = mAge_id ;

		TVRC_R(mSession->createPIN( pid, vals, 4),mTest) ;

		mNextKey++ ;
	}

#ifdef BIG_DATA
	const static int mCntClients = 3000 ;
#else
	const static int mCntClients = 30 ;
#endif

	PropertyID mLibraryCardNumber_id ; // Primary key of the table
	PropertyID mFirstName_id ; // First name
	PropertyID mLastName_id ;
	PropertyID mAge_id ;

	int mNextKey ; // Library cards numbered sequentially as they are assigned 
				// Review: Probably this should be stored somewhere in the store so that it doesn't need to be
				// recalculated?
} ;

class BookTable : public BaseTable
{
	// Represents the collection of books in the library
public:
	void Init(ITest* inTest,ISession* inSession)
	{
		BaseTable::Init( inTest,inSession, "Books" ) ;
		mISBN_id = MVTApp::getProp( inSession, string( mTableName + ".isbn" ).c_str() ) ;
		mTitle_id = MVTApp::getProp( inSession, string( mTableName + ".title" ).c_str() ) ;
		mAuthor_id = MVTApp::getProp( inSession, string( mTableName + ".author" ).c_str() ) ;

		CreateClass( &mISBN_id, 1 ) ;
#if USE_PIN_REF == 0
		CreateKeyLookupFamily( mISBN_id ) ;
#endif
	}

	void GenerateData()
	{
		mTest->getLogger().out() << "Creating " << mCntBooks << " books" << endl ;
		int isbn = rand() ; // First number random, rest are sequential to avoid duplicates

		for ( int i = 0 ; i < mCntBooks ; i++ )
		{
			string strTitle, strAuthor ;
			MVTRand::getString( strTitle, 5, 10, false,true ) ;
			MVTRand::getString( strAuthor, 5, 10, false,true ) ;

			AddBook( isbn++, strTitle.c_str(), strAuthor.c_str() ) ;
		}
	}

	void AddBook( int isbn, const char * inTitle, const char * inAuthor )
	{
		PID pid ;
		Value vals[3] ;

		vals[0].set( isbn ) ; vals[0].property = mISBN_id ; // ISBN is presumably unique and good key
		vals[1].set( inTitle ) ; vals[1].property = mTitle_id ;
		vals[2].set( inAuthor ) ; vals[2].property = mAuthor_id ;

		TVRC_R(mSession->createPIN( pid, vals, 3),mTest) ;
	}

#ifdef BIG_DATA
	const static int mCntBooks = 10000 ;
#else
	const static int mCntBooks = 100 ;
#endif

	PropertyID mISBN_id ; 
	PropertyID mAuthor_id ; 
	PropertyID mTitle_id ;
} ;

class LoanTable : public BaseTable
{
	// Record of which books are ACTIVELY borrowed

public:
	void Init(ITest* inTest, ISession* inSession, BookTable& inBooks, ClientTable& inClients )
	{
		mBooks = &inBooks ;
		mClients = &inClients ;

		BaseTable::Init( inTest, inSession, "Loans" ) ;
		
		// This property will contain the id of the book.
		// So it could presumably use the same property id as is used in the 
		// Book table (the isbn number).  However that property is currently used
		// to decide membership in the class.
		// IMPORTANT NOTE: Because more typical relational database setup is being modelled the
		// bookid remembers the ISBN value.  However the mvstore also would support a direct reference
		// to the PIN representing the book.  That would probably be faster and better.
		mBook_id = MVTApp::getProp( inSession, string( mTableName + ".book" ).c_str() ) ;

		mClient_id = MVTApp::getProp( inSession, string( mTableName + ".client" ).c_str() ) ;
		mBorrowDate_id = MVTApp::getProp( inSession, string( mTableName + ".borrowdate" ).c_str() ) ;
		mDueDate_id = MVTApp::getProp( inSession, string( mTableName + ".duedate" ).c_str() ) ;

		// There is no natural key for this table
		// However we generate unique property ids so any one of them can be used to distinguish membership
		CreateClass( &mBook_id, 1 ) ;

#if USE_PIN_REF == 0
		// A book can only be borrowed once, so we know this will be unique
		// and can index it for fast lookup

		// (with pin references it is unnecessary)
		CreateKeyLookupFamily( mBook_id ) ;
#endif
	}

	void GenerateData()
	{
		mTest->getLogger().out() << "Creating loans..." ;

		mCntLoans = 0 ;

		CmvautoPtr<IStmt> qBooks(mBooks->GetClassQuery()) ;
		uint64_t cnt ; qBooks->count( cnt ) ; TV_R( (int)cnt == mBooks->mCntBooks, mTest ) ;
		if ( cnt == 0 ) return ;
		ICursor* lC = NULL;
		qBooks->execute(&lC);
		CmvautoPtr<ICursor> rBooks(lC);
			
		size_t resultPos = 0 ;

		// Do a pass through the books and add
		// loan records for some of the entries	
		for ( int i = 0 ; i < mBooks->mCntBooks ;  )
		{
			CmvautoPtr<IPIN> randBook ;

			while(true ) 
			{
				// No random access to query results, so we move forward pin by pin
				CmvautoPtr<IPIN> bookIter(rBooks->next()) ;
				TV_R( bookIter.IsValid(),mTest ) ;
				resultPos++ ;
				if ( resultPos == size_t(i + 1) )
				{
					randBook.Attach(bookIter.Detach()) ;
					break ;
				}
			}

			/*int isbn =*/ randBook->getValue(mBooks->mISBN_id)->i ;
			int idOfClient = MVTRand::getRange(0,mClients->mCntClients-1) ; // Assuming no insert/deletes have happened

			uint64_t borrowDate = MVTRand::getDateTime(mSession,true/*allow future*/) ;
			
			// Due date is two weeks later 
			//(REVIEW: is there some easier way, e.g. to get the numeric value of two weeks and add it directly?)
			DateTime dtTwoWeeks ;
			TVRC_R(mSession->convDateTime( borrowDate, dtTwoWeeks ),mTest) ;
			dtTwoWeeks.day += 14 ;
			if ( dtTwoWeeks.day > 28 )
			{
				dtTwoWeeks.day -= 28 ;
				dtTwoWeeks.month += 1 ;
				if ( dtTwoWeeks.month > 12 )
				{
					dtTwoWeeks.month -= 12 ;
					dtTwoWeeks.year += 1 ;
				}
			}

			uint64_t twoWeeks ;
			TVRC_R( mSession->convDateTime( dtTwoWeeks, twoWeeks ),mTest ) ;

			Value valClientID ; valClientID.set( idOfClient ) ; 
			CmvautoPtr<IPIN>  pinClient( mClients->LookupByKey( valClientID ) ) ;

			AddLoan( randBook, pinClient.Get(), borrowDate, twoWeeks ) ;

#ifdef BIG_DATA
			i += MVTRand::getRange( 1,50 ) ;
#else
			i += MVTRand::getRange( 1,4 ) ;
#endif
			mCntLoans++ ;
		}

		mTest->getLogger().out() << mCntLoans << " generated"  << endl ;
	}

	void AddLoan( IPIN * inBook, IPIN * inClient, uint64_t borrowDate, uint64_t dueDate )
	{
		PID pid ;
		Value vals[4] ;

#if USE_PIN_REF
		vals[0].set( inBook->getPID() ) ; vals[0].property = mBook_id ; // Reference to book
#else
		vals[0].set( inBook->getValue( mBooks->mISBN_id )->i ) ; vals[0].property = mBook_id ; // ISBN is presumably unique and good key
#endif

#if USE_PIN_REF
		vals[1].set( inClient->getPID() ) ; vals[1].property = mClient_id ;
#else
		vals[1].set( inClient->getValue( mClients->mLibraryCardNumber_id )->i  ) ; vals[1].property = mClient_id ;
#endif
		vals[2].setDateTime( borrowDate ) ; vals[2].property = mBorrowDate_id ;
		vals[3].setDateTime( dueDate ) ; vals[3].property = mDueDate_id ;

		TVRC_R(mSession->createPIN( pid, vals, 4),mTest) ;
	}

	int mCntLoans ;

	PropertyID mBook_id ;	 // Foreign key, but will also be unique in this list
	PropertyID mClient_id ;  // Foreign key
	PropertyID mBorrowDate_id ;
	PropertyID mDueDate_id ;

	BookTable * mBooks ;
	ClientTable * mClients ;
} ;

// Publish this test.
class TestRDBMS : public ITest
{
	public:
		TEST_DECLARE(TestRDBMS);
		virtual char const * getName() const { return "testrdbms"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "Test MVStore As Relational Database"; }
		
		virtual int execute();
		virtual void destroy() { delete this; }

		virtual bool includeInSmokeTest(char const *& pReason) const { return true; }
		virtual bool isStandAloneTest()const {return true;}
	protected:
		void doTest() ;
		void LoanReport();
		void FindOverdueBooks() ;
	private:
		ISession * mSession ;

		ClientTable mClientTable ;
		BookTable mBookTable ;
		LoanTable mLoanTable ;
};
TEST_IMPLEMENT(TestRDBMS, TestLogger::kDStdOut);

int TestRDBMS::execute()
{
	bool bStarted = MVTApp::startStore() ;
	if ( !bStarted ) { TVERIFY2(0,"Could not start store, bailing out completely") ; return RC_FALSE ; }

	mSession = MVTApp::startSession();
	TVERIFY( mSession != NULL ) ;
	
	doTest() ;

	mSession->terminate(); 
	MVTApp::stopStore();  

	return RC_OK  ;
}

void TestRDBMS::doTest()
{
	mClientTable.Init(this,mSession) ;
	mBookTable.Init(this,mSession) ;
	mLoanTable.Init(this,mSession,mBookTable,mClientTable) ;

	mClientTable.GenerateData() ;
	mBookTable.GenerateData() ;
	mLoanTable.GenerateData() ;

	//This is very simple (indexed) lookup by client number
	Value clientNumber ;
	clientNumber.set( 8 ) ; // Find client with library card #8
	CmvautoPtr<IPIN> pinClient( mClientTable.LookupByKey( clientNumber )) ;
	TVERIFY( pinClient.IsValid() ) ;

	// Get the earliest active loan
	CmvautoPtr<IStmt> qLoans(mLoanTable.GetClassQuery()) ;
	OrderSeg order={NULL,mLoanTable.mBorrowDate_id,0,0};
	qLoans->setOrder( &order, 1 ) ; 
	ICursor* lC = NULL;
	TVERIFYRC(qLoans->execute(&lC));
	CmvautoPtr<ICursor> res(lC);
	CmvautoPtr<IPIN> firstLoan(res->next()) ;

	// Get the client associated with that loan
	const Value * client = firstLoan->getValue( mLoanTable.mClient_id ) ; 

#if USE_PIN_REF
	// Follow reference directly
	TVERIFY(client->type == VT_REFID);
	CmvautoPtr<IPIN> pinClientWithLoan1(mSession->getPIN(client->id));
#else
	CmvautoPtr<IPIN> pinClientWithLoan1(mClientTable.LookupByKey( *client )) ;
#endif

#if 0
	// Referential integrity demonstration
	// We can delete this client even though some loan
	// references it, which in turn would crash the LoanReport code.
	// It is up us make sure references between tables are coherent
	// or that we have error handling for those scenarios.
	pinClientWithLoan1->deletePIN() ;
	pinClientWithLoan1.Detach() ;
#endif

	// A more advanced "join" like operation
	LoanReport() ;

	FindOverdueBooks() ;

#if 0
	// Data can be erased e.g.
	mLoanTable.Flush() ;
	mBookTable.Flush() ;
	mClientTable.Flush() ;
#endif
}

void TestRDBMS::LoanReport()
{
	// "Join" - Resolve the client and book numbers to generate complete loan report
	CmvautoPtr<IStmt> qAllLoans( mLoanTable.GetClassQuery() ) ;
	
	OrderSeg lOrder={NULL,mLoanTable.mDueDate_id,ORD_DESC,0} ;
	qAllLoans->setOrder( &lOrder, 1) ; // Sorting by due date

	ICursor* lC = NULL;
	TVERIFYRC(qAllLoans->execute(&lC));
	CmvautoPtr<ICursor> rAllLoans(lC);

	mLogger.out() << "Loan Report (PARTIAL)" ;

	long cntLines = 0 ;

	for ( IPIN * pinLoan = NULL ; ( pinLoan = rAllLoans->next() ) != NULL ; cntLines++) 
	{
		// Joins not implemented yet so we do manual queries on each loan to 
		// lookup associated elements
		const Value * client = pinLoan->getValue( mLoanTable.mClient_id ) ; 
		const Value * book = pinLoan->getValue( mLoanTable.mBook_id ) ; 
		TVERIFY( client != NULL && book != NULL ) ;

#if USE_PIN_REF
		IPIN * pinClient = mSession->getPIN( *client ) ;
#else
		IPIN * pinClient = mClientTable.LookupByKey( *client ) ;
#endif

#if USE_PIN_REF		
		IPIN * pinBook = mSession->getPIN( *book ) ; // No query necessary, direct lookup
#else
		IPIN * pinBook = mBookTable.LookupByKey( *book ) ;
#endif

		TVERIFY( pinClient != NULL && pinBook != NULL ) ;

		// Now we can print the the information from all three pins to give
		// the aggregate information
#if USE_PIN_REF		
		int clientID = pinClient->getValue( mClientTable.mLibraryCardNumber_id )->i ;
		int bookID = pinBook->getValue( mBookTable.mISBN_id )->i ;
#else
		int clientID = client->i ;
		int bookID = book->i ;
#endif

		mLogger.out() << "Client " << clientID
					 << " (Name: " << pinClient->getValue( mClientTable.mFirstName_id)->str << ")"
					 << " borrowed " << bookID 
					 << " (Title: " << pinBook->getValue( mBookTable.mTitle_id )->str << ")" ;

		DateTime dueDate ; uint64_t dueDateInternal = pinLoan->getValue( mLoanTable.mDueDate_id )->ui64 ; 
		mSession->convDateTime( dueDateInternal, dueDate ) ;

		mLogger.out() << " Due " << dueDate.day << "/" << dueDate.month <<  "/" << dueDate.year << endl ;

		pinClient->destroy() ;
		pinBook->destroy() ;
		pinLoan->destroy() ;

		if ( cntLines > 10 ) 
			break ; // Point proven without printing hundreds of lines
	}

	mLogger.out() << endl << endl ;
}

void TestRDBMS::FindOverdueBooks()
{
	CmvautoPtr<IStmt> qAllLoans( mLoanTable.GetClassQuery() ) ;

	uint64_t cntTotalLoans ;
	qAllLoans->count( cntTotalLoans ) ;

	// Here add another condition on top of the 

	CmvautoPtr<IStmt> qLoansTimeRange( mLoanTable.GetClassQuery() ) ;

	Value args[2] ;
	args[0].setVarRef(0, mLoanTable.mDueDate_id ) ;

	TIMESTAMP tsNow ; getTimestamp( tsNow ) ; 
#ifdef SUPPORT_NOW_IN_DATE_QUERY
	args[1].setNow() ; 
#else
	// Old workaround
	args[1].setDateTime( tsNow ) ;
#endif

	CmvautoPtr<IExprTree> expr(mSession->expr(OP_LE,2,args)) ;
	TVERIFYRC(qLoansTimeRange->addCondition(0, expr )) ;

	uint64_t cntOverdue ;
	TVERIFYRC(qLoansTimeRange->count( cntOverdue )) ;

	ICursor* lR = NULL;
	TVERIFYRC(qLoansTimeRange->execute(&lR)) ;
	IPIN * pin ;
	while(NULL != ( pin = lR->next()) )
	{
		uint64_t dueDate = pin->getValue( mLoanTable.mDueDate_id )->ui64 ;
		TVERIFY( dueDate < tsNow ) ;
		pin->destroy() ;
	}
	lR->destroy() ;
	mLogger.out() << cntTotalLoans << " books are borrowed, and " << cntOverdue << " are overdue" << endl ;
	TVERIFY( cntOverdue > 0 ) ; // It was be huge fluke of the random number generation, otherwise seems to be bug in setNow()
}
