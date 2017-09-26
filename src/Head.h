#pragma once
#include "Base.h"
#include "Types.h"
#include <memory>

class Head : public Base
{
protected:
    Role* role_ = nullptr;
    static Texture square_;  //画血条专用的纹理，需要时改色，拉伸至适合的地方
public:
    Head(Role* r = nullptr);
    virtual ~Head();

    virtual void draw();
    void setRole(Role* r) { role_ = r; }
    Role* getRole() { return role_; }
};

