// Read an INI file into easy-to-access name/value pairs.

// inih and INIReader are released under the New BSD license (see LICENSE.txt).
// Go to the project home page for more info:
//
// https://github.com/benhoyt/inih

#ifndef __INIREADER_H__
#define __INIREADER_H__

#include <map>
#include <string>
#include <vector>

// Read an INI file into easy-to-access name/value pairs. (Note that I've gone
// for simplicity here rather than speed, but it should be pretty decent.)
class INIReader
{
public:
    // Construct INIReader and parse given filename. See ini.h for more info
    // about the parsing.
    INIReader() { error_ = 0; }
    void load(std::string content);

    // Return the result of ini_parse(), i.e., 0 on success, line number of
    // first error on parse error, or -1 on file open error.
    int parseError() const;

    // Get a string value from INI file, returning default_value if not found.
    std::string getString(std::string section, std::string key, std::string default_value) const;

    // Get an integer (long) value from INI file, returning default_value if
    // not found or not a valid integer (decimal "1234", "-1234", or hex "0x4d2").
    long getInt(std::string section, std::string key, long default_value) const;

    // Get a real (floating point double) value from INI file, returning
    // default_value if not found or not a valid floating point value
    // according to strtod().
    double getReal(std::string section, std::string key, double default_value) const;

    // Get a boolean value from INI file, returning default_value if not found or if
    // not a valid true/false value. Valid true values are "true", "yes", "on", "1",
    // and valid false values are "false", "no", "off", "0" (not case sensitive).
    bool getBoolean(std::string section, std::string key, bool default_value) const;

    bool hasSection(std::string section);
    bool hasKey(std::string section, std::string key);

    std::vector<std::string> getAllSections();
    std::vector<std::string> getAllKeys(std::string section);

    void setKey(std::string section, std::string key, std::string value);
    void eraseKey(std::string section, std::string key);
    void print();

private:
    struct SectionKey
    {
        std::string section, key;
        bool operator<(const SectionKey& sk2) const { return section + " " + key < sk2.section + " " + sk2.key; }
    };
    int error_;
    std::map<SectionKey, std::string> values_;
    static SectionKey makeKey(std::string section, std::string key);
    static int valueHandler(void* user, const char* section, const char* key, const char* value);
};

#endif    // __INIREADER_H__
