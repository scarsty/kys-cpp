#pragma once
#include "TextBox.h"
#include "Types.h"

//绘制带人物头像的简明状态
//注意，部分类型继承此类，是为了使用role指针
class Head : public TextBox
{
protected:
    Role* role_ = nullptr;
    int style_ = 0;
    int width_ = 800;
    int always_light_ = 0;
    int HP_;
    int MP_;
    int PhysicalPower_;

public:
    Head(Role* r = nullptr);
    virtual ~Head();

    virtual void draw() override;
    void setRole(Role* r);
    Role* getRole() { return role_; }
    void setStyle(int i) { style_ = i; }
    void setAlwaysLight(int i) { always_light_ = i; }
    int getWidth() { return width_; }
    virtual void onPressedOK() override {}
    virtual void onPressedCancel() override {}
    virtual void backRun() override;
};

