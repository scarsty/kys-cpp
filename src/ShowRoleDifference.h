#pragma once
#include "Font.h"
#include "Head.h"
#include "TextBox.h"
#include "convert.h"

//这个类专用于显示两个Role的不同，供升级，吃药等显示前后比较
//可以在属性变化前，以一临时对象记录，再比较前后的变化
class ShowRoleDifference : public TextBox
{
public:
    ShowRoleDifference();
    ~ShowRoleDifference();
    ShowRoleDifference(Role* r1, Role* r2) : ShowRoleDifference() { setTwinRole(r1, r2); }

    Role *role1_ = nullptr, *role2_ = nullptr;

    void setTwinRole(Role* r1, Role* r2)
    {
        role1_ = r1;
        role2_ = r2;
    }

    virtual void onPressedOK() override { exitWithResult(0); }
    virtual void onPressedCancel() override { exitWithResult(-1); }

    virtual void draw() override;

    void setShowHead(bool s) { show_head_ = s; }

    //void setBlackScreen(bool b) { black_screen_ = b; }

private:
    template <typename T>
    void showOneDifference(T& pro1, const std::string& format_str, int size, BP_Color c, int& x, int& y, int force = 0)
    {
        //注意，以下操作并不安全，请自己看着办
        auto diff = (char*)&pro1 - (char*)role1_;
        if (diff > sizeof(Role) || diff < 0) { return; }
        auto p1 = pro1;
        auto p2 = *(T*)(diff + (char*)role2_);
        if (p1 != p2 || force)
        {
            auto str = fmt::format(format_str.c_str(), p1, p2);
            Font::getInstance()->draw(str, size, x, y, c);
            y += size + 5;
        }
    }
    std::shared_ptr<Head> head1_, head2_;

    bool show_head_ = true;
    bool black_screen_ = true;
};
