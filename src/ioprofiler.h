/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _IOPROFILER_H
#define _IOPROFILER_H

/*
IOProfiler driver for MvStore Kernel

For further documentation see the Wiki.

This code is intended for performance testing.  It tracks page reads and writes to the store file and store
logs. It does not record timing, but rather quantity of writes

It can also be used with tests, sdrcl, dumpload, and the application.

Example usage with kernel tests:
	tests testbasic '-ioinit={stdio}{ioprofiler,profilepath:'c:\\temp'}'

For the application set a key like this into \config\store.pinode.conf

	ioconfig=ioinit={stdio}{ioprofiler}

By default, on shutdown this driver reproduces two report files listing all page i/o, pageio.txt, pageio.csv
for the entire session.  These files are written in $PI_HOME\logs. You can override by passing a profilerpath
parameter (see example above).  If neither $PI_HOME or the profilepath is set then it will write to the working
directory.

However you can also use this code to track i/o for a smaller sampling time.  For do this call setParam
with reset at the beginning of the sampling, and then call it with profilerreport and/or profilecvsline
and/or profilerpagereport at the end of the sample period.  If you use this technique then no default
file is written at shutdown.  See kernel tests for examples using this technique.

Supported parameters:

key            value      description
profilereset      ignored    reset all i/o stats to zero
pagesize	      bytes      inform driver of the store page size.  Default is 32K
profilepath       path       set a path for reports to go in
profilereport     LOC*       write a human readable summary of i/o (overwrites any file)
profilepagereport LOC*       write a complete list of pages, with read and write count for each
							 (TIP: use store doctor to further process this file)
profilecsvline    LOC*       write a single comma-separated line summarizing
                             the i/o (normal use is to append into .csv file)
snap              LOC*       for convenience: profilecsvline + reset
csvprofilelog     LOC*       periodically output a profilecsvline to the given file (LOC)
                             while the store is running
csvprofileperiod  ms         the time (in milliseconds) between profile csv sample lines
                             by default, this will be 10 seconds.

*LOC can be empty to use stdout, a file name in which case PI_HOME\logs or
profilepath is used, or a full path.

The profilecsvline file is a bit difficult to read manually but best for importing and graphing many profiling intervals in excel
*/

#define CSVLINEHEADERS \
    "timestamp,readstorekb,writestorekb,readstorelog,writestorelog,"    \
	"asyncw_calls,asyncw_pages,asyncw_avg,asyncw_max," \
	"syncw_calls,syncw_pages,syncw_avg,syncw_max," \
	"asyncr_calls,asyncr_pages,asyncr_avg,asyncr_max," \
	"syncr_calls,syncr_pages,syncr_avg,syncr_max," \
    "asyncr_pending,asyncr_max_pending,asyncw_pending,asyncw_max_pending,async_nop_replies\n"


#include "storeiobase.h"
#include "syncTest.h"
#include <sys/stat.h>

#include <time.h>
#ifdef WIN32
    static inline long __getTimeInMs() { return ( 1000 / CLOCKS_PER_SEC ) * ::clock() ; }
#else
    static inline long __getTimeInMs() { struct timespec ts; 
    
#ifdef Darwin
	struct timeval tv; gettimeofday(&tv, NULL);
	ts.tv_sec=tv.tv_sec; ts.tv_nsec=tv.tv_usec*1000;
#else
	::clock_gettime(CLOCK_REALTIME,&ts);
#endif	

    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }
#endif


static std::string getenvvar(const char *pKey)
{
	std::string retVal = "";
#ifdef WIN32
	DWORD ldword = GetEnvironmentVariableA(pKey,NULL,0);
	if(ldword)
	{
		char *pval = new char[ldword];
		DWORD lldword = GetEnvironmentVariableA(pKey,pval,ldword);
		pval[lldword < ldword ? lldword : ldword - 1] = '\0';
		retVal = pval;
		delete[] pval;
	}
#else
	char *pval = getenv(pKey);
	retVal = pval ? pval : "";
#endif
	return retVal;
};


class IOProfiler : public IOBase
{
private:
	struct batchstat
	{
		batchstat() { reset(); }
		void reset() { cntCalls=ttlPages=max=0; }

