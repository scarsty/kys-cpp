#pragma once
#include "../others/zip.h"
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
    bool opened() const { return zip_ != nullptr; }

    void openFile(const std::string& filename);
    std::string readEntryName(const std::string& entry_name) const;
    std::vector<std::string> getEntryNames() const;

    static int zip(const std::string& zip_file, const std::vector<std::string>& files);
    static int unzip(const std::string& zip_file, const std::vector<std::string>& files);
};
