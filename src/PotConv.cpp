#include "PotConv.h"


PotConv::PotConv()
{
}


PotConv::~PotConv()
{
}

std::string PotConv::conv(const std::string& src, const char* from, const char* to)
{
    //const char *from_charset, const char *to_charset, const char *inbuf, size_t inlen, char *outbuf;
    size_t inlen = std::min((int)src.length(), CONV_BUFFER_SIZE);
    size_t outlen = CONV_BUFFER_SIZE;

    char in[CONV_BUFFER_SIZE] = { '\0' };
    char out[CONV_BUFFER_SIZE] = { '\0' };

    char* pin = in, *pout = out;
    memcpy(in, src.c_str(), inlen);
    iconv_t cd;
    cd = iconv_open(to, from);
    if (cd == nullptr) { return ""; }
    if (iconv(cd, &pin, &inlen, &pout, &outlen) == -1)
    {
        out[0] = '\0';
    }
    iconv_close(cd);
    return out;
    return src;
}

std::string PotConv::conv(const std::string& src, const std::string& from, const std::string& to)
{
    return conv(src, from.c_str(), to.c_str());
}

std::string PotConv::to_read(const std::string& src)
{
#ifdef _WIN32
    return src;
#else
    return conv(src, "cp936", "utf-8");
#endif
}

