#include "File.h"
#include <iostream>

File File::file;

File::File()
{
}


File::~File()
{
}

/**
*  读取文件
*  @param [in] 文件路径，缓存容器，文件大小
*  @return 文件读取成功返回true，失败返回false 
*/

bool File::readFile(const char * filename, char** s, int* len)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "Can not open file %s\n", filename);
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

void File::readFile(const char * filename, void* s, int len)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "Can not open file %s\n", filename);
        return;
    }
	fseek(fp, 0, 0);
    fread(s, len, 1, fp);
    fclose(fp);
}

