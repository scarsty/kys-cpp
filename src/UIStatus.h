#pragma once
#include "Button.h"
#include "Element.h"
#include "Menu.h"
#include "Types.h"

class UIStatus : public Element
{
public:
    UIStatus();
    ~UIStatus();

protected:
    Button button_medicine_;
    Button button_detoxification_;
    Button button_leave_;

    Menu menu_;

    bool show_button_ = true;
    Role* role_ = nullptr;

public:
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;
    virtual void onPressedOK() override;
    void setShowButton(bool b) { show_button_ = b; }

    void setRole(Role* r) { role_ = r; }
    void setRoleName(std::string name);
    Role* getRole() { return role_; }
};
