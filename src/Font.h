#pragma once
#include <string>
#include <map>
#include "Engine.h"

class Font
{
private:
    Font() {}
    ~Font();
    static Font font_;
    std::map<int, BP_Texture*> buffer_;  //缓存所有已经画过的字体

    std::string fontnamec_ = "../game/font/chinese.ttf";
    std::string fontnamee_ = "../game/font/english.ttf";

    int calIndex(int size, uint16_t c) { return size * 0x1000000 + c; }
public:
    static Font* getInstance() { return &font_; };
    void draw(std::string text, int size, int x, int y, BP_Color color, uint8_t alpha = 255);
};

