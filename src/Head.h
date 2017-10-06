#pragma once
#include "Element.h"
#include "Types.h"
#include <memory>

//绘制带人物头像的简明状态
//注意，部分类型继承此类，是为了使用role指针
class Head : public Element
{
protected:
    Role* role_ = nullptr;
public:
    Head(Role* r = nullptr);
    virtual ~Head();

    virtual void draw() override;
    void setRole(Role* r) { role_ = r; }
    Role* getRole() { return role_; }
};

