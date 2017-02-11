#ifndef _INI_READER_H_
#define _INI_READER_H_

#include<fstream>
#include<map>
#include<list>
#include<string>
using namespace std;

/*
.ini 文件 解析器
Section和Key只能为字母、数字、下划线、空格、减号、点（且首字符只能为字母数字或下划线，自动忽略前导后导空格）
Value不能为制表符、换行、分号等（自动忽略前导后导空格，值可以为空）
整行不能超过4096字节
某个标识符不能超过2048字节
*/
class IniReader
{
public:
	/**
	@Purpose: 构造函数
	@Parameter: file_name 文件名（路径）
	@Throw: 构造失败则throw IniReaderError
	*/
	IniReader(string file_name);
	/**
	@Purpose: 析构函数
	*/
	~IniReader();
	/**
	@Purpose: 列出Section
	@Parameter: section_address 字符串数组的地址（用于存放结果，函数内部分配内存，为NULL表示不需要结果）
	@Return: Section的个数
	*/
	int ListSection(string** section_address = NULL);
	/**
	@Purpose: 列出某Section的Key
	@Parameter: section Section字符串
	@Parameter: key_address 字符串数组的地址（用于存放结果，函数内部分配内存，为NULL表示不需要结果）
	@Return: Key的个数
	*/
	int ListKey(string section, string** key_address = NULL);
	/**
	@Purpose: 获取某Section下某Key对应的Value
	@Parameter: section Section字符串
	@Parameter: key Key字符串
	@Parameter: default_value 默认值（表示没有此Section下的Key-Value对，则返回这个默认值）
	@Return: Value字符串
	*/
	string GetValue(string section, string key, string default_value = "");
private:
	map<string, map<string, string>*> smap; //Section-Key-Value Map
	list<string> slist; //Section List
	map<string, list<string>*> skmap; //Section-(Key List) Map
};

/**
IniReader错误
错误类型：
-1：文件打开失败
-2：Section不以字母或下划线开头
-3：Section没有闭合
-4：键值对缺少=
-5：缺少值（已废弃）
-6：发现没有属于任何Section的Key-Value对
*/
struct IniReaderError
{
	int type; //错误类型
	int line; //行数
	IniReaderError(int t, int l);
};

#endif