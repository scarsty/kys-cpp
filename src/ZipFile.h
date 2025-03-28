#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct zip;
typedef struct zip zip_t;

class ZipFile
{
public:
    ZipFile();
    ~ZipFile();

private:
    zip_t* zip_ = nullptr;
    std::shared_ptr<std::mutex> mutex_ = std::make_shared<std::mutex>();

public:
    bool opened() const { return zip_ != nullptr; }

    void open(const std::string& zip_filename);
    void setPassword(const std::string& password) const;
    std::string readFile(const std::string& filename) const;
    std::vector<std::string> getFileNames() const;
};
