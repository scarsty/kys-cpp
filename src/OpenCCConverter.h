#pragma once
#include "opencc/opencc.h"

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
