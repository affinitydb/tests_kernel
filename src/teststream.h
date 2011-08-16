/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _TestStringStream_H
#define _TestStringStream_H

#ifdef WIN32
	#include <windows.h>
#else 
	#include <dirent.h>
	#include<sys/types.h>
	#include<sys/stat.h>
#endif

extern "C" {
#include "md5.h"
}

/*
Generic implementation(s) of IStream for usage by 
tests
*/

// TODO: add specific Anti Leak detection (e.g. static counter
// for number of constructor/destructor calls)

class TestStringStream : public IStream
{
	/* generate a stream with no specific content 
	   based on the size specified in constructor.

	   All bytes are set to the letter 'e'

	   TODO: unicode
	   */
public:
	TestStringStream(size_t psize, ValueType inType = VT_STRING ):size(psize), remaining(psize), type(inType){}
	virtual size_t read(void * buf, size_t maxLength) {
		size_t consumed = remaining < maxLength ? remaining:maxLength;
		remaining-=consumed;
		memset(buf,'e',consumed) ; // set buffer to all bytes to a specific byte, e.g. 'e'
								   // TODO: set a valid Unicode character
		return consumed;
	} 
	virtual ValueType dataType() const { /* should be VT_BSTR or VT_STRING*/ return type; }
	virtual size_t readChunk(uint64_t pSeek, void * buf, size_t maxLength) {
		// TODO IMPLEMENT
		cout << "Read chunk!";return 0;
	}
	virtual	IStream * clone() const { return new TestStringStream(size, type); }
	virtual	RC reset() { remaining=size; return RC_OK;  }
	virtual void destroy() { delete this; }
	virtual	uint64_t length() const { return size;}

	// TODO: Add more specific content and add a validate method

protected:
	size_t size;
	size_t remaining;
	ValueType type ;
};


class TestStream : public IStream
{
	/* 
	This testing stream implementation allocates a buffer so that
	a test can set very specific binary content	
	*/
public:
	TestStream(size_t psize, const unsigned char * inBuffer = NULL, ValueType inType = VT_STRING )
		: mSize(psize)
		, mRemaining(psize)
		, mType(inType)
	{
		assert( mSize >= 2 ) ;

		if ( inType == VT_STRING && inBuffer != NULL && strlen((const char*)inBuffer)==size_t(psize) )
		{
			mSize += sizeof(char); // null termination
		}


		mBuffer = (unsigned char*)malloc( sizeof(unsigned char) * mSize ) ;
		if(NULL == mBuffer) {
			cout<<"***Failed to allocate memory***\n";
			assert(0);
		}
		else
		{
			if ( inBuffer != NULL ) 
			{
				memcpy( mBuffer, inBuffer, mSize ) ;
			}
			else
			{
				memset(mBuffer,'z',mSize) ;
				mBuffer[mSize-1] = '\0' ; // Null termination to help any debugging
			}
		}
	}

	~TestStream()
	{
		free(mBuffer) ; mBuffer = NULL ;
	}

	virtual size_t read(void * buf, size_t maxLength) 
	{
		size_t consumed = mRemaining < maxLength ? mRemaining:maxLength;
		memcpy(buf, &(mBuffer[mSize-mRemaining]),consumed);
		mRemaining-=consumed;
		return consumed;
	} 
	virtual ValueType dataType() const 
	{	/* should be VT_BSTR or VT_STRING*/ 
		return mType;
	}
	virtual size_t readChunk(uint64_t pSeek, void * buf, size_t maxLength) 
	{
		// TODO IMPLEMENT
		assert(false);
		cout << "Read chunk!";return 0;
	}
	virtual	IStream * clone() const 
	{ 
		return new TestStream(mSize, getBuffer(), mType); 
	}
	virtual	RC reset() 
	{ 
		mRemaining=mSize; 
		return RC_OK;  
	}
	virtual void destroy() { delete this; }
	virtual	uint64_t length() const { return mSize;}

public:
	// Test can set specific content
	unsigned char * getBuffer() { return mBuffer ; }

