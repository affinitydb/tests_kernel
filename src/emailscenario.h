/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#ifndef _EMAILSCENARIO_H
#define _EMAILSCENARIO_H

#define COMPILE_TEXT_IMPORTER 1 // Uses some code from mvcore

/*
Helper classes for simluting import of email data
*/
#include "app.h"
#include "tests.h"
#include "pinschema.h"
#include "mvauto.h"
#include <iomanip>

class EmailPinHelp : public PinSchema
{
public:
	EmailPinHelp(ISession* inSession)
		: PinSchema( inSession )
	{
#if 1
		// Define content of email pin
		addProperty( "EntryID",          VT_STRING,  SF_FAMILY, OP_EQ ) ;
		addProperty( "Subject",          VT_STRING,  SF_SSV|SF_FT|SF_NOSTOPWORDS ) ;
		addProperty( "Body",             VT_STRING,  SF_SSV|SF_FT|SF_NOSTOPWORDS ) ;
		addProperty( "Tag",              VT_STRING,  SF_COLLECTION|SF_FAMILY|SF_FT, OP_EQ ) ;
		addProperty( "CreationTime",     VT_DATETIME,SF_FAMILY ) ;
		addProperty( "HTMLBody",         VT_STRING,  SF_SSV ) ;
		addProperty( "binary",           VT_STRING,  SF_SSV ) ;
		addProperty( "Importance",       VT_STRING,  0 ) ;
		addProperty( "SenderEmailType",  VT_STRING,  0 ) ;
		addProperty( "Sensitivity",      VT_STRING,  0 ) ;
		addProperty( "SenderEmailAddress",VT_STRING, SF_FT ) ;
		addProperty( "SenderName",       VT_STRING,  SF_FT ) ;
		addProperty( "mime",             VT_STRING,  0 ) ;
		addProperty( "To",               VT_STRING,  SF_COLLECTION|SF_FT ) ;
		addProperty( "Cc",               VT_STRING,  SF_COLLECTION|SF_FT ) ;
		addProperty( "ReplyRecipients",	 VT_STRING,  SF_COLLECTION|SF_FT ) ;
#else
	// TEMPORARY TO TEST INDEX Size
		addProperty( "EntryID",          VT_STRING,  0, OP_EQ ) ;
		addProperty( "Subject",          VT_STRING,  SF_SSV|SF_NOSTOPWORDS ) ;
		addProperty( "Body",             VT_STRING,  SF_SSV|SF_NOSTOPWORDS ) ;
		addProperty( "Tag",              VT_STRING,  SF_COLLECTION, OP_EQ ) ;
		addProperty( "CreationTime",     VT_DATETIME,0 ) ;
		addProperty( "HTMLBody",         VT_STRING,  SF_SSV ) ;
		addProperty( "binary",           VT_STRING,  SF_SSV ) ;
		addProperty( "Importance",       VT_STRING,  0 ) ;
		addProperty( "SenderEmailType",  VT_STRING,  0 ) ;
		addProperty( "Sensitivity",      VT_STRING,  0 ) ;
		addProperty( "SenderEmailAddress",VT_STRING, 0 ) ;
		addProperty( "SenderName",       VT_STRING,  0 ) ;
		addProperty( "mime",             VT_STRING,  0 ) ;
		addProperty( "To",               VT_STRING,  SF_COLLECTION ) ;
		addProperty( "Cc",               VT_STRING,  SF_COLLECTION ) ;
		addProperty( "ReplyRecipients",	 VT_STRING,  SF_COLLECTION ) ;
#endif

		postAddProps() ;
	}
} ;


class ImporterCallbacks
{	
public:
	virtual bool foundFolder( const char * inFolder ) = 0 ;

	// Assumption is that foundFolder would have already been called for any particular inFolder argument

	virtual bool beginEmail( const char * inFolder, TypedPinCreator ** outPinCreator ) = 0 ;
	virtual bool endEmail( TypedPinCreator * ioPinCreator ) = 0 ;
} ;

class EmailSource
{
	// Classes implementing this interface act as a source of email data
	// They call the ImporterCallbacks methods as it finds folders and emails.
public:
	virtual bool import( int inMaxMails, ImporterCallbacks * inImporter ) = 0 ;

} ;

class EmailSourceFactory
{
	// Get an instance of one of the EmailSources
	// (For use in conjunction with EmailImporter)
public:
	static EmailSource * getTextFileReader( ISession* inSession, ITest * inTest, const char * inDir ) ;
	static EmailSource * getRandomEmailGenerator( ISession* inSession, ITest * inTest ) ;
} ;

