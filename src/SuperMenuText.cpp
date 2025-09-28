#include "SuperMenuText.h"
#include "../others/Hanz2Piny.h"
#include "Font.h"
#include "PotConv.h"
#include <algorithm>
#include <cmath>
#include <utility>

// Backward-compatible constructor: always creates an InputBox
SuperMenuText::SuperMenuText(const std::string& title, int fontSize,
    const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage)
    : SuperMenuText(title, fontSize, allItems, itemsPerPage, {}, true)
{
}

// New constructor: can control InputBox creation
SuperMenuText::SuperMenuText(const std::string& title, int fontSize,
    const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage,
    const std::vector<Color>& itemColors, bool needInputBox)
    : items_(allItems), itemsPerPage_(itemsPerPage), itemColors_(itemColors), fontSize_(fontSize)
{
    if (needInputBox) {
        inputBox_ = std::make_shared<InputBox>(title, fontSize);
        addChild(inputBox_);
    } else {
        titleBox_ = std::make_shared<TextBox>();
        titleBox_->setText(title);
        titleBox_->setFontSize(fontSize_);
        addChild(titleBox_);
    }

    if (itemColors_.size() < items_.size()) {
        itemColors_.resize(items_.size(), {255,255,255,255});
    }

    previousButton_ = std::make_shared<Button>();
    previousButton_->setText("上一頁PgUp");
    addChild(previousButton_);

    nextButton_ = std::make_shared<Button>();
    nextButton_->setText("下一頁PgDown");
    addChild(nextButton_);

    selections_ = std::make_shared<MenuText>();
    addChild(selections_);

    setAllChildState(NodeNormal);
    isDefaultPage_ = false;
    currentPage_ = 0;
    maxPages_ = 1;
    defaultPage();

    // Default match function (pinyin search)
    std::function<bool(const std::string&, const std::string&)> match = [&](const std::string& text, const std::string& name) -> bool
    {
        std::string pinyin = Hanz2Piny::hanz2pinyin(name);
        int p = 0;
        for (int i = 0; i < text.size(); i++)
        {
            int p1 = pinyin.find_first_of(text[i], p);
            if (p1 < 0)
            {
                return false;
            }
            p = p1 + 1;
        }
        return true;
    };
    setMatchFunction(match);
}

void SuperMenuText::setInputPosition(int x, int y)
{
    if (inputBox_) {
        inputBox_->setInputPosition(x, y);
        selections_->setPosition(x, y + fontSize_ * 3);
        previousButton_->setPosition(x, y - fontSize_ * 1.5);
        nextButton_->setPosition(x + fontSize_ * 5, y - fontSize_ * 1.5);
    } else {
        titleBox_->setPosition(x, y);
        selections_->setPosition(x, y + fontSize_ * 2);
        previousButton_->setPosition(x, y - 30);
        nextButton_->setPosition(x + 150, y - 30);
    }
}

void SuperMenuText::addDrawableOnCall(std::shared_ptr<DrawableOnCall> doc)
{
    drawableDocs_.push_back(doc);
    addChild(doc);
}

void SuperMenuText::setMatchFunction(std::function<bool(const std::string&, const std::string&)> match)
{
    matchFunction_ = match;
}

void SuperMenuText::defaultPage()
{
    if (isDefaultPage_) return;
    std::vector<std::string> displays;
    std::vector<Color> colors;
    searchResultIndices_.clear();
    activeIndices_.clear();
    for (int i = 0; i < items_.size(); i++) {
        if (i < std::min((std::size_t)itemsPerPage_, items_.size())) {
            activeIndices_.push_back(i);
            displays.push_back(items_[i].second);
            colors.push_back(itemColors_[i]);
        }
        searchResultIndices_.push_back(i);
    }
    currentPage_ = 0;
    updateMaxPages();
    selections_->setStrings(displays, colors);
    if (!displays.empty()) {
        selections_->forceActiveChild(0);
    }
    isDefaultPage_ = true;
}

