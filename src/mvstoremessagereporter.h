/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#ifndef _PITSTOREMESSAGEREPORTER
#define _PITSTOREMESSAGEREPORTER

#include "tests.h"
#include "mvstoreapi.h"
//#include "dlmvstore.h"

using namespace Afy;

class MvStoreMessageReporter : public IReport
{
	// Class to capture messages coming from the store.
	// Note: in IPC mode no messages will reach this.
public:
	// Based on the msgLevel enum inside mvstore
	enum logMsgLevel { LMSG_CRITICAL = 100, LMSG_ERROR = 4, LMSG_WARNING = 3, LMSG_NOTICE = 2, LMSG_INFO = 1, LMSG_DEBUG = 0 } ;

	MvStoreMessageReporter() ;
	~MvStoreMessageReporter() ;

	void enable(bool inEnable) { mEnable=inEnable; /* can put into silent mode */ }
	void setReportingLevel(logMsgLevel inLevel) { mLevel = inLevel ; } /* fine tuned control of which messages are shown */

	void setLogSize(unsigned int size,bool mPrc) { maxLogSize = (unsigned long long)size*1024*1024/100; mSingleProcess = mPrc; }// Assuming max length of log = 100
	//void init(mvcore::DynamicLinkMvstore * inDynamicLinkMvstore) ;
	void init() ;
	void term() ;
	void report(void *ns,int level,const char *str,const char *file=0,int lineno=-1) ;

	void* declareNamespace(char const *)
	{
		return NULL ; 
	}

	bool wasErrorLogged() const ;
	void reset() ;

	bool mIgnoreMissingStoreWarning ;
private:
	//mvcore::DynamicLinkMvstore * mDynamicLinkMvstore ;
	TestLogger mLogger ;
	logMsgLevel mLevel ; /* Set to LMSG_WARNING or LMSG_ERROR to hide verbose messages*/
	bool mEnable ;
	bool mErrorLogged ;
	bool mSingleProcess;
	enum {DEFAULT_LOG_SIZE = 1048576}; /*This is for 100MB log file size, assuming each error log can eat up maximum 100bytes*/
	unsigned long long logSize,maxLogSize;
} ;
#endif
