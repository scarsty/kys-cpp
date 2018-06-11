#include "Option.h"

Option Option::option_;

Option::Option(const std::string& filename)
    : Option()
{
    loadIniFile(filename);
}

void Option::loadIniFile(const std::string& filename)
{
    std::string content = convert::readStringFromFile(filename);
    loadIniString(content);
    print();
}

void Option::loadIniString(const std::string& content)
{
    ini_reader_.load(content);
    //包含文件仅载入一层
    auto filenames = convert::splitString(getString("", "load_ini"), ",");
    for (auto filename : filenames)
    {
        if (filename != "")
        {
            ini_reader_.load(convert::readStringFromFile(filename));
        }
    }
    for (auto section : ini_reader_.getAllSections())
    {
        for (auto key : ini_reader_.getAllKeys(section))
        {
            setOption(section, dealString(key), ini_reader_.getString(section, key, ""));
        }
    }
}

//提取字串属性，会去掉单引号，双引号
std::string Option::getString(const std::string& section, const std::string& key, std::string v /*= ""*/)
{
    std::string str = ini_reader_.getString(section, dealString(key), v);
    convert::replaceAllString(str, "\'", "");
    convert::replaceAllString(str, "\"", "");
    return str;
}

void Option::print()
{
    for (auto section : ini_reader_.getAllSections())
    {
        for (auto key : ini_reader_.getAllKeys(section))
        {
            fprintf(stdout, "[%s] %s = %s\n", section.c_str(), key.c_str(), getString(section, key).c_str());
        }
    }
}

void Option::loadSaveValues()
{
#define GET_VALUE_INT(v)            \
    do                              \
    {                               \
        v = this->getInt("constant", #v, v);    \
        printf("%s = %d\n", #v, v); \
    } while (0)

    GET_VALUE_INT(MaxLevel);
    GET_VALUE_INT(MaxHP);
    GET_VALUE_INT(MaxMP);
    GET_VALUE_INT(MaxPhysicalPower);

    GET_VALUE_INT(MaxPosion);

    GET_VALUE_INT(MaxAttack);
    GET_VALUE_INT(MaxDefence);
    GET_VALUE_INT(MaxSpeed);

    GET_VALUE_INT(MaxMedcine);
    GET_VALUE_INT(MaxUsePoison);
    GET_VALUE_INT(MaxDetoxification);
    GET_VALUE_INT(MaxAntiPoison);

    GET_VALUE_INT(MaxFist);
    GET_VALUE_INT(MaxSword);
    GET_VALUE_INT(MaxKnife);
    GET_VALUE_INT(MaxUnusual);
    GET_VALUE_INT(MaxHiddenWeapon);

    GET_VALUE_INT(MaxKnowledge);
    GET_VALUE_INT(MaxMorality);
    GET_VALUE_INT(MaxAttackWithPoison);
    GET_VALUE_INT(MaxFame);
    GET_VALUE_INT(MaxIQ);

    GET_VALUE_INT(MaxExp);

    GET_VALUE_INT(MoneyItemID);
    GET_VALUE_INT(CompassItemID);
}
