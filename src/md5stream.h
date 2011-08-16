/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _md5stream_h
#define _md5stream_h

#include <sstream>
#include "serialization.h"

extern "C" {
#include "md5.h"
}

class Md5Stream : public std::basic_ostream<char>
{
	protected:
		MD5_CTX mMd5;
		class Md5StreamBuf : public std::basic_streambuf<char>
		{
			protected:
				virtual int_type overflow(int_type c) { mBuf[mBufPtr++] = c; mBuf[mBufPtr] = 0; put_all(c == '\n'); return traits_type::not_eof(c); }
				virtual int	sync() { put_all(true); return 0; }
			private:
				MD5_CTX * const mMd5;
				char mBuf[0x100];
				int mBufPtr;
				void put_all(bool pForce) { if (pForce || mBufPtr >= 0x100 - 1) { MD5_Update(mMd5, (void *)mBuf, mBufPtr); mBufPtr = 0; mBuf[0] = 0; } assert(pbase() == pptr()); }
			public:
				Md5StreamBuf(MD5_CTX * pMd5) : mMd5(pMd5), mBufPtr(0) { mBuf[0] = 0; }
		};
	public:
		Md5Stream() : std::basic_ostream<char>(new Md5StreamBuf(&mMd5)) { memset(&mMd5, 0, sizeof(mMd5)); MD5_Init(&mMd5); }
		void flush_md5(unsigned char pBuf[16]) { flush(); MD5_Final(pBuf, &mMd5); }
};

#endif
