/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef __FS_FILEINFO__
#define __FS_FILEINFO__
#include<string>
#ifdef WIN32
#include <windows.h>
#else
#include <dirent.h>
#include<sys/types.h>
#include<sys/stat.h>
#endif

#ifdef WIN32
#else
struct LINUX_FIND_DATA
{
    struct stat m_statObject;
    struct dirent* dp;
};

#endif

#ifdef WIN32
	#define FS_PATH_DELIM '\\'
	#define FS_PATH_DELIM_STR "\\"

	#define FS_WRONG_PATH_DELIM '/'
	#define FS_DOT_DIRECTORY ".\\"
	#define FS_DOTDOT_DIRECTORY "..\\"
	#define FS_FILEHANDLE HANDLE
#else
	#define FS_PATH_DELIM '/'
	#define FS_PATH_DELIM_STR "/"
	#define FS_WRONG_PATH_DELIM '\\'
	#define FS_DOT_DIRECTORY "./"
	#define FS_DOTDOT_DIRECTORY "../"
	#define FS_FILEHANDLE DIR*
#endif

#ifdef WIN32
#define FILEINFO_DATATYPE WIN32_FIND_DATAA
#else
#define FILEINFO_DATATYPE LINUX_FIND_DATA
#endif

class FSFileInfo
{
private:
	FILEINFO_DATATYPE m_FindData;
public:
	FSFileInfo(){}
	FILEINFO_DATATYPE& getFileInfo();
	const std::string getFileName();
	long getFileSize();
	bool isDirectory();
	void setFileInfo(FILEINFO_DATATYPE file_infoHandle);

};
#endif

