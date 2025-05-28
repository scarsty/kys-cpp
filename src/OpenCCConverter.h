#pragma once
#ifdef USE_OPENCC
#ifndef __ANDROID__
#include "opencc/opencc.h"
#else
#include "opencc.h"
#endif
#else
#include <string>
using opencc_t = void*;
#endif

class OpenCCConverter
{
public:
    OpenCCConverter();
    virtual ~OpenCCConverter();
    std::string UTF8s2t(const std::string& in) { return utf8(in, cc_s2t); }
    std::string UTF8t2s(const std::string& in) { return utf8(in, cc_t2s); }

    std::string CP936s2t(const std::string& in);
    void set(const std::string& setfile);

    static OpenCCConverter* getInstance()
    {
        static OpenCCConverter cc;
        return &cc;
    }

private:
    opencc_t cc_t2s = nullptr, cc_s2t = nullptr;
    std::string utf8(const std::string& in, opencc_t cc);
};
