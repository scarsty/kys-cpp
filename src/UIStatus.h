#pragma once
#include "Element.h"
#include "Button.h"
#include "Types.h"

class UIStatus : public Element
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
    Role* role_ = nullptr;
public:
    virtual void dealEvent(BP_Event& e) override;
    virtual void pressedOK() override;
    void setShowButton(bool b) { show_button_ = b; }

    void setRole(Role* r) { role_ = r; }
    Role* getRole() { return role_; }
};

