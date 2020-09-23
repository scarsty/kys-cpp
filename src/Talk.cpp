#include "Talk.h"
#include "Engine.h"
#include "Font.h"
#include "PotConv.h"
#include <iostream>
#include <string>

void Talk::draw()
{
    if (!content_.empty())
    {
        Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, x_ + 225, y_ + 65, 530, 150);
        if (head_id_ >= 0)
        {
            if (head_style_ == 0)
            {
                TextureManager::getInstance()->renderTexture("head", head_id_, x_ + 50, y_ + 50);
            }
            else
            {
                TextureManager::getInstance()->renderTexture("head", head_id_, x_ + 770, y_ + 50);
            }
        }
        int end_line = current_line_ + height_;
        if (end_line > content_lines_.size()) { end_line = content_lines_.size(); }
        for (int i = current_line_; i < end_line; i++)
        {
            Font::getInstance()->draw(content_lines_[i], 24, x_ + 250, y_ + 75 + 25 * (i - current_line_), { 255, 255, 255, 255 });
        }
    }
}

void Talk::onPressedOK()
{
    if (current_line_ + height_ >= content_lines_.size())
    {
        setExit(true);
    }
    else
    {
        current_line_ += height_;
    }
    //e.type = BP_FIRSTEVENT;
}

void Talk::onEntrance()
{
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