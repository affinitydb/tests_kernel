#ifndef _TESTS_INMEM_H
#define _TESTS_INMEM_H

#ifdef WIN32
	#include <process.h>
	#include <aclapi.h>
	/*
	#ifndef FILE_MAP_EXECUTE // Win XP Sp2 define...
		#define FILE_MAP_EXECUTE 0x20
	#endif
	*/
#else
	#include <sys/mman.h>
	#include <fcntl.h>
	#include <errno.h>
#endif
#include <string.h>

#define INMEM_DEFAULT_NAME "affinity.inmem" // could even be a store file, arguably (100% mapped at startup); depends on how Mark implements, especially logging...

namespace InMem
{
	#ifdef WIN32
		struct FileMapping
		{
			std::string mName;
			HANDLE mFile;
			HANDLE mFileMapping;
			void * mAddress;
			FileMapping() : mFile(NULL), mFileMapping(NULL), mAddress(NULL) {}
		};
	#else
		struct FileMapping
		{
			std::string mName;
			int mFileMapping;
			void * mAddress;
			FileMapping() : mFileMapping(NULL), mAddress(NULL) {}
		};
	#endif

	bool openFileMapping(char const * pMappingName, void * pHint, size_t pSize, FileMapping & pMapping)
	{
		#ifdef WIN32
			pMapping.mFile = NULL;
			pMapping.mAddress = NULL;

			pMapping.mFileMapping = ::OpenFileMapping(FILE_MAP_WRITE/*| FILE_MAP_EXECUTE*/, TRUE, pMappingName);
			if (!pMapping.mFileMapping)
				{ fprintf(stderr, "Failed to OpenFileMapping %s (error=%d)\n", pMappingName, ::GetLastError()); return false; }

			pMapping.mAddress = ::MapViewOfFileEx(pMapping.mFileMapping, FILE_MAP_WRITE/* | FILE_MAP_EXECUTE*/, 0, 0, 0, pHint);
			if (!pMapping.mAddress)
			{
				fprintf(stderr, "Failed to map %s at %p (error=%d)\n", pMappingName, pHint, ::GetLastError());
				::CloseHandle(pMapping.mFileMapping); pMapping.mFileMapping = NULL;
				return false;
			}
			pSize = 0; // Just to avoid the unused param compiler warning.
		#else
			pMapping.mAddress = NULL;

			pMapping.mFileMapping = ::shm_open(pMappingName, O_RDWR, 0666);
			if (-1 == pMapping.mFileMapping)
				{ fprintf(stderr, "Failed to shm_open %s (error=%d)\n", pMappingName, errno); return false; }

			pMapping.mAddress = ::mmap(pHint, pSize, PROT_READ | PROT_WRITE/*| PROT_EXEC*/, MAP_SHARED, pMapping.mFileMapping, 0);
			if (0 == pMapping.mAddress || (void *)-1 == pMapping.mAddress || (pHint && pHint != pMapping.mAddress))
			{
				fprintf(stderr, "Failed to map %s at %p (error=%d)\n", pMappingName, pHint, errno);
				if (pMapping.mAddress)
					{ ::munmap(pMapping.mAddress, pSize); pMapping.mAddress = 0; }
				::close(pMapping.mFileMapping); ::shm_unlink(pMappingName); pMapping.mFileMapping = 0;
				return false;
			}
		#endif
		pMapping.mName = pMappingName;
		printf("openFileMapping %s at %p succeeded\n", pMappingName, pHint);
		return true;
	}

