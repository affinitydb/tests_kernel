/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _IOTRACER_H
#define _IOTRACER_H

/*
This is a pass-through implementation of the IStoreIO interface.
It adds the ability to log i/o calls to the std out.

All actual I/O is performed by routing down the chain.

I/O can be very verbose, especially writes to the log files.  For this reason
the i/o to the data and log file can be configured separately.  However even
when log file writers are not logged the action of opening and closing any file
is shown.

It also acts as a bit of test code, in that it will warn if 
async calls are still pending when files are closed.

TODO: Support more config params
Handle temp files more cleanly
REVIEW REVIEW: stdout makes sense for command line tools but mvlog macros might be useful for app usage
*/

#include <iostream>
#include <sstream>
#include <fstream>

#ifndef _MAX_PATH
// For Linux
#define _MAX_PATH 1024
#endif


#ifndef MAXLOGFILESIZE
#define MAXLOGFILESIZE 0x1000000ul // Stolen from kernel, only necessary for calculating 
								   // fristd::endly file offsets, e.g. LSN 
#endif

#include "storeiobase.h"


class IOTracer : public IOBase
{ 
	void (*mAsyncIOCallback)(iodesc*);
	bool mTraceDat;
	bool mTraceLogs;
	bool mTraceAsync;
	ulong mPageSize;
	FileID mDatFile;
	volatile long mPendingAsync;	
	int mLogIndex[FIO_MAX_OPENFILES]; // Tracks which slots are log files,
									  // and contains the 0-based index of the log file
	ostream * mOutput;  // either mTraceFile or stdout
	ofstream mTraceFile;
public:
	IOTracer(IStoreIO* ioImpl) : IOBase(ioImpl),mAsyncIOCallback(NULL),mTraceDat(true),mTraceLogs(false)
		,mTraceAsync(false),mPageSize(0x8000)
		,mDatFile(INVALID_FILEID),mPendingAsync(0),mOutput(&std::cout)
	{
		for(int i=0;i<FIO_MAX_OPENFILES;i++) mLogIndex[i]=-1;
	}
	
	~IOTracer()
	{
		if (mPendingAsync!=0)
			out() << "ERROR: Pending or mismatched Async i/o calls detected" << mPendingAsync << std::endl;

		if (mTraceFile.is_open())
			mTraceFile.close();
	}

	
	//
	// IStoreIO implementation
	// Kernel will call:
	//

	const char * getType() const { return "iotracer"; }

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
		else if ( 0 == strcmp(key,"traceio") )
		{
			// Should we have a convention for how to express booleans?
			// Also any convention for how to treat an empty value?
			mTraceDat = ( 0 != strcmp(value,"0") ) ;
			rc = RC_OK;
		}
		else if ( 0 == strcmp(key,"tracefile") )
		{
			// Set a file to output into
			// Necessary in cases where stdout is not available.
			// Can be used to change output file or unset back to stdout
			if (mTraceFile.is_open())
				mTraceFile.close();
			mOutput = &std::cout;
			if ( value != NULL && *value!=0 )
			{
				mTraceFile.open(value,ios::out);	
				if (mTraceFile.is_open())
				{
					mOutput = &mTraceFile ;
				}
				else
				{
					std::cout << "Error opening tracefile " << value << endl;
				}
			}
			rc = RC_OK;
		}
		else if ( 0 == strcmp(key,"tracelogio") )
		{
			mTraceLogs = ( 0 != strcmp(value,"0") ) ;	
			rc = RC_OK;
		}
		else if ( 0 == strcmp(key,"traceasync") )
		{
			mTraceAsync = ( 0 != strcmp(value,"0") ) ;	
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
		FileID fidrequest = fid;

		RC rc = mIO->open(fid,fname,dir,flags);

		if ( rc == RC_OK )
		{
			if (fname==NULL) {
				// Its a temp file
			}
			else if( strstr(fname,DATAFILESUFFIX) != NULL) mDatFile=fid;
			else {
				const char * pJustFilename=strstr(fname,STOREPREFIX);
				if (pJustFilename!=NULL){
					char whichLogSeries ; // A,B....
					ulong nLogFile = 0;
					sscanf(pJustFilename,LOGPREFIX"%c%08lX"LOGFILESUFFIX, &whichLogSeries, &nLogFile) ;
					mLogIndex[fid]=nLogFile;
				}
			}
		}

		if ( mTraceDat || mTraceLogs )
		{
			out() << "OPEN " ;	

			if ( rc == RC_OK )
			{
				if ( fname == NULL )
				{
					char filenm[_MAX_PATH]; filenm[0]=0;
					mIO->getFileName(fid,filenm,_MAX_PATH);
					out() << filenm;
				}
				else 
				{
					out() << fname ;				
					out() << " dir: " << (dir==NULL?"":dir) ;
				}
				out() << " as File " << (int)fid ;
			}
			else if ( fname != NULL )
			{			
				out() << "FAILED " << fname << " dir: " << (dir==NULL?"":dir) ;
			}

			if ( flags & FIO_CREATE )
				out() << " CREATE";
			if ( flags & FIO_NEW )
				out() << " NEW";
			if ( flags & FIO_TEMP )
				out() << " TEMP";

			if ( flags & FIO_REPLACE &&
				 fidrequest != INVALID_FILEID )
				out() << " REPLACE (Close any file open as " << (int)fidrequest << ")";
			else if ( fidrequest != INVALID_FILEID )
				out() << " Requesting slot " << (int)fidrequest << " (fail if already used)";

			out() << std::endl;
		}

		return rc;
	}
	RC		close(FileID fid) 
	{ 
		char filenm[_MAX_PATH]; filenm[0]=0;
		mIO->getFileName(fid,filenm,_MAX_PATH);

		if ( mTraceDat || mTraceLogs ) 
		{
			out() << "CLOSE " << (int)fid << "(" << filenm << ")" << " Pending async: " << mPendingAsync << std::endl;
		}

		// May have been a tmp file but we don't track the ids explicitly
		if ( fid==mDatFile ) mDatFile=INVALID_FILEID;
		else mLogIndex[fid]=-1;
	
		return mIO->close(fid); 
	}

