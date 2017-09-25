#pragma once
#include "Base.h"

class Button : public Base
{
public:
    Button() {}
    Button(const std::string& path, int n1, int n2 = -1, int n3 = -1);

    virtual ~Button();

    //void InitMumber();
    void dealEvent(BP_Event& e) override;
    void draw();
    
private:
    Texture* normal = nullptr, *pass = nullptr, *press = nullptr; //三种状态的按钮图片
    std::string text_ = "";
    int font_size_ = 20;
public:

    void setText(std::string text);
    void setFongSize(int size) { font_size_ = size; }
};

