#pragma once
#include "UI.h"
#include "Button.h"

/**
* @file      Menu.h
* @brief     ≤Àµ•¿‡
* @author    xiaowu
*/

class Menu : public UI
{
protected:
    int result_ = -1;
public:
    Menu();
    virtual ~Menu();

    std::vector<Button*> bts;
    Texture* tex_ = nullptr;

    void draw() override;
    void dealEvent(BP_Event& e) override;
    void addButton(Button* b, int x, int y);
    int disappear = 0;
    void setDisappear(int d) { disappear = d; }
    static int menu(std::vector<Button*> bts, int x, int y);
public:
    int getResult() { auto r = result_; result_ = -1; return r; }
};

class MenuText : public Menu
{
private:
    std::vector<std::string> items_;
public:
    MenuText() {}
    ~MenuText() {}
    MenuText(std::vector<std::string> items);
    void setItems(std::vector<std::string> items);
    void draw() override;
    void dealEvent(BP_Event& e) override;
};



