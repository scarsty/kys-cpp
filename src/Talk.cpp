#include "Talk.h"
#include "Engine.h"
#include "Font.h"
#include "TextureManager.h"
#include <string>

void Talk::draw()
{
    int w = Engine::getInstance()->getPresentWidth();
    int h = Engine::getInstance()->getPresentHeight();

    if (!content_.empty())
    {
        x_ = 250;
        int x_text = x_ + 25;
        if (head_id_ >= 0)
        {
            if (head_style_ == 0)
            {
                x_ = 250;    // Adjust x_ for left-aligned head
                TextureManager::getInstance()->renderTexture("head", head_id_, x_ - 200, y_ + 50);
                x_text = x_ + 25;
            }
            else
            {
                x_ = w - 530 - 250;    // Adjust x_ for right-aligned head
                x_text = x_ + 25;
                TextureManager::getInstance()->renderTexture("head", head_id_, x_ + 530 + 50, y_ + 50);
            }
        }
        Engine::getInstance()->fillColor({ 0, 0, 0, 160 }, x_, y_ + 65, 530, 150);
        int end_line = current_line_ + height_;
        if (end_line > content_lines_.size()) { end_line = content_lines_.size(); }
        for (int i = current_line_; i < end_line; i++)
        {
            Font::getInstance()->draw(content_lines_[i], 24, x_text, y_ + 75 + 34 * (i - current_line_), { 255, 255, 255, 255 });
        }
    }
}

void Talk::onPressedOK()
{
    Engine::getInstance()->setInterValControllerPress(200);
    if (current_line_ + height_ >= content_lines_.size())
    {
        setExit(true);
    }
    else
    {
        current_line_ += height_;
    }
    //e.type = BP_FIRSTEVENT;
    result_ = 0;
}

void Talk::onEntrance()
{
    int w = Engine::getInstance()->getPresentWidth();
    //width_ = (w - 400) / 12;    // 每行宽度
    content_lines_.clear();
    current_line_ = 0;
    int i = 0;
    while (i < content_.size())
    {
        int len = 0;
        //计算英文个数
        int eng_count = 0;
        int j = i;
        for (; j < content_.size();)
        {
            if (uint8_t(content_.at(j)) > 128)
            {
                j += 3;
                len += 2;
            }
            else
            {
                eng_count++;
                j++;
                len += 1;
            }
            if (len >= width_)
            {
                break;
            }
        }
        auto line = content_.substr(i, j - i);
        content_lines_.push_back(line);
        i = j;
    }
}