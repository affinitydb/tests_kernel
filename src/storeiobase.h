/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _STOREIOBASE_H
#define _STOREIOBASE_H

#include "../../kernel/include/storeio.h"


using namespace MVStoreKernel;

/*
An optional base class implementing IStoreIO.
It passes all work to an underlying implementation.
This can be used as the base on a intercepting driver,
which catches i/o calls, overrides some certain portion of behavior
and otherwise delegates everything to the normal driver.

Drivers that natively the full i/o code
or which delegate to multiple drivers should derive 
directly from IStoreIO
*/
class IOBase : public IStoreIO
{ 
protected:
	IStoreIO *mIO;// Device to delegate to
public:
	IOBase(IStoreIO* ioImpl) : mIO(ioImpl)
	{
	}
	
	virtual ~IOBase() {}
	const char * getType() const { return "iobase"; }
	void	init(void (*asyncIO)(iodesc*))
	{
#if 0
		// For a derived object to intercept async i/o callback 
		// it should remember the asyncIO argument, then pass its own local function
		// to init.  When that local function is called it would do its work, then
		// delegate up by calling asyncIO

		// Example
		mParentCallback=asyncIO;
		mIO->init(localCallback);
#endif	
		
		mIO->init(asyncIO);
	}

	RC setParam(const char *key, const char *value, bool broadcast ) { if (broadcast) return mIO->setParam(key,value,broadcast); else return RC_FALSE; }

	virtual RC		open(FileID& fid,const char *fname,const char *dir,ulong flags) 
	{ return mIO->open(fid,fname,dir,flags); }

	off64_t	getFileSize(FileID fid) const { return mIO->getFileSize(fid); }
	size_t  getFileName(FileID fid,char buf[],size_t l) const { return mIO->getFileName(fid,buf,l); }
	RC      growFile(FileID file, off64_t newsize) { return mIO->growFile(file,newsize); }
	RC		close(FileID fid) { return mIO->close(fid); }
	void  closeAll(FileID fid) { return mIO->closeAll(fid); }
	RC	listIO(int mode,int nent,iodesc* const* pcbs)
	{
		// If you wish to intercept async i/o calls you would probably
		// push your "this" pointer or some other state into the aio_ptr 
		// array and increment aio_ptrpos
		//(also see comments in init)
#if 0
		int i;
		if ( mode == LIO_NOWAIT )
		{
			for ( i = 0 ; i < nent ; i++ ) 
			{
				assert(pcbs[i]->aio_ptrpos<FIO_MAX_PLUGIN_CHAIN);
				pcbs[i]->aio_ptr[pcbs[i]->aio_ptrpos++]=this;
			}
		}
#endif	

		return mIO->listIO(mode,nent,pcbs);
	}

	bool	asyncIOEnabled() const { return mIO->asyncIOEnabled(); }
	RC deleteFile(const char *fname) { return mIO->deleteFile(fname); }
	void deleteLogFiles(ulong maxFile,const char *lDir,bool fArchived) 	{ return mIO->deleteLogFiles(maxFile,lDir,fArchived); }
	void destroy() { 
		if (mIO) { mIO->destroy(); mIO=NULL; }
		delete this;
	}
protected:
	IOBase();
} ;


#endif
