#include "Menu.h"
#include "Button.h"
#include "Font.h"

Menu::Menu()
{
    result_ = 0;
}

Menu::~Menu()
{
}

void Menu::draw()
{
    if (tex_)
    {
        TextureManager::getInstance()->renderTexture(tex_, x_, y_);
    }
    if (text_.size() > 0)
    {
        Font::getInstance()->draw(text_, font_size_, x_ + text_x_, y_ + text_y_, { 255, 255, 255, 255 });
    }
}

void Menu::dealEvent(BP_Event& e)
{
    //此处处理键盘响应
    if (e.type == BP_KEYDOWN)
    {
        if (e.key.keysym.sym == BPK_LEFT || e.key.keysym.sym == BPK_UP)
        {
            setAllChildState(Normal);
            result_ = findNextVisibleChild(result_, -1);
            childs_[result_]->setState(Pass);
        }
        if (e.key.keysym.sym == BPK_RIGHT || e.key.keysym.sym == BPK_DOWN)
        {
            setAllChildState(Normal);
            result_ = findNextVisibleChild(result_, 1);
            childs_[result_]->setState(Pass);
        }
    }

    //获取当前正在被激活的按钮，主要针对鼠标
    for (int i = 0; i < childs_.size(); i++)
    {
        if (childs_[i]->getState() != Normal)
        {
            result_ = i;
        }
    }

    //重置当前被激活的按钮状态
    if (result_ >= 0 && childs_[result_]->getState() == Normal)
    {
        childs_[result_]->setState(Pass);
    }
}

void Menu::arrange(int x, int y, int inc_x, int inc_y)
{
    for (auto c : childs_)
    {
        if (c->getVisible())
        {
            c->setPosition(x_ + x, y_ + y);
            x += inc_x;
            y += inc_y;
        }
    }
}

void Menu::pressedOK()
{
    setExit(true);
}

MenuText::MenuText(std::vector<std::string> items) : MenuText()
{
    setStrings(items);
}

void MenuText::setStrings(std::vector<std::string> strings)
{
    clearChilds();
    int len = 0;
    int i = 0;
    for (auto& str : strings)
    {
        if (str.length() > len) { len = str.length(); }
        auto b = new Button();
        b->setText(str);
        addChild(b, 0, i * 25);
        i++;
    }
    w_ = 10 * len;
    h_ = 25 * strings.size();
}


