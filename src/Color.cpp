#include "Color.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else

#endif

Color Color::color_;

Color::Color()
{
    if (color_.color_map_.empty())
    {
        color_.color_map_ =
        {
            { CONSOLE_COLOR_NONE, "\e[0m" },
            { CONSOLE_COLOR_RED, "\e[0;31m" },
            { CONSOLE_COLOR_LIGHT_RED, "\e[1;31m" },
            { CONSOLE_COLOR_GREEN, "\e[0;32m" },
            { CONSOLE_COLOR_LIGHT_GREEN, "\e[1;32m" },
            { CONSOLE_COLOR_BLUE, "\e[0;34m" },
            { CONSOLE_COLOR_LIGHT_BLUE, "\e[1;34m" },
            { CONSOLE_COLOR_WHITE, "\e[1;37m" },
            { CONSOLE_COLOR_BLACK, "\e[0;30m" },
        };
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbiInfo);
        color_.old_color_ = csbiInfo.wAttributes;
#endif
    }
}


Color::~Color()
{
}

void Color::set(int c)
{
#ifdef _MSC_VER
    if (c != CONSOLE_COLOR_NONE)
    {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c);
    }
    else
    {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color_.old_color_);
    }
#else
    fprintf(stdout, color_.color_map_[c].c_str());
#endif
}
