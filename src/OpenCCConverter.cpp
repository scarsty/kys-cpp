#include "OpenCCConverter.h"
#include "GameUtil.h"
#include "PotConv.h"

OpenCCConverter::OpenCCConverter()
{
    cc_s2t = opencc_open((GameUtil::PATH() + "cc/s2t.json").c_str());
    if (cc_s2t == (decltype(cc_s2t))-1)
    {
        cc_s2t = nullptr;
    }
    cc_t2s = opencc_open((GameUtil::PATH() + "cc/t2s.json").c_str());
    if (cc_t2s == (decltype(cc_t2s))-1)
    {
        cc_t2s = nullptr;
    }
}

OpenCCConverter::~OpenCCConverter()
{
    if (cc_s2t)
    {
        opencc_close(cc_s2t);
    }
    if (cc_t2s)
    {
        opencc_close(cc_t2s);
    }
}

std::string OpenCCConverter::CP936s2t(const std::string& in)
{
    return PotConv::utf8tocp936(UTF8s2t(PotConv::cp936toutf8(in)));
}

void OpenCCConverter::set(const std::string& setfile)
{
    //有内存泄露，不管了
    cc_s2t = opencc_open((GameUtil::PATH() + setfile).c_str());
}

std::string OpenCCConverter::utf8(const std::string& in, opencc_t cc)
{
    if (cc == nullptr)
    {
        return in;
    }
    std::string str;
    str.resize(in.size());
    opencc_convert_utf8_to_buffer(cc, in.c_str(), in.size(), &str[0]);
    return str;
}
