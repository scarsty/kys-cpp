// Read an INI file into easy-to-access name/value pairs.

// inih and INIReader are released under the New BSD license (see LICENSE.txt).
// Go to the project home page for more info:
//
// https://github.com/benhoyt/inih

#ifndef __INIREADER_H__
#define __INIREADER_H__

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

//Stop parsing on first error (default is to keep parsing).
#ifndef INI_STOP_ON_FIRST_ERROR
#define INI_STOP_ON_FIRST_ERROR 0
#endif

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <algorithm>
#include <map>
#include <string>
#include <vector>

struct CaseInsensitivityCompare
{
    bool operator()(const std::string& l, const std::string& r) const
    {
        auto l1 = l;
        auto r1 = r;
        std::transform(l1.begin(), l1.end(), l1.begin(), ::tolower);
        std::transform(r1.begin(), r1.end(), r1.begin(), ::tolower);
        return l1 < r1;
    }
};

// Read an INI file into easy-to-access name/value pairs. (Note that I've gone
// for simplicity here rather than speed, but it should be pretty decent.)
template <class COM1, class COM2>
class INIReader
{
public:
    // Construct INIReader and parse given filename. See ini.h for more info
    // about the parsing.
    INIReader() {}

    void loadFile(std::string filename)
    {
        FILE* fp = fopen(filename.c_str(), "rb");
        if (!fp)
        {
            fprintf(stderr, "Cannot open file %s\n", filename.c_str());
            return;
        }
        fseek(fp, 0, SEEK_END);
        int length = ftell(fp);
        fseek(fp, 0, 0);
        std::string str;
        str.resize(length, '\0');
        fread((void*)str.c_str(), length, 1, fp);
        fclose(fp);
        load(str);
    }

    void load(std::string content)
    {
        error_ = ini_parse_content(content);
    }

    // Return the result of ini_parse(), i.e., 0 on success, line number of
    // first error on parse error, or -1 on file open error.
    int parseError() const
    {
        return error_;
    }

    // Get a string value from INI file, returning default_value if not found.

    std::string getString(std::string section, std::string key, std::string default_value) const
    {
        if (values_.count(section) == 0)
        {
            return default_value;
        }
        if (values_.at(section).count(key) > 0)
        {
            return values_.at(section).at(key);
        }
        else
        {
            return default_value;
        }
    }

    // Get an integer (long) value from INI file, returning default_value if
    // not found or not a valid integer (decimal "1234", "-1234", or hex "0x4d2").
    long getInt(std::string section, std::string key, long default_value) const
    {
        auto valstr = getString(section, key, "");
        const char* value = valstr.c_str();
        char* end;
        // This parses "1234" (decimal) and also "0x4D2" (hex)
        long n = strtol(value, &end, 0);
        return end > value ? n : default_value;
    }

    // Get a real (floating point double) value from INI file, returning
    // default_value if not found or not a valid floating point value
    // according to strtod().
    double getReal(std::string section, std::string key, double default_value) const
    {
        auto valstr = getString(section, key, "");
        const char* value = valstr.c_str();
        char* end;
        double n = strtod(value, &end);
        return end > value ? n : default_value;
    }

    // Get a boolean value from INI file, returning default_value if not found or if
    // not a valid true/false value. Valid true values are "true", "yes", "on", "1",
    // and valid false values are "false", "no", "off", "0" (not case sensitive).
    bool getBoolean(std::string section, std::string key, bool default_value) const
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

    //check one section exist or not
    int hasSection(std::string section)
    {
        return values_.count(section);
    }

    //check one section and one key exist or not
    int hasKey(std::string section, std::string key)
    {
        return values_.count(section) > 0 ? values_.at(section).count(key) : 0;
    }

    std::vector<std::string> getAllSections()
    {
        std::vector<std::string> ret;
        for (auto& value : values_)
        {
            ret.push_back(value.first);
        }
        return ret;
    }

    std::vector<std::string> getAllKeys(std::string section)
    {
        std::vector<std::string> ret;
        if (values_.count(section) == 0)
        {
            return ret;
        }
        for (auto& kv : values_.at(section))
        {
            ret.push_back(kv.first);
        }
        return ret;
    }

    void setKey(std::string section, std::string key, std::string value)
    {
        values_[section][key] = value;
    }

    void eraseKey(std::string section, std::string key)
    {
        values_[section].erase(key);
    }

    void print()
    {
        for (auto& skv : values_)
        {
            fprintf(stdout, "[%s]\n", skv.first.c_str());
            for (auto& kv : skv.second)
            {
                fprintf(stdout, "%s = %s\n", kv.first.c_str(), kv.second.c_str());
            }
            fprintf(stdout, "\n");
        }
    }

private:
    typedef std::map<std::string, std::map<std::string, std::string, COM2>, COM1> values_type;
    int error_ = 0;
    values_type values_;
    int valueHandler(const std::string& section, const std::string& key, const std::string& value)
    {
        values_[section][key] = value;
        return 0;
    }

private:
    int ini_parse_content(const std::string& content)
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
        auto lines = splitString(content, "\n\r\0", false);
        return ini_parse_lines(lines);
    }

    int ini_parse_lines(std::vector<std::string>& lines)
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
            {
                continue;
            }

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
};

typedef INIReader<CaseInsensitivityCompare, CaseInsensitivityCompare> INIReaderNormal;

#endif    // __INIREADER_H__
