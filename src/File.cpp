#include "File.h"
#include <iostream>
#include <fstream>
#include <time.h>
#ifdef _WIN32
#include <Windows.h>
#include <strsafe.h>
#else
#include "dirent.h"
#endif
#ifdef __GNUC__
#include <sys/types.h>
#include <sys/stat.h>
#endif

File::File()
{
}

File::~File()
{
}

bool File::fileExist(const std::string& filename)
{
    if (filename.length() <= 0)
    {
        return false;
    }

    std::fstream file;
    bool ret = false;
    file.open(filename.c_str(), std::ios::in);
    if (file)
    {
        ret = true;
        file.close();
    }
    return ret;
}

bool File::readFile(const std::string& filename, char** s, int* len)
{
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp)
    {
        fprintf(stderr, "Can not open file %s\n", filename.c_str());
        return false;
    }
    fseek(fp, 0, SEEK_END);
    int length = ftell(fp);
    *len = length;
    fseek(fp, 0, 0);
    *s = new char[length + 1];
    for (int i = 0; i <= length; (*s)[i++] = '\0');
    fread(*s, length, 1, fp);
    fclose(fp);
    return true;
}

void File::readFile(const std::string& filename, void* s, int len)
{
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp)
    {
        fprintf(stderr, "Can not open file %s\n", filename.c_str());
        return;
    }
    fseek(fp, 0, 0);
    fread(s, len, 1, fp);
    fclose(fp);
}

void File::writeFile(const std::string& filename, void* s, int len)
{
    FILE* fp = fopen(filename.c_str(), "wb");
    if (!fp)
    {
        fprintf(stderr, "Can not open file %s\n", filename.c_str());
        return;
    }
    fseek(fp, 0, 0);
    fwrite(s, len, 1, fp);
    fclose(fp);
}

std::string File::getFileExt(const std::string& filename)
{
    int pos_p = getLastPathPos(filename);
    int pos_d = filename.find_last_of('.');
    if (pos_p < pos_d)
    {
        return filename.substr(pos_d + 1);
    }
    return "";
}

//find the last point as default, and find the first when mode is 1
std::string File::getFileMainname(const std::string& filename, FindMode mode)
{
    int pos_p = getLastPathPos(filename);
    int pos_d = filename.find_last_of('.');
    if (mode == FINDFIRST)
    {
        pos_d = filename.find_first_of('.', pos_p + 1);
    }
    if (pos_p < pos_d)
    {
        return filename.substr(0, pos_d);
    }
    return filename;
}

std::string File::getFilenameWithoutPath(const std::string& filename)
{
    int pos_p = getLastPathPos(filename);
    if (pos_p != std::string::npos)
    {
        return filename.substr(pos_p + 1);
    }
    return filename;
}

std::string File::changeFileExt(const std::string& filename, const std::string& ext)
{
    auto e = ext;
    if (e != "" && e[0] != '.')
    {
        e = "." + e;
    }
    return getFileMainname(filename) + e;
}

std::string File::getFilePath(const std::string& filename)
{
    int pos_p = getLastPathPos(filename);
    if (pos_p != std::string::npos)
    {
        return filename.substr(0, pos_p);
    }
    return "";
}

std::vector<std::string> File::getFilesInDir(std::string dirname)
{
#ifdef _WIN32
    WIN32_FIND_DATAA ffd;
    //LARGE_INTEGER filesize;
    std::string szDir;
    //size_t length_of_arg;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    DWORD dwError = 0;
    std::vector<std::string> ret;

    szDir = dirname + "\\*";
    hFind = FindFirstFileA(szDir.c_str(), &ffd);

    if (INVALID_HANDLE_VALUE == hFind)
    {
        fprintf(stderr, "get file name error\n");
        return ret;
    }
    do
    {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            std::string filename = ffd.cFileName;//(const char*)
            std::string filedir = filename;
            ret.push_back(filedir);
        }
    }
    while (FindNextFileA(hFind, &ffd) != 0);


    dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES)
    {
        fprintf(stderr, "FindFirstFile error\n");
        return ret;
    }
    FindClose(hFind);
    return ret;
#else
    DIR* dir;
    struct dirent* ptr;
    dir = opendir(dirname.c_str());
    std::vector<std::string> ret;
    while ((ptr = readdir(dir)) != NULL)
    {
        std::string path = std::string(ptr->d_name);
        ret.push_back(path);
    }
    closedir(dir);
    //std::sort(ret.begin(), ret.end());
    return ret;
#endif
}

int File::getLastPathPos(const std::string& filename)
{
    int pos_win = std::string::npos;
#ifdef _WIN32
    pos_win = filename.find_last_of('\\');
#endif // _WIN32
    int pos_other = filename.find_last_of('/');
    if (pos_win == std::string::npos)
    {
        return pos_other;
    }
    else
    {
        if (pos_other == std::string::npos)
        {
            return pos_win;
        }
        else
        {
            return pos_other > pos_win ? pos_other : pos_win;
        }
    }
}

char* File::getIdxContent(std::string filename_idx, std::string filename_grp, std::vector<int>* offset, std::vector<int>* length)
{
    int* Ridx;
    int len = 0;
    File::readFile(filename_idx, (char**)&Ridx, &len);

    offset->resize(len / 4 + 1);
    length->resize(len / 4);
    offset->at(0) = 0;
    for (int i = 0; i < len / 4; i++)
    {
        (*offset)[i + 1] = Ridx[i];
        (*length)[i] = (*offset)[i + 1] - (*offset)[i];
    }
    int total_length = offset->back();
    delete[] Ridx;

    auto Rgrp = new char[total_length];
    File::readFile(filename_grp, Rgrp, total_length);
    return Rgrp;
}

std::string File::getFileTime(std::string filename)
{
#if defined(__clang__) && defined(_WIN32)
    struct __stat64 s;
    int sss = __stat64(filename.c_str(), &s);
#else
    struct stat s;
    int sss = stat(filename.c_str(), &s);
#endif
    struct tm* filedate = NULL;
    time_t tm_t = 0;
    uint32_t dos_date;
    if (sss == 0)
    {
        tm_t = s.st_mtime;
        filedate = localtime(&tm_t);
        char buf[128] = { 0 };
        strftime(buf, 64, "%Y-%m-%d  %H:%M:%S", filedate);
        fprintf(stdout, "%s:%s\n", filename.c_str(), buf);
        return buf;
    }
    return "--------------------";  //20
}