		uint32_t cntCalls; // Total number of time this type of batch operation occurred
		uint32_t ttlPages; // Total number of pages written in all operations
		uint32_t max;	   // Maximum pages in a single operation
	} ;

    struct auto_lock
    {
        inline auto_lock( MVTestsPortability::Mutex & l ) : the_lock(l)
        {
            the_lock.lock();
        }

        inline ~auto_lock()
        {
            the_lock.unlock();
        }
    private:
        MVTestsPortability::Mutex & the_lock;
    };

    struct pending_stat
    {
        pending_stat() : value(0), max_value(0) {}

        volatile uint32_t value;
        volatile uint32_t max_value;


        inline uint32_t operator += ( uint32_t v )
        {
            value += v;
            if (value > max_value)
                max_value = value;
            return value;
        }

        inline uint32_t operator -= ( uint32_t v )
        {
            return value -= v;
        }
    };

public:
	IOProfiler(IStoreIO* ioImpl) : IOBase(ioImpl)
		,mDatFile(INVALID_FILEID)
		,mPageSize(0x8000) // Default to 32K
		,mxPages(0)
		,mPageCnt(0)
		,mPageWrites(NULL)
		,mPageReads(NULL)
		,mReadBytes(0)
		,mWriteBytes(0)
		,mReadBytesPageLog(0)
		,mWriteBytesPageLog(0)
		,mReadBytesPageStore(0)
		,mWriteBytesPageStore(0)
		,mReadBytesPageTemp(0)
		,mWriteBytesPageTemp(0)
		,mReportOnClose(true)
        ,mCsvProfilePeriodInMs(10000) // 10 seconds
        ,mCsvLastProfileDumpTime(0)
        ,mAsyncNopReplies(0)
	{
		memset( mLogFiles, 0, sizeof(mLogFiles));

		reset_impl() ;

		// By default reports written to $PI_HOME/logs, if PI_HOME env variable set
		// however a full path can be specified for the individual commands, or an
		// alternative path set
		std::string home=getenvvar("PI_HOME");
		if ( !home.empty())
		{
			mReportPath=home;
			mReportPath+="/logs/";
		}
	}

	~IOProfiler()
	{
        csv_dump_until_now();

		//REVIEW: can also be on demand via setParam
		//this is sort of a backdoor if it was never done on demand
		if (mReportOnClose)
		{
			setParam("profilereport","pageio.txt",false);
			setParam("profilepagereport","pageio.csv",false);
		}

		free(mPageReads);
		free(mPageWrites);
	}

    void init( void (*asyncIO)(iodesc*) )
    {
        assert( asyncIO != 0 );

        mParentCallback = asyncIO;
        mIO->init(localCallback);
    }

	const char * getType() const { return "ioprofiler"; }

