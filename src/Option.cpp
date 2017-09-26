#include "Option.h"

void Option::loadIniFile(const std::string& filename)
{
    std::string content = convert::readStringFromFile(filename);
    loadIniString(content);
}

void Option::loadIniString(const std::string& content)
{
    std::string str = de(content);
    ini_reader_.load(str);
    //包含文件仅载入一层
    auto filenames = convert::splitString(getString("load_ini"), ",");
    for (auto filename : filenames)
    {
        if (filename != "")
        {
            ini_reader_.load(de(convert::readStringFromFile(filename)));
        }
    }
}

//提取字串属性，会去掉单引号，双引号
std::string Option::getStringFromSection(const std::string& section, const std::string& name, std::string v /*= ""*/)
{
    auto str = ini_reader_.Get(section, name, v);
    convert::replaceAllString(str, "\'", "");
    convert::replaceAllString(str, "\"", "");
    return str;
}

int Option::getIntFromSection2(const std::string& section, const std::string& name, int default_value)
{
    return int(getRealFromSection2(section, name, default_value));
}

//首先获取公共部分，再以公共部分为默认值获取特殊部分
double Option::getRealFromSection2(const std::string& section, const std::string& name, double default_value)
{
    double a = getReal(name, default_value);
    a = getRealFromSection(section, name, a);
    return a;
}

std::string Option::getStringFromSection2(const std::string& section, const std::string& name, std::string default_value)
{
    std::string a = getString(name, default_value);
    a = getStringFromSection(section, name, a);
    return a;
}

void Option::setOptions(std::string section, std::string name_values)
{
    setOptions(section, convert::splitString(name_values, ";"));
}

void Option::setOptions(std::string section, std::vector<std::string> name_values)
{
    for (auto name_value : name_values)
    {
        convert::replaceAllString(name_value, " ", "");
        auto p = name_value.find("=");
        if (p != std::string::npos)
        {
            auto name = name_value.substr(0, p);
            auto value = name_value.substr(p + 1);
            setOption(section, name, value);
        }
    }
}


