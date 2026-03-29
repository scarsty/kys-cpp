#include "TextBox.h"
#include "Font.h"
#include "TextureManager.h"

void TextBox::setFontSize(int size)
{
    for (auto c : childs_)
    {
        auto t = std::dynamic_pointer_cast<TextBox>(c);
        if (t)
        {
            t->setFontSize(size);
        }
    }
    font_size_ = size;
    if (resize_with_text_)
    {
        w_ = font_size_ * text_.length() / 2;
        h_ = font_size_;
    }
}

void TextBox::setText(std::string text)
{
    text_ = text;
    if (resize_with_text_ && texture_normal_id_ < 0)
    {
        w_ = font_size_ * Font::getTextDrawSize(text_) / 2;
        h_ = font_size_;
    }
}

void TextBox::setAlphaBox(Color outline_color, Color background_color)
{
    outline_color_ = outline_color;
    background_color_ = background_color;
    have_alpha_box_ = true;
}

void TextBox::setTexture(const std::string& path, int normal_id, int pass_id /*= -1*/, int press_id /*= -1*/)
{
    if (pass_id < 0) { pass_id = normal_id; }
    if (press_id < 0) { press_id = normal_id; }
    texture_path_ = path;
    texture_normal_id_ = normal_id;
    texture_pass_id_ = pass_id;
    texture_press_id_ = press_id;
}

void TextBox::setTextColor(Color c1, Color c2, Color c3)
{
    color_normal_ = c1;
    color_pass_ = c2;
    color_press_ = c3;
}

void TextBox::draw()
{
    if (!texture_path_.empty())
    {
        if (Engine::uiStyle() == 1)
        {
            // Replace texture background with dark translucent rounded box
            int radius = 8;
            Color bg = { 0, 0, 0, 180 };
            Color outline = { 180, 170, 140, 200 };
            Engine::getInstance()->fillRoundedRect(bg, x_, y_, w_, h_, radius);
            Engine::getInstance()->drawRoundedRect(outline, x_, y_, w_, h_, radius);
        }
        else
        {
            TextureManager::getInstance()->renderTexture(texture_path_, texture_normal_id_, x_, y_, TextureManager::RenderInfo{ { 255, 255, 255, 255 }, 255 });
        }
    }
    if (have_alpha_box_)
    {
        auto rect = Font::getBoxSize(Font::getTextDrawSize(text_), font_size_, x_ + text_x_, y_ + text_y_);
        if (Engine::uiStyle() == 1)
        {
            int radius = 6;
            Engine::getInstance()->fillRoundedRect(background_color_, rect.x, rect.y, rect.w, rect.h, radius);
            Engine::getInstance()->drawRoundedRect(outline_color_, rect.x, rect.y, rect.w, rect.h, radius);
        }
        else
        {
            // 背景
            Engine::getInstance()->fillColor(background_color_, rect.x, rect.y, rect.w, rect.h);
            // 上面的
            Engine::getInstance()->fillColor(outline_color_, rect.x, rect.y, rect.w, 1);
            // 下边的
            Engine::getInstance()->fillColor(outline_color_, rect.x, rect.y + rect.h, rect.w, 1);
            // 左边
            Engine::getInstance()->fillColor(outline_color_, rect.x, rect.y, 1, rect.h);
            // 右边
            Engine::getInstance()->fillColor(outline_color_, rect.x + rect.w, rect.y, 1, rect.h);
        }
    }
    //实际上仅用了一个颜色，需要有颜色变化请用button
    if (!text_.empty())
    {
        Color draw_color = color_normal_;
        if (Engine::uiStyle() == 1 && draw_color.r < 64 && draw_color.g < 64 && draw_color.b < 64)
        {
            draw_color = { 240, 230, 210, 255 };
        }
        // In dark mode, if we already drew a background (texture_path_ or alpha_box), skip drawWithBox to avoid double background
        bool skip_box = (Engine::uiStyle() == 1) && (!texture_path_.empty() || have_alpha_box_);
        if (have_box_ && !skip_box)
        {
            Font::getInstance()->drawWithBox(text_, font_size_, x_ + text_x_, y_ + text_y_, draw_color, 255);
        }
        else
        {
            Font::getInstance()->draw(text_, font_size_, x_ + text_x_, y_ + text_y_, draw_color, 255);
        }
    }
}
