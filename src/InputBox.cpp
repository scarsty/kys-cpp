#include "InputBox.h"
#include "Engine.h"
#include "PotConv.h"
#include "Font.h"

InputBox::InputBox(const std::string & title, int font_size) : title_(title), font_size_(font_size)
{
    pre_stop_ = false;
    Engine::getInstance()->startTextInput();
}

InputBox::~InputBox()
{
    Engine::getInstance()->stopTextInput();
}

void InputBox::dealEvent(BP_Event & e)
{
    // TODO: 删除
    // 在此之前，简单的处理方式就是不允许删除，取消可允许重新输入
    switch (e.type) {
    case BP_TEXTINPUT: {
        auto converted = PotConv::conv(e.text.text, "utf-8", "cp936");
        printf("input %s\n", converted.c_str());
        setText(getText() + converted);
        break;
    }
    case BP_TEXTEDITING: {
        printf("editing %s\n", e.edit.text);
        break;
    }
    }
}

void InputBox::draw()
{
    Font::getInstance()->drawWithBox(title_, font_size_, x_ + text_x_, y_ + text_y_, color_, 255);
    Font::getInstance()->draw(text_, font_size_, x_ + text_x_, y_ + text_y_ + font_size_ + 10, color_, 255);
}

void InputBox::setInputPosition(int x, int y)
{
    text_x_ = x;
    text_y_ = y;
    Engine::getInstance()->setTextInputRect(x, y + font_size_ * 2);
}
