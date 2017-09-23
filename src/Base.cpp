#include "Base.h"
#include <stdio.h>
#include <stdarg.h>

std::vector<Base*> Base::base_vector_;

Base::Base()
{
}

Base::~Base()
{

}

void Base::LOG(const char* format, ...)
{
    char s[1000];
    va_list arg_ptr;
    va_start(arg_ptr, format);
    vsprintf(s, format, arg_ptr);
    va_end(arg_ptr);
    fprintf(stderr, s);
}

bool Base::inSide(int x, int y)
{
    return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_;
}