void SuperMenuText::flipPage(int pageIncrement)
{
    if (currentPage_ + pageIncrement >= 0 && currentPage_ + pageIncrement < maxPages_)
    {
        currentPage_ += pageIncrement;
        int startIdx = currentPage_ * itemsPerPage_;
        std::vector<std::string> displays;
        std::vector<Color> colors;
        activeIndices_.clear();
        for (std::size_t i = startIdx; i < std::min(searchResultIndices_.size(), (std::size_t)startIdx + itemsPerPage_); i++)
        {
            int idx = searchResultIndices_[i];
            activeIndices_.push_back(idx);
            displays.push_back(items_[idx].second);
            colors.push_back(itemColors_[idx]);
        }
        selections_->setStrings(displays, colors);
    }
}

void SuperMenuText::search(const std::string& text)
{
    std::vector<std::string> results;
    std::vector<Color> colors;
    activeIndices_.clear();
    searchResultIndices_.clear();
    for (int i = 0; i < items_.size(); i++)
    {
        bool matched = false;
        auto& opt = items_[i];
        if (Font::getInstance()->T2S(opt.second).contains(text) || (matchFunction_ && matchFunction_(text, opt.second)))
        {
            matched = true;
        }
        if (matched)
        {
            if (results.size() < itemsPerPage_)
            {
                results.emplace_back(opt.second);
                colors.push_back(itemColors_[i]);
                activeIndices_.push_back(i);
            }
            searchResultIndices_.push_back(i);
        }
    }
    updateMaxPages();
    currentPage_ = 0;
    selections_->setStrings(results, colors);
    selections_->forceActiveChild(0);
    isDefaultPage_ = false;
}

void SuperMenuText::updateMaxPages()
{
    maxPages_ = std::ceil((searchResultIndices_.size() / (double)itemsPerPage_));
}

void SuperMenuText::dealEvent(EngineEvent& e)
{
    if (previousButton_->getState() == NodePress && e.type == EVENT_MOUSE_BUTTON_UP)
    {
        flipPage(-1);
        previousButton_->setState(NodeNormal);
        previousButton_->setResult(-1);
    }
    else if (nextButton_->getState() == NodePress && e.type == EVENT_MOUSE_BUTTON_UP)
    {
        flipPage(1);
        nextButton_->setState(NodeNormal);
        nextButton_->setResult(-1);
    }

    bool research = false;
    std::string searchText;
    if (inputBox_)
    {
        searchText = Font::getInstance()->T2S(inputBox_->getText());
    }

    // Only handle search if inputBox_ is present
    if (inputBox_) {
        switch (e.type)
        {
        case EVENT_TEXT_INPUT:
        case EVENT_KEY_DOWN:
            research = true;
            break;
        default:
            break;
        }
    }

    switch (e.type)
    {
    case EVENT_KEY_UP:
        switch (e.key.key)
        {
        case K_PAGEUP:
            flipPage(-1);
            break;
        case K_PAGEDOWN:
            flipPage(1);
            break;
        }
        break;
    case EVENT_MOUSE_WHEEL:
        if (e.wheel.y > 0)
        {
            flipPage(-1);
        }
        else if (e.wheel.y < 0)
        {
            flipPage(1);
        }
        break;
    default:
        break;
    }

    if (inputBox_ && searchText.empty())
    {
        defaultPage();
    }

    if (research && inputBox_)
    {
        search(searchText);
    }

    int selectionIdx = selections_->getActiveChildIndex();
    if (selectionIdx >= 0 && selectionIdx < activeIndices_.size())
    {
        int idx = activeIndices_[selectionIdx];
        for (auto& doc : drawableDocs_)
        {
            doc->updateScreenWithID(items_[idx].first);
        }
    }
    else
    {
        for (auto& doc : drawableDocs_)
        {
            doc->updateScreenWithID(-1);
        }
    }

    if (selections_->getResult() >= 0)
    {
        auto selected = selections_->getResultString();
        result_ = activeIndices_[selections_->getResult()];
        if (result_ >= 0)
        {
            result_ = items_[result_].first;
        }
        setExit(true);
    }
}

void SuperMenuText::onEntrance()
{
    if (inputBox_) inputBox_->onEntrance();
    RunNode::onEntrance();
}

void SuperMenuText::onExit()
{
    if (inputBox_) inputBox_->onExit();
    RunNode::onExit();
}
