#pragma once
#include "UIStatus.h"

//这个类专用于显示两个Role的不同，供升级，吃药等显示前后比较
//可以在属性变化前，以一临时对象记录，再比较前后的变化
class ShowRoleDifference : public UIStatus
{
public:
    ShowRoleDifference();
    ~ShowRoleDifference();

    Role* role1_ = nullptr, *role2_ = nullptr;

    void setTwinRole(Role* r1, Role* r2) { role1_ = r1; role2_ = r2; }

    virtual void pressedOK() override { exitWithResult(0); }
    virtual void pressedCancel() override { exitWithResult(-1); }

    virtual void draw() override;

private:
    template<typename T> void showOneDifference(T& pro1, const std::string& format_str, int size, BP_Color c, int& x, int& y)
    {
        //注意，以下操作并不安全，请自己看着办
        auto p1 = pro1;
        auto p2 = *(T*)((char*)&pro1 - (char*)role1_ + (char*)role2_);
        if (p1 != p2)
        {
            auto str = convert::formatString(format_str.c_str(), p1, p2);
            Font::getInstance()->draw(str, size, x, y, c);
            y += size + 5;
        }
    }
    template<typename T> void showOneDifference(T& pro1a, T& pro1b, const std::string& format_str, int size, BP_Color c, int& x, int& y)
    {
        //注意，以下操作并不安全，请自己看着办
        auto p1a = pro1a;
        auto p2a = *(T*)((char*)&pro1a - (char*)role1_ + (char*)role2_);
        auto p1b = pro1b;
        auto p2b = *(T*)((char*)&pro1b - (char*)role1_ + (char*)role2_);
        if (p1a != p2a || p1b != p2b)
        {
            auto str = convert::formatString(format_str.c_str(), p1a, p1b, p2a, p2b);
            Font::getInstance()->draw(str, size, x, y, c);
            y += size + 5;
        }
    }
    Head* head1_ = nullptr;
    Head* head2_ = nullptr;

};



