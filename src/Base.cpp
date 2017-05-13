#include "Base.h"
#include <stdio.h>
#include <stdarg.h>

std::vector<Base*> Base::m_vcBase;



Base::Base()
{
}


Base::~Base()
{
}

void Base::LOG(const char *format, ...)
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
	return x > this->m_nx&& x< this->m_nx + this->m_nw && y > this->m_ny&& y < this->m_ny + this->m_nh;
}
