#pragma once
#include "Base.h"
#include "Types.h"
#include <memory>

//注意，部分节点继承至此，是为了使用role指针
class Head : public Base
{
protected:
    Role* role_ = nullptr;
private:
    static Texture square_;  //画血条专用的纹理，需要时改色，拉伸至适合的地方
public:
    Head(Role* r = nullptr);
    virtual ~Head();

    virtual void draw();
    void setRole(Role* r) { role_ = r; }
    Role* getRole() { return role_; }
};

