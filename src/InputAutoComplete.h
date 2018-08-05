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
    
    // �������벻�����򲻷���
    // ����resultFunc�ķ���int���û��Զ���Ǹ������
    // < 0 �������֤ʧ��
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