/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef MVTArgs_H
#define MVTArgs_H

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>

/*
 * The goal of the class to parse the standard main(int argc, char *var)
 * command line string and remember the parameters in the following manner: 
 * 
 * - original command line; 
 * - vector<string> , where each element corresponds to one of char **var[] elements;
 * - map(key, value), which was build from '-parameter=value' element within command line; 
 * - map(key, value), which was build from '--parameter=value' or 'parameter=value' elements
 *                   within the command line.
 *
 * Interface functions  provide easy access to the parameters. 
 */

class MVTArgs
{
public:
	typedef std::map<std::string,std::string> ARGSMap;
    typedef std::map<std::string,std::string>::iterator mappos;
    typedef std::pair<ARGSMap::iterator,bool> dpair;
	typedef std::map<std::string,ARGSMap*> revARGMap;
    typedef std::map<std::string,ARGSMap*>::iterator maprouter;
    typedef std::vector<std::string> cmdARGS;
	
	enum SF {
		P_ALL    = 1,
		P_GLOBAL = 2,
		P_LOCAL  = 4,
		P_NFRMT  = 8
	};
	
	MVTArgs(unsigned int c, char **v);
	~MVTArgs();
	
	//Functions below - most common interface:
	virtual bool get_paramOldFashion(int &argc, char**& argv);
	virtual unsigned long get_cntParamTotal()const{return (unsigned long) m_var.size();}
    
	virtual const char * get_arvByIndex(unsigned int ind){
		return (ind>m_var.size()? NULL :m_var[ind].c_str());
	}
	
	template<class Key> bool param_present(Key arv)const { return m_revargmap.find(arv)==m_revargmap.end()? false :true;}	
	template<class Key, class Val> bool get_param(Key skey, Val & value,SF sf = P_ALL)
	{
		std::istringstream stm; mappos pos;
		if(P_ALL == sf){
		   maprouter rpos = m_revargmap.find(skey);  	   
		   if( rpos == m_revargmap.end()) return false;
		   pos = (*(*rpos).second).find(skey);
		}else if(P_GLOBAL == sf){
		   pos = m_gparam.find(skey);  	   
		   if( pos == m_gparam.end()) return false;
		}else if(P_LOCAL == sf){
		   pos = m_testparam.find(skey);  	   
		   if( pos == m_testparam.end()) return false;
		}
		stm.str((*pos).second); stm >> value; return true; 
	};
	virtual std::string get_allparamstr(std::string *pval=NULL,SF sf=P_ALL);
	
	
	//Functions below - advanced interface, less used but usefull:
	virtual unsigned long get_cntParamGlobal()const{return (unsigned long) m_gparam.size();}
	virtual unsigned long get_cntParamTest()const{return (unsigned long)m_testparam.size();}
	virtual unsigned long get_cntParamNFrmt()const{return (unsigned long)m_nfrmtParam.size();}
	
	virtual ARGSMap * get_globalParam() {return &m_gparam;}
	virtual ARGSMap * get_localParam()  {return &m_testparam;}
	virtual ARGSMap * get_cmdFlags()    {return &m_nfrmtParam;}
	
	private: 
    MVTArgs() {}
	MVTArgs(const MVTArgs& cp){}
	
	char **  var;           //pointer to original command line 
	unsigned int cnt;       //pointer to original param counter 
	
	cmdARGS   m_var;        //array of all arquments, including substituted from @file(s)
		
	ARGSMap   m_nfrmtParam; //parameteres with no format... 
	ARGSMap   m_gparam;     //map of global parameters, format -pname=pvalue;
	ARGSMap   m_testparam;  //map of local(test) parameters in pairs; formats: --pname=pvalue   	
	                        //or pname=pvalue;
	revARGMap m_revargmap;  //'main index ' for parameter look-up
	
	std::vector<char **>   m_cleanupChPP;
    std::vector<std::string *>  m_cleanupS;

	int normalizeString(std::string & pString,std::string &key, std::string &value);
    int normalizeString(std::string & pString,bool lowcase = false);
};

#endif
