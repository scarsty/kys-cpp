#include "OpenCCConverter.h"
#include "PotConv.h"

OpenCCConverter::OpenCCConverter()
{
    cc = opencc_open("../game/cc/s2t.json");
}

OpenCCConverter::~OpenCCConverter()
{
    opencc_close(cc);
}

std::string OpenCCConverter::convertUTF8(const std::string& in)
{
    std::string str;
    str.resize(in.size());
    opencc_convert_utf8_to_buffer(cc, in.c_str(), in.size(), &str[0]);
    return str;
}

std::string OpenCCConverter::convertCP936(const std::string& in)
{
    return PotConv::utf8tocp936(convertUTF8(PotConv::cp936toutf8(in)));
}
