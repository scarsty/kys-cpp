#pragma once
#include <string>
#include <vector>

class Zip
{
public:
    Zip();
    ~Zip();

    static int zip(std::string zip_file, std::vector<std::string> files);
    static int unzip(std::string zip_file, std::vector<std::string> files);
};
