/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _PHOTOSCENARIO_H
#define _PHOTOSCENARIO_H

#include "app.h"
#include "tests.h"
#include "mvauto.h"

// Optional Test baseclass to help reproduce MV Photo bugs.
//
// You can derive form this class instead of ITest directly
// when trying to reproduce bugs that happen more specifially 
// in the classes, families or datatypes that are used by the photo app.
//
// This is not a replacement for more focused, generalized testing,
// but meant to help for quickly reproducing more focused photo scenarios,
// with less duplicated setup code.
// e.g. when creating "testcustomXXX" type scenarios.
// 

class PhotoScenario : public ITest, public IStoreNotification
{
	public:
		/* Don't override unless you really have to
		   because this implementation will setup the photo data,
		   create store and session etc */
		virtual int execute(); 

	protected:
		/* 
		Implement test here.  Use TVERIFY macros to
		flag any unexpected situations
		*/
		virtual void doTest() = 0 ; 

		/* 
		Fill in store with your specific data.
		Default implementation adds some generic photo pins
		*/

		virtual void preTestInitialize();  // Called before store created/opened.
										   // This is chance to override default values of pin count etc.

		virtual void initializeTest();	   // Called after store and session created but 
											// before any pins generated. This is a chance to set session
											// flags or override default values

		virtual void setPostFix() ;  // Override if you don't want a random string added to all classes
									 // and properties


		virtual void populateStore() ; // Override this if you don't like the way the default photo pins are
									   // generated.  Note: You can control some of the generation 
										// through setting flags in  initializeTest.  Or if the overall
										// generation doesn't work you can write your own using the lower level 
										// helper functions used by this method. Or you can call the base class,
										// then add your own extra data.

	public:
		// IStoreNotification methods
		// Override these and set mEnableNotifCB to true if you want to test notification. 
		virtual	void notify(IStoreNotification::NotificationEvent *events,unsigned nEvents,uint64_t txid) { ; }
		virtual	void replicationNotify(IStoreNotification::NotificationEvent *events,unsigned nEvents,uint64_t txid) { ; }
		virtual	void txNotify(IStoreNotification::TxEventType,uint64_t txid) { ; }



		size_t getPinCount() { return mPinCount; } // Number of pins generated
		size_t getTagCount() {return mTagCount;}

		// Helper functions
	protected:
		PropertyID getProp( const char * inPropName ) ;
		ClassID getClass(const char * inClassName, ISession *pSession = NULL  ) ;
		void runClassQueries() ; 
		IStmt * createClassQuery
		( 
			const char * inClassName, 
			STMT_OP sop=STMT_QUERY,
			unsigned int inCntVars = 0,
			Value * inVars = NULL,
			unsigned int inCntOrderProps = 0,
			const OrderSeg * inOrderProps = NULL,
			const char * inFTSearch = NULL
		) ;
		// Variation with Session argument (for multi-thread tests)
		IStmt * createClassQueryS 
		( 
			ISession* inSession,
			const char * inClassName,			
			STMT_OP sop=STMT_QUERY,
			unsigned int inCntVars = 0,				
			Value * inVars = NULL,						
			unsigned int inCntOrderProps = 0,
			const OrderSeg * inOrder = NULL,			
			const char * inFTSearch = NULL
		) ;
		PID addPhotoPin
		( 
			const char * inName = NULL, 
			const char * inFSPath = NULL, 
			uint64_t inDate = 0, 
			unsigned long inStrmsize = 0,
			bool	bCreateMixed = true
		) ;

		void createPhotoClasses() ;
		void addRandomTags(size_t cntTagPool) ;
		void generateFolderPathPool( size_t cntFolders ) ;
		bool createAppPINs();
		PID createFeedPIN(const char *pFolderName);
		PID createFolderPIN(PID pPID, const char *pFolderName);
		PID createTagPIN(const char *pTagName = NULL);
		void registerProperties() ;

		/*
		Member variables available for usage in the derived classes
		*/

		ISession * mSession;
		MVStoreKernel::StoreCtx *mStoreCtx;
		size_t mPinCount ; // Default number of pins added to the store
		size_t mTagCount; // Default number of tags in the app
		size_t mFolderCount; // Default number of folders
		
