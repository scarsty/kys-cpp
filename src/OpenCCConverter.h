#pragma once
#include "opencc/opencc.h"

class OpenCCConverter
{
public:
    OpenCCConverter();
    virtual ~OpenCCConverter();
    std::string convertUTF8(const std::string& in);
    std::string convertCP936(const std::string& in);

    static OpenCCConverter* getInstance()
    {
        static OpenCCConverter cc;
        return &cc;
    }

private:
    opencc_t cc;
};
