#pragma once
#include "TextBox.h"
#include <map>

class Menu : public TextBox
{
public:
    Menu();
    virtual ~Menu();

public:
    virtual void dealEvent(EngineEvent& e) override;
    void arrange(int x, int y, int inc_x, int inc_y);
    virtual void onPressedOK() override;
    virtual void onPressedCancel() override;
    virtual void onEntrance() override;
    virtual void onExit() override;

    void setStartItem(int s) { start_ = s; }

    bool checkAllNormal();

    void setLRStyle(int i) { lr_style_ = i; }

    void setUDStyle(int i) { ud_style_ = i; }

    bool checkAllMouseNotIn();

protected:
    int start_ = 0;
    int lr_style_ = 0;    //左右切换的方式，十字键或肩键
    int ud_style_ = 0;    //上下切换的方式，若为1，只接受翻页键
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
    std::map<std::string, std::shared_ptr<RunNode>> childs_text_;
    std::string getStringFromResult(int i);

    std::string getResultString() { return getStringFromResult(result_); }

    int getResultFromString(std::string str);
};
