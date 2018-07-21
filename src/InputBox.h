#pragma once

#include "TextBox.h"
#include <string>

class InputBox : public Element
{
public:
    InputBox(const std::string& title, int font_size_ = 20);
    virtual ~InputBox();
    void dealEvent(BP_Event& e) override;

    // Ö»ÈÏEnter
    virtual bool isPressOK(BP_Event& e) override
    {
        return e.type == BP_KEYUP && e.key.keysym.sym == BPK_RETURN;
    }
    virtual void draw() override;
    void setText(const std::string& text) { text_ = text; }
    std::string  getText() { return text_; };
    void setInputPosition(int x, int y);
    void setTextColor(BP_Color c) { color_ = c; }
    virtual void onPressedOK() override { exitWithResult(0); }
    virtual void onPressedCancel() override { setText(""); }

protected:
    std::string text_ = "";
    std::string title_;
    int font_size_;
    int text_x_ = 0, text_y_ = 0;
    BP_Color color_ = { 32, 32, 32, 255 };

};