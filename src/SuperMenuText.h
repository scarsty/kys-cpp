#pragma once
#include "Button.h"
#include "DrawableOnCall.h"
#include "InputBox.h"
#include "Menu.h"
#include "RunNode.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class SuperMenuText : public RunNode
{
public:
    // Backward-compatible constructor (with input box)
    SuperMenuText(const std::string& title, int fontSize, const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage);

    // New constructor with color support and input box control
    SuperMenuText(const std::string& title, int fontSize, const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage, const std::vector<Color>& itemColors, bool needInputBox);

    virtual ~SuperMenuText() = default;
    void dealEvent(EngineEvent& e) override;
    virtual void setInputPosition(int x, int y);
    void addDrawableOnCall(std::shared_ptr<DrawableOnCall> doc);
    void setMatchFunction(std::function<bool(const std::string&, const std::string&)> match);

    void setSearchTextVisible(bool v) { if (inputBox_) inputBox_->setVisible(v); }
    void setShowNavigationButtons(bool v)
    {
        previousButton_->setVisible(v);
        nextButton_->setVisible(v);
    }

    void setInputText(const std::string& text) { if (inputBox_) inputBox_->setText(text); }
    std::string getInputText() const { return inputBox_ ? inputBox_->getText() : std::string(); }

    void onEntrance() override;
    void onExit() override;

private:
    void defaultPage();
    void flipPage(int pageIncrement);
    void search(const std::string& text);
    void updateMaxPages();

    std::shared_ptr<InputBox> inputBox_; // Only present if needed, always added as child if present
    std::shared_ptr<TextBox> titleBox_;
    std::shared_ptr<Button> previousButton_, nextButton_;
    int currentPage_ = 0;
    int maxPages_ = 1;
    int itemsPerPage_ = 10;
    bool isDefaultPage_ = false;
    int fontSize_ = 20;
    std::shared_ptr<MenuText> selections_;

    std::vector<std::pair<int, std::string>> items_;
    std::vector<Color> itemColors_;

    std::vector<int> activeIndices_;
    std::vector<int> searchResultIndices_;
    std::vector<std::shared_ptr<DrawableOnCall>> drawableDocs_;
    std::function<bool(const std::string&, const std::string&)> matchFunction_;
    
};