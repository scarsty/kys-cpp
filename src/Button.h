#pragma once
#include "Base.h"

class Button : public Base
{
public:
    Button() {}
    Button(const std::string& path, int normal_id, int pass_id = -1, int press_id = -1);
    void setTexture(const std::string& path, int normal_id, int pass_id = -1, int press_id = -1);
    virtual ~Button();

    //void InitMumber();
    void dealEvent(BP_Event& e) override;
    void draw();
    
private:
    std::string tex_path_ = "";
    int tex_normal_id_ = -1, tex_pass_id_ = -1, tex_press_id_ = -1; //三种状态的按钮图片
    std::string text_ = "";
    int font_size_ = 20;
public:

    void setText(std::string text);
    void setFongSize(int size) { font_size_ = size; }
};

