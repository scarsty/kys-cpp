#pragma once
#include "TextBox.h"
#include <vector>

class TextBoxRoll : public TextBox
{
public:
    TextBoxRoll();
    virtual ~TextBoxRoll();

    using TextColorLines = std::vector<std::vector<std::pair<BP_Color, std::string>>>;

private:
    TextColorLines texts_;    //行，颜色，文字
    int roll_line_ = -1;
    int begin_line_ = 0;

public:
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;
    void setTexts(TextColorLines texts) { texts_ = texts; }
    void setRollLine(int rl) { roll_line_ = rl; }
};
