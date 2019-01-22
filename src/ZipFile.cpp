#include "ZipFile.h"
#include "../others/zip.h"

ZipFile::ZipFile()
{
}

ZipFile::~ZipFile()
{
}

int ZipFile::zip(std::string zip_file, std::vector<std::string> files)
{
    struct zip_t* zip = zip_open(zip_file.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    {
        for (auto file : files)
        {
            zip_entry_open(zip, file.c_str());
            {
                zip_entry_fwrite(zip, file.c_str());
            }
            zip_entry_close(zip);
        }
    }
    zip_close(zip);
    return 0;
}

int ZipFile::unzip(std::string zip_file, std::vector<std::string> files)
{
    struct zip_t* zip = zip_open(zip_file.c_str(), 0, 'r');
    {
        for (auto file : files)
        {
            zip_entry_open(zip, file.c_str());
            {
                zip_entry_fread(zip, file.c_str());
            }
            zip_entry_close(zip);
        }
    }
    zip_close(zip);
    return 0;
}