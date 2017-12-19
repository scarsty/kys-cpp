#include "libconvert.h"
#include <stdio.h>

#ifdef _MSC_VER
#define vsprintf vsprintf_s
//#define fopen fopen_s
#endif

std::string convert::readStringFromFile(const std::string& filename)
{
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp)
    {
        fprintf(stderr, "Can not open file %s\n", filename.c_str());
        return "";
    }
    fseek(fp, 0, SEEK_END);
    int length = ftell(fp);
    fseek(fp, 0, 0);
    std::string str;
    str.resize(length, '\0');
    fread((void*)str.c_str(), length, 1, fp);
    fclose(fp);
    return str;
}

void convert::writeStringToFile(const std::string& str, const std::string& filename)
{
    FILE* fp = fopen(filename.c_str(), "wb");
    int length = str.length();
    fwrite(str.c_str(), length, 1, fp);
    fclose(fp);
}

void convert::writeStringAppendToFile(const std::string& str, FILE* fp)
{
    int length = str.length();
    fwrite(str.c_str(), length, 1, fp);
    fputc('\n', fp);
}

int convert::replaceString(std::string& s, const std::string& oldstring, const std::string& newstring, int pos0/*=0*/)
{
    int pos = s.find(oldstring, pos0);
    if (pos >= 0)
    {
        s.erase(pos, oldstring.length());
        s.insert(pos, newstring);
    }
    return pos + newstring.length();
}

int convert::replaceAllString(std::string& s, const std::string& oldstring, const std::string& newstring)
{
    int pos = s.find(oldstring);
    while (pos >= 0)
    {
        s.erase(pos, oldstring.length());
        s.insert(pos, newstring);
        pos = s.find(oldstring, pos + newstring.length());
    }
    return pos + newstring.length();
}

void convert::replaceStringInFile(const std::string& oldfilename, const std::string& newfilename, const std::string& oldstring, const std::string& newstring)
{
    std::string s = readStringFromFile(oldfilename);
    if (s.length() <= 0) { return; }
    replaceString(s, oldstring, newstring);
    writeStringToFile(s, newfilename);
}

void convert::replaceAllStringInFile(const std::string& oldfilename, const std::string& newfilename, const std::string& oldstring, const std::string& newstring)
{
    std::string s = readStringFromFile(oldfilename);
    if (s.length() <= 0) { return; }
    replaceAllString(s, oldstring, newstring);
    writeStringToFile(s, newfilename);
}

std::string convert::formatString(const char* format, ...)
{
    char s[1000];
    va_list arg_ptr;
    va_start(arg_ptr, format);
    vsprintf(s, format, arg_ptr);
    va_end(arg_ptr);
    return s;
}

void convert::formatAppendString(std::string& str, const char* format, ...)
{
    char s[1000];
    va_list arg_ptr;
    va_start(arg_ptr, format);
    vsprintf(s, format, arg_ptr);
    va_end(arg_ptr);
    str += s;
}

std::string convert::findANumber(const std::string& s)
{
    bool findPoint = false;
    bool findNumber = false;
    bool findE = false;
    std::string n;
    for (int i = 0; i < s.length(); i++)
    {
        char c = s[i];
        if (c >= '0' && c <= '9' || c == '-' || c == '.' || c == 'e' || c == 'E')
        {
            if (c >= '0' && c <= '9' || c == '-')
            {
                findNumber = true;
                n += c;
            }
            if (c == '.')
            {
                if (!findPoint)
                {
                    n += c;
                }
                findPoint = true;
            }
            if (c == 'e' || c == 'E')
            {
                if (findNumber && !(findE))
                {
                    n += c;
                    findE = true;
                }
            }
        }
        else
        {
            if (findNumber)
            {
                break;
            }
        }
    }
    return n;
}

unsigned convert::findTheLast(const std::string& s, const std::string& content)
{
    int pos = 0, prepos = 0;
    while (pos >= 0)
    {
        prepos = pos;
        pos = s.find(content, prepos + 1);
        //printf("%d\n",pos);
    }
    return prepos;
}

std::vector<std::string> convert::splitString(std::string str, std::string pattern)
{
    std::string::size_type pos;
    std::vector<std::string> result;
    str += pattern; //扩展字符串以方便操作
    int size = str.size();

    for (int i = 0; i < size; i++)
    {
        pos = str.find(pattern, i);
        if (pos < size)
        {
            std::string s = str.substr(i, pos - i);
            result.push_back(s);
            i = pos + pattern.size() - 1;
        }
    }
    return result;
}

bool convert::isProChar(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'z') || (c >= '(' && c <= ')');
}

