#include "config.h"
#include "IniFile.h"

Config* Config::m_pInstance = new Config;
Config* Config::GetInstance()
{
	return m_pInstance;
}


void Config::GetWindowParameter()
{
	INIFile inifile;
	inifile.Create("sysfile\System.ini");
	string valWidth, valHeight;
	inifile.GetVar("System", "WindowsWidth", valWidth);
	inifile.GetVar("System", "WindowsHeight", valHeight);
	if (valWidth != ""&&valHeight != "")
	{
		WindowsHeight = atoi(valHeight.c_str());
		WindowsWidth = atoi(valWidth.c_str());
	}
	else
	{
		WindowsWidth = 1200;
		WindowsHeight = 600;
	}

}

void Config::GetWindowTitle()
{
	WindowsTitle = "ÈËÔÚ½­ºþ1.0";
}