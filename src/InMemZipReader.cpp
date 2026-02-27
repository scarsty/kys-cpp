#include "InMemZipReader.h"
#include "filefunc.h"
#include "zip.h"

void InMemZipReader::ZipDeleter::operator()(zip_t* z) const
{
    zip_close(z);
}

void InMemZipReader::ZipFileDeleter::operator()(zip_file_t* zf) const
{
    zip_fclose(zf);
}

bool InMemZipReader::openRead(const std::string& zip_filename)
{
    zip_.reset();
    data_.clear();

    data_ = filefunc::readFile(zip_filename);
    if (data_.empty())
    {
        return false;
    }

    zip_error_t error;
    zip_error_init(&error);
    auto error_guard = std::unique_ptr<zip_error_t, decltype(&zip_error_fini)>(&error, zip_error_fini);

    auto src = std::unique_ptr<zip_source_t, decltype(&zip_source_free)>(
        zip_source_buffer_create(data_.data(), data_.size(), 0, &error), zip_source_free);
    if (!src)
    {
        data_.clear();
        return false;
    }

    auto* z = zip_open_from_source(src.get(), ZIP_RDONLY, &error);
    if (!z)
    {
        data_.clear();
        return false;
    }

    src.release();    // ownership transferred to z
    zip_.reset(z);
    return true;
}

std::string InMemZipReader::readFile(const std::string& filename) const
{
    if (!zip_)
    {
        return {};
    }

    std::unique_ptr<zip_file_t, ZipFileDeleter> zf(
        zip_fopen(zip_.get(), filename.c_str(), ZIP_FL_UNCHANGED));
    if (!zf)
    {
        return {};
    }

    zip_stat_t zs;
    zip_stat_init(&zs);
    zip_stat(zip_.get(), filename.c_str(), ZIP_FL_UNCHANGED, &zs);

    std::string content(zs.size, '\0');
    zip_fread(zf.get(), content.data(), zs.size);
    return content;
}
