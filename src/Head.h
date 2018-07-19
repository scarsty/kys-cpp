#pragma once
#include "TextBox.h"
#include "Types.h"

//���ƴ�����ͷ��ļ���״̬
//ע�⣬�������ͼ̳д��࣬��Ϊ��ʹ��roleָ��
class Head : public TextBox
{
protected:
    Role* role_ = nullptr;
    bool only_head_ = false;
public:
    Head(Role* r = nullptr);
    virtual ~Head();

    virtual void draw() override;
    void setRole(Role* r) { role_ = r; }
    Role* getRole() { return role_; }
    void setOnlyHead(bool b) { only_head_ = b; }

    virtual void onPressedOK() override {}
    virtual void onPressedCancel() override {}
};

