#include "File.h"
#include <iostream>
#include <fstream>
#include "minizip/zip.h"
#include "minizip/unzip.h"
#include "minizip/minishared.h"
#include <time.h>

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

int File::zip(std::string zip_file, std::vector<std::string> files)
{
    zip_fileinfo zi = { 0 };
    zipFile zip = zipOpen(zip_file.c_str(), APPEND_STATUS_CREATE);
    for (auto filename : files)
    {
        FILE* fp;
        if ((fp = fopen(filename.c_str(), "rb")) == NULL)
        {
            break;
        }
        fseek(fp, 0, SEEK_END);
        int length = ftell(fp);
        fseek(fp, 0, 0);
        auto s = (char*)malloc(length + 1);
        fread(s, length, 1, fp);
        fclose(fp);
        get_file_date(zip_file.c_str(), &zi.dos_date);
        //zi.dos_date = time(NULL);
        zipOpenNewFileInZip(zip, filename.c_str(), &zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
        zipWriteInFileInZip(zip, s, length);
        zipCloseFileInZip(zip);
        free(s);
    }
    zipClose(zip, NULL);
    return 0;
}

int File::unzip(std::string zip_file, std::vector<std::string> files)
{
    unzFile zip;
    zip = unzOpen(zip_file.c_str());
    for (auto filename : files)
    {
        unz_file_info zi = { 0 };
        if (unzLocateFile(zip, filename.c_str(), NULL) != UNZ_OK)
        {
            break;
        }
        char s[100];
        unzGetCurrentFileInfo(zip, &zi, s, 100, NULL, 0, NULL, 0);
        unzOpenCurrentFile(zip);

        FILE* fp;
        if ((fp = fopen(filename.c_str(), "wb")) == NULL)
        {
            break;
        }
        const int size_buf = 8192;
        void* buf = malloc(size_buf);
        uint32_t c = 0;
        while (c < zi.uncompressed_size)
        {
            int len = zi.uncompressed_size - c;
            if (len > size_buf) { len = size_buf; }
            unzReadCurrentFile(zip, buf, size_buf);
            fwrite(buf, len, 1, fp);
            c += size_buf;
        }
        fclose(fp);
        free(buf);
    }
    unzClose(zip);
    return 0;
}

std::string File::getFileTime(std::string filename)
{
    struct stat s;
    struct tm* filedate = NULL;
    time_t tm_t = 0;

    uint32_t dos_date;

    int sss = stat(filename.c_str(), &s);
    if (sss == 0)
    {
        tm_t = s.st_mtime;
        filedate = localtime(&tm_t);
        char buf[128] = { 0 };
        strftime(buf, 64, "%Y-%m-%d  %H:%M:%S", filedate);
        printf("%s:%s\n", filename.c_str(), buf);
        return buf;
    }
    return "--------------------";  //20
}
