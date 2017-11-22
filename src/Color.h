#pragma once
#include <map>
#include <string>

typedef enum 
{
    CONSOLE_COLOR_NONE = -1,
    CONSOLE_COLOR_RED = 4,
    CONSOLE_COLOR_LIGHT_RED = 12,
    CONSOLE_COLOR_GREEN = 2,
    CONSOLE_COLOR_LIGHT_GREEN = 10,
    CONSOLE_COLOR_BLUE = 1,
    CONSOLE_COLOR_LIGHT_BLUE = 9,
    CONSOLE_COLOR_WHITE = 7,
    CONSOLE_COLOR_BLACK = 0,
} ConsoleColor;

class Color
{
private:
    Color();
    virtual ~Color();

    static Color color_;
    unsigned short old_color_;
    std::map<ConsoleColor, std::string> color_map_;

public:
    static void set(ConsoleColor c);
};

