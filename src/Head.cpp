
#include "Head.h"
int Head::inEvent = -1;
int Head::CurEvent = -1;
int Head::CurItem = -1;
extern std::string GBKToUTF8(const std::string& strGBK)
{
	std::string strOutUTF8 = "";
	WCHAR * str1;
	int n = MultiByteToWideChar(CP_ACP, 0, strGBK.c_str(), -1, NULL, 0);
	str1 = new WCHAR[n];
	MultiByteToWideChar(CP_ACP, 0, strGBK.c_str(), -1, str1, n);
	n = WideCharToMultiByte(CP_UTF8, 0, str1, -1, NULL, 0, NULL, NULL);
	char * str2 = new char[n];
	WideCharToMultiByte(CP_UTF8, 0, str1, -1, str2, n, NULL, NULL);
	strOutUTF8 = str2;
	delete[] str1;
	str1 = NULL;
	delete[] str2;
	str2 = NULL;
	return strOutUTF8;
}
extern std::string Utf8ToUnicode(const std::string& utf)
{
	int dwUnicodeLen = MultiByteToWideChar(CP_UTF8, 0, utf.c_str(), -1, NULL, 0);
	size_t num = dwUnicodeLen * sizeof(wchar_t);
	wchar_t *pwText = (wchar_t*)malloc(num);
	memset(pwText, 0, num);
	MultiByteToWideChar(CP_UTF8, 0, utf.c_str(), -1, pwText, dwUnicodeLen);
	std::string str;
	str = (char*)(pwText);
	return str;
}

extern std::string UnicodeToUtf8(const std::string& unicode)
{
	int len;
	wchar_t *wc = (wchar_t *)unicode.c_str();
	len = WideCharToMultiByte(CP_UTF8, 0, wc, -1, NULL, 0, NULL, NULL);
	char *szUtf8 = (char*)malloc(len + 1);
	memset(szUtf8, 0, len + 1);
	WideCharToMultiByte(CP_UTF8, 0, wc, -1, szUtf8, len, NULL, NULL);
	return szUtf8;
}