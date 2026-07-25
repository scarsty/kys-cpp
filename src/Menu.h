#pragma once
#include "TextBox.h"
#include <map>

constexpr RunNode::PointerResult menuContainerPointerResult(const PointerEvent&)
{
    return RunNode::PointerResult::Ignored;
}

template <class HitTest>
int menuPointerPressedChild(const PointerEvent& event, int childCount, HitTest hitTest)
{
    if (event.button != SDL_BUTTON_LEFT || event.phase != PointerPhase::ButtonDown)
    {
        return -1;
    }
    for (int i = childCount - 1; i >= 0; --i)
    {
        if (hitTest(i))
        {
            return i;
        }
    }
    return -1;
}

constexpr RunNode::State menuChildStateAfterPointerReset(int child, int activeChild)
{
    return child == activeChild ? RunNode::NodePass : RunNode::NodeNormal;
}

class Menu : public TextBox
{
public:
    Menu();
    virtual ~Menu();

public:
    virtual void dealEvent(EngineEvent& e) override;
    PointerResult onPointerEvent(const PointerEvent& event) override;
    void arrange(int x, int y, int inc_x, int inc_y);
    virtual void onPressedOK() override;
    virtual void onPressedCancel() override;
    virtual void onEntrance() override;
    virtual void onExit() override;
    void onPointerInputReset() override;

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
    void setStrings(std::vector<std::string> items, std::vector<Color> colors = {},
        std::vector<Color> outlineColors = {}, std::vector<bool> animateOutlines = {},
        std::vector<int> outlineThicknesses = {});
    //void draw() override;

    std::vector<std::string> strings_;
    std::map<std::string, std::shared_ptr<RunNode>> childs_text_;
    std::string getStringFromResult(int i);

    std::string getResultString() { return getStringFromResult(result_); }

    int getResultFromString(std::string str);
};
