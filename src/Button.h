#pragma once
#include "TextBox.h"

class Button : public TextBox
{
public:
    Button() {}
    Button(const std::string& path, int normal_id, int pass_id = -1, int press_id = -1);
    virtual ~Button();

    //void InitMumber();
    void dealEvent(BP_Event& e) override;
    void draw();
};