	RC growFile(FileID file, off64_t newsize) 
	{ 
		RC rc = mIO->growFile(file,newsize); 

		if ( mTraceDat || mTraceLogs ) 
		{
			out() << "GROWFILE " << (int)file << " newsize: 0x" << std::hex << newsize << " RC " << std::dec << rc << std::endl;
		}

		return rc;
	}

	void  closeAll(FileID fid) 
	{		
		if ( mTraceDat || mTraceLogs ) 
		{
			out() << "CLOSE " ;			
			if (fid==0)
				out() << "ALL";
			else
				out() << "ALL with FileID>=" << (int)fid;
			out() << "Pending async: " << mPendingAsync;
			out() << std::endl;
		}

		if ( fid<=mDatFile ) mDatFile=INVALID_FILEID;
		for (FileID iter=fid;iter<FIO_MAX_OPENFILES;iter++)
		{
			mLogIndex[iter]=-1;
		}

		mIO->closeAll(fid); 
	}


	RC	listIO(int mode,int nent,iodesc* const* pcbs)
	{
		int i;
		if ( mode == LIO_NOWAIT )
		{
			for ( i = 0 ; i < nent ; i++ ) 
			{
				assert(pcbs[i]->aio_ptrpos<FIO_MAX_PLUGIN_CHAIN);
				pcbs[i]->aio_ptr[pcbs[i]->aio_ptrpos++]=this;
				InterlockedIncrement(&mPendingAsync);
			}
		}
	
		// For async i/o it is not safe to read the pcbs struct
		// after the call.  We could trace at the time of completion,
		// e.g. in the callback itself but for the moment build the 
		// trace info in advance then 
		// add the error details before printing out

		stringstream *msgs=new stringstream[nent];

		if (mTraceDat || mTraceLogs) {
			THREADID th = getThreadId();
			if ( nent > 1 ) {
				out() << "[" << th << "] Batch IO size " << nent << "..." << std::endl;
			}
			for ( i = 0 ; i < nent ; i++ ) 
			{
				if (pcbs[i]==NULL)
				{
					out() << "Warning: got NULL iodesc in listIO" << std::endl; 
					continue; 
				}

				if (pcbs[i]->aio_rc == RC_OK)
				{
					if (mTraceDat==false && pcbs[i]->aio_fildes == mDatFile)
						continue ;
					
					if (mTraceLogs==false && pcbs[i]->aio_fildes != mDatFile)
						continue ;
				}

				traceIO(mode,pcbs[i],msgs[i]);
			}
		}

		RC rc = mIO->listIO(mode,nent,pcbs);

		// From this point on pcbs cannot be read anymore for the async case, because it is deallocated
		// in another thread!

		if (mTraceDat || mTraceLogs) {
			for ( i = 0 ; i < nent ; i++ ) 
			{
				if (!msgs[i].str().empty())
				{
					// Add in the error info
					if ( rc != RC_OK ) { 
						msgs[i] << " FAILED";
					}
					if (  mode == LIO_WAIT && pcbs[i]->aio_rc != RC_OK )
					{
						msgs[i] << " aio_rc=" << std::dec << pcbs[i]->aio_rc << std::endl;
					}
					out() << msgs[i].str() << std::endl ;
				}
				else if ( rc != RC_OK || (mode == LIO_WAIT && pcbs[i]->aio_rc != RC_OK))
				{
					// Log file problem or something else
					// TODO:
					out() << "Error in untraced io call " << rc << "," <<  pcbs[i]->aio_rc << std::endl ;
				}
			}
		}

		delete[] msgs;
		return rc;
	}

