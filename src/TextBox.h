#pragma once
#include "Element.h"

class TextBox : public Element
{
public:
    TextBox() {}
    virtual ~TextBox() {}
    void dealEvent(BP_Event& e) override;

protected:
    std::string text_ = "";
    int font_size_ = 20;
    int text_x_ = 0, text_y_ = 0;
    Texture* tex_ = nullptr;
    BP_Color color_normal_ = { 32, 32, 32, 255 };
    BP_Color color_pass_ = { 255, 255, 255, 255 };
    BP_Color color_press_ = { 255, 0, 0, 255 };
public:
    void setFontSize(int size);
    void setText(std::string text);
    void setTexture(Texture* t) { tex_ = t; }
    void setTextPosition(int x, int y) { text_x_ = x; text_y_ = y; }  //注意：这个会导致焦点出现问题，通常是为了实现一些其他效果，请勿任意使用
    void setColor(BP_Color c1, BP_Color c2, BP_Color c3);

    virtual void draw() override;
};

