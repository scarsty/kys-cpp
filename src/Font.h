#pragma once
#include "Engine.h"
#include <map>
#include <string>
#include <unordered_map>

class Font
{
private:
    Font();

    std::string fontnamec_;
    std::string fontnamee_;

    int stat_message_ = 0;

    int simplified_ = 1;

    std::map<uint32_t, std::map<int, BP_Texture*>> buffer_;    //缓存画过的字体

    std::unordered_map<std::string, std::string> t2s_buffer_;    //缓存繁体转简体的结果

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
    void setSimplified(int s) { simplified_ = s; }
};
