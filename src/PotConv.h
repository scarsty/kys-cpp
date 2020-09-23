#pragma once

#include "iconv.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>

class PotConv
{
public:
    PotConv();
    virtual ~PotConv();

    static std::string conv(const std::string& src, const char* from, const char* to);
    static std::string conv(const std::string& src, const std::string& from, const std::string& to);
    static std::string cp936toutf8(const std::string& src) { return conv(src, "cp936", "utf-8"); }
    static std::string cp950toutf8(const std::string& src) { return conv(src, "cp950", "utf-8"); }
    static std::string cp950tocp936(const std::string& src) { return conv(src, "cp950", "cp936"); }
    static std::string utf8tocp936(const std::string& src) { return conv(src, "utf-8", "cp936"); }
    static void fromCP950ToCP936(char* s)
    {
        auto str = PotConv::cp950tocp936(s);
        memcpy(s, str.data(), str.length());
    }
    static void fromCP950ToUTF8(char* s)
    {
        auto str = PotConv::cp950toutf8(s);
        memcpy(s, str.data(), str.length());
    }
    static void fromCP936ToUTF8(char* s)
    {
        auto str = PotConv::cp936toutf8(s);
        memcpy(s, str.data(), str.length());
    }
    static std::string to_read(const std::string& src);

private:
    std::map<std::string, iconv_t> cds_;
    static PotConv potconv_;
    static iconv_t createcd(const char* from, const char* to);
};
