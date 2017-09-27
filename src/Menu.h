#pragma once
#include "Base.h"

class Menu : public Base
{
public:
    Menu();
    virtual ~Menu();
private:
    Texture* tex_ = nullptr;
    std::string title_;
public:
    void setTexture(Texture* t) { tex_ = t; }
    void setTitle(std::string t) { title_ = t; }
    void draw() override;
    void dealEvent(BP_Event& e) override;
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



