// Read an INI file into easy-to-access name/value pairs.

// inih and INIReader are released under the New BSD license (see LICENSE.txt).
// Go to the project home page for more info:
//
// https://github.com/benhoyt/inih

#include "INIReader.h"
#include "ini.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

void INIReader::load(std::string content)
{
    //convert::replaceAllString(content, "\r", "\n");
    //content += "\n";
    error_ = ini_parse_content(content.c_str(), valueHandler, this);
}

int INIReader::parseError() const
{
    return error_;
}

std::string INIReader::getString(std::string section, std::string key, std::string default_value) const
{
    auto sk = makeKey(section, key);
    // Use _values.find() here instead of _values.at() to support pre C++11 compilers
    return values_.count(sk) ? values_.find(sk)->second : default_value;
}

long INIReader::getInt(std::string section, std::string key, long default_value) const
{
    auto valstr = getString(section, key, "");
    const char* value = valstr.c_str();
    char* end;
    // This parses "1234" (decimal) and also "0x4D2" (hex)
    long n = strtol(value, &end, 0);
    return end > value ? n : default_value;
}

double INIReader::getReal(std::string section, std::string key, double default_value) const
{
    auto valstr = getString(section, key, "");
    const char* value = valstr.c_str();
    char* end;
    double n = strtod(value, &end);
    return end > value ? n : default_value;
}

bool INIReader::getBoolean(std::string section, std::string key, bool default_value) const
{
    auto valstr = getString(section, key, "");
    // Convert to lower case to make string comparisons case-insensitive
    std::transform(valstr.begin(), valstr.end(), valstr.begin(), ::tolower);
    if (valstr == "true" || valstr == "yes" || valstr == "on" || valstr == "1")
    {
        return true;
    }
    else if (valstr == "false" || valstr == "no" || valstr == "off" || valstr == "0")
    {
        return false;
    }
    else
    {
        return default_value;
    }
}

bool INIReader::hasSection(std::string section)
{
    for (auto& value : values_)
    {
        if (value.first.section == section)
        {
            return true;
        }
    }
    return false;
}

bool INIReader::hasKey(std::string section, std::string key)
{
    for (auto& value : values_)
    {
        if (value.first.section == section && value.first.key == key)
        {
            return true;
        }
    }
    return false;
}

std::vector<std::string> INIReader::getAllSections()
{
    std::vector<std::string> ret;
    for (auto& value : values_)
    {
        ret.push_back(value.first.section);
    }
    std::sort(ret.begin(), ret.end());
    ret.erase(std::unique(ret.begin(), ret.end()), ret.end());
    return ret;
}

std::vector<std::string> INIReader::getAllKeys(std::string section)
{
    std::vector<std::string> ret;
    for (auto& value : values_)
    {
        if (value.first.section == section)
        {
            ret.push_back(value.first.key);
        }
    }
    return ret;
}

void INIReader::setKey(std::string section, std::string key, std::string value)
{
    auto sk = makeKey(section, key);
    values_[sk] = value;
}

void INIReader::eraseKey(std::string section, std::string key)
{
    auto sk = makeKey(section, key);
    values_.erase(sk);
}

void INIReader::print()
{
    for (auto& value : values_)
    {
        fprintf(stdout, "[%s] %s, %s\n", value.first.section.c_str(), value.first.key.c_str(), value.second.c_str());
    }
}

INIReader::SectionKey INIReader::makeKey(std::string section, std::string key)
{
    // Convert to lower case to make section/name lookups case-insensitive
    std::transform(section.begin(), section.end(), section.begin(), ::tolower);
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    SectionKey sk = { section, key };
    return sk;
}

int INIReader::valueHandler(void* user, const char* section, const char* key, const char* value)
{
    INIReader* reader = (INIReader*)user;
    auto sk = makeKey(section, key);
    //if (reader->_values[key].size() > 0)
    //{ reader->_values[key] += "\n"; }
    reader->values_[sk] = value;
    return 1;
}
