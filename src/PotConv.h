#pragma once

extern "C"
{
#ifdef _MSC_VER
#include "iconv/iconv.h"
#else
#include <iconv.h>
#endif
}

#include <cstring>
#include <string>
#include <algorithm>

#define CONV_BUFFER_SIZE 2048

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
    static void fromCP950ToCP936(char* s)
    {
        auto str = PotConv::cp950tocp936(s);
        memcpy(s, str.data(), str.length());
    }
    static std::string to_read(const std::string& src);
};

