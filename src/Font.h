#pragma once
#include "Engine.h"
#include <map>
#include <string>

class Font
{
private:
    Font();

    std::string fontnamec_ = "../game/font/chinese.ttf";
    std::string fontnamee_ = "../game/font/english.ttf";

    int stat_message_ = 0;

    std::map<uint32_t, std::map<int, BP_Texture*>> buffer_;    //缓存画过的字体

public:
    static Font* getInstance()
    {
        static Font f;
        return &f;
    }
    static BP_Rect getBoxSize(int textLen, int size, int x, int y);
    void setStatMessage(int s) { stat_message_ = s; }
    void draw(const std::string& text, int size, int x, int y, BP_Color color = { 255, 255, 255, 255 }, uint8_t alpha = 255);
    void drawWithBox(const std::string& text, int size, int x, int y, BP_Color color = { 255, 255, 255, 255 }, uint8_t alpha = 255, uint8_t alpha_box = 255);
    void drawText(const std::string& fontname, std::string& text, int size, int x, int y, uint8_t alpha, int align, BP_Color c);
    void clearBuffer();
    int getBufferSize();
    static int getTextDrawSize(const std::string& text);
};
