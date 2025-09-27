#include "InputBox.h"
#include "Engine.h"
#include "Font.h"

InputBox::InputBox()
{
}

InputBox::InputBox(const std::string& title, int font_size) :
    title_(title)
{
    font_size_ = font_size;
}

InputBox::~InputBox()
{
}

void InputBox::dealEvent(EngineEvent& e)
{
    switch (e.type)
    {
    case EVENT_TEXT_INPUT:
    {
        //auto converted = Font::getInstance()->T2S(e.text.text);
        //converted = PotConv::conv(converted, "utf-8", "cp936");
        //LOG("input %s\n", converted.c_str());
        text_ += e.text.text;
        break;
    }
    case EVENT_TEXT_EDITING:
    {
        //看起来不太正常，待查
        //auto composition = e.edit.text;
        //auto cursor = e.edit.start;
        //auto selection_len = e.edit.length;
        //LOG("editing %s\n", e.edit.text);
        break;
    }
    case EVENT_KEY_DOWN:
        if (e.key.key == K_BACKSPACE)
        {
            if (text_.size() >= 1)
            {
                uint8_t c = text_.back();
                text_.pop_back();
                if (c >= 128 && text_.size() >= 2 && uint8_t(text_.back()) >= 128)
                {
                    text_.pop_back();
                    text_.pop_back();
                }
            }
        }
        break;
    case EVENT_KEY_UP:
        if (e.key.key == K_RETURN)
        {
            //if (!text_.empty())
            //{
            result_ = 0;
            exit_ = true;
            //}
        }
        break;
    }
}

void InputBox::draw()
{
    if (getVisible()) {
        Font::getInstance()->drawWithBox(title_, font_size_, x_, y_, color_, 255);
        Font::getInstance()->drawWithBox(text_ + "_", font_size_, text_x_, text_y_, color_, 255);
        Engine::getInstance()->setTextInputArea(text_x_, text_y_, font_size_ * 15, font_size_);
    }
}

void InputBox::setInputPosition(int x, int y)
{
    x_ = x;
    y_ = y;
    text_x_ = x;
    text_y_ = y + font_size_ * 1.5;
}

void InputBox::onEntrance()
{
    Engine::getInstance()->startTextInput();
}

void InputBox::onExit()
{
    Engine::getInstance()->stopTextInput();
}
