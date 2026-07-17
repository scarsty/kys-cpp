#pragma once
#include "Engine.h"
#include "SimpleCC.h"
#include <map>
#include <string>
#include <unordered_map>

class UIRenderer;

class Font
{
    friend class UIRenderer;

private:
    Font();

    std::string fontnamec_;
    std::string fontnamee_;

    int stat_message_ = 0;

    int simplified_ = 1;

    std::map<std::string, TTF_Font*> font_buffer_;    //缓存字体文件

    std::map<uint32_t, std::map<int, Texture*>> buffer_;    //缓存画过的字体

    std::unordered_map<std::string, std::string> t2s_buffer_;    //缓存繁体转简体的结果

    SimpleCC cct2s_;

    int renderText(const std::string& text, int size, int x, int y, Color color, uint8_t alpha);

public:
    static Font* getInstance()
    {
        static Font f;
        return &f;
    }
    static Rect getBoxRect(int textLen, int size, int x, int y);
    void setStatMessage(int s) { stat_message_ = s; }
    int draw(const std::string& text, int size, int x, int y, Color color = { 255, 255, 255, 255 }, uint8_t alpha = 255);
    void drawWithBox(const std::string& text, int size, int x, int y, Color color = { 255, 255, 255, 255 }, uint8_t alpha = 255, uint8_t alpha_box = 255);
    void executeDrawCalls();
    void clearBuffer();
    int getBufferSize();
    static int getTextDrawSize(const std::string& text);
    void setSimplified(int s) { simplified_ = s; }
    std::string T2S(const std::string& str) { return cct2s_.conv(str); }
};
