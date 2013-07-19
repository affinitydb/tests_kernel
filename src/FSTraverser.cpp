/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "stdafx.h"
#include "FSTraverser.h"
#include<iostream>
#include<algorithm>

#ifdef POSIX
#include <fnmatch.h>
#endif

using namespace std;

long FSTraverser::getFirstFile( FSFileInfo& fileInfo, const char* pcDirectory, const char* pcSearchName )
{
	string strDirectoryPath (pcDirectory);
	if((strDirectoryPath[strDirectoryPath.length() - 1] != FS_PATH_DELIM )&&(strDirectoryPath[strDirectoryPath.length() - 1] != FS_WRONG_PATH_DELIM ))
		strDirectoryPath += FS_PATH_DELIM ;
	
	#ifdef WIN32
		if(pcSearchName == NULL) 
			return INVALID_SEARCH_STRNG;
		strDirectoryPath += pcSearchName;
		m_fileHandle  = FindFirstFileA(strDirectoryPath.c_str(), &fileInfo.getFileInfo());	
		if (m_fileHandle == INVALID_HANDLE_VALUE)
			return GetLastError();
		return 0;
	#else
		m_strDirPath = strDirectoryPath ;//shud this be an only linux member ????
		m_strFileName = pcSearchName ;//shud this be an only linux member ????
		m_fileHandle = opendir(strDirectoryPath.c_str());
		if (m_fileHandle == NULL)
			return INVALID_SEARCH_STRNG;
		else
			return FSTraverser::getNextFile(fileInfo);
	#endif
}

long FSTraverser::getNextFile(FSFileInfo& fileInfo )
{
	if( m_fileHandle == INVALID_HANDLE_VALUE )
		return FS_INVALID_HANDLE_VALUE;
	FILEINFO_DATATYPE fileInfoHandle;

#ifdef WIN32
	if(FindNextFileA(m_fileHandle,&fileInfoHandle) == (BOOL)true)
	{
		fileInfo.setFileInfo(fileInfoHandle);
		return 0;
	}
#else
	while( (fileInfoHandle.dp = readdir (m_fileHandle)) != NULL )
	{
		if( isMatching(fileInfoHandle.dp->d_name, m_strFileName ))
		{
			stat((m_strDirPath+(fileInfoHandle.dp->d_name)).c_str(),&fileInfoHandle.m_statObject);
			fileInfo.setFileInfo(fileInfoHandle);
			return 0;
		}
	}
#endif
	return NO_MORE_FILES_FOUND;
}

long FSTraverser::getFirstDir(FSFileInfo& fileInfo, const char* pcDirectory)
{
	if(getFirstFile(fileInfo,pcDirectory,"*")==0)
	{
		do{
			if(fileInfo.isDirectory() && fileInfo.getFileName() != FS_DOT_DIRECTORY && fileInfo.getFileName() != FS_DOTDOT_DIRECTORY)
				return 0;
		}while(getNextFile(fileInfo)==0);
	}

	return NO_MORE_FILES_FOUND;
}

long FSTraverser::getNextDir(FSFileInfo& fileInfo)
{
	while(getNextFile(fileInfo)==0)
	{
		if(fileInfo.isDirectory() && fileInfo.getFileName() != FS_DOT_DIRECTORY && fileInfo.getFileName() != FS_DOTDOT_DIRECTORY)
			return 0;
	}

	return NO_MORE_FILES_FOUND;
}

void FSTraverser::getAllfiles(vector<string>& foundPaths ,const char* pcDirectory, const char* pcSearchName, bool recursive)
{
	string strDirectoryPath (pcDirectory);
	if(strDirectoryPath[strDirectoryPath.length() - 1] != FS_PATH_DELIM )
		strDirectoryPath += FS_PATH_DELIM ;

	getAllMatchingFilesInDir(foundPaths,strDirectoryPath,pcSearchName);
	if(recursive)
	{
		FSFileInfo fileInfo	;
		FSTraverser traverserObj;
		if(traverserObj.getFirstFile(fileInfo,strDirectoryPath.c_str(),FS_ALLFILES) == 0)
		do
		{
			if(fileInfo.isDirectory() && (fileInfo.getFileName() != FS_DOT_DIRECTORY) && (fileInfo.getFileName() != FS_DOTDOT_DIRECTORY ) )
			{
				string subDir = strDirectoryPath;
				subDir += fileInfo.getFileName();
				getAllfiles(foundPaths,subDir.c_str(),pcSearchName,recursive);
			}
		}while(traverserObj.getNextFile(fileInfo) != NO_MORE_FILES_FOUND);
	}
}
void FSTraverser::getAllDirs(vector<string>& foundPaths ,const char* pcDirectory, bool recursive,const char* pcSearch)
{
		FSFileInfo fileInfo	;
		FSTraverser traverserObj;
		if(traverserObj.getFirstFile(fileInfo,pcDirectory,pcSearch) == 0)
		do
		{
			if(fileInfo.isDirectory() && (fileInfo.getFileName() != FS_DOT_DIRECTORY) && (fileInfo.getFileName() != FS_DOTDOT_DIRECTORY ) )
			{
				string subDir = pcDirectory;

				subDir += "/" + fileInfo.getFileName();
                if(recursive)
                {
				    getAllDirs(foundPaths,subDir.c_str(),true);
                }
				foundPaths.push_back(subDir);
			}
		}while(traverserObj.getNextFile(fileInfo) != NO_MORE_FILES_FOUND);
}

void FSTraverser::close()
{
		#ifdef WIN32
		if( m_fileHandle != INVALID_HANDLE_VALUE )FindClose(m_fileHandle);
		#else
		if( m_fileHandle != INVALID_HANDLE_VALUE )closedir(m_fileHandle);
		#endif
		m_fileHandle = INVALID_HANDLE_VALUE;
}

void FSTraverser::getAllMatchingFilesInDir(vector<string>& fileList,string pcDirectory, string pcSearchName)
{
		FSFileInfo fileInfo	;
		FSTraverser traverserObj;

		if(traverserObj.getFirstFile(fileInfo,pcDirectory.c_str(),pcSearchName.c_str())==0)
		do
		{
			if(!fileInfo.isDirectory() && fileInfo.getFileName() != FS_DOT_DIRECTORY && fileInfo.getFileName() != FS_DOTDOT_DIRECTORY )
			{
				fileList.push_back(pcDirectory + fileInfo.getFileName());
			}
		}while(traverserObj.getNextFile(fileInfo)!= NO_MORE_FILES_FOUND);
}


bool FSTraverser::isMatching(string foundName, string givenName)
{
#ifdef WIN32
	return false;
#else
	if( fnmatch(givenName.c_str(),foundName.c_str(),0) == FNM_NOMATCH)
		return false;
	else 
		return true;
#endif
}
