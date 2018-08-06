#pragma once

#include "TextBox.h"
#include <string>

class InputBox : public TextBox
{
public:
    InputBox();
    InputBox(const std::string& title, int font_size);
    virtual ~InputBox();
    void setTitle(const std::string& t) { title_ = t; }

    void dealEvent(BP_Event& e) override;

    virtual void draw() override;
    void setText(const std::string& text) { text_ = text; }
    std::string  getText() { return text_; };
    virtual void setInputPosition(int x, int y);
    void setTextColor(BP_Color c) { color_ = c; }

    virtual void onPressedCancel() override { exitWithResult(-1); }
    virtual void onPressedOK() override { }
    virtual void onEntrance() override;
    virtual void onExit() override;

protected:
    std::string title_;
    int text_x_ = 0, text_y_ = 0;
    BP_Color color_ = { 32, 32, 32, 255 };
};
