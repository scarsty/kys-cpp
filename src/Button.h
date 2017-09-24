#pragma once
#include "Base.h"

class Button : public Base
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
    
private:
    Texture* normal = nullptr, *pass = nullptr, *press = nullptr; //三种状态的按钮图片
    int state_; //按钮状态
    std::string text_ = "";
    int font_size_ = 20;
public:
    void setState(ButtonState s) { state_ = s; }
    void setText(std::string text);
    void setFongSize(int size) { font_size_ = size; }
};

