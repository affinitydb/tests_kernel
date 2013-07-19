/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "stdafx.h"
#include "FSFileInfo.h"

using namespace std;

const string FSFileInfo::getFileName()
{
	#ifdef WIN32
	if(FSFileInfo::isDirectory())
	{
		string strDir = m_FindData.cFileName;
		char acTemp[2]={FS_PATH_DELIM,0};
		strDir.append(acTemp);
		return strDir.c_str();
	}
	else
	{
		return string(m_FindData.cFileName);
	}
	#else
		if(FSFileInfo::isDirectory())
		{
			string strDir = m_FindData.dp->d_name;
			char acTemp[2]={FS_PATH_DELIM,0};
			strDir.append(acTemp);
			return strDir.c_str();
		}
		else
		{
			return string(m_FindData.dp->d_name);
		}	

	#endif	

}
long FSFileInfo::getFileSize()
{
	#ifdef WIN32
		//adding two longs and storing back in a long.. might overflow!!! ;
		return (long)m_FindData.nFileSizeHigh + (long)m_FindData.nFileSizeHigh ; 
	#else
		return (long)m_FindData.m_statObject.st_size;
	#endif	
}
bool FSFileInfo::isDirectory()
{
	#ifdef WIN32
		if (m_FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			return true;
		return false;
	#else
		if(S_ISDIR(m_FindData.m_statObject.st_mode))
			return true ;
		else
			return false;
	#endif	
}
FILEINFO_DATATYPE& FSFileInfo::getFileInfo()
{
	return m_FindData;
}


void FSFileInfo::setFileInfo(FILEINFO_DATATYPE file_infoHandle)
{
	m_FindData = file_infoHandle;	
}
