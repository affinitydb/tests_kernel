/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "utils.h"
#include <errno.h>

#ifndef WIN32
#include <arpa/inet.h>
#endif

using namespace mvcore;

#define ENTRY( x ) case(x) : return #x;

std::string utilities::errno2str(int error)
{
	// linux error codes (although also possible if using equiv sys calls on windows)

	// errno values
	switch(error)
	{
	ENTRY(ENOENT)
	ENTRY(ENOMEM)
	ENTRY(EACCES)
	ENTRY(EBUSY)
	ENTRY(EEXIST)
	ENTRY(ENODEV)
	ENTRY(ENOTDIR)
	ENTRY(EINVAL)
	ENTRY(ENOLCK)
	ENTRY(ENOTEMPTY)
	}
	char buf[16];
	sprintf(buf,"%d",error);
	return buf;
}

#ifdef WIN32
std::string utilities::winerr2str(int error, bool userfriendly)
{
	// ::GetLastError values in winerror.h
	// Note: this doesn't handle the HRESULT equivalents 0x8....
	// but they can be supported if needed

	if ( !userfriendly )
	{
		switch(error)
		{
		ENTRY(ERROR_SUCCESS)
		ENTRY(ERROR_FILE_NOT_FOUND)
		ENTRY(ERROR_PATH_NOT_FOUND)
		ENTRY(ERROR_TOO_MANY_OPEN_FILES)
		ENTRY(ERROR_ACCESS_DENIED)
		ENTRY(ERROR_INVALID_HANDLE)
		ENTRY(ERROR_NOT_ENOUGH_MEMORY)
		ENTRY(ERROR_OUTOFMEMORY)
		ENTRY(ERROR_INVALID_DRIVE)
		ENTRY(ERROR_NOT_SAME_DEVICE)
		ENTRY(ERROR_WRITE_PROTECT)
		ENTRY(ERROR_SHARING_VIOLATION)
		ENTRY(ERROR_LOCK_VIOLATION)
		ENTRY(ERROR_HANDLE_DISK_FULL)
		ENTRY(ERROR_NOT_SUPPORTED)
		ENTRY(ERROR_INVALID_PARAMETER)
		ENTRY(ERROR_OPEN_FAILED)
		ENTRY(ERROR_DISK_FULL)
		ENTRY(ERROR_INVALID_NAME)
		ENTRY(ERROR_DIR_NOT_EMPTY)
		ENTRY(ERROR_ALREADY_EXISTS)
		}

		// For other errors fall through and print windows string
	}

	// Get error description from Windows
	// REVIEW: perhaps should always use this, rather than the options above,
	// because the results will be more friendly?
    LPVOID msgBuf = NULL;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR) &msgBuf,
		0, NULL );

	if ( msgBuf == NULL )
	{
		return "";
	}
	std::string errMsg((char*)msgBuf);
	errMsg = errMsg.substr(0,errMsg.length()-1); // Get rid of line return
	return errMsg;
}
#endif

std::string utilities::err2str(int error)
{
	// Helper for logging meaningful string from 
	// common windows/linux error codes
#ifdef WIN32
	return winerr2str(error);
#else
	return errno2str(error);
#endif
}

#if 0
bool utilities::isLan(uint32_t ipAddr)
{
    static uint32_t begin172 = ntohl(inet_addr("172.16.0.0"));
    static uint32_t end172 = ntohl(inet_addr("172.31.255.255"));

    static uint32_t begin10 = ntohl(inet_addr("10.0.0.0"));
    static uint32_t end10 = ntohl(inet_addr("10.255.255.255"));

    static uint32_t begin192 = ntohl(inet_addr("192.168.0.0"));
    static uint32_t end192 = ntohl(inet_addr("192.168.255.255"));

    if (begin192 <= ipAddr && end192 >= ipAddr)
        return true;
    else if (begin10 <= ipAddr && end10 >= ipAddr)
        return true;
    else if (begin172 <= ipAddr && end172 >= ipAddr)
        return true;

    return false;
}

bool utilities::isLan(const char* pcIP)
{
    if (!pcIP)
        return false;

    return isLan(ntohl(inet_addr(pcIP)));
}

bool utilities::isLocal(uint32_t ipAddr)
{
     static uint32_t begin127 = ntohl(inet_addr("127.0.0.1"));
     static uint32_t end127 = ntohl(inet_addr("127.255.255.255"));   

    if (begin127 <= ipAddr && end127 >= ipAddr)
        return true;

    return false;
}

bool utilities::isLocal(const char* pcIP)
{
     if (!pcIP)
        return false;

     return isLocal(ntohl(inet_addr(pcIP)));
}

bool utilities::isInternal(uint32_t ipAddr)
{
    return (isLan(ipAddr) || isLocal(ipAddr));
}

bool utilities::isInternal(const char* pcIP)
{
    if (!pcIP)
        return false;

    return (isLan(pcIP) || isLocal(pcIP));
}
#endif
