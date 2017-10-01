#pragma once
#include <string>

class File
{
private:
    File();
    virtual ~File();
    static File file;
public:
    static bool fileExist(const std::string& filename);
    static bool readFile(const std::string& filename, char** s, int* len);
    static void readFile(const std::string& filename, void* s, int len);
    static void writeFile(const std::string& filename, void* s, int len);

    template <class T> static void readDataToVector(char* data, int length, std::vector<T>& v)
    {
        int count = length / sizeof(T);
        v.resize(count);
        memcpy(&v[0], data, count * sizeof(T));
    }

    template <class T> static void readDataToVector(char* data, int length, std::vector<T>& v, int length_one)
    {
        int count = length / length_one;
        v.resize(count);
        for (int i = 0; i < count; i++)
        {
            memcpy(&v[i], data + length_one * i, length_one);
        }
    }

    template <class T> static void readFileToVector(std::string filename, std::vector<T>& v)
    {
        char* buffer;
        int length;
        readFile(filename, &buffer, &length);
        readDataToVector(buffer, length, v);
        delete[] buffer;
    }

};

