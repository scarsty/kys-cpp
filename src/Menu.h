#pragma once
#include "Element.h"

class Menu : public Element
{
public:
    Menu();
    virtual ~Menu();
protected:
    Texture* tex_ = nullptr;
    std::string title_;
    int title_x_ = 0, title_y_ = 0;
public:
    void setTexture(Texture* t) { tex_ = t; }
    void setTitle(std::string t) { title_ = t; }
    void setTitlePosition(int x, int y) { title_x_ = x; title_y_ = y; }
    void draw() override;
    void dealEvent(BP_Event& e) override;
    void arrange(int x, int y, int inc_x, int inc_y);
};

class MenuText : public Menu
{
public:
    MenuText() {}
    virtual ~MenuText() {}
    MenuText(std::vector<std::string> items);
    void setStrings(std::vector<std::string> items);
    void draw() override;
};

class TextBox : public Menu
{
public:
    TextBox() {}
    virtual ~TextBox() {}
    void dealEvent(BP_Event& e) override;
};

