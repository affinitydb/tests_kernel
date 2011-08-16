/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "emailscenario.h"

// This email is of the "average" size found in newgroup postings (1280 bytes)
static const char * typicalEmail = 
"From: BK\n" \
"Sent: Thursday, October 26, 2006 7:54\n" \
"Subject: Re: copying formula down\n" \
"\n" \
"Of course!!  I'm just getting used to nested IF statements, and I forgot all \n"\
"about it.\n"\
"\n"\
"Thanks!!\n"\
"\n"\
"\n"\
"Max <demechanik@yahoo.com> wrote in message \n"\
"news:eyAT5VK%23GHA.4376@TK2MSFTNGP03.phx.gbl...\n"\
"> Try instead in B1: =IF(A1=\"\",\"\",IF(A1+7<TODAY(),\"overdue\",\"\"))\n"\
"> Copy down as far as required.\n"
"> The additional front trap: =IF(A1=\"\",\"\",... will take care of the problem.\n"
"> -- \n"
"> Max\n"
"> Singapore\n"
"> http://savefile.com/projects/236895\n"
"> xdemechanik\n"
"> --- \n"
"> \"BK\" <nospam@nospam.com> wrote in message \n"
"> news:%23YWgxJJ%23GHA.2180@TK2MSFTNGP05.phx.gbl...\n"
">> Using Excel 2003\n"
">>\n"
">> Column A is dates.  These dates plus 7 are being compared to the current \n"
">> date to determine whether \"overdue\" gets displayed in Column D.  I'm \n"
">> using the following IF statement:\n"
">>\n"\
">> If A1+7<today(),\"overdue\",\" \"\n"\
">>\n"
">> When I copy the formula down, rows that have not yet had data entered \n"\
">> into them are all displaying \"overdue\" because Column A is blank.  Do I \n"\
">> just need to remember to copy the formula down every time I enter a new \n"\
">> row of data, or is there some way I can modify my IF statement to only do \n"
">> the calculation if there is data in Column A?\n"\
">>\n" \
">\n" \
">\n"
;

class RandomEmailGenerator : public EmailSource
{
public:
	//
	// This class acts as a "fake" source of email data
	// e.g. rather than a real outlook pst it just generates some pseudo-real 
	// email data.  It has advantage of not needing any input data.
	// but it is NOT INTENDED FOR FULL TEXT INDEX testing
	//

	RandomEmailGenerator( ISession* inSession, ITest * inTest ) 
		: mSession( inSession )
		, mTest( inTest )
	{
		mTags.Init( 100,	// Number of tags
					4, 12,  // Range of length
					false ) ; // Allow spaces

		const char * emails[] = { 
				"Akuti@fakedomain.com","Amul@fakedomain.com","Amulya@fakedomain.com",
				"Athalia@fakedomain.com","Atmajyoti@fakedomain.com","Atman@fakedomain.com",
				"Baka@fakedomain.com","Bali@fakedomain.com","Deepak@fakedomain.com",
				"Dharma@fakedomain.com","Haresh@fakedomain.com","Harish@fakedomain.com",
				"Jafar@fakedomain.com","Jaidev@fakedomain.com","Janak@fakedomain.com",
				"Jeevan@fakedomain.com","Kajal@fakedomain.com","Kajol@fakedomain.com",
				"Karishma@fakedomain.com","Kunal@fakedomain.com","Lakshman@fakedomain.com",
				"Madan@fakedomain.com","Madhav@fakedomain.com","Madhavi@fakedomain.com",
				"Manisha@fakedomain.com","Mihir@fakedomain.com","Neel@fakedomain.com",
				"Nikhil@fakedomain.com","Nirvana@fakedomain.com",
				"" } ; // Must terminate with empty string
		mEmailAddresses.Init( emails ) ;
	}

	virtual bool import( int inMaxMails, ImporterCallbacks * inImporter )
	{
		int cntEmails = 0 ;
		int cntAvgEmailsPerFolder = 40 ; // As per Shivam's email, there is some randomization

		while( true )
		{
			string folderName = MVTRand::getString2( 5, 15, false ) ;
			inImporter->foundFolder( folderName.c_str() ) ;

			int cntEmailsThisFolder = MVTRand::getRange(cntAvgEmailsPerFolder/2,cntAvgEmailsPerFolder+cntAvgEmailsPerFolder/2) ;
			for ( int i = 0 ; i < cntEmailsThisFolder ; i++ )
			{
				if (!genRandomEmail( folderName.c_str(), inImporter ))
					return false ;

				cntEmails++ ;
				if ( cntEmails == inMaxMails )
					return true ;
			}
		}
		assert(false) ;
		return false ;
	}

