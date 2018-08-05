#include "InputAutoComplete.h"
#include "Menu.h"
#include "Save.h"

InputAutoComplete::InputAutoComplete(const std::string& title, int font_size, const std::vector<std::string>& initialOptions) : 
    InputBox(title, font_size),
    initialOptions_(initialOptions)
{
    selections_ = new MenuText();    
    selections_->setStrings(initialOptions);
    addChild(selections_);
}

void InputAutoComplete::setSearchFunction(std::function<std::vector<std::string>(const std::string&)> searchFunc)
{
    searchFunc_ = searchFunc;
}

void InputAutoComplete::setValidation(std::function<int(const std::string&)> resultFunc)
{
    resultFunc_ = resultFunc;
}

void InputAutoComplete::setInputPosition(int x, int y)
{
    InputBox::setInputPosition(x, y);
    selections_->setPosition(x, y + 100);
}

void InputAutoComplete::trySearchAndSetSelections(const std::string & text)
{
    if (searchFunc_) {
        auto results = searchFunc_(text_);
        selections_->setVisible(results.size() != 0);
        selections_->setStrings(results);
        if (results.size() > 0) {
            selections_->forceActiveChild(0);
        }
    }
}

void InputAutoComplete::dealEvent(BP_Event& e)
{
    InputBox::dealEvent(e);

    bool refresh = false;
    if (e.key.keysym.sym == BPK_BACKSPACE || BP_TEXTINPUT == e.type) {
        refresh = true;
    }
    if (refresh) {                                                  // ����
        if (text_.size() == 0 || !searchFunc_) {                    // ���û�����룬����ʾinitialOptions
            selections_->setStrings(initialOptions_);
            selections_->setVisible(true);
        }
        else if (searchFunc_) {
            trySearchAndSetSelections(text_);
        }
    }
    else {
        // ���˲˵���ĳ��ѡ��
        if (result_ == -1 && selections_->getResult() >= 0) {
            selections_->setExit(false);
            auto selected = selections_->getResultString();
            // �������һ���Ļ���ֱ�ӵ�ȷ��
            if (text_ == selected) {
                result_ = 0;
                setExit(true);
            }
            else {
                text_ = selections_->getResultString();
                trySearchAndSetSelections(text_);
            }
            selections_->setResult(-1);
        }
    }

    // ���˻س�(result_�㲻��-1��)������Ҫ��֤
    if (result_ != -1 && resultFunc_) {
        result_ = resultFunc_(text_);
        if (result_ == -1) {
            // ���Ե�����ʾ֮���
        }
        exit_ = result_ != -1;
    }

}