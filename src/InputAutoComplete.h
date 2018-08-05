#pragma once
#include "Menu.h"
#include "Button.h"
#include "Save.h"
#include "Button.h"
#include "GameUtil.h"
#include "InputBox.h"
#include <functional>
#include <vector>
#include <string>

class InputAutoComplete : public InputBox 
{
public:
    InputAutoComplete(const std::string& title, int font_size, const std::vector<std::string>& initialOptions);

    void setSearchFunction(std::function<std::vector<std::string>(const std::string&)> searchFunc);
    virtual void setInputPosition(int x, int y) override;
    
    // 若是输入不符合则不返回
    // 其中resultFunc的返还int是用户自定义非负数编号
    // < 0 则代表验证失败
    void setValidation(std::function<int(const std::string&)> resultFunc);

    void dealEvent(BP_Event& e) override;

    virtual void onPressedCancel() override { exitWithResult(-1); }

protected:
    std::function<std::vector<std::string>(const std::string&)> searchFunc_;
    std::function<int(const std::string&)> resultFunc_;

private:
    void trySearchAndSetSelections(const std::string& text);

    MenuText * selections_;
    std::vector<std::string> initialOptions_;
};