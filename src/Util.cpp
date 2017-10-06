#include "Util.h"
#include <stdio.h>
#include <stdarg.h>

Util::Util()
{
}


Util::~Util()
{
}

void Util::LOG(const char* format, ...)
{
    char s[1000];
    va_list arg_ptr;
    va_start(arg_ptr, format);
    vsprintf(s, format, arg_ptr);
    va_end(arg_ptr);
    fprintf(stderr, s);
}
