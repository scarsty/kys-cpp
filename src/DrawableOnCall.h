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
    DrawableOnCall(std::function<void(DrawableOnCall*)> draw, std::function<void(DrawableOnCall*, EngineEvent&)> deal_event);
    virtual ~DrawableOnCall() = default;
    void onEntrance() override { if (entrance_) entrance_(); }
    void setEntrance(std::function<void()> en) { entrance_ = en; }
    void setDealEvent(std::function<void(DrawableOnCall*, EngineEvent&)> deal_event) { deal_event_ = deal_event; }
    void updateScreenWithID(int id);
    int getID();
    void draw() override;
    virtual void dealEvent(EngineEvent& e) override;
private:
    int id_;
    std::function<void(DrawableOnCall*)> draw_;
    std::function<void(DrawableOnCall*, EngineEvent&)> deal_event_;
    std::function<void()> entrance_;
};