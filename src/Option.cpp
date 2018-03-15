#include "Option.h"

Option Option::option_;

void Option::loadIniFile(const std::string& filename)
{
    std::string content = convert::readStringFromFile(filename);
    content = dealString(content);
    loadIniString(content);
    print();
}

void Option::loadIniString(const std::string& content)
{
    ini_reader_.LoadData(content);
    //包含文件仅载入一层
    auto filenames = convert::splitString(getString("load_ini"), ",");
    for (auto filename : filenames)
    {
        if (filename != "")
        {
            ini_reader_.LoadData(dealString(convert::readStringFromFile(filename)));
        }
    }
}

//提取字串属性，会去掉单引号，双引号
std::string Option::getStringFromSection(const std::string& section, const std::string& name, std::string v /*= ""*/)
{
    std::string str = ini_reader_.GetValue(dealString(section).c_str(), dealString(name).c_str(), v.c_str());
    convert::replaceAllString(str, "\'", "");
    convert::replaceAllString(str, "\"", "");
    return str;
}

void Option::print()
{
    CSimpleIniA::TNamesDepend sections;
    ini_reader_.GetAllSections(sections);
    for (auto section : sections)
    {
        CSimpleIniA::TNamesDepend keys;
        ini_reader_.GetAllKeys(section.pItem, keys);
        for (auto key : keys)
        {
            fprintf(stdout, "[%s] %s = %s\n", section.pItem, key.pItem, ini_reader_.GetValue(section.pItem, key.pItem));
        }
    }
}

void Option::loadSaveValues()
{
#define GET_VALUE_INT(v) do {v = this->getInt(#v, v); printf("%s = %d\n", #v, v);} while(0)

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

