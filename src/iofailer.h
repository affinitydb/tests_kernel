/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _IOFAILER_H
#define _IOFAILER_H

//
// This is an implementation of the io purely for testing purposes.
// It can inject failures of i/o operations which will excercise the error handling 
// code.  It can potentially be an easier way to force these error situations to occur
// than actual reproduction. 
// 
// example usage (when the store already exists)
// tests testbig '-ioinit={stdio}{iofailer,failflags:8}'
// 8 = FAIL_GROWFILE so it means that the store file the attempt to grow the store file will fail, similar
// to out of disk space error.
//
// For some of these behaviors to be interesting it is necessary to set the parameter after the store is already
// open.  For example FAIL_READS will totally prevent the store and logs from opening so you can't get anywhere,
// but to have the reads start to fail after some portion of time has passed could be interesting.
// It is possible to change the flag at runtime with the setParam (exposed as setStoreIoParam for most clients)
//

#include <iostream>
#include <sstream>

#include "storeiobase.h"

class IOFailer : public IOBase
{ 
	enum FailRule
	{
		// Flags to set 
		FAIL_READS = 0x1,
		FAIL_WRITES = 0x2,
		FAIL_LOG_GROWTH = 0x4,
		FAIL_GROWFILE = 0x8,
		FAIL_OPEN = 0x10,			// 16
		FAIL_CLOSE = 0x20,			// 32
		FAIL_ASYNC_WRITES = 0x40,	// 64  // Other writes will succeed
		FAIL_HIGH_READS = 0x80,		// 128 // Reads beyond page 0x200 fail.  Basically a deferred fail
	} ;

	void (*mAsyncIOCallback)(iodesc*);
	int mFlags; 
	ulong mPageSize;
	FileID mDatFile;	
public:
	IOFailer(IStoreIO* ioImpl) : IOBase(ioImpl),mAsyncIOCallback(NULL),mFlags(0),mPageSize(0x8000)
		,mDatFile(INVALID_FILEID)
	{
		std::cout << "IOFailer installed" << std::endl;
	}
	
	~IOFailer()
	{
		std::cout << "IOFailer terminated" << std::endl;
	}

	//
	// IStoreIO implementation
	// Kernel will call:
	//

	const char * getType() const { return "iofailer"; }

	void	init(void (*asyncIO)(iodesc*))
	{
		// Remember the callback
		mAsyncIOCallback = asyncIO;

		// Ensure that i/o layer will call this object rather than directly to mAsyncIOCallback
		mIO->init(asyncIOCallback);
	}

	/*
	//These specialized interface methods were replaced by setParam
	void setPageSize( ulong s ) { mPageSize=s; }
	void config( bool bTraceDat, bool bTraceLogs ) { mTraceDat = bTraceDat; mTraceLogs = bTraceLogs ; }
	*/

	RC setParam(const char *key, const char *value, bool broadcast ) 
	{ 
		RC rc = RC_FALSE;
		if ( 0 == strcmp(key,"pagesize") )
		{
			mPageSize=atoi(value);
			rc = RC_OK;
		}
		else if ( 0 == strcmp(key,"failflags") )
		{
			// A little painful - you must provide the decimal version of the 
			// flags from FailRule that you want to set
			mFlags = atoi(value);
			rc = RC_OK;
		}

		if (broadcast)
		{
			RC rcbroadcast=mIO->setParam(key,value,broadcast);
			if (rc==RC_FALSE) return rcbroadcast;
		}			
		return rc;
	}

	virtual RC		open(FileID& fid,const char *fname,const char *dir,ulong flags) 
	{ 
		if (mFlags & FAIL_OPEN ) return RC_NOACCESS;

		RC rc = mIO->open(fid,fname,dir,flags);

		if ( rc == RC_OK )
		{
			if( fname!=NULL && strstr(fname,DATAFILESUFFIX) != NULL) mDatFile=fid;
		}
		return rc;
	}
	RC		close(FileID fid) 
	{ 
		if (mFlags & FAIL_CLOSE ) return RC_OTHER;
		return mIO->close(fid); 
	}

	void  closeAll(FileID fid) 
	{		
		mIO->closeAll(fid); 
	}


	RC	listIO(int mode,int nent,iodesc* const* pcbs)
	{
		// There are a lot of potential ways to fail here
		int i;
		if ( mode == LIO_NOWAIT )
		{
			for ( i = 0 ; i < nent ; i++ ) 
			{
				assert(pcbs[i]->aio_ptrpos<FIO_MAX_PLUGIN_CHAIN);
				pcbs[i]->aio_ptr[pcbs[i]->aio_ptrpos++]=this;
			}
		}
	
		bool bFailAll = false;
		if ( nent > 0 )
		{
			// Brutal complete failure

			if ( (mFlags & FAIL_READS) && (pcbs[0]->aio_lio_opcode == LIO_READ))
				bFailAll = true;

			if ( (mFlags & FAIL_HIGH_READS) && (pcbs[0]->aio_lio_opcode == LIO_READ) && pcbs[0]->aio_fildes == mDatFile)
			{
				// Basically a deferred failure, but not a very natural failure
				if ( ( pcbs[0]->aio_offset / mPageSize ) >= 0x200 ) bFailAll = true;
			}

			if ( (mFlags & FAIL_WRITES) && pcbs[0]->aio_lio_opcode == LIO_WRITE )
				bFailAll = true;
		
			if ( mFlags & FAIL_ASYNC_WRITES && pcbs[0]->aio_lio_opcode == LIO_WRITE && mode == LIO_NOWAIT )
				bFailAll = true;

			// Special but likely cause of failure - e.g. out of disk space for log files
			// Kernel doesn't use growfile

			if ( mFlags & FAIL_LOG_GROWTH && pcbs[0]->aio_lio_opcode == LIO_WRITE && pcbs[0]->aio_fildes != mDatFile )
			{
				//TODO: not distinguishing between temp file and log file
				if ( pcbs[0]->aio_offset >= mIO->getFileSize(pcbs[0]->aio_fildes  ) )
				{
					bFailAll = true;
				}
			}
		}

		if ( bFailAll )
		{
			for ( i = 0 ; i < nent ; i++ ) 
			{		
				pcbs[i]->aio_rc = RC_INTERNAL; //??? what code
				if ( mode == LIO_NOWAIT )
				{
					asyncIOCallback(pcbs[i]); // Notify async calls even for failure case
				}
			}
			return RC_INTERNAL;
		}
		else
		{
			return mIO->listIO(mode,nent,pcbs);
		}
	}

	RC growFile(FileID file, off64_t newsize) 
	{ 
		if ( mFlags & FAIL_GROWFILE	) return RC_FULL;
		return mIO->growFile(file,newsize); 
	}

	void deleteLogFiles(ulong maxFile,const char *lDir,bool fArchived) 
	{
		return mIO->deleteLogFiles(maxFile,lDir,fArchived);
	}

	// Callback from mIO for LIO_NOWAIT
	static void asyncIOCallback(iodesc* desc)
	{		
		// Intercept callback

		// Pop stack of contexts to retrieve "this" pointer
		IOFailer * pThis = (IOFailer *)desc->aio_ptr[--desc->aio_ptrpos];

		//then forward to the next guy above
		pThis->mAsyncIOCallback(desc);
	}

private:
	IOFailer();

} ;


#endif