	const unsigned char * getBuffer() const { return mBuffer ; }

protected:
	unsigned char * mBuffer ;
	size_t mSize;
	size_t mRemaining; // Stream position for iteration
	ValueType mType ;
};


class MyStream : public MVStore::IStream
{
	// Similar to TestStringStream (it used to be cut and paste in several tests)
	// This stream contains a predictable content and can test Values to see if they were generated
	// by it.

	protected:
		size_t const mLength;
		ValueType const mVT;
		char const mStartChar;
		size_t mSeek;
	public:
		MyStream(size_t pLength, char pStartChar = '0', ValueType pVT = VT_STRING) 
			: mLength(pLength), mVT(pVT), mStartChar(pStartChar), mSeek(0) {}
		virtual ValueType dataType() const { return mVT; }
		virtual	uint64_t length() const { return mLength; }
		virtual size_t read(void * buf, size_t maxLength) 
		{ 
			size_t const lLength = min(size_t(mLength - mSeek), maxLength); 
			for (size_t i = 0; i < lLength; i++) 
				((char *)buf)[i] = getCharAt(mSeek + i, mStartChar); 
			mSeek += lLength; 
			return lLength; 
		}
		virtual size_t readChunk(uint64_t pSeek, void * buf, size_t maxLength) 
		{ 
			mSeek = (unsigned long)pSeek; 
			return read(buf, maxLength); 
		}
		virtual	IStream * clone() const { return new MyStream(mLength,mStartChar,mVT); }
		virtual	RC reset() { mSeek = 0; return RC_OK; }
		virtual void destroy() { delete this; }
	public:

		// Static methods to test a Value to see if it contains a valid instance of this test class
		static char getCharAt(size_t pIndex, char pStartChar = '0') { return pStartChar + (char)(pIndex % 10); }
		static bool checkStream(TestLogger & pLogger, Value const & pStreamV, size_t pExpectedLen, char pStartChar = '0', ValueType pVT = VT_STRING)
		{
			bool lSuccess = true;
			if (pVT == pStreamV.type)
			{
				if (pStreamV.length != pExpectedLen)
				{
					pLogger.out() << "Error in retrieved string (length:" << (unsigned long)pStreamV.length << ", expected:" << (unsigned long)pExpectedLen << ")!" << std::endl;
					lSuccess = false;
				}
				size_t i;
				for (i = 0; i < pStreamV.length && lSuccess; i++)
				{
					if (pStreamV.str[i] != MyStream::getCharAt((unsigned long)i, pStartChar))
					{
						pLogger.out() << "Error in retrieved string! pos: " << (int) i << " chars: " << pStreamV.str[i] << " vs " << MyStream::getCharAt((unsigned long)i, pStartChar) << std::endl;
						lSuccess = false;
					}
				}
			}
			else if (VT_STREAM == pStreamV.type)
			{
				size_t i;
				char lC;
				if (pStreamV.stream.is->length() != pExpectedLen)
				{
					pLogger.out() << "Error (a) in retrieved stream (length:" << (unsigned long)pStreamV.stream.is->length() << ", expected:" << (unsigned long)pExpectedLen << ")!" << std::endl;
					lSuccess = false;
				}
				for (i = 0; 0 != pStreamV.stream.is->read(&lC, 1) && lSuccess; i++)
				{
					#if !defined(NDEBUG) // Don't abuse good things...
						if (i > 1000)
							break;
					#endif
					if (lC != MyStream::getCharAt(i, pStartChar))
					{
						pLogger.out() << "Error in retrieved stream! pos:" << i << " chars: " << lC << " vs " <<  MyStream::getCharAt(i, pStartChar) << std::endl;
						lSuccess = false;
					}
				}

				#if !defined(NDEBUG) // Don't abuse good things...
				if (lSuccess && pExpectedLen != pStreamV.stream.is->length())
				#else
				if (lSuccess && (pExpectedLen != pStreamV.stream.is->length() || i != pExpectedLen))
				#endif
				{
					pLogger.out() << "Error (b) in retrieved stream (length:" << i << ", expected:" << (unsigned long)pExpectedLen << ")!" << std::endl;
					lSuccess = false;
				}
			}
			else
			{
				pLogger.out() << "Unexpected type for stream!" << std::endl;
				lSuccess = false;
			}
			return lSuccess;
		}
};