	bool genRandomEmail( const char * inFolder, ImporterCallbacks * inImporter  )
	{
		TypedPinCreator * pPinCreator = NULL ;
		inImporter->beginEmail( inFolder, &pPinCreator ) ;

		pPinCreator->add( "SenderName", "Albert Johnson" );
		pPinCreator->add( "Subject", "Have you heard about the sale at Walmart?" );

		// Same typical length text used for all emails
		pPinCreator->add( "Body", typicalEmail ) ;

		vector<string> allCC ;
		vector<string> allTo ;
		vector<string> allRR ;
		mEmailAddresses.getPoolSelection( allCC, 5 /*percentage chance */ , true /* allow empty CC */ ) ;
		mEmailAddresses.getPoolSelection( allTo, 5, false ) ;
		mEmailAddresses.getPoolSelection( allRR, 5, false ) ;

		pPinCreator->add( "Cc", allCC ) ;
		pPinCreator->add( "To", allTo ) ;
		pPinCreator->add( "ReplyRecipients", allRR ) ;

		pPinCreator->add( "SenderEmailAddress", mEmailAddresses.getStr() ) ;
		pPinCreator->addDateTime( "CreationTime", MVTRand::getDateTime(mSession,false) ) ;

		pPinCreator->add( "EntryID", MVTRand::getString2(50,50,false,true) ) ;

		// According to spec these will be 2 x and 3 x the size of the 
		// body.  We don't index this data so its contents aren't very important

		string strMoreData( typicalEmail ) ;
		strMoreData += typicalEmail ;
		pPinCreator->add("HTMLBody", strMoreData );

		strMoreData += typicalEmail ;
		pPinCreator->add( "binary", strMoreData ) ;

		pPinCreator->add( "mime", "Mail/outlook" );
		pPinCreator->add( "Importance", "High" ) ;
		pPinCreator->add( "SenderEmailType", "SMTP" ) ;
		pPinCreator->add( "Sensitivity", "Personal" ) ; //...Private, Confidential...

		vector<string> aTags ;
		mTags.getPoolSelection( aTags, 10, false ) ;
		pPinCreator->add( "Tag", aTags ) ;

		return inImporter->endEmail( pPinCreator ) ;
	}

private:
	ISession * mSession ;
	ITest * mTest ;
	RandStrPool mTags ;
	RandStrPool mEmailAddresses ;
} ;

#if COMPILE_TEXT_IMPORTER

#include "FSTraverser.h" // Cross platform, 

//Review: Hack: to avoid this dependency we compile 
//FSTraverser.cpp directly into test project
//#pragma comment (lib, "mvcore.lib")

class TextEmailReader : public EmailSource
{
public:
	// This class will import real email messages 
	// that are stored on disk as individual files.
	// Use a utility like "outport" to generate these
	// files from your pst file

/*
Expected format is like this: 

From: Pegasus (MVP)
Sent: Tuesday, November 07, 2006 0:19
Subject: Re: Is it a
<Message body for rest of the file>


This format is good for Full Text index testing but it doesn't 
have all the normal email properties that can be taken out of outlook.
Those properties are faked by this class

TODO: Add attachments, they are actually available in outport inside subfolders named
after the message.

*/
	TextEmailReader( ISession* inSession, ITest * inTest, const char * inDir ) 
		: mCntMailsFound( 0 )
		, mCntMaxMails( 0 )
		, mImporter( NULL )
		, mSession( inSession )
		, mTest( inTest )
		, mEmailDir( inDir )
	{
		// Simple modelling of potential tags - a combination of fake folder
		// names and fake user classification
		const char * tags[] = { 
				"Inbox","Sent Items","Spam","Drafts","Infected","Deleted Items",
				"Holiday","Work","Archive","Education", "Finance", "Conference", "Expense Reports",
				"Blue","Green","Yellow","Red","Pink","Purple","Orange", ""
				} ;
		mTags.Init( tags ) ;

		// My news based emails don't have a to 
		const char * emails[] = { 
				"Akuti@fakedomain.com","Amul@fakedomain.com","Amulya@fakedomain.com",
				"Athalia@fakedomain.com","Atmajyoti@fakedomain.com","Atman@fakedomain.com",
				"Baka@fakedomain.com","Bali@fakedomain.com","Deepak@fakedomain.com",
				"Dharma@fakedomain.com","Haresh@fakedomain.com","Harish@fakedomain.com",
				"Jafar@fakedomain.com","Jaidev@fakedomain.com","Janak@fakedomain.com",
				"Jeevan@fakedomain.com","Kajal@fakedomain.com","Kajol@fakedomain.com",
				"Karishma@fakedomain.com","Kunal@fakedomain.com","Lakshman@fakedomain.com",
				"Madan@fakedomain.com","Madhav@fakedomain.com","Madhavi@fakedomain.com",
				"Manisha@fakedomain.com","Mihir@fakedomain.com","Neel@fakedomain.com",
				"Nikhil@fakedomain.com","Nirvana@fakedomain.com",
				"" } ; // Must terminate with empty string
		mEmailAddresses.Init( emails ) ;

#ifdef WIN32
		if ( mEmailDir[mEmailDir.size()-1] != '\\' )
			mEmailDir += '\\' ;
#else
		if ( mEmailDir[mEmailDir.size()-1] != '/' )
			mEmailDir += '/' ;
#endif
	}

