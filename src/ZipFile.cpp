#include "ZipFile.h"
#include <string.h>

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
    zip_ = zip_open(filename.c_str(), 0, 'r');
}

std::string ZipFile::readEntryName(const std::string& entry_name)
{
    std::string content;
    if (zip_)
    {
        if (zip_entry_open(zip_, entry_name.c_str()) == 0)
        {
            void* buffer;
            size_t size = 0;
            zip_entry_read(zip_, &buffer, &size);
            content.resize(size);
            memcpy((void*)content.data(), buffer, size);
            free(buffer);
            zip_entry_close(zip_);
        }
    }
    return content;
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