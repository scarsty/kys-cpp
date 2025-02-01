#pragma once
#include "TextBox.h"

class Button : public TextBox
{
    uint8_t alpha_ = 255;

public:
    Button() { resize_with_text_ = true; }

    Button(const std::string& path, int normal_id, int pass_id = -1, int press_id = -1);

    virtual ~Button();

    //void InitMumber();
    void dealEvent(EngineEvent& e) override;
    void draw() override;

    int getTexutreID() { return texture_normal_id_; }

    void setAlpha(uint8_t alpha) { alpha_ = alpha; }

    int button_id_ = -1;
};

class ButtonGetKey : public Button
{
public:
    virtual ~ButtonGetKey();
    void dealEvent(EngineEvent& e) override;

    virtual void onPressedOK() override {}

    virtual void onPressedCancel() override {}
};