	virtual bool import( int inMaxMails, ImporterCallbacks * inImporter )
	{
		mImporter = inImporter ;
		mCntMailsFound = 0 ;
		mCntMaxMails = inMaxMails ;
		recurseFolder( mEmailDir.c_str() ) ;
		return true ;
	}
protected:
	bool recurseFolder( const char * inFolder )
	{
		mImporter->foundFolder( inFolder ) ; // REVIEW: we don't know yet if any emails exist in this folder

		FSFileInfo ctxt ;
		FSTraverser fst ;

		// Visit files
		if (!enumFiles( inFolder ))
			return false;
	
		// Visit subdirectories
		long bDone = fst.getFirstDir( ctxt, inFolder ); 

		while(!bDone)
		{
			std::string str = ctxt.getFileName();

			// str will be subdir like "Excel\"
			// ctxt.getFileInfo().cFileName has just "excel"

			std::string fullpath = inFolder + str ;

			if ( !recurseFolder( fullpath.c_str() ) )
				return false ;

			if ( mCntMailsFound == mCntMaxMails )
				return true ;

			bDone = fst.getNextDir(ctxt) ;
		}
		return true ;
	}

	bool enumFiles( const char * inFolder )
	{
		FSFileInfo ctxt ;
		FSTraverser fst ;
		long bDone = fst.getFirstFile(ctxt, inFolder,"*.txt" ); 
		while(!bDone)
		{
			std::string str = ctxt.getFileName();
			std::string strFullPath = inFolder + str ;

			importFile( inFolder, strFullPath.c_str() ) ;

			mCntMailsFound++ ;
			if ( mCntMailsFound >= mCntMaxMails )
				return true ;
				
			bDone = fst.getNextFile(ctxt) ;
		}
		return true ;
	}

	bool importFile(const char * inFolder, const char * inFullPath)
	{
		TypedPinCreator * pPinCreator ;
		if ( !mImporter->beginEmail( inFolder, &pPinCreator ) )
			return false ;

		// Note: currently minimal error handling for invalid files
		FILE * f = fopen( inFullPath, "r" ) ;
		char line[1024]; 

		// Sender name
		if (line != fgets(line, 1024, f)) assert(false);
		line[strlen(line)-1] = '\0' ;    // get rid of \n
		string senderName( line + 6 ) ; // 6 == strlen( "From: ")
		pPinCreator->add( "SenderName", senderName); 

		// Date
		if (line != fgets(line, 1024, f)) assert(false);
		line[strlen(line)-1] = '\0' ; // get rid of \n
		// TODO??? Convert "Sent: Tuesday, November 07, 2006 9:35" into a store Date Time
		pPinCreator->addDateTime( "CreationTime", MVTRand::getDateTime(mSession,false) );

		if (line != fgets(line, 1024, f)) assert(false);
		line[strlen(line)-1] = '\0' ; // get rid of \n
		string subject( line + 9 ) ;
		pPinCreator->add( "Subject", subject ) ;

		string body ;
		while( fgets( line, 1024, f ) )
		{
			body += line ;
		}

		pPinCreator->add( "Body", body ) ;

		fclose(f);

		//
		// Fill out some of the other fields more randomly
		//

		pPinCreator->add( "mime", "Mail/outlook" );
		pPinCreator->add( "Importance", "High" ) ;
		pPinCreator->add( "SenderEmailType", "SMTP" ) ;
		pPinCreator->add( "Sensitivity", "Personal" ) ; //...Private, Confidential...

		pPinCreator->add( "HTMLBody", body ) ;
		pPinCreator->add( "binary", senderName + "\n" + subject + "\n" + body ) ;
		pPinCreator->add( "EntryID", MVTRand::getString2( 50 ) );

		vector<string> addresses ;
		addresses.push_back( mEmailAddresses.getStr() ) ; // Random

		pPinCreator->add( "To", addresses ) ;
		pPinCreator->add( "SenderEmailAddress",  mEmailAddresses.getStr() ) ;

		vector<string> tags ;
		mTags.getPoolSelection( tags, 10, true ) ;  // Could be accumulated list of folders
		pPinCreator->add( "Tag", tags ) ;

		addresses.clear() ;
		mEmailAddresses.getPoolSelection( addresses, 5, true ) ;
		pPinCreator->add( "Cc", addresses ) ;
		
		return mImporter->endEmail( pPinCreator ) ;
	}

private:
	int mCntMailsFound ;
	int mCntMaxMails ;
	ImporterCallbacks * mImporter ;
	ISession * mSession ;
	ITest * mTest ;
	RandStrPool mTags ;
	RandStrPool mEmailAddresses ;
	string mEmailDir ;
} ;

#endif

EmailSource * EmailSourceFactory::getRandomEmailGenerator( ISession* inSession, ITest * inTest )
{
	return new RandomEmailGenerator( inSession, inTest ) ;
} 

EmailSource * EmailSourceFactory::getTextFileReader( ISession* inSession, ITest * inTest, const char * inDir )
{
#if COMPILE_TEXT_IMPORTER
	return new TextEmailReader( inSession, inTest, inDir ) ;
#else
	return NULL ;
#endif
}
