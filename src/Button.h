#pragma once
#include "UI.h"

typedef std::function<void()> ButtonFunc;
#define BIND_FUNC(func) std::bind(&func, this)

/**
* @file      Button.h
* @brief     按钮类
* @author    xiaowu
* @Remark    20170710 修改底层读图函数
             20170714 哇，看了一下这个写的有很多问题啊，比如按钮的状态响应，应该自己去完成啊，而不是用菜单类去完成，菜单类完成的应该是自己的东西

*/

class Button : public UI
{
public:
    Button() {}
    Button(const std::string& path, int n1, int n2 = -1, int n3 = -1);

    enum ButtonState
    {
        Normal,
        Pass,
        Press,
    };

    virtual ~Button();

    //void InitMumber();
    void dealEvent(BP_Event& e) override;
    void draw();

    ButtonFunc func = nullptr;
    void setFunction(ButtonFunc func) { this->func = func; }
private:
    Texture* normal = nullptr, *pass = nullptr, *press = nullptr; //三种状态的按钮图片
    int state_; //按钮状态
public:
    void setState(ButtonState s) { state_ = s; }
};

