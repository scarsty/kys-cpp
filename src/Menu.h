#pragma once
#include "Base.h"

class Menu : public Base
{
public:
    Menu();
    virtual ~Menu();

    Texture* tex_ = nullptr;
    void draw() override;
    void dealEvent(BP_Event& e) override;
    int disappear = 0;
    void setDisappear(int d) { disappear = d; }
};

class MenuText : public Menu
{
public:
    MenuText() {}
    virtual ~MenuText() {}
    MenuText(std::vector<std::string> items);
    void setStrings(std::vector<std::string> items);
    void draw() override;
    void dealEvent(BP_Event& e) override;
};