	RC setParam(const char *key, const char *value, bool broadcast )
	{
		RC rc = RC_FALSE;
		if ( 0 == strcmp(key,"profilereset") )
		{
			reset();
			rc = RC_OK;
		}
		else if ( 0 == strcmp(key,"pagesize") )
		{
			// Must be set externally if running at a different page size than default
			// that is hardcoded in constructor
			mPageSize=atoi(value);
			rc = RC_OK;
		}
		else if ( 0 == strcmp(key,"profilepath") )
		{
			mReportPath=value;
			size_t len = mReportPath.length();
			if ( len > 0 && mReportPath[len-1] != '\\' && mReportPath[len-1] != '/' )
			{
				mReportPath+="/"; // Enforce slash at the end
			}
			rc = RC_OK;
		}
		else if ( 0 == strcmp(key,"profilereport") )
		{
			// Note: this is using "setParam" to provoke
			// an action rather than to configure the driver.

			// value is optional and contains a filename to write
			// into.  Otherwise stdout is used

			stringstream str ;
			report(str);

			rc = RC_OK;
			if ( value==NULL || strlen(value) == 0 )
			{
				std::cout << str.str();
			}
			else
			{
				if (!dumpToFile(str,value))
				{
					return RC_INVPARAM;
				}
			}
			mReportOnClose=false;
		}
		else if ( 0 == strcmp(key,"profilecsvline") )
		{
			stringstream str ;
			csvreport(str);

			rc = RC_OK;
			if ( value==NULL || strlen(value) == 0 )
			{
				std::cout << str.str();
			}
			else
			{
				if (!dumpToFile(str,value,true /*append*/,CSVLINEHEADERS))
				{
					return RC_INVPARAM;
				}
			}
			mReportOnClose=false;
		}
		else if ( 0 == strcmp(key,"profilepagereport") )
		{
			stringstream str ;
			pagereport(str);

			rc = RC_OK;
			if ( value==NULL || strlen(value) == 0 )
			{
				std::cout << str.str();
			}
			else
			{
				if (!dumpToFile(str,value))
				{
					return RC_INVPARAM;
				}
			}
			mReportOnClose=false;
		}
		else if ( 0 == strcmp(key,"snap") )
		{
			// "Macro" so that a single call gets latest stats and resets
			// Expected that the same file is used for each value
			// so that they get appended together
			rc=setParam("profilecsvline",value,broadcast);
			if ( rc==RC_OK )
				rc=setParam("profilereset",value,broadcast);
		}
        else if (0 == strcmp(key, "csvprofilelog"))
        {
            mCsvProfileLog = value;
            rc = RC_OK;
        }
        else if (0 == strcmp(key, "csvprofileperiod"))
        {
            mCsvProfilePeriodInMs = atoi(value);
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
		RC rc = mIO->open(fid,fname,dir,flags);
		if ( rc != RC_OK ) return rc ;
		if ( fname==NULL ) return rc ;
		if( strstr(fname,DATAFILESUFFIX) != NULL) mDatFile=fid;
		else mLogFiles[fid] = true;
		return rc;
	}

	RC		close(FileID fid)
	{
		// May have been a tmp file but we don't track the ids explicitly
		if ( fid==mDatFile ) mDatFile=INVALID_FILEID;
		else mLogFiles[fid]=false;

		return mIO->close(fid);
	}

	RC	listIO(int mode,int nent,iodesc* const* pcbs)
	{
        csv_dump_until_now();

		for ( int i = 0 ; i < nent ; i++ )
		{
			uint32_t page=(uint32_t)(pcbs[i]->aio_offset/(off64_t)mPageSize);

			if ( pcbs[i]->aio_lio_opcode == LIO_READ )
			{
				readOP(pcbs[i]->aio_fildes,pcbs[i]->aio_nbytes,page);
			}
			else if ( pcbs[i]->aio_lio_opcode == LIO_WRITE )
			{
				writeOP(pcbs[i]->aio_fildes,pcbs[i]->aio_nbytes,page);
			}
		}

		// This code assumes that all operations in a single listIO call
		// are same type (destination/read or write)
		if ( nent > 0 && pcbs[0]->aio_fildes == mDatFile && pcbs[0]->aio_offset > 0 /*exclude master page*/ )
		{
			batchstat * opToUpdate = NULL ;

			if ( pcbs[0]->aio_lio_opcode == LIO_READ )
			{
				if ( mode == LIO_NOWAIT )
				{
					opToUpdate = &mStoreReadASync;
				}
				else
				{
					opToUpdate = &mStoreReadSync;
				}
			}
			else if ( pcbs[0]->aio_lio_opcode == LIO_WRITE )
			{
				if ( mode == LIO_NOWAIT )
				{
					opToUpdate = &mStoreWriteASync;
				}
				else
				{
					opToUpdate = &mStoreWriteSync;
				}
			}

			if ( opToUpdate != NULL )
			{
                auto_lock exclusive(mLock);

				opToUpdate->cntCalls++;
				opToUpdate->ttlPages+=nent;
				if ( opToUpdate->max < (uint32_t)nent ) opToUpdate->max=nent ;
			}
		}

		if ( mode == LIO_NOWAIT )
		{
			for (int i = 0 ; i < nent ; ++i )
			{
				assert(pcbs[i]->aio_ptrpos<FIO_MAX_PLUGIN_CHAIN);
				pcbs[i]->aio_ptr[pcbs[i]->aio_ptrpos++]=this;

                auto_lock exclusive(mLock);
                switch (pcbs[i]->aio_lio_opcode)
                {
                case LIO_READ:
                    mAsyncReadsPending += 1;
                    break;
                case LIO_WRITE:
                    mAsyncWritesPending += 1;
                    break;
                }
			}
		}

		return mIO->listIO(mode,nent,pcbs);
	}

	void reset()
    {
		// Call this to initialize the profiler for data collection,
		// or reset the profiler between time segments
		auto_lock exclusive(mLock);
        reset_impl();
    }

	void reset_impl()
	{
		if ( mPageReads != NULL ) memset(mPageReads,0,sizeof(uint32_t)*mxPages);
		if ( mPageWrites != NULL ) memset(mPageWrites,0,sizeof(uint32_t)*mxPages);
		mReadBytes = 0;
		mWriteBytes = 0;
		mReadBytesPageLog = 0 ;
		mWriteBytesPageLog = 0 ;
		mReadBytesPageStore = 0 ;
		mWriteBytesPageStore = 0 ;
		mReadBytesPageTemp = 0 ;
		mWriteBytesPageTemp = 0 ;

		mStoreWriteSync.reset();
		mStoreWriteASync.reset();
		mStoreReadSync.reset();
		mStoreReadASync.reset();
	}

	void analyzeHits(
		std::ostream & pOs,
		const char *description,
		uint32_t * inArray,
		uint32_t inSize)
	{
		// handle read or write "hits"
		if ( inSize == 0 ) return ;

		uint32_t cntHits = 0 ;     // Total number of pages during period
		uint32_t cntMostHits = 0 ; // Count of i/o on the most active page
		uint32_t cntPagesWithHits = 0 ; // Distinct pages that were written

		for ( uint32_t i = 1 /*exclude master page*/ ; i < inSize ; i++ )
		{
			cntHits += inArray[i] ;
			if ( inArray[i] > cntMostHits )
				cntMostHits = inArray[i] ;
			if ( inArray[i] > 0 )
				cntPagesWithHits++ ;
		}

		pOs << "IO " << description << ": " << cntHits << " over " << cntPagesWithHits
			<< " pages.  Average: " << (cntPagesWithHits?double(cntHits)/cntPagesWithHits:0)
			<< " Worst: " << cntMostHits << endl ;
	}


	void report(std::ostream & pOs)
	{
		auto_lock exclusive(mLock);

		pOs << "----I/O Report (KiloBytes)-----" << endl;
		pOs << "Total Reads (KB):" << (mReadBytes/1024)
			<< " Writes: " << (mWriteBytes/1024)<< endl
			<< endl
			<< "Breakdown:"<<endl
			<< "\tStore Reads: " << (mReadBytesPageStore/1024)
			<< " Writes: " << (mWriteBytesPageStore/1024)<< endl

			<< "\tLog Reads: " << (mReadBytesPageLog/1024)
			<< " Writes: " << (mWriteBytesPageLog/1024)<< endl

			<< "\tTemp File Reads: " << (mReadBytesPageTemp/1024)
			<< " Writes: " << (mWriteBytesPageTemp/1024)<< endl

			<< endl ;

		pOs << "----Store I/O Analysis-----" << endl;
		analyzeHits(pOs,"reads",mPageReads,mPageCnt);
		analyzeHits(pOs,"writes",mPageWrites,mPageCnt);

		pOs << endl;

		writeBatchLine("Sync reads",mStoreReadSync,pOs);
		writeBatchLine("Async reads",mStoreReadASync,pOs);
        pOs << "Async reads pending (current / max): "
            << mAsyncReadsPending.value
            << "/"
            << mAsyncReadsPending.max_value << ")"
            << endl
            << endl;

		writeBatchLine("Sync writes",mStoreWriteSync,pOs);
		writeBatchLine("Async writes",mStoreWriteASync,pOs);
        pOs << "Async writes pending (current / max): "
            << mAsyncWritesPending.value
            << "/"
            << mAsyncWritesPending.max_value
            << endl
            << endl;

		pOs << "Aborted async operations: "
            << mAsyncNopReplies
            << endl;
	}


	void csvreport(std::ostream & pOs)
	{
        auto_lock exclusive(mLock);

        csvreport_impl(pOs, __getTimeInMs());
    }

	void csvreport_impl(std::ostream & pOs, long timeInMs)
	{
		// One line, for excel charting etc
		// NOTE: Keep CSVLINEHEADERS in sync with any column changes!

		pOs << timeInMs << "," ;

		pOs << (mReadBytesPageStore/1024) << ","
			<< (mWriteBytesPageStore/1024)<< ","
			<< (mReadBytesPageLog/1024)<< ","
			<< (mWriteBytesPageLog/1024)<< "," ;

		csvbatchdata(mStoreWriteASync,pOs); pOs << "," ;
		csvbatchdata(mStoreWriteSync,pOs); pOs << "," ;
		csvbatchdata(mStoreReadSync,pOs); pOs << ",";
		csvbatchdata(mStoreReadASync,pOs);  pOs << ",";

        pOs << mAsyncReadsPending.value << ","
            << mAsyncReadsPending.max_value << ","
            << mAsyncWritesPending.value << ","
            << mAsyncWritesPending.max_value << ","
            << mAsyncNopReplies << endl;
	}


	void pagereport(std::ostream & pOs)
	{
		// TIP: store dr can analyze a file formatted this way
		// to summarize by page type.
		// see store/utils/storedr/common/ioanalyze.h

		auto_lock exclusive(mLock);

		pOs << "Page,Reads,Writes" << endl;
		// Dump	full list of files in a csv friendly format
		for ( uint32_t i = 0 ; i < mPageCnt ; i++ )
		{
			pOs << std::dec << i << "," << mPageReads[i]
				<< "," << mPageWrites[i] << endl;
		}
	}

private:
	void readOP( FileID id, size_t inSize, uint32_t inPage )
	{
		auto_lock exclusive(mLock);

		mReadBytes += inSize ;

		if ( id == mDatFile )
		{
			growOnDemand(inPage);

			mReadBytesPageStore += inSize ;
			mPageReads[inPage]++ ;
		}
		else if (mLogFiles[id])
		{
			mReadBytesPageLog += inSize ;
		}
		else
		{
			mReadBytesPageTemp += inSize;
		}
	}
	void writeOP( FileID id, size_t inSize, uint32_t inPage )
	{
		auto_lock exclusive(mLock);
		mWriteBytes += inSize ;

		if ( id == mDatFile )
		{
			growOnDemand(inPage);
			mWriteBytesPageStore += inSize ;
			mPageWrites[inPage]++ ;
		}
		else if (mLogFiles[id])
		{
			mWriteBytesPageLog += inSize ;
		}
		else
		{
			mWriteBytesPageTemp += inSize;
		}
	}

	void growOnDemand( uint32_t inPage )
	{
		// Array of statistics for each pages grows as the store file grows

		if ( mxPages <= inPage )
		{
			uint32_t oldSize = mxPages;

			if ( mxPages == 0 ) mxPages=0x200;

			while ( mxPages < inPage ) mxPages=mxPages*2;

			if ( mPageWrites != NULL )
				mPageWrites=(uint32_t*)realloc(mPageWrites,sizeof(uint32_t)*mxPages);
			else
				mPageWrites=(uint32_t*)malloc(sizeof(uint32_t)*mxPages);

			if ( mPageReads != NULL )
				mPageReads=(uint32_t*)realloc(mPageReads,sizeof(uint32_t)*mxPages);
			else
				mPageReads=(uint32_t*)malloc(sizeof(uint32_t)*mxPages);

			for ( uint32_t i = oldSize ; i < mxPages ; i++ )
			{
				mPageWrites[i]=mPageReads[i]=0;
			}

			mPageCnt = inPage;
		}
		else
		{
			assert(mPageWrites!=NULL);
			assert(mPageReads!=NULL);
		}

		if ( mPageCnt <= inPage ) mPageCnt = inPage + 1 ;
	}

	bool dumpToFile(stringstream & content, const char * file, bool append=false, const char * introLine=NULL)
	{
		string path;

		if ( NULL == strstr(file,"/") && NULL == strstr(file,"\\") && !mReportPath.empty() )
		{
			// use path if file doesn't already specify one
			// (looking for slashes is an imperfect check)
			path=mReportPath;
		}

		path += file ;

		struct stat results;
		bool bExists = ( stat(path.c_str(), &results) == 0 ) ;

		FILE * f = fopen( path.c_str(), append?"ab":"wb" );
		if ( f == NULL )
		{
			std::cout << "Can't open IOProfiler report file " << path.c_str() << endl ;
			return false;
		}

		if (introLine != NULL && (!bExists || !append))
		{
			// Line that is only added at the very top (e.g. for csv headers)
			fwrite(introLine,1,strlen(introLine),f);
		}

		size_t len=content.str().size();

		size_t w = fwrite(content.str().c_str(),1,len,f);
		fclose(f);

		if ( w != len )
		{
			cout << "Fail to write all of report" << endl ;
			return false;
		}
		return true;
	}

	void writeBatchLine(const char * indesc, batchstat & stats, std::ostream & pOs)
	{
		// Print 1 line summary about a type of i/o operation
		double avg=0.0;
		if ( stats.cntCalls > 0 ) avg=double(stats.ttlPages)/stats.cntCalls;

		pOs<<indesc<<": Ops: "<<stats.cntCalls<<" TtlPages: "<<stats.ttlPages<< " Max Batch: " << stats.max<< std::endl;
	}

	void csvbatchdata(batchstat & stats, std::ostream & pOs)
	{
		// Portion of a csv line with info about a type of call
		pOs << stats.cntCalls << ","
			<< stats.ttlPages << ","
			<< (stats.cntCalls==0?0.0:double(stats.ttlPages)/double(stats.cntCalls)) << ","
			<< stats.max ;
	}

    void csv_dump_until_now()
    {
        if (mCsvProfileLog[0] == 0)
            return;

        auto_lock exclusive(mLock);

        long current_time = __getTimeInMs();
        long cut_off_time = current_time - mCsvProfilePeriodInMs;
        bool first_time   = (mCsvLastProfileDumpTime == 0);
        while (cut_off_time > mCsvLastProfileDumpTime)
        {
            stringstream str ;

            mCsvLastProfileDumpTime += first_time
                ? current_time
                : mCsvProfilePeriodInMs;

			csvreport_impl(str, mCsvLastProfileDumpTime);
            reset_impl();
            dumpToFile(
                str,
                mCsvProfileLog.c_str(),
                !first_time,  // append
                CSVLINEHEADERS);
        }
    }

private:

    void decrement_pending( iodesc * in_iodesc )
    {
        auto_lock exclusive(mLock);

        switch (in_iodesc->aio_lio_opcode)
        {
        case LIO_READ:
            mAsyncReadsPending -= 1;
            break;
        case LIO_WRITE:
            mAsyncWritesPending -= 1;
            break;
        case LIO_NOP:
            mAsyncNopReplies += 1;
            break;
        }
    }

    static void localCallback( iodesc * in_iodesc )
    {
        assert( in_iodesc->aio_ptrpos != 0 );
        if (in_iodesc->aio_ptrpos == 0 )
            return;

        IOProfiler * profiler = (IOProfiler*)(in_iodesc->aio_ptr[--(in_iodesc->aio_ptrpos)]);

        profiler->csv_dump_until_now();
        profiler->decrement_pending(in_iodesc);
        profiler->mParentCallback(in_iodesc);
    }

	FileID mDatFile;
	ulong  mPageSize;
	string mReportPath; // Optional path to write all files.  Should terminate with a slash

	uint32_t mxPages ; // Allocated size
	uint32_t mPageCnt ; // <= mxPages
	uint32_t *mPageWrites; // Dynamic array
	uint32_t *mPageReads;

	uint64_t mReadBytes ; // total of all file i/o
	uint64_t mWriteBytes ;

	uint64_t mReadBytesPageLog ;
	uint64_t mWriteBytesPageLog ;

	uint64_t mReadBytesPageStore ;
	uint64_t mWriteBytesPageStore ;

	uint64_t mReadBytesPageTemp ;
	uint64_t mWriteBytesPageTemp ;

	batchstat mStoreWriteSync ;
	batchstat mStoreWriteASync ;
	batchstat mStoreReadSync ;
	batchstat mStoreReadASync ;

	bool mReportOnClose;
	MVTestsPortability::Mutex mLock ;  // pageWrite can be called from different threads
								// can't use mvcore version because of dependencies this brings in

	bool mLogFiles[FIO_MAX_OPENFILES]; // Track which files are logs

    string mCsvProfileLog;
    long   mCsvProfilePeriodInMs;
    long   mCsvLastProfileDumpTime;

    pending_stat mAsyncReadsPending;
    pending_stat mAsyncWritesPending;
    volatile uint32_t mAsyncNopReplies;

    void (*mParentCallback) ( iodesc *);
} ;

#endif
