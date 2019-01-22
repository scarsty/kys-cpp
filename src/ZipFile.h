#pragma once
#include <string>
#include <vector>

class ZipFile
{
public:
    ZipFile();
    ~ZipFile();

    static int zip(std::string zip_file, std::vector<std::string> files);
    static int unzip(std::string zip_file, std::vector<std::string> files);
};
