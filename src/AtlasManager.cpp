#include "AtlasManager.h"

#include "filefunc.h"
#include "strfunc.h"

#include <glaze/json.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#endif

namespace AtlasManifestFormat
{
constexpr auto kAtlasBlobExtension = ".atlas";
constexpr auto kAtlasManifestExtension = ".atlas.json";
constexpr auto kAtlasReadOptions = glz::opts{.error_on_unknown_keys = false};

struct Record
{
    std::string path;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

struct Data
{
    std::vector<Record> records;
};
}    // namespace AtlasManifestFormat

namespace
{
std::string normalizeAtlasPath(const std::string& filename)
{
    auto normalized = strfunc::toLowerCase(filename);
    strfunc::replaceAllSubStringRef(normalized, "\\", "/");
    return normalized;
}
}    // namespace

class AtlasManager::Storage
{
public:
    virtual ~Storage() = default;
    virtual bool openBlob(const std::string& blob_path) = 0;
    virtual std::uint64_t blobSize() const = 0;
    virtual std::string read(std::uint64_t offset, std::uint64_t size) const = 0;
};

#ifdef _WIN32
class MappedAtlasStorage final : public AtlasManager::Storage
{
public:
    ~MappedAtlasStorage() override
    {
        close();
    }

    bool openBlob(const std::string& blob_path) override
    {
        close();

        auto file = CreateFileW(
            std::filesystem::path(blob_path).c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        LARGE_INTEGER file_size{};
        if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart < 0)
        {
            CloseHandle(file);
            return false;
        }

        file_handle_ = file;
        blob_size_ = static_cast<std::uint64_t>(file_size.QuadPart);
        if (blob_size_ == 0)
        {
            return true;
        }

        mapping_handle_ = CreateFileMappingW(file_handle_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping_handle_)
        {
            close();
            return false;
        }

        mapped_data_ = static_cast<const std::byte*>(MapViewOfFile(mapping_handle_, FILE_MAP_READ, 0, 0, 0));
        if (!mapped_data_)
        {
            close();
            return false;
        }

        return true;
    }

    std::uint64_t blobSize() const override
    {
        return blob_size_;
    }

    std::string read(std::uint64_t offset, std::uint64_t size) const override
    {
        if (offset > blob_size_ || size > blob_size_ - offset)
        {
            return {};
        }
        if (size == 0)
        {
            return {};
        }
        if (!mapped_data_)
        {
            return {};
        }

        auto begin = reinterpret_cast<const char*>(mapped_data_ + offset);
        return std::string(begin, begin + static_cast<size_t>(size));
    }

private:
    void close()
    {
        if (mapped_data_)
        {
            UnmapViewOfFile(mapped_data_);
            mapped_data_ = nullptr;
        }
        if (mapping_handle_)
        {
            CloseHandle(mapping_handle_);
            mapping_handle_ = nullptr;
        }
        if (file_handle_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(file_handle_);
            file_handle_ = INVALID_HANDLE_VALUE;
        }
        blob_size_ = 0;
    }

    HANDLE file_handle_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_handle_ = nullptr;
    const std::byte* mapped_data_ = nullptr;
    std::uint64_t blob_size_ = 0;
};
#else
class StreamAtlasStorage final : public AtlasManager::Storage
{
public:
    ~StreamAtlasStorage() override
    {
        close();
    }

    bool openBlob(const std::string& blob_path) override
    {
        close();

        stream_.open(blob_path, std::ios::binary);
        if (!stream_)
        {
            return false;
        }

        stream_.seekg(0, std::ios::end);
        const auto end_pos = stream_.tellg();
        if (end_pos < 0)
        {
            close();
            return false;
        }

        blob_size_ = static_cast<std::uint64_t>(end_pos);
        return true;
    }

    std::uint64_t blobSize() const override
    {
        return blob_size_;
    }

    std::string read(std::uint64_t offset, std::uint64_t size) const override
    {
        if (offset > blob_size_ || size > blob_size_ - offset)
        {
            return {};
        }
        if (size == 0)
        {
            return {};
        }

        std::string content(static_cast<std::size_t>(size), '\0');
        std::size_t done = 0;
        while (done < static_cast<std::size_t>(size))
        {
            const auto target = static_cast<std::streamoff>(offset + done);
            stream_.clear();
            stream_.seekg(target);
            if (!stream_)
            {
                return {};
            }

            stream_.read(content.data() + done, static_cast<std::streamsize>(size - done));
            const auto got = static_cast<std::size_t>(stream_.gcount());
            if (got == 0)
            {
                return {};
            }
            done += got;
        }
        return content;
    }

private:
    void close()
    {
        stream_.close();
        stream_.clear();
        blob_size_ = 0;
    }

    mutable std::ifstream stream_;
    std::uint64_t blob_size_ = 0;
};
#endif

AtlasManager::AtlasManager() = default;
AtlasManager::~AtlasManager() = default;
AtlasManager::AtlasManager(AtlasManager&&) noexcept = default;
AtlasManager& AtlasManager::operator=(AtlasManager&&) noexcept = default;

void AtlasManager::close()
{
    storage_.reset();
    entries_.clear();
}

bool AtlasManager::openRead(const std::string& base_path)
{
    close();

    const auto manifest_path = base_path + AtlasManifestFormat::kAtlasManifestExtension;
    const auto blob_path = base_path + AtlasManifestFormat::kAtlasBlobExtension;
    if (!filefunc::fileExist(manifest_path) || !filefunc::fileExist(blob_path))
    {
        return false;
    }

    auto manifest = filefunc::readFileToString(manifest_path);
    if (manifest.empty())
    {
        return false;
    }

    AtlasManifestFormat::Data manifest_data;
    if (const auto result = glz::read<AtlasManifestFormat::kAtlasReadOptions>(manifest_data, manifest); result)
    {
        return false;
    }

    std::unique_ptr<Storage> storage;
#ifdef _WIN32
    storage = std::make_unique<MappedAtlasStorage>();
#else
    storage = std::make_unique<StreamAtlasStorage>();
#endif
    if (!storage->openBlob(blob_path))
    {
        return false;
    }

    const auto blob_size = storage->blobSize();
    std::unordered_map<std::string, Entry> parsed_entries;
    for (const auto& record : manifest_data.records)
    {
        if (record.path.empty())
        {
            return false;
        }
        if (record.offset > blob_size || record.size > blob_size - record.offset)
        {
            return false;
        }

        parsed_entries[normalizeAtlasPath(record.path)] = Entry{record.offset, record.size};
    }

    if (parsed_entries.empty())
    {
        return false;
    }

    storage_ = std::move(storage);
    entries_ = std::move(parsed_entries);
    return true;
}

std::string AtlasManager::readFile(const std::string& filename) const
{
    if (!storage_)
    {
        return {};
    }

    const auto it = entries_.find(normalizeAtlasPath(filename));
    if (it == entries_.end())
    {
        return {};
    }

    const auto& entry = it->second;
    if (entry.size == 0)
    {
        return {};
    }
    return storage_->read(entry.offset, entry.size);
}