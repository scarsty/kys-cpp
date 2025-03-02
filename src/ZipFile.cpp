#include "ZipFile.h"
#include "../others/zip.h"
#include <cstring>

ZipFile::ZipFile()
{
}

ZipFile::~ZipFile()
{
    if (zip_)
    {
        zip_close(zip_);
    }
}

void ZipFile::openFile(const std::string& filename)
{
    if (zip_)
    {
        zip_close(zip_);
    }
    zip_ = zip_open(filename.c_str(), 0, 'r');
}

std::string ZipFile::readEntryName(const std::string& entry_name) const
{
    std::string content;
    if (zip_)
    {
        if (zip_entry_open(zip_, entry_name.c_str()) == 0)
        {
            size_t size = zip_entry_size(zip_);
            content.resize(size);
            zip_entry_noallocread(zip_, content.data(), size);
            zip_entry_close(zip_);
        }
    }
    return content;
}

std::vector<std::string> ZipFile::getEntryNames() const
{
    std::vector<std::string> entry_names;
    zip_t* zip = zip_open("foo.zip", 0, 'r');
    int i, n = zip_entries_total(zip);
    for (i = 0; i < n; ++i)
    {
        zip_entry_openbyindex(zip, i);
        {
            const char* name = zip_entry_name(zip);
            entry_names.emplace_back(name);
            int isdir = zip_entry_isdir(zip);
            unsigned long long size = zip_entry_size(zip);
            unsigned int crc32 = zip_entry_crc32(zip);
        }
        zip_entry_close(zip);
    }
    zip_close(zip);
    return entry_names;
}

int ZipFile::zip(const std::string& zip_file, const std::vector<std::string>& files)
{
    zip_t* zip = zip_open(zip_file.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    for (const auto& file : files)
    {
        zip_entry_open(zip, file.c_str());
        {
            zip_entry_fwrite(zip, file.c_str());
        }
        zip_entry_close(zip);
    }

    zip_close(zip);
    return 0;
}

int ZipFile::unzip(const std::string& zip_file, const std::vector<std::string>& files)
{
    zip_t* zip = zip_open(zip_file.c_str(), 0, 'r');
    for (const auto& file : files)
    {
        zip_entry_open(zip, file.c_str());
        {
            zip_entry_fread(zip, file.c_str());
        }
        zip_entry_close(zip);
    }

    zip_close(zip);
    return 0;
}