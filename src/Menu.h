#pragma once
#include "TextBox.h"

class Menu : public TextBox
{
public:
    Menu();
    virtual ~Menu();
public:
    //virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;;
    void arrange(int x, int y, int inc_x, int inc_y);
    virtual void pressedOK() override;
    virtual void pressedCancel() override { exitWithResult(-1); }
};

class MenuText : public Menu
{
public:
    MenuText() {}
    virtual ~MenuText() {}
    MenuText(std::vector<std::string> items);
    void setStrings(std::vector<std::string> items);
    //void draw() override;

    std::vector<std::string> strings_;
    std::map<std::string, Element*> childs_text_;
    std::string getStringFromResult(int i);
    std::string getResultString() { return getStringFromResult(result_); }
    int getResultFromString(std::string str);
};



