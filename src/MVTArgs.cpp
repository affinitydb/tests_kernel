/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

/* implementeation file */
#include <algorithm>
#include "MVTArgs.h"

MVTArgs::~MVTArgs()
{
  for(std::vector<char **>::iterator p = m_cleanupChPP.begin(); p != m_cleanupChPP.end(); p++) delete (*p);
  for(std::vector<std::string *>::iterator p = m_cleanupS.begin(); p != m_cleanupS.end(); p++) delete (*p);
}

bool MVTArgs::get_paramOldFashion(int &argc, char**& argv)
	{
	   char** largv;	
       argc = (int)m_var.size();
	   if(NULL == argv){ 
		   largv = argv = new char*[argc]; 
		   if( NULL == largv) return false;
		   m_cleanupChPP.push_back(largv);
	   }else
			largv = argv;
	   for(cmdARGS::iterator p = m_var.begin(); p!= m_var.end(); p++,largv++)
			*largv = (char *)(*p).c_str();
	   return true;
}

MVTArgs::MVTArgs(unsigned int c, char **v):var(v),cnt(c){
   
	m_gparam.insert(ARGSMap::value_type(std::string("pname"),std::string(var[0])));
	m_revargmap[std::string("pname")] = &m_gparam;
	m_var.push_back(std::string(var[0]));
	
	if(1 == cnt) return;
	//Assembling the line of parameters. 
	//Parameter @file is treated as file with parameters inside 
	//@file is going to be substituded with corresponding parameters ...  
	for (unsigned int ii = 1; ii < cnt ; ii++)
	{
	  std::string pString(var[ii]); 
	   	
	  if('@' == ((char *)pString.c_str())[0])
	  {
		pString = pString.substr(1,pString.length());
		std::ifstream lIs(pString.c_str(),std::ifstream::in);
        while(!lIs.eof() && lIs.good()){   	 	
		    lIs >> pString;
			if(lIs.eof()) break;
			m_var.push_back(pString);
		}
		lIs.close();  
	  }else
		{
			m_var.push_back(pString);
		}
	}
	
	for (cmdARGS::iterator p = m_var.begin(); p != m_var.end(); p++)
	{
	   std::string key, value, pString = *p; 
	   switch(normalizeString(pString,key,value))
		{
			case 0: 
	            m_gparam.insert(ARGSMap::value_type(key,value));
			    m_revargmap[key] = &m_gparam;
				break;
			case 1: 
	        case 2: 
				m_testparam.insert(ARGSMap::value_type(key,value));
			    m_revargmap[key] = &m_testparam;
				break;
			case (-1): 
				m_nfrmtParam.insert(ARGSMap::value_type(value,"1"));
	            m_revargmap[value] = &m_nfrmtParam;
				break;
			
			default:
				break;
    	}
	}
}

int MVTArgs::normalizeString(std::string & pString,std::string &key, std::string &value)
{
   normalizeString(pString);	
   int split = (int)pString.find("=");
   if((-1) != split){
     value = pString.substr(split+1, pString.length()); 
	 if("" == value) return (-2);  
     key   = pString.substr(0, split);
	  
	 normalizeString(key,true);  
	 if(('-' == ((char *)key.c_str())[0]) && ('-' == ((char *)key.c_str())[1]))
		{
			key = key.substr(2,key.length());
			return 1;
		}else if('-' == ((char *)key.c_str())[0])
		  {
			key = key.substr(1,key.length());
			return 0;
		  }   
	return 2;
   }else
	{
	  value = pString;
	}
   return split; 	
}

int MVTArgs::normalizeString(std::string & pString, bool lowcase)
{
	// Remove heading spaces/tabs.
	while (!pString.empty() && isspace(pString.at(0)))
		pString.erase(pString.begin());
	// Remove trailing spaces/tabs.
	for (;;)
	{
		if (0 == pString.length()) break;
		if (!isspace(pString.at(pString.length() - 1))) break;
		pString.erase(pString.length() - 1);
	}
	// Convert to lower caps.afafa
	if (lowcase && (!pString.empty()))
	    std::transform(pString.begin(), pString.end(), pString.begin(), (int(*)(int))tolower);
    return 0;
}

std::string MVTArgs::get_allparamstr(std::string * pval,SF sf)
{
		if(pval == NULL){ pval = new std::string; m_cleanupS.push_back(pval);} 
		
	    if(P_ALL == sf){
	    	for(cmdARGS::iterator p = m_var.begin(); p != m_var.end(); p++)
		 	{
		   		(*pval) += (*p)+ " ";
			}
		}else{
			if(sf&P_GLOBAL)
			{
				for(mappos p = m_gparam.begin(); p != m_gparam.end(); p++)
		 		{
		   			(*pval) += "-" + (*p).first+ "=" + (*p).second + " ";
				}
			}
			if(sf&P_LOCAL)
			{
				for(mappos p = m_testparam.begin(); p != m_testparam.end(); p++)
		 		{
		   			(*pval) += "--" + (*p).first+ "=" + (*p).second + " ";
				}
			}
			if(sf&P_LOCAL)
			{
				for(mappos p = m_nfrmtParam.begin(); p != m_nfrmtParam.end(); p++)
		 		{
		   			(*pval) += "-" + (*p).first + " ";
				}
			}
		}	
		return (*pval); 
}
