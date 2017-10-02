#pragma once
#include "Element.h"
#include "Types.h"
#include <memory>

//注意，部分节点继承至此，是为了使用role指针
class Head : public Element
{
protected:
    Role* role_ = nullptr;
public:
    Head(Role* r = nullptr);
    virtual ~Head();

    virtual void draw();
    void setRole(Role* r) { role_ = r; }
    Role* getRole() { return role_; }
};

