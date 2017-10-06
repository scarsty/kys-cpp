#include "TextBox.h"

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