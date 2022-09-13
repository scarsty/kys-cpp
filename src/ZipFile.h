#pragma once
#include <zip.h>
#include <string>
#include <vector>

class ZipFile
{
public:
    ZipFile();
    ~ZipFile();

private:
    zip_t* zip_ = nullptr;

public:
    bool opened() { return zip_ != nullptr; }
    void openFile(const std::string& filename);
    std::string readEntryName(const std::string& entry_name);
    std::vector<std::string> getEntryNames();

};
