/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#ifndef  __FS_TRAVERSER__
#define __FS_TRAVERSER__

#include "FSFileInfo.h"
#include<string>
#include<vector>

#ifdef WIN32
	#include <windows.h>
#else
	#include <dirent.h>
	#include<sys/types.h>
	#include<sys/stat.h>
#endif


#ifdef WIN32
	#define FS_PATH_DELIM '\\'
	#define FS_WRONG_PATH_DELIM '/'
	#define FS_DOT_DIRECTORY ".\\"
	#define FS_DOTDOT_DIRECTORY "..\\"
	#define FS_FILEHANDLE HANDLE
	#define FS_ALLFILES			"*.*"
#else
	#define FS_PATH_DELIM '/'
	#define FS_WRONG_PATH_DELIM '\\'
	#define FS_DOT_DIRECTORY "./"
	#define FS_DOTDOT_DIRECTORY "../"
	#define FS_FILEHANDLE DIR*
	#define INVALID_HANDLE_VALUE NULL
	#define FS_ALLFILES			"*"
#endif

#define INVALID_SEARCH_STRNG -1
#define NO_MORE_FILES_FOUND -2
#define FS_INVALID_HANDLE_VALUE -4

class FSTraverser
{
private:
	FS_FILEHANDLE m_fileHandle;
	#ifdef WIN32
	#else
	std::string m_strFileName;//shud be only for linux
	std::string m_strDirPath;//shud be only for linux
	#endif

private:
	//helper functions
	bool isMatching(std::string foundName, std::string givenName);
	static void getAllMatchingFilesInDir(std::vector<std::string>& fileList,std::string pcDirectory,std::string pcSearchName);
	
public:
	FSTraverser():m_fileHandle(INVALID_HANDLE_VALUE){}
	~FSTraverser()
	{
		close();
	}
    
	long getFirstDir(FSFileInfo& fileInfo, const char* pcDirectory);
	long getNextDir(FSFileInfo& fileInfo);
	long getFirstFile( FSFileInfo& fileInfo, const char* pcDirectory, const char* pcSearchName );	
	long getNextFile(FSFileInfo& fileInfo);
	static void getAllfiles(std::vector<std::string>& foundPaths ,const char* pcDirectory, const char* pcSearchName, bool recursive);
	static void getAllDirs(std::vector<std::string>& foundPaths ,const char* pcDirectory, bool recursive,const char* pcSearch=FS_ALLFILES);
	void close();
};
#endif
