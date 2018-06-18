#pragma once
#include <string>
#include <vector>

class File
{
private:
    File();
    virtual ~File();
public:
    template <class T> static void readDataToVector(char* data, int length, std::vector<T>& v)
    {
        readDataToVector(data, length, v, sizeof(T));
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
        char* buffer = nullptr;
        int length;
        if (readFile(filename, &buffer, &length))
        {
            readDataToVector(buffer, length, v);
            delete[] buffer;
        }
    }

    template <class T> static void writeVectorToData(char* data, int length, std::vector<T>& v, int length_one)
    {
        int count = length / length_one;
        v.resize(count);
        for (int i = 0; i < count; i++)
        {
            memcpy(data + length_one * i, &v[i], length_one);
        }
    }
    static bool fileExist(const std::string& filename);
    static bool readFile(const std::string& filename, char** s, int* len);
    static void readFile(const std::string& filename, void* s, int len);
    static void writeFile(const std::string& filename, void* s, int len);

    enum FindMode
    {
        FINDLAST = 0,
        FINDFIRST = 1,
    };

    static std::string getFileExt(const std::string& filename);
    static std::string getFileMainname(const std::string& fileName, FindMode mode = FINDLAST);
    static std::string getFilenameWithoutPath(const std::string& fileName);
    static std::string changeFileExt(const std::string& filename, const std::string& ext);
    static std::string getFilePath(const std::string& filename);
    static std::vector<std::string> getFilesInDir(std::string dirname);
    static int getLastPathPos(const std::string& filename);

    static std::string getFileTime(std::string filename);
};

