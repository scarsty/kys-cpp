#pragma once
#include <string>

class File
{
private:
    File();
    virtual ~File();
    static File file;
public:
    static bool readFile(const std::string& filename, char** s, int* len);
    static void readFile(const std::string& filename, void* s, int len);
    static void writeFile(const std::string& filename, void* s, int len);
};

