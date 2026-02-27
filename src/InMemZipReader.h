#pragma once
#include <memory>
#include <string>
#include <vector>

struct zip;
typedef struct zip zip_t;
struct zip_file;
typedef struct zip_file zip_file_t;

// Read-only zip reader that loads the entire zip into memory on open.
// All subsequent reads are pure in-memory operations — no filesystem access.
// Useful for slow/high-latency backends like WasmFS fetch.
class InMemZipReader
{
public:
    InMemZipReader() = default;
    ~InMemZipReader() = default;

    InMemZipReader(const InMemZipReader&) = delete;
    InMemZipReader& operator=(const InMemZipReader&) = delete;
    InMemZipReader(InMemZipReader&&) = default;
    InMemZipReader& operator=(InMemZipReader&&) = default;

    bool openRead(const std::string& zip_filename);
    bool opened() const { return zip_ != nullptr; }
    std::string readFile(const std::string& filename) const;

private:
    struct ZipDeleter
    {
        void operator()(zip_t* z) const;
    };
    struct ZipFileDeleter
    {
        void operator()(zip_file_t* zf) const;
    };

    std::unique_ptr<zip_t, ZipDeleter> zip_;
    std::vector<char> data_;
};
