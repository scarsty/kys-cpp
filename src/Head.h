#pragma once
#include <windows.h>
#include <string>
static class Head
{
public:
	static int inEvent;	//是否正在事件中
	static int CurEvent;
	static int CurItem;
	
};
struct NAlist
{
	short  Number, Amount;
};
extern std::string GBKToUTF8(const std::string& strGBK);
extern std::string Utf8ToUnicode(const std::string& utf);
extern std::string UnicodeToUtf8(const std::string& unicode);