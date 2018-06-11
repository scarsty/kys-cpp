// Read an INI file into easy-to-access name/value pairs.

// inih and INIReader are released under the New BSD license (see LICENSE.txt).
// Go to the project home page for more info:
//
// https://github.com/benhoyt/inih

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include "ini.h"
#include "INIReader.h"
#include <string>

using std::string;

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

string INIReader::getString(string section, string name, string default_value) const
{
    string key = makeKey(section, name);
    // Use _values.find() here instead of _values.at() to support pre C++11 compilers
    return values_.count(key) ? values_.find(key)->second : default_value;
}

long INIReader::getInt(string section, string name, long default_value) const
{
    string valstr = getString(section, name, "");
    const char* value = valstr.c_str();
    char* end;
    // This parses "1234" (decimal) and also "0x4D2" (hex)
    long n = strtol(value, &end, 0);
    return end > value ? n : default_value;
}

double INIReader::getReal(string section, string name, double default_value) const
{
    string valstr = getString(section, name, "");
    const char* value = valstr.c_str();
    char* end;
    double n = strtod(value, &end);
    return end > value ? n : default_value;
}

bool INIReader::getBoolean(string section, string name, bool default_value) const
{
    string valstr = getString(section, name, "");
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
        if (value.first.find_first_of(section + "=") == 0)
        {
            return true;
        }
    }
    return false;
}

bool INIReader::hasKey(std::string section, std::string name)
{
    for (auto& value : values_)
    {
        if (value.first == section + "=" + name)
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
        auto p = value.first.find("=");
        if (p != std::string::npos)
        {
            ret.push_back(value.first.substr(0, p));
        }
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
        auto p = value.first.find(section + "=");
        if (p == 0)
        {
            ret.push_back(value.first.substr(section.size() + 1));
        }
    }
    return ret;
}

void INIReader::setKey(std::string section, std::string name, std::string value)
{
    string key = makeKey(section, name);
    values_[key] = value;
}

void INIReader::print()
{
    for (auto& value : values_)
    {
        fprintf(stdout, "%s, %s\n", value.first.c_str(), value.second.c_str());
    }
}

string INIReader::makeKey(string section, string name)
{
    string key = section + "=" + name;
    // Convert to lower case to make section/name lookups case-insensitive
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    return key;
}

int INIReader::valueHandler(void* user, const char* section, const char* name,
    const char* value)
{
    INIReader* reader = (INIReader*)user;
    string key = makeKey(section, name);
    //if (reader->_values[key].size() > 0)
    //{ reader->_values[key] += "\n"; }
    reader->values_[key] = value;
    return 1;
}
