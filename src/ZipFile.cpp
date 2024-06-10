#include "ZipFile.h"
#include <zip.h>

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
    zip_ = zip_open(filename.c_str(), ZIP_RDONLY, NULL);
}

std::string ZipFile::readEntryName(const std::string& entry_name)
{
    std::string content;
    zip_file_t *zip_file;
    struct zip_stat zs;
    if (zip_)
    {
        zip_file = zip_fopen(zip_, entry_name.c_str(), ZIP_FL_UNCHANGED);
        if (zip_file != NULL)
        {
            zip_stat_init(&zs);
            zip_stat(zip_, entry_name.c_str(), ZIP_FL_UNCHANGED, &zs);
            content.resize(zs.size);
            zip_fread(zip_file, content.data(), zs.size);
            zip_fclose(zip_file);
        }
    }
    return content;
}

std::vector<std::string> ZipFile::getEntryNames()
{
    std::vector<std::string> files;
    if (zip_)
    {
        int i, n = zip_get_num_entries(zip_, ZIP_FL_UNCHANGED);
        for (i = 0; i < n; ++i)
        {
                const char* name = zip_get_name(zip_, i, ZIP_FL_UNCHANGED);
                files.push_back(name);
        }
    }
    return files;
}
