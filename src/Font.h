#pragma once
#include <string>
#include "Engine.h"

class Font
{
private:
    static Font font_;
public:
    Font() {}
    ~Font() {}
    static Font* getInstance() { return &font_; };
    void draw(std::string text, int size, int x, int y, BP_Color c, uint8_t alpha = 255);
};

