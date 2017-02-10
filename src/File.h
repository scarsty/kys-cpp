#pragma once

class File
{
private:
    File();
    virtual ~File();
    static File file;
public:
    static void readFile(const char* filename, unsigned char** s, int* len);
    static void readFile(const char* filename, void* s, int len);
};