	void  traceIO( int mode, const iodesc * io, stringstream& s)
	{
		THREADID th = getThreadId();	

		s << "\t" << (int)io->aio_fildes << " ";
		s << "[" << th << "] " ;
		if (io->aio_lio_opcode == LIO_READ ) s << "READ " ;
		else if (io->aio_lio_opcode == LIO_WRITE )s << "WRITE " ;
		else s << "NO_OP " ;

		if ( mode == LIO_WAIT ) s << "WAIT " ;
		else s << "ASYNC " ;

		if ( io->aio_fildes == mDatFile || mLogIndex[io->aio_fildes]==-1 ) 
		{

			if (io->aio_fildes != mDatFile)
			{
				s << "TMPFILE ";
			}

			if ( mPageSize > 0 )
			{
				if ( io->aio_offset==0 && io->aio_fildes == mDatFile )
				{
					// This happens frequently, with size 0x1000, so explain it
					s << "Master Record (page 0)";
				}
				else
				{
					s << "page " << std::hex << (io->aio_offset/mPageSize) ;
					if ( io->aio_nbytes != mPageSize ) 
					{
						s << " len 0x" << std::hex << (unsigned int)io->aio_nbytes ;
					}
				}
			}
		}
		else
		{
			s << "Log: " << mLogIndex[io->aio_fildes] << " ";

			uint64_t lsn = io->aio_offset + (mLogIndex[io->aio_fildes]* MAXLOGFILESIZE);
		
			char MSG[64];
			sprintf(MSG,"LSN: "_LX_FM, lsn) ;
			s << MSG;

			s << " len 0x" << std::hex << (unsigned int)io->aio_nbytes ;
		}
		
		// REVIEW: If the call hasn't occurred yet this value may be bogus
		// Caller should initialize before calling listio
		if ( io->aio_rc != RC_OK )
		{
			s << " aio_rc=" << std::dec << io->aio_rc << std::endl;
		}
	}

	void deleteLogFiles(ulong maxFile,const char *lDir,bool fArchived) 
	{
		if (mTraceDat || mTraceLogs)
		{
			if ( fArchived ) out() << "ARCHIVE ";
			else out() << "DELETE ";

			if (maxFile==~0ul)
				out() << "ALL logFiles";
			else
				out() << "logfiles up to " << maxFile ;
			out() << " in " << (lDir==NULL?"":lDir) << std::endl;
		}

		return mIO->deleteLogFiles(maxFile,lDir,fArchived);
	}

	// Callback from mIO for LIO_NOWAIT
	static void asyncIOCallback(iodesc* desc)
	{		
		// Intercept callback

		// Pop stack of contexts to retrieve "this" pointer
		IOTracer * pThis = (IOTracer *)desc->aio_ptr[--desc->aio_ptrpos];

		InterlockedDecrement(&(pThis->mPendingAsync));

		if ( ( pThis->mTraceDat || pThis->mTraceLogs ) && desc->aio_rc != RC_OK )
		{
			stringstream s; 
			s << "ERROR - Async failure -> " ;

			pThis->traceIO(LIO_NOWAIT,desc,s);				
			pThis->out() << s.str() << std::endl; 
		}
		else if ( pThis->mTraceAsync )
		{
			stringstream s; 
			s << "\nAsync Complete: " ;
			pThis->traceIO(LIO_NOWAIT,desc,s);				
			pThis->out() << s.str() << std::endl; 
		}

		//then forward to the next guy above
		pThis->mAsyncIOCallback(desc);
	}

private:
	ostream & out() { return *mOutput; }
	IOTracer();
} ;


#endif
