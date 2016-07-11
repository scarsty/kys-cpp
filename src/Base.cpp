#include "Base.h"
#include <stdio.h>
#include <stdarg.h>

std::vector<Base*> Base::baseVector;

Base::Base()
{
}


Base::~Base()
{
}

void Base::log(const char *format, ...)
{
	char s[1000];
	va_list arg_ptr;
	va_start(arg_ptr, format);
	vsprintf(s, format, arg_ptr);
	va_end(arg_ptr);
	fprintf(stderr, s);
}
