/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "mvstoremessagereporter.h"
#include "app.h"

MvStoreMessageReporter::MvStoreMessageReporter() 
	//: mDynamicLinkMvstore(NULL)
	: mIgnoreMissingStoreWarning(true)
	, mEnable(true)
	, logSize(0)
	, maxLogSize(DEFAULT_LOG_SIZE)
{
	setReportingLevel( LMSG_DEBUG ) ;
}

MvStoreMessageReporter::~MvStoreMessageReporter()
{
	//assert( mDynamicLinkMvstore == NULL ) ; // disabled because it is still called when test cancelled
}

//void MvStoreMessageReporter::init(mvcore::DynamicLinkMvstore * inDynamicLinkMvstore)
void MvStoreMessageReporter::init()
{
	//mDynamicLinkMvstore=inDynamicLinkMvstore;

	// REVIEW: Not currently available in IPC, which directs messages into
	// the mvlog system.  Even if it was available its not so clear
	// what the behavior should be because one client would potentially 
	// steal all the store messages
	//if ( mDynamicLinkMvstore->isInProc() )
	//	mDynamicLinkMvstore->setReport( this ) ; 
	setReport(this);
}
void MvStoreMessageReporter::term()
{
#if 0
	if ( mDynamicLinkMvstore )
	{
		mDynamicLinkMvstore->setReport( NULL ) ; 
		mDynamicLinkMvstore=NULL;
	}
#endif
}

void MvStoreMessageReporter::report(void *ns,int level,const char *str,const char *file,int lineno)
{
	if ( !mEnable )
		return ;

	if ( logSize > maxLogSize )
	{
			if (mSingleProcess)
			{
				mLogger.out() << "Logger has reached the maximum limit.."<<maxLogSize<<"  hence bailing out.\n";
				exit(-1);
			}
			else
				return ;
	}

	// TODO 5941 - coordinate with the logger of active test(s)
	logMsgLevel lSeverity = (logMsgLevel)level ;

	if ( lSeverity < mLevel )
		return ;

	if (mIgnoreMissingStoreWarning && 
		0 == strncasecmp("AfyDB not found in directory",str,30) )
	{
		// This distracting warning is logged often by tests which 
		// delete the store file to start from scratch
		return; 
	}

	switch(lSeverity)
	{
	case(LMSG_CRITICAL)	:mLogger.out()<<"CRITICAL:" ; mErrorLogged = true ; break ;
	case(LMSG_ERROR)	:mLogger.out()<<"ERROR:" ; mErrorLogged = true ; break ;
	case(LMSG_WARNING)	:mLogger.out()<<"WARNING:" ; break ;
	case(LMSG_NOTICE)	:mLogger.out()<<"NOTICE:" ; break ;
	case(LMSG_INFO)		:mLogger.out()<<"INFO:" ; break ;
	case(LMSG_DEBUG)	:mLogger.out()<<"DEBUG:" ; break ;
	}
	mLogger.out() << str ;
	logSize++;
}

bool MvStoreMessageReporter::wasErrorLogged() const
{
	return mErrorLogged ;
}
void MvStoreMessageReporter::reset() 
{ 
	mErrorLogged = false ; 
} 
