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
    std::vector<std::string> buffer_;    // 添加数据时先复制到这里，否则close的时候，原内容如果已经被销毁就会出错

public:
    bool opened() const { return zip_ != nullptr; }

    void openRead(const std::string& zip_filename);
    void openWrite(const std::string& zip_filename);
    void create(const std::string& zip_filename);
    void setPassword(const std::string& password) const;
    std::string readFile(const std::string& filename) const;
    void readFileToBuffer(const std::string& filename, std::vector<char>& content) const;
    void addData(const std::string& filename, const char* p, int size);
    void addFile(const std::string& filename, const std::string& filename_in);
    std::vector<std::string> getFileNames() const;
};
