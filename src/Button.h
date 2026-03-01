#pragma once
#include "TextBox.h"
#include <optional>

class Button : public TextBox
{
    uint8_t alpha_ = 192;
    bool text_only_ = false;
    std::optional<Color> custom_outline_;
    bool animate_outline_ = false;
    int outline_thickness_ = 1;

public:
    Button() { resize_with_text_ = true; }

    Button(const std::string& path, int normal_id, int pass_id = -1, int press_id = -1);

    virtual ~Button();

    //void InitMumber();
    void dealEvent(EngineEvent& e) override;
    void draw() override;

    int getTexutreID() { return texture_normal_id_; }

    void setAlpha(uint8_t alpha) { alpha_ = alpha; }
    void setTextOnly(bool t) { text_only_ = t; }
    void setCustomOutline(Color c) { custom_outline_ = c; }
    void clearCustomOutline() { custom_outline_.reset(); }
    void setAnimateOutline(bool a) { animate_outline_ = a; }
    void setOutlineThickness(int t) { outline_thickness_ = t; }

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
