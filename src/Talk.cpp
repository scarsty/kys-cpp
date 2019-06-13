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
        int len = width_;
        if (i + len >= content_.size()) { len = content_.size() - i; }

        auto line = content_.substr(i, len);

        //计算英文个数
        int eng_count = 0;
        for (int j = 0; j < line.size();)
        {
            if (uint8_t(line.at(j)) > 128)
            {
                j += 2;
            }
            else
            {
                eng_count++;
                j++;
            }
        }
        //若英文字符为奇数个，且最后一个字为中文，则多算一个字符
        if (eng_count % 2 == 1 && len == width_ && uint8_t(content_.at(i + len)) >= 128)
        {
            len++;
            line = content_.substr(i, len);
        }
        content_lines_.push_back(line);
        i += len;
    }
}
