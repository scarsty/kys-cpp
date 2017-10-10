#pragma once
#include "Element.h"
#include "Types.h"

class ShowExp : public Element
{
public:
    ShowExp();
    ~ShowExp();

    std::vector<Role*> roles_;

    virtual void pressedOK() override { exitWithResult(0); }
    virtual void pressedCancel() override { exitWithResult(-1); }
    virtual void draw() override;
    void setRoles(std::vector<Role*> r) { roles_ = r; }
};

