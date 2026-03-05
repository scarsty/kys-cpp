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

struct SuperMenuTextExtraOptions
{
    std::vector<Color> itemColors_;
    std::vector<Color> outlineColors_;     // Per-item custom outline color (empty = default)
    std::vector<bool> animateOutlines_;    // Per-item animated outline flag (empty = no animation)
    std::vector<int> outlineThicknesses_;  // Per-item outline thickness (empty = default 1)
    bool needInputBox_ = true;
    bool confirmation_ = false;
    bool exitable_ = false;
    bool returnIdxOnly = false;
};

class SuperMenuText : public RunNode
{
public:
    // Backward-compatible constructor (with input box)
    SuperMenuText(const std::string& title, int fontSize, const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage);

    // New constructor with color support and input box control
    SuperMenuText(const std::string& title, int fontSize, const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage, SuperMenuTextExtraOptions extraOpts);

    virtual ~SuperMenuText() = default;
    void dealEvent(EngineEvent& e) override;
    virtual void setInputPosition(int x, int y);
    void addDrawableOnCall(std::shared_ptr<DrawableOnCall> doc);
    void setMatchFunction(std::function<bool(const std::string&, const std::string&)> match);

    void setSearchTextVisible(bool v)
    {
        if (inputBox_)
        {
            inputBox_->setVisible(v);
        }
    }
    void setShowNavigationButtons(bool v)
    {
        showNavButtons_ = v;
        previousButton_->setVisible(v);
        nextButton_->setVisible(v);
        // Re-layout if position was already set
        if (inputPosSet_)
            setInputPosition(inputX_, inputY_);
    }

    void setInputText(const std::string& text)
    {
        if (inputBox_)
        {
            inputBox_->setText(text);
        }
    }
    std::string getInputText() const { return inputBox_ ? inputBox_->getText() : std::string(); }

    void setDoubleTapMode(bool v) { doubleTapMode_ = v; }

    void onEntrance() override;
    void onExit() override;

protected:
    void setConfirmation(bool confirmation) { extraOpts_.confirmation_ = confirmation; }

private:
    void defaultPage();
    void flipPage(int pageIncrement);
    void search(const std::string& text);
    void updateMaxPages();
    void updateNavigationButtons();

    std::shared_ptr<InputBox> inputBox_;    // Only present if needed, always added as child if present
    std::shared_ptr<TextBox> titleBox_;
    std::shared_ptr<Button> previousButton_, nextButton_;
    int currentPage_ = 0;
    int maxPages_ = 1;
    int itemsPerPage_ = 10;
    bool isDefaultPage_ = false;
    int fontSize_ = 20;
    std::shared_ptr<MenuText> selections_;

    std::vector<std::pair<int, std::string>> items_;
    SuperMenuTextExtraOptions extraOpts_;

    std::vector<int> activeIndices_;
    std::vector<int> searchResultIndices_;
    std::vector<std::shared_ptr<DrawableOnCall>> drawableDocs_;
    std::function<bool(const std::string&, const std::string&)> matchFunction_;
    bool showNavButtons_ = true;
    bool inputPosSet_ = false;
    int inputX_ = 0, inputY_ = 0;
    bool doubleTapMode_ = false;
    int lastTappedIdx_ = -1;
    // Timestamp (ms) when lastTappedIdx_ was locked.  Used to enforce a
    // minimum gap between lock and commit so burst events from a single
    // browser gesture (WASM batches touchstart→touchend→synthetic mousedown
    // →mouseup all in one rAF callback) cannot fire both actions at once.
    double tapLockTime_ = -1.0;
    static constexpr double kDoubleTapMinIntervalMs = 200.0;
};