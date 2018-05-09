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

    BP_Texture* indexTex(int size, uint16_t c);
public:
    static Font* getInstance() { return &font_; };
    void draw(const std::string& text, int size, int x, int y, BP_Color color = { 255, 255, 255, 255 }, uint8_t alpha = 255);
    void drawWithBox(const std::string& text, int size, int x, int y, BP_Color color = { 255, 255, 255, 255 }, uint8_t alpha = 255, uint8_t alpha_box = 255);
};

