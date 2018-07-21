#include "Engine.h"
#include "Font.h"
#include "InputBox.h"
#include "PotConv.h"

InputBox::InputBox(const std::string& title, int font_size)
    : title_(title)
{
    font_size_ = font_size;
}

InputBox::~InputBox()
{
}

void InputBox::dealEvent(BP_Event& e)
{
    switch (e.type)
    {
    case BP_TEXTINPUT:
    {
        auto converted = PotConv::conv(e.text.text, "utf-8", "cp936");
        //printf("input %s\n", converted.c_str());
        text_ += converted;
        break;
    }
    case BP_TEXTEDITING:
    {
        //看起来不太正常，待查
        //auto composition = e.edit.text;
        //auto cursor = e.edit.start;
        //auto selection_len = e.edit.length;
        //printf("editing %s\n", e.edit.text);
        break;
    }
    case BP_KEYDOWN:
        if (e.key.keysym.sym == BPK_BACKSPACE)
        {
            if (text_.size() >= 1)
            {
                text_.pop_back();
                if (text_.size() >= 1 && uint8_t(text_.back()) >= 128)
                {
                    text_.pop_back();
                }
            }
        }
        break;
    case BP_KEYUP:
        if (e.key.keysym.sym == BPK_RETURN)
        {
            exit_ = true;
        }
        break;
    }
}

void InputBox::draw()
{
    Font::getInstance()->drawWithBox(title_, font_size_, x_, y_, color_, 255);
    Font::getInstance()->drawWithBox(text_ + "_", font_size_, text_x_, text_y_, color_, 255);
    Engine::getInstance()->setTextInputRect(text_x_, text_y_, font_size_ * text_.size(), font_size_);
}

void InputBox::setInputPosition(int x, int y)
{
    x_ = x;
    y_ = y;
    text_x_ = x;
    text_y_ = y + font_size_ * 2;
}

void InputBox::onEntrance()
{
    Engine::getInstance()->startTextInput();
}

void InputBox::onExit()
{
    Engine::getInstance()->stopTextInput();
}
