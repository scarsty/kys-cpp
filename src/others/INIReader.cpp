// Read an INI file into easy-to-access name/value pairs.

// inih and INIReader are released under the New BSD license (see LICENSE.txt).
// Go to the project home page for more info:
//
// https://github.com/benhoyt/inih

#include "INIReader.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

//Nonzero to allow multi-line value parsing, in the style of Python's
//configparser. If allowed, ini_parse() will call the handler with the same
//name for each subsequent line parsed.
#ifndef INI_ALLOW_MULTILINE
#define INI_ALLOW_MULTILINE 1
#endif

//Nonzero to allow a UTF-8 BOM sequence (0xEF 0xBB 0xBF) at the start of
//the file. See http://code.google.com/p/inih/issues/detail?id=21
#ifndef INI_ALLOW_BOM
#define INI_ALLOW_BOM 1
#endif

//Nonzero to allow inline comments (with valid inline comment characters
//specified by INI_INLINE_COMMENT_PREFIXES). Set to 0 to turn off and match
//Python 3.2+ configparser behaviour.
#ifndef INI_ALLOW_INLINE_COMMENTS
#define INI_ALLOW_INLINE_COMMENTS 1
#endif
#ifndef INI_INLINE_COMMENT_PREFIXES
#define INI_INLINE_COMMENT_PREFIXES ";#"
#endif

//Nonzero to use stack, zero to use heap (malloc/free).
#ifndef INI_USE_STACK
#define INI_USE_STACK 1
#endif

//Stop parsing on first error (default is to keep parsing).
#ifndef INI_STOP_ON_FIRST_ERROR
#define INI_STOP_ON_FIRST_ERROR 0
#endif

//Maximum line length for any line in INI file.
#ifndef INI_MAX_LINE
#define INI_MAX_LINE 200
#endif

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

void INIReader::load(std::string content)
{
    //convert::replaceAllString(content, "\r", "\n");
    //content += "\n";
    error_ = ini_parse_content(content);
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

int INIReader::valueHandler(const std::string& section, const std::string& key, const std::string& value)
{
    auto sk = makeKey(section, key);
    //if (reader->_values[key].size() > 0)
    //{ reader->_values[key] += "\n"; }
    values_[sk] = value;
    return 1;
}

int INIReader::ini_parse_content(const std::string& content)
{
    //copied from libconvert with little modification
    auto splitString = [](std::string str, std::string pattern, bool ignore_psspace)
    {
        std::string::size_type pos;
        std::vector<std::string> result;
        if (pattern.empty())
        {
            pattern = ",;| ";
        }
        str += pattern[0];
        bool have_space = pattern.find(" ") != std::string::npos;
        int size = str.size();
        for (int i = 0; i < size; i++)
        {
            if (have_space)
            {
                while (str[i] == ' ')
                {
                    i++;
                }
            }
            pos = str.find_first_of(pattern, i);
            if (pos < size)
            {
                std::string s = str.substr(i, pos - i);
                if (ignore_psspace)
                {
                    auto pre = s.find_first_not_of(" ");
                    auto suf = s.find_last_not_of(" ");
                    if (pre != std::string::npos && suf != std::string::npos)
                    {
                        s = s.substr(pre, suf - pre + 1);
                    }
                }
                if (s != "")
                {
                    result.push_back(s);    //strip blank lines
                }
                i = pos;
            }
        }
        return result;
    };
    auto c = splitString(content, "\n\r\0", false);
    return ini_parse_lines(c);
}

int INIReader::ini_parse_lines(std::vector<std::string>& lines)
{
    /* Return pointer to first non-whitespace char in given string. */
    auto lskip = [](std::string s) -> std::string
    {
        auto pre = s.find_first_not_of(" ");
        if (pre != std::string::npos)
        {
            return s.substr(pre);
        }
        else
        {
            return "";
        }
    };

    /* Strip whitespace chars off end of given string, in place. Return s. */
    auto rstrip = [](std::string s) -> std::string
    {
        auto suf = s.find_last_not_of(" ");
        if (suf != std::string::npos)
        {
            return s.substr(0, suf + 1);
        }
        else
        {
            return "";
        }
    };

    /* Uses a fair bit of stack (use heap instead if you need to) */
    std::string section = "";
    std::string prev_key = "";
    int lineno = 0;
    int error = 0;

    /* Scan all lines */
    for (auto line : lines)
    {
        lineno++;
#if INI_ALLOW_BOM
        if (lineno == 1 && line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
        {
            line = line.substr(3);
        }
#endif
        line = lskip(rstrip(line));    //remove spaces on two sides
        if (line == "")
        { continue; }

        if (line[0] == ';' || line[0] == '#')
        {
            /* Per Python configparser, allow both ; and # comments at the start of a line */
        }
#if INI_ALLOW_MULTILINE
        else if (prev_key != "" && line[0] == ' ')
        {
            /* Non-blank line with leading whitespace, treat as continuation of previous name's value (as per Python config parser). */
            if (valueHandler(section, prev_key, line) && !error)
            {
                error = lineno;
            }
        }
#endif
        else if (line[0] == '[')
        {
            /* A "[section]" line */
            auto end = line.find_first_of("]");
            if (end != std::string::npos)
            {
                section = line.substr(1, end - 1);
                prev_key = "";
            }
            else if (!error)
            {
                /* No ']' found on section line */
                error = lineno;
            }
        }
        else
        {
            /* Not a comment, must be a name[=:]value pair */
            auto assign_char = line.find_first_of("=:");
            if (assign_char != std::string::npos)
            {
                auto key = rstrip(line.substr(0, assign_char));
                auto value = lskip(line.substr(assign_char + 1));
#if INI_ALLOW_INLINE_COMMENTS
                auto end = value.find_first_of(INI_INLINE_COMMENT_PREFIXES);
                if (end != std::string::npos)
                {
                    value = value.substr(0, end);
                }
#endif
                value = rstrip(value);

                /* Valid name[=:]value pair found, call handler */
                prev_key = key;
                if (valueHandler(section, key, value) && !error)
                {
                    error = lineno;
                }
            }
            else if (!error)
            {
                /* No '=' or ':' found on name[=:]value line */
                error = lineno;
            }
        }

#if INI_STOP_ON_FIRST_ERROR
        if (error)
        {
            break;
        }
#endif
    }
    return error;
}
