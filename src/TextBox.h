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
public:
    void setFontSize(int size);
    void setText(std::string text);
    void setTexture(Texture* t) { tex_ = t; }
    void setTextPosition(int x, int y) { text_x_ = x; text_y_ = y; }
};