	// Note: Fails if the mapping already exists; use openFileMapping first, if that's your intention.
	bool createFileMapping(char const * pMappingName, char const * pFileName, void * pHint, size_t pSize, FileMapping & pMapping)
	{
		#ifdef WIN32
			pMapping.mFile = NULL;
			pMapping.mFileMapping = NULL;
			pMapping.mAddress = NULL;

			// Make sure we don't fight with another daemon.
			{
				HANDLE const lFileMapping = ::OpenFileMapping(FILE_MAP_WRITE, TRUE, pMappingName);
				if (lFileMapping)
				{
					fprintf(stderr, "Unexpected: file mapping %s already open by somebody\n", pMappingName);
					::CloseHandle(lFileMapping);
					return false;
				}
			}

			SECURITY_ATTRIBUTES * lSAptr = NULL;
			#if 1
				SECURITY_DESCRIPTOR lSD;
				::InitializeSecurityDescriptor(&lSD, SECURITY_DESCRIPTOR_REVISION);
				::SetSecurityDescriptorDacl(&lSD, TRUE, 0, FALSE);
				SECURITY_ATTRIBUTES lSA;
				lSA.nLength = sizeof(lSA);
				lSA.lpSecurityDescriptor = &lSD;
				lSA.bInheritHandle = FALSE;
				lSAptr = &lSA;
			#endif

			pMapping.mFile = ::CreateFile(pFileName, GENERIC_READ | GENERIC_WRITE/* | FILE_EXECUTE*/, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
			if (!pMapping.mFile)
				{ fprintf(stderr, "Failed to CreateFile %s (error=%d)\n", pFileName, ::GetLastError()); return false; }
			::SetFilePointer(pMapping.mFile, (LONG)pSize, 0, FILE_BEGIN);
			::SetEndOfFile(pMapping.mFile);

			unsigned long const lProtection = PAGE_READWRITE;
			pMapping.mFileMapping = ::CreateFileMapping(pMapping.mFile, lSAptr, SEC_COMMIT | lProtection, 0, 0, pMappingName);
			if (!pMapping.mFileMapping)
			{
				fprintf(stderr, "Failed to CreateFileMapping %s (error=%d)\n", pMappingName, ::GetLastError());
				::CloseHandle(pMapping.mFile); pMapping.mFile = NULL;
				return false;
			}

			pMapping.mAddress = ::MapViewOfFileEx(pMapping.mFileMapping, FILE_MAP_WRITE/* | FILE_MAP_EXECUTE*/, 0, 0, 0, pHint);
			if (!pMapping.mAddress)
			{
				fprintf(stderr, "Failed to map %s at %p (error=%d)\n", pMappingName, pHint, ::GetLastError());
				::CloseHandle(pMapping.mFileMapping); pMapping.mFileMapping = NULL;
				::CloseHandle(pMapping.mFile); pMapping.mFile = NULL;
				return false;
			}
		#else
			pMapping.mFileMapping = -1;
			pMapping.mAddress = (void *)-1;

			// Make sure we don't fight with another daemon.
			{
				int const lFileMapping = ::shm_open(pMappingName, O_RDWR, 0);
				if (-1 != lFileMapping)
				{
					#if 1 // Review: Why does this always happen?
						fprintf(stderr, "Unexpected: file mapping %s already open by somebody!\n", pMappingName);
					#endif
					::close(lFileMapping);
					/**/return false;
				}
			}

			::shm_unlink(pMappingName);
			pMapping.mFileMapping = ::shm_open(pMappingName, O_RDWR | O_CREAT | O_EXCL, 0666);
			if (-1 == pMapping.mFileMapping)
				{ fprintf(stderr, "Failed to shm_open %s (error=%d)\n", pMappingName, errno); return false; }
			::ftruncate(pMapping.mFileMapping, pSize);

			pMapping.mAddress = ::mmap(pHint, pSize, PROT_READ | PROT_WRITE/* | PROT_EXEC*/, MAP_SHARED, pMapping.mFileMapping, 0);
			if (0 == pMapping.mAddress || (void *)-1 == pMapping.mAddress || (pHint && pHint != pMapping.mAddress))
			{
				fprintf(stderr, "Failed to map %s at %p (error=%d)\n", pMappingName, pHint, errno);
				if (pMapping.mAddress)
					{ ::munmap(pMapping.mAddress, pSize); pMapping.mAddress = 0; }
				::close(pMapping.mFileMapping); ::shm_unlink(pMappingName); pMapping.mFileMapping = -1;
				return false;
			}

			#ifdef TESTMEMORY
				memset(pMapping.mAddress, 0, pSize);
			#endif
		#endif
		pMapping.mName = pMappingName;
		printf("createFileMapping %s at %p succeeded\n", pMappingName, pHint);
		return true;
	}
	
	void closeFileMapping(FileMapping const & pMapping)
	{
		#ifdef WIN32
			::CloseHandle(pMapping.mFileMapping);
			::CloseHandle(pMapping.mFile);
		#else
			::close(pMapping.mFileMapping);
			::shm_unlink(pMapping.mName.c_str());
		#endif
	}
};

#endif
