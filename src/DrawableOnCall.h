#pragma once
#include "RunNode.h"
#include <functional>

// 这个类就干一件事
// 用户传递函数指针瞎画，(直接把自己作为参数)
// 然后有一个updateScreenID(int id)接口
// 就是方便不用干什么都搞1W个类
class DrawableOnCall : public RunNode {
public:
    DrawableOnCall(std::function<void(DrawableOnCall*)> draw);
    virtual ~DrawableOnCall() = default;
    virtual void onEntrance() { if (entrance_) entrance_(); }
    void setEntrance(std::function<void()> en) { entrance_ = en; }
    void updateScreenWithID(int id);
    int getID();
    virtual void draw();
private:
    int id_;
    std::function<void(DrawableOnCall*)> draw_;
    std::function<void()> entrance_;
};