#pragma once
#include "Element.h"
#include <functional>

// �����͸�һ����
// �û����ݺ���ָ��Ϲ����(ֱ�Ӱ��Լ���Ϊ����)
// Ȼ����һ��updateScreenID(int id)�ӿ�
// ���Ƿ��㲻�ø�ʲô����1W����
class DrawableOnCall : public Element {
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