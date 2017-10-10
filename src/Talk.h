#pragma once
#include "Head.h"
#include <vector>
#include <string>

class Talk : public Head
{
public:
    Talk() {}
    Talk(std::string c, int h = -1) : Talk() { setContent(c); setHeadID(h); }
    virtual ~Talk() {}

    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;
private:
    std::string content_;
    int head_id_ = -1;
public:
    void setContent(std::string c) { content_ = c; }
    void setHeadID(int h) { head_id_=h; }

    virtual void pressedOK() override { exitWithResult(0); }
    virtual void pressedCancel() override { exitWithResult(-1); }
};