class TimedActionReporter
{
	// Simple class, just for reporting average time to perform an action that
	// is performed with a batch size.  e.g. committing pins in batches of 10 or 100 pins.
	//
	// If the batch size is small we only want to report every once in a while (the "window" size)
	// so that the console output doesn't slow down the test.  But those reports should be 
	// the average timing of all actions during the window.  This is important because
	// it helps smooth out the spikes in individual small actions - the timing mechanism used here 
	// is not very accurate.
	//
	// If the batch size is big we just dump the results for each batch.
	// 
	// This class assumes that the batch size does not change
	// Potentially it could set the window size according to the actual timing (e.g.
	// report more often if the actions are slower even for a batch)  But this would
	// make the data analysis a bit more tricky 
public:
	TimedActionReporter() : m_reportWindowTime(0) {} 
	void addAction( int intSampleSize, int intActionTime, int ttlSize, std::ostream & pOs )
	{
		if ( intSampleSize >= m_reportWindowSize )
		{
			report( intSampleSize, intActionTime, ttlSize, pOs ) ;
		}
		else
		{
			assert( m_reportWindowSize % intSampleSize == 0 ) ; // Assumption about batch sizes

			m_reportWindowTime += intActionTime ;

			if ( ttlSize % m_reportWindowSize == 0 )
			{
				// report average of all the actions during this window
				report( m_reportWindowSize, m_reportWindowTime, ttlSize, pOs ) ;
				m_reportWindowTime = 0 ;
			}
		}
	}
private:
	void report(int intWindowSize, int intWindowTime, int ttlSize, std::ostream & pOs)
	{
		pOs	<< ttlSize << "\t"
			<< std::setprecision(3) << fixed 
			<< 1.0 * intWindowTime / intWindowSize << endl ;

		//Profiling info can be collected, but you must have run the test
		//with the ioprofiler enabled.  Example:
		//tests testemailimport 5000 1 c:\\oe\\astext -ioinit={stdio}{ioprofiler}
#if 0  // remove temporarily		
		MVTApp::sDynamicLinkMvstore->setIoParam(MVTApp::Suite().mStoreCtx,"profilecsvline","emailstoreio.csv");
		MVTApp::sDynamicLinkMvstore->setIoParam(MVTApp::Suite().mStoreCtx,"profilereset","");
#endif
	}
	static const int m_reportWindowSize = 100 ; // How often to report
	int m_reportWindowTime ;
} ;

class EmailImporter : public ImporterCallbacks
{
	// Framework for the email import process.
	// However this class doesn't produce the actual email data,
	// it relies on the EmailSource implementation to produce the actual
	// email data
public:
	EmailImporter( ISession* inSession, ITest * inTest, int inPinsPerBatch = 100 ) 
		: mSession( inSession)
		, mTest( inTest )
		, mBatchSize( inPinsPerBatch )
		, mEmailHelp( inSession )
	{
	}

	bool doIt( EmailSource * inSource, int inMaxPins )
	{
		mCommitTimes = 0 ;
		mBatchPinCreateTimes = 0 ;
		mCntEmails = 0 ;
		bool success = inSource->import( inMaxPins, this ) ;

		if ( success )
		{
			// Commit any final batch
			success = commitBatch() ;		
		}

		assert( mCntEmails <= inMaxPins ) ;

		if ( mCntEmails < inMaxPins )
			mTest->getLogger().out() << "\nOnly " << mCntEmails << " imported" << endl ;

		mTest->getLogger().out() << "\nTotal Pin Commit time (ms): " << mCommitTimes 
			<< std::setprecision(3) << fixed << endl
			<< " Avg:    " << ( mCommitTimes / ( 1. * mCntEmails ) ) << endl
			<< " Pins/S: " << std::setprecision(1) << ( 1000. * mCntEmails / mCommitTimes ) << endl ;

		return success ;
	}

	virtual bool foundFolder( const char * inFolder )
	{
		// TODO: Model any folder pin
		return true ;
	}

	bool beginEmail( const char * inFolder, TypedPinCreator ** outPinCreator )
	{
		*outPinCreator = new TypedPinCreator( mSession, &mEmailHelp ) ;
		return true ;
	}

	bool endEmail( TypedPinCreator * ioPinCreator )
	{
		mCntEmails++ ;
		// Not specifying MODE_COPY_VALUES so memory ownership passes to store
		long lBeginTime = getTimeInMs() ;	
		IPIN * pin = mSession->createPIN( ioPinCreator->mVals, ioPinCreator->mValPos, 0 ) ;
		long lCommitTime = getTimeInMs() - lBeginTime ;

		mBatchPinCreateTimes += lCommitTime ;

		ioPinCreator->detach() ;

		ioPinCreator->destroy() ;

		if ( pin == NULL )
		{	
			TV_R(!"Failure to create uncommitedPIN",mTest) ;
			return false ;
		}

		mBatchPins.push_back( pin );

		if ( (int)mBatchPins.size() == mBatchSize )
		{
			if ( !commitBatch() )
				return false ;

			assert( mBatchPins.empty() ) ;
		}
		return true ;		
	}

private:
	bool commitBatch()
	{
		// REVIEW: How will batch commits fit with the concept of transactions?
		int cntPinsInBatch = (int) mBatchPins.size() ; // Could be less than mBatchSize

		if ( cntPinsInBatch == 0 ) return true ;

#if 0 // #8422    	
		if ( mCntEmails == 36293 )
		{
			DebugBreak() ;
		}
#endif

		long lBeginTime = getTimeInMs() ;	
		RC rc = mSession->commitPINs( &(mBatchPins[0]), cntPinsInBatch ) ;
		long lCommitTime = getTimeInMs() - lBeginTime ;

		// Timing/progress periodically logged by this helper
		mActionReporter.addAction(cntPinsInBatch,lCommitTime+mBatchPinCreateTimes,mCntEmails,mTest->getLogger().out()) ;

		mCommitTimes += lCommitTime + mBatchPinCreateTimes ;

		TVRC_R(rc,mTest);

		for ( int i = 0 ; i < cntPinsInBatch ; i++ )
		{
			mBatchPins[i]->destroy() ;
		}
		mBatchPins.clear() ;
		mBatchPinCreateTimes = 0 ;
		return ( rc == RC_OK ) ;
	}

private:
	ISession * mSession ;
	ITest * mTest ;

	int mBatchSize ;
	vector<IPIN *> mBatchPins ;

	EmailPinHelp mEmailHelp ;
	TimedActionReporter mActionReporter ;

	long mBatchPinCreateTimes ; // Accumulated createPIN cost for this batch
	long mCommitTimes ;
	int mCntEmails ;	
} ;

#endif
