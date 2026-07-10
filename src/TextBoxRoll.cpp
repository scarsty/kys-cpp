#include "TextBoxRoll.h"
#include "Font.h"
#include "GameUtil.h"

TextBoxRoll::TextBoxRoll()
{
}

TextBoxRoll::~TextBoxRoll()
{
}

void TextBoxRoll::draw()
{
    int x = x_, y = y_;
    int i = -1, line_count = 0;    //初值
    for (auto& line : texts_)
    {
        i++;
        if (i < begin_line_)
        {
            continue;
        }
        if (roll_line_ > 0 && line_count >= roll_line_)
        {
            break;
        }
        for (auto& color_text : line)
        {
            Font::getInstance()->draw(color_text.second, font_size_, x, y, color_text.first, color_text.first.a);
            x += font_size_ / 2 * color_text.second.length();
        }
        line_count++;
        x = x_;
        y += font_size_;
    }
}

void TextBoxRoll::dealEvent(EngineEvent& e)
{
    if (roll_line_ <= 0)
    {
        return;
    }
    begin_line_ = GameUtil::limit(begin_line_, 0, int(texts_.size() - roll_line_));
}

RunNode::PointerResult TextBoxRoll::onPointerEvent(const PointerEvent& event)
{
    if (roll_line_ <= 0 || event.phase != PointerPhase::Wheel)
    {
        return RunNode::onPointerEvent(event);
    }
    begin_line_ += event.wheel.y < 0 ? 1 : -1;
    begin_line_ = GameUtil::limit(begin_line_, 0, int(texts_.size() - roll_line_));
    return PointerResult::Handled;
}