		int mNumPINsPerFolder;
		bool mParentFolder; // If true then create one parent folder and sub folders under it
		vector<PID> mPids ; //The pins added to the store
		vector<string> mTagPool ; // Some random tags added to the pins
		vector<PID> mTagPINs;
		vector<string> mFilePathPool ; // Some random filepaths
		string mPostfix ; // Random postfix to add to property names,classes so that 
						  // each execution of the tests is isolated

		string mHostID ; // Default host id for the refresh-node-id property
		bool mCreateAppPINs; // Whether use createAppPINs() or addPhotoPIN()
		bool mCreateTagPINs;
		bool mCreateACLs ; // Whether to add ACL property

		unsigned int mNotificationMeta; // Notification value set
		bool mEnableNotifCB;


		// The common image properties are defined as member variables
		// so that the code is more readable, e.g. rather than prop[7] we can say imageStatus_id
		// Image PIN properties
		PropertyID fs_path_id ;		
		PropertyID mime_id ;
		PropertyID name_id ;
		PropertyID lastmodified_id;
		PropertyID created_id;
		PropertyID binary_id ;
		PropertyID date_id ;
		PropertyID exif_model_id;
		PropertyID exif_fnumber_id;
		PropertyID exif_picturetaken_id;
		PropertyID exif_exposuretime_id;
		PropertyID exif_isospeedratings_id;
		PropertyID exif_flash_id;
		PropertyID width_id;
		PropertyID height_id;
		PropertyID preview_id;
		PropertyID exif_make_id;
		PropertyID fs_path_index_id;
		PropertyID cache_id;
		PropertyID refreshNodeID_id;
		PropertyID exif_width_id;
		PropertyID exif_height_id;
		PropertyID sourcenode_id;
		PropertyID tag_id ;		
		PropertyID imgStatus_id ;
		PropertyID posts_id;		

		//Feed PIN properties
		PropertyID feedtype_id;
		PropertyID feedinfo_id;
		PropertyID feedautorefresh_id;
		PropertyID feedfullsize_id;
		PropertyID feedimportsubfolder_id;
		PropertyID feedflattensubfolder_id;
		PropertyID autogen_id;

		// Folder PIN properties
		PropertyID shortname_id;
		PropertyID content_id;
		PropertyID autogenerated_id;
		PropertyID photo_count_id;
		PropertyID fs_folder_id;
		PropertyID feed_id;
		PropertyID album_count_id;

		PropertyID acceptemail_id;
		PropertyID tagword_id ;
		PropertyID prop_system_id;		
		PropertyID cluster_id; 
		PropertyID email_id;
		PropertyID visIdentity_id;
		PropertyID tabname_id;
		PropertyID owner_id_id; 
		PropertyID owner_name_id;
		PropertyID tagcount_id;
		PropertyID mvstoreexTS_id;
	
};

class PhotoScenarioStream : public MVStore::IStream
{
	protected:
		size_t const mLength;
		ValueType const mVT;
		char const mStartChar;
		size_t mSeek;
	public:
		PhotoScenarioStream(size_t pLength, char pStartChar = '0', ValueType pVT = VT_STRING) : mLength(pLength), mVT(pVT), mStartChar(pStartChar), mSeek(0) {}
		virtual ValueType dataType() const { return mVT; }
		virtual	uint64_t length() const { return 0; }
		virtual size_t read(void * buf, size_t maxLength) {size_t const lLength = min(mLength - mSeek, maxLength); for (size_t i = 0; i < lLength; i++) ((char *)buf)[i] = getCharAt(mSeek + i, mStartChar); mSeek += lLength; return lLength; }
		virtual size_t readChunk(uint64_t pSeek, void * buf, size_t maxLength) { mSeek = (unsigned long)pSeek; return read(buf, maxLength); }
		virtual	IStream * clone() const { return new PhotoScenarioStream(mLength); }
		virtual	RC reset() { mSeek = 0; return RC_OK; }
		virtual void destroy() { delete this; }
	public:
		static char getCharAt(size_t pIndex, char pStartChar = '0') { return pStartChar + (char)(pIndex % 10); }
};



#endif