class CFileStream :
	public MVStore::IStream
{
public:
	CFileStream(const std::string &pFilePath,bool pIsBinary = false)
		:path(pFilePath),bBinary(pIsBinary)
	{
		fp.open(pFilePath.c_str(),std::ios_base::in|std::ios::binary);
		pos = 0;fSize=-1;
	};
	virtual ~CFileStream(void){if(fp.is_open())fp.close();};
	// IStream interface:
	virtual ValueType		dataType() const {return bBinary ? VT_BSTR : VT_STRING;} ;
	virtual uint64_t		length() const
	{
		uint64_t filesize = 0;
		#ifdef WIN32
				// Try to obtain hFile's size 
				WIN32_FIND_DATA FindFileData;
				HANDLE hFind = FindFirstFile(path.c_str(), &FindFileData);
				if (hFind != INVALID_HANDLE_VALUE) 
				{
					filesize = (uint64_t)(((uint64_t)FindFileData.nFileSizeHigh) * MAXDWORD) + (uint64_t)(FindFileData.nFileSizeHigh) + (uint64_t)FindFileData.nFileSizeLow;
					FindClose(hFind);
				}
		#else
				struct stat64 lFileStat; 
				if (0 == stat64( path.c_str(), &lFileStat )) 
					filesize = lFileStat.st_size;
		#endif
		return filesize;
	};
	virtual size_t	read(void *buf,size_t maxLength)
	{
		if(!fp.is_open()) return 0;
		if(fSize==-1){
			fp.seekg(0, std::ios::end);
			fSize = (long)fp.tellg();
			fp.seekg (0, std::ios::beg);
		}

		size_t rdLen = fSize - this->pos;
		if (maxLength < rdLen) {
		rdLen = maxLength;
		}
		    
		fp.read((char*)buf,rdLen);
		pos += (long)rdLen;
		return rdLen;
	};
	virtual size_t	readChunk(uint64_t offset,void *buf,size_t length)
	{
		if(!fp.is_open()) return 0;
		fp.seekg((off_t)offset,std::ios::beg);
		pos = (long)offset;
		return read(buf,length);
	};
	virtual IStream			*clone() const
	{
		return new CFileStream(path,bBinary);
	};
	virtual RC				reset()
	{
		fp.seekg(0,std::ios::beg);
		pos = 0;
		return RC_OK;
	};
	virtual void			destroy()
	{
		delete this;
	};

	
private:
	std::ifstream fp;
	std::string path;
	long pos;
	long fSize;
	bool bBinary;
};

class Md5TestStream : public std::basic_ostream<char>
{
	protected:
		MD5_CTX mMd5;
		class Md5TestStreamBuf : public std::basic_streambuf<char>
		{
			protected:
				virtual int_type overflow(int_type c) { mBuf[mBufPtr++] = c; mBuf[mBufPtr] = 0; put_all(c == '\n'); return traits_type::not_eof(c); }
				virtual int	sync() { put_all(true); return 0; }
			private:
				MD5_CTX * const mMd5;
				char mBuf[0x100];
				int mBufPtr;
				void put_all(bool pForce) { 
					if (pForce || mBufPtr >= 0x100 - 1) 
					{
						MD5_Update(mMd5, (void *)mBuf, mBufPtr); mBufPtr = 0; mBuf[0] = 0; } 
						assert(pbase() == pptr()); 
				}
			public:
				Md5TestStreamBuf(MD5_CTX * pMd5) : mMd5(pMd5), mBufPtr(0) { mBuf[0] = 0; }
		};
	public:
		Md5TestStream() : std::basic_ostream<char>(new Md5TestStreamBuf(&mMd5)) { memset(&mMd5, 0, sizeof(mMd5)); MD5_Init(&mMd5); }
		void flush_md5(unsigned char pBuf[32]) { flush(); MD5_Final(pBuf, &mMd5); }
};

#endif
