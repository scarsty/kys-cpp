#include "IniFile.h"

/* 是否是Value的合法字符 */
inline bool IsValue(char& c)
{
	if ((c > 31 && c != 127 && c != ';') || c < 0)
		return true;
	return false;
}

/* 是否是字母 */
inline bool IsLetter(char& c)
{
	if (c >= 'a' && c <= 'z')
		return true;
	if (c >= 'A' && c <= 'Z')
		return true;
	return false;
}

/* 是否是数字 */
inline bool IsDigital(char& c)
{
	if (c >= '0' && c <= '9')
		return true;
	return false;
}

/* 是否是合法的标识符 */
inline bool IsIdentifier(char& c)
{
	if (IsLetter(c) || IsDigital(c) || '_' == c || '.' == c || ' ' == c || '-' == c)
		return true;
	return false;
}

/* 是否是合法的标识符首字符 */
inline bool IsIdentifierFirst(char& c)
{
	if (IsLetter(c) || '_' == c || IsDigital(c))
		return true;
	return false;
}

/* 去除尾部空格（end为'\0'的位置） */
inline void DeleteBlank(char* s, int end)
{
	int i = end - 1;
	while (s[i] == ' ')
		i--;
	s[++i] = '\0';
}

IniReaderError::IniReaderError(int t, int l)
{
	type = t;
	line = l;
}

IniReader::IniReader(string file_name)
{
	ifstream in(file_name.c_str());
	if (in.fail())
		throw IniReaderError(-1, 0); //文件打开失败
	char s[4096]; //记录字符
	char v[2048]; //记录标识
	int i, j; //计数器
	int line = 0; //行数
	string section; //当前Section
	string key; //当前key
	list<string>* cur_kl = NULL; //当前Key列表
	map<string, string>* cur_kv = NULL; //当前键值Map 
	while (!in.eof())
	{
		line++; //行数++
		in.getline(s, 4096); //读一行
		i = 0;
		while (s[i] == ' ')
			i++; //忽略前导空格
		j = 0;
		v[0] = '\0';
		if (s[i] == '[') //Section
		{
			i++;
			while (s[i] == ' ')
				i++; //忽略前导空格
			if (IsIdentifierFirst(s[i]))
				v[j++] = s[i++]; //标识符首字符
			if (j == 0)
				throw IniReaderError(-2, line); //Section不以字母或下划线开头
			while (IsIdentifier(s[i]))
				v[j++] = s[i++]; //读取标识符
			v[j] = '\0';
			DeleteBlank(v, j); //去除后导空格
			if (s[i] != ']')
				throw IniReaderError(-3, line); //Section没有闭合
			string ts(v);
			section = ts; //获取Section
			slist.push_back(section); //加入ListSection
			cur_kl = new list<string>(); //构造Key列表
			cur_kv = new map<string, string>(); //构造Key-Value对
			skmap.insert(pair<string, list<string>*>(section, cur_kl)); //加入skmap
			smap.insert(pair<string, map<string, string>*>(section, cur_kv)); //加入smap
		}
		else if (IsIdentifierFirst(s[i])) //Key-Value对
		{
			v[j++] = s[i];
			i++;
			while (IsIdentifier(s[i]))
				v[j++] = s[i++]; //读取Key
			v[j] = '\0';
			DeleteBlank(v, j); //去除后导空格
			string ts(v);
			key = ts;
			if (s[i] != '=')
				throw IniReaderError(-4, line); //键值对缺少=
			i++; //跳过=
			while (s[i] == ' ')
				i++; //忽略空格
			j = 0;
			v[0] = '\0';
			while (IsValue(s[i]))
				v[j++] = s[i++]; //读取值
			v[j] = '\0';
			DeleteBlank(v, j); //去除后导空格
							   /*
							   可以为空值（因此注释了）
							   if(j==0)
							   throw IniReaderError(-5,line); //缺少值
							   */
			string value(v);
			if (cur_kl == NULL)
				throw IniReaderError(-6, line); //发现没有属于任何Section的Key-Value对
			cur_kl->push_back(key); //加入列表
			cur_kv->insert(pair<string, string>(key, value)); //加入映射
		}
	}
}

IniReader::~IniReader()
{
	//避免内存泄露
	if (smap.size() > 0)
		for (map<string, map<string, string>*>::iterator i = smap.begin(); i != smap.end(); i++)
			delete i->second;
	if (skmap.size() > 0)
		for (map<string, list<string>*>::iterator i = skmap.begin(); i != skmap.end(); i++)
			delete i->second;
}

int IniReader::ListSection(string** section_address)
{
	if (section_address == NULL || slist.size() == 0)
		return slist.size();
	(*section_address) = new string[slist.size()]; //分配内存
	int j = 0;
	for (list<string>::iterator i = slist.begin(); i != slist.end(); i++, j++)
		(*section_address)[j] = *i; //遍历拷贝
	return j;
}

int IniReader::ListKey(string section, string** key_address)
{
	list<string>* l;
	if (skmap.count(section) > 0)
		l = skmap[section];
	else
		return 0;
	if (key_address == NULL || l->size() == 0)
		return l->size();
	(*key_address) = new string[l->size()];
	int j = 0;
	for (list<string>::iterator i = l->begin(); i != l->end(); i++, j++)
		(*key_address)[j] = *i;
	return j;
}

string IniReader::GetValue(string section, string key, string default_value)
{
	map<string, string>* m;
	if (smap.count(section) < 1)
		return default_value;
	m = smap[section];
	if (m->count(key) < 1)
		return default_value;
	return (*m)[key];
}