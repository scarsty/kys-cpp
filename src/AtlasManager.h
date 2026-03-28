#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class AtlasManager
{
public:
    AtlasManager();
    ~AtlasManager();

    class Storage;

    AtlasManager(const AtlasManager&) = delete;
    AtlasManager& operator=(const AtlasManager&) = delete;
    AtlasManager(AtlasManager&&) noexcept;
    AtlasManager& operator=(AtlasManager&&) noexcept;

    bool openRead(const std::string& base_path);
    bool opened() const { return storage_ != nullptr; }
    std::string readFile(const std::string& filename) const;

private:
    struct Entry
    {
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
    };

    void close();

    std::unique_ptr<Storage> storage_;
    std::unordered_map<std::string, Entry> entries_;
};