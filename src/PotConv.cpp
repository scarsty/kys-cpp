#include "PotConv.h"

PotConv::PotConv()
{
}

PotConv::~PotConv()
{
    for (auto& cd : cds_)
    {
        iconv_close(cd.second);
    }
}

std::string PotConv::conv(const std::string& src, const char* from, const char* to)
{
    //const char *from_charset, const char *to_charset, const char *inbuf, size_t inlen, char *outbuf;
    iconv_t cd = createcd(from, to);
    if (cd == nullptr)
    {
        return "";
    }
    size_t inlen = src.length();
    size_t outlen = src.length() * 2;
    auto in = new char[inlen + 1];
    auto out = new char[outlen + 1];
    memset(in, 0, inlen + 1);
    memcpy(in, src.c_str(), inlen);
    memset(out, 0, outlen + 1);
    char *pin = in, *pout = out;
    if (iconv(cd, &pin, &inlen, &pout, &outlen) == -1)
    {
        out[0] = '\0';
    }
    std::string result(out, src.length() * 2 - outlen);
    delete[] in;
    delete[] out;
    return result;
}

std::string PotConv::conv(const std::string& src, const std::string& from, const std::string& to)
{
    return conv(src, from.c_str(), to.c_str());
}

std::string PotConv::to_read(const std::string& src)
{
#ifdef _WIN32
    return conv(src, "utf-8", "cp936");
#else
    return src;
#endif
}

PotConv PotConv::potconv_;

iconv_t PotConv::createcd(const char* from, const char* to)
{
    std::string cds = std::string(from) + std::string(to);
    if (potconv_.cds_.count(cds) == 0)
    {
        iconv_t cd;
        cd = iconv_open(to, from);
        potconv_.cds_[cds] = cd;
        return cd;
    }
    else
    {
        return potconv_.cds_[cds];
    }
}
