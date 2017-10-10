#pragma once
#include "Head.h"
#include "Button.h"

class UIStatus : public Head
{
public:
    UIStatus();
    ~UIStatus();

    virtual void draw() override;

private:
    Button* button_medcine_;
    Button* button_detoxification_;
    Button* button_leave_;
    bool show_button_ = true;
public:
    virtual void dealEvent(BP_Event& e) override;
    virtual void pressedOK() override;
    void setShowButton(bool b) { show_button_ = b; }
};

