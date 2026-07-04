#pragma once
#include "RunNode.h"
#include "Types.h"

class ShowExp : public RunNode
{
public:
    ShowExp();
    ~ShowExp();

    std::vector<Role*> roles_;

    virtual void dealEvent(EngineEvent& e) override;
    virtual void onPressedOK() override;
    virtual void onPressedCancel() override { exitWithResult(-1); }
    virtual void draw() override;
    void setRoles(std::vector<Role*> r) { roles_ = r; }
    void setText(const std::string text) { text_ = text; }

private:
    std::string text_;
    bool ok_enabled_ = false;
};
