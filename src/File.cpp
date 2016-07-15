#include "File.h"
#include <iostream>

File File::file;

File::File()
{
}


File::~File()
{
}

void File::readFile(const char * filename, unsigned char** s, int* len)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "Can not open file %s\n", filename);
        return;
    }
    fseek(fp, 0, SEEK_END);
    int length = ftell(fp);
    *len = length;
    fseek(fp, 0, 0);
    *s = new unsigned char[length + 1];
    for (int i = 0; i <= length; (*s)[i++] = '\0');
    fread(*s, length, 1, fp);
    fclose(fp);
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
