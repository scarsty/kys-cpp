#pragma once
#include "TextBox.h"

class Button : public TextBox
{
public:
    Button() { resize_with_text_ = true; }
    Button(const std::string& path, int normal_id, int pass_id = -1, int press_id = -1);

    virtual ~Button();

    //void InitMumber();
    void dealEvent(BP_Event& e) override;
    void draw() override;
    int getTexutreID() { return texture_normal_id_; }
};

