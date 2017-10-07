#pragma once
#include "TextBox.h"

class Button : public TextBox
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
public:
    int getNormalTextureID() { return tex_normal_id_; }
};

