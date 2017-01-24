#ifndef INIFILE_H
#define INIFILE_H

#include <map>
#include <string>
#include <vector>

using namespace std;

typedef map<string,string> mapDirectory;
typedef map<string,string>::iterator itmapDirectory;

typedef struct _Section
{
	mapDirectory dMap;
	string SectionName;
}Section;


typedef vector<Section> vecMap;
typedef vector<Section>::iterator itVector;

class INIFile
{
public:
	inline bool IsCreated(){return m_bCreated;};
	bool Save(string filename);
	bool GetVar(string strSection,string varName,string &varvalue);
	bool SetVar(string strSection,string varName = "",string varvalue = "",bool breplace = true);
	bool ProcessLine(string strLine);
	INIFile();
	INIFile(string filename);
	bool Create(string filename);
//private:
	vecMap m_FileContent; 
	bool m_bCreated;
};
#endif
