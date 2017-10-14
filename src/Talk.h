#pragma once
#include "Element.h"
#include <vector>
#include <string>

class Talk : public Element
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
    int head_style_ = 0;
    int current_line_ = 0;
    int width_ = 40;
    int height_ = 5;
    std::vector<std::string> contents_;
public:
    void setContent(std::string c) { content_ = c; }
    void setHeadID(int h) { head_id_=h; }
    void setHeadStyle(int s) { head_style_ = s; }
    virtual void onEntrance() override;

    DEFAULT_OK_EXIT;
    DEFAULT_CANCEL_EXIT;
};

