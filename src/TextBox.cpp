#include "TextBox.h"
#include "Font.h"

void TextBox::dealEvent(BP_Event& e)
{
    if (e.type == BP_MOUSEBUTTONUP || e.type == BP_KEYUP)
    {
        setExit(true);
    }
}

void TextBox::setFontSize(int size)
{
    for (auto c : childs_)
    {
        auto t = dynamic_cast<TextBox*>(c);
        if (t)
        {
            t->setFontSize(size);
        }
    }
    font_size_ = size;
}

void TextBox::setText(std::string text)
{
    text_ = text;
    w_ = font_size_ * text.length() / 2;
    h_ = font_size_;

}

void TextBox::setColor(BP_Color c1, BP_Color c2, BP_Color c3)
{
    color_normal_ = c1;
    color_pass_ = c2;
    color_press_ = c3;
}

void TextBox::draw()
{
    //实际上仅用了一个颜色，需要有颜色变化请用button
    Font::getInstance()->drawWithBox(text_, font_size_, x_ + text_x_, y_ + text_y_, color_normal_, 255);
}
