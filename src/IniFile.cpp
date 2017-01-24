#include "IniFile.h"
#include <fstream>

/*	测试代码
	INIFile inifile;
	inifile.Create("abc.ini");
	string val;
	inifile.GetVar("SERVER","PORT",val);
	AfxMessageBox(val.c_str());
	inifile.SetVar("SERVER","PORT","test");
	inifile.Save("abc.ini");
*/

INIFile::INIFile()
{
	m_FileContent.erase(m_FileContent.begin(),m_FileContent.end());
	m_bCreated = false;
}

INIFile::INIFile(string filename)
{
	m_FileContent.erase(m_FileContent.begin(),m_FileContent.end());
	m_bCreated = false;
	Create(filename);
}

bool INIFile::Create(string filename)
{
	ifstream ifile;
	string strLine;
	if (filename == "")
	{
		return false;
	}
	ifile.open(filename.c_str());
	if (!ifile.is_open())
	{
		FILE *pfile = fopen(filename.c_str(),"wb+");
		if (pfile == NULL)
		{
			return false;
		}
		fclose(pfile);
		ifile.open(filename.c_str());
	}
	while (getline(ifile,strLine))
	{
		ProcessLine(strLine);
	}
	ifile.close();
	m_bCreated = true;
	return true;
}



bool INIFile::ProcessLine(string strLine)
{
	string key,val,sect;
	if (strLine[0] == '#')
	{
		//如果是注释直接返回
		return false;
	}
	if (strLine[0] == '[')
	{
		sect = strLine.substr(1,strLine.find("]")-1);
		SetVar(sect);
	}else if (strLine.find("=") != 0)
	{
		string::size_type ePos = strLine.find("=");
		SetVar(m_FileContent.back().SectionName,strLine.substr(0,ePos),strLine.substr(ePos+1,strLine.size()),false);
	}else
	{
		return false;
	}
	return true;
}

bool INIFile::SetVar(string strSection, string varName, string varvalue, bool breplace)
{
	//先查找段
	itVector it = m_FileContent.begin();
	for (it;it!=m_FileContent.end();it++)
	{
		if (it->SectionName == strSection)
		{
			break;
		}
	}
	//如果是加入段
	if (varName == "" && varvalue == "")
	{
		if (it != m_FileContent.end() && !breplace)
		{
			return false;
		}
		if (it != m_FileContent.end() && breplace)
		{
			m_FileContent.erase(it);
		}
		//加入这个段
		Section t;
		t.SectionName = strSection;
		m_FileContent.push_back(t);
	}else
	{
		//加入变量
		if (it == m_FileContent.end())
		{
			Section t;
			t.SectionName = strSection;
			m_FileContent.push_back(t);
			it = m_FileContent.end()-1;
		}
		if (breplace)
		{
			it->dMap.erase(varName);
		}
		it->dMap.insert(make_pair(varName,varvalue));
	}
	return true;
}

bool INIFile::GetVar(string strSection, string varName, string &varvalue)
{
	//先查找段
	itVector it = m_FileContent.begin();
	for (it;it!=m_FileContent.end();it++)
	{
		if (it->SectionName == strSection)
		{
			break;
		}
	}
	//如果段不存在
	if (it == m_FileContent.end())
	{
		return false;
	}
	//查找字典
	itmapDirectory it2 = it->dMap.find(varName);
	if (it2 == it->dMap.end())
	{
		return false;
	}
	varvalue = it2->second;
	return true;
}

bool INIFile::Save(string filename)
{
	ofstream ofile;
	string strLine;
	if (filename == "")
	{
		return false;
	}
	ofile.open(filename.c_str());
	if (!ofile.is_open())
	{
		return false;
	}
	itVector it = m_FileContent.begin();
	for (it;it!=m_FileContent.end();it++)
	{
		ofile<<"["<<it->SectionName<<"]"<<endl;
		itmapDirectory itd = it->dMap.begin();
		for (itd;itd!=it->dMap.end();itd++)
		{
			ofile<<itd->first<<"="<<itd->second<<endl;
		}
	}
	ofile.close();
	return false;
}

