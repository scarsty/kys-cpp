#pragma once
#include "TextBox.h"

class Menu : public TextBox
{
public:
    Menu();
    virtual ~Menu();
public:
    virtual void dealEvent(BP_Event& e) override;
    void arrange(int x, int y, int inc_x, int inc_y);
    virtual void onPressedOK() override;
    virtual void onEntrance() override;
    virtual void frontRun() override;
    void setStartItem(int s) { start_ = s; }
    DEFAULT_CANCEL_EXIT;
protected:
    int start_ = 0;
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



