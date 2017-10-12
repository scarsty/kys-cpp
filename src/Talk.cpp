#include "Talk.h"
#include <string>
#include <iostream>
#include "Engine.h"
#include "PotConv.h"
#include "Font.h"

void Talk::draw()
{
    if (!content_.empty())
    {
        Engine::getInstance()->fillColor({ 0, 0, 0, 192 }, x_ + 225, y_ + 65, 530, 150);
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
        if (end_line > contents_.size()) { end_line = contents_.size(); }
        for (int i = current_line_; i < end_line; i++)
        {
            Font::getInstance()->draw(contents_[i], 24, x_ + 250, y_ + 75 + 25 * (i - current_line_), { 255, 255, 255, 255 });
        }
    }
}

void Talk::dealEvent(BP_Event& e)
{
    if (e.type == BP_KEYUP)
    {
        if (current_line_ + height_ >= contents_.size())
        {
            setExit(true);
        }
        else
        {
            current_line_ += height_;
        }
        e.type = BP_FIRSTEVENT;
    }
}

void Talk::onEntrance()
{
    contents_.clear();
    current_line_ = 0;
    for (int i = 0; i < content_.size(); i += width_)
    {
        int len = width_;
        if (i + len >= content_.size()) { len = content_.size() - i; }
        contents_.push_back(content_.substr(i, len));
    }
}

