#include "OpenCCConverter.h"
#include "GameUtil.h"
#include "PotConv.h"

#ifdef _WIN32
#include "windows.h"
#endif

OpenCCConverter::OpenCCConverter()
{
#ifdef USE_OPENCC
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
#endif
}

OpenCCConverter::~OpenCCConverter()
{
#ifdef USE_OPENCC
    if (cc_s2t)
    {
        opencc_close(cc_s2t);
    }
    if (cc_t2s)
    {
        opencc_close(cc_t2s);
    }
#endif
}

std::string OpenCCConverter::UTF8t2s(const std::string& in)
{
#ifdef USE_OPENCC
    return utf8(in, cc_t2s);
#else
#ifdef _WIN32
    std::string str = PotConv::utf8tocp936(in), str2;
    str2.resize(str.size() + 1);
    LCMapStringA(LOCALE_SYSTEM_DEFAULT, LCMAP_SIMPLIFIED_CHINESE, str.c_str(), str.size(), &str2[0], str2.size() + 1);
    return PotConv::cp936toutf8(str2);
#endif
#endif
    return in;
}

std::string OpenCCConverter::CP936s2t(const std::string& in)
{
    return PotConv::utf8tocp936(UTF8s2t(PotConv::cp936toutf8(in)));
}

void OpenCCConverter::set(const std::string& setfile)
{
#ifdef USE_OPENCC
    //有内存泄露，不管了
    cc_s2t = opencc_open((GameUtil::PATH() + setfile).c_str());
#endif
}

std::string OpenCCConverter::utf8(const std::string& in, opencc_t cc)
{
#ifdef USE_OPENCC
    if (cc == nullptr)
    {
        return in;
    }
    std::string str;
    str.resize(in.size());
    opencc_convert_utf8_to_buffer(cc, in.c_str(), in.size(), &str[0]);
    return str;
#else
    return in;
#endif
}
