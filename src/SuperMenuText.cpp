#include "SuperMenuText.h"
#include "../others/Hanz2Piny.h"
#include "Font.h"
#include "PotConv.h"
#include <algorithm>
#include <cmath>
#include <utility>

// Backward-compatible constructor: always creates an InputBox
SuperMenuText::SuperMenuText(const std::string& title, int fontSize,
    const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage) : SuperMenuText(title, fontSize, allItems, itemsPerPage, SuperMenuTextExtraOptions{})
{
}

// New constructor: can control InputBox creation
SuperMenuText::SuperMenuText(const std::string& title, int fontSize,
    const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage,
    SuperMenuTextExtraOptions extraOpts) : items_(allItems),
                                           itemsPerPage_(itemsPerPage),
                                           fontSize_(fontSize),
                                           extraOpts_{ extraOpts }
{
    if (extraOpts_.needInputBox_)
    {
        inputBox_ = std::make_shared<InputBox>(title, fontSize_);
        addChild(inputBox_);
    }
    else
    {
        titleBox_ = std::make_shared<TextBox>();
        titleBox_->setText(title);
        titleBox_->setFontSize(fontSize_);
        addChild(titleBox_);
    }

    if (extraOpts_.itemColors_.size() < items_.size())
    {
        extraOpts_.itemColors_.resize(items_.size(), { 128, 128, 128, 255 });
    }

    previousButton_ = std::make_shared<Button>();
    previousButton_->setText("上一頁");
    previousButton_->setFontSize(fontSize_);
    addChild(previousButton_);

    nextButton_ = std::make_shared<Button>();
    nextButton_->setText("下一頁");
    nextButton_->setFontSize(fontSize_);
    addChild(nextButton_);

    selections_ = std::make_shared<MenuText>();
    selections_->setFontSize(fontSize_);
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
        std::size_t p = 0;
        for (std::size_t i = 0; i < text.size(); i++)
        {
            std::size_t p1 = pinyin.find_first_of(text[i], p);
            if (p1 == std::string::npos)
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
    inputX_ = x;
    inputY_ = y;
    inputPosSet_ = true;

    int navHeight = showNavButtons_ ? (fontSize_ + 12) : 0;
    int navBtnGap = Font::getTextDrawSize(previousButton_->getText()) * fontSize_ / 2 + 24;

    if (inputBox_)
    {
        if (showNavButtons_)
        {
            previousButton_->setPosition(x, y);
            nextButton_->setPosition(x + navBtnGap, y);
        }
        inputBox_->setInputPosition(x, y + navHeight);
        selections_->setPosition(x, y + navHeight + fontSize_ * 3);
    }
    else
    {
        if (showNavButtons_)
        {
            previousButton_->setPosition(x, y);
            nextButton_->setPosition(x + navBtnGap, y);
        }
        titleBox_->setPosition(x, y + navHeight);
        selections_->setPosition(x, y + navHeight + fontSize_ + 16);
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
    if (isDefaultPage_)
    {
        return;
    }
    std::vector<std::string> displays;
    std::vector<Color> colors;
    std::vector<Color> outlines;
    std::vector<bool> animates;
    std::vector<int> thicknesses;
    searchResultIndices_.clear();
    activeIndices_.clear();
    for (int i = 0; i < items_.size(); i++)
    {
        if (i < std::min((std::size_t)itemsPerPage_, items_.size()))
        {
            activeIndices_.push_back(i);
            displays.push_back(items_[i].second);
            colors.push_back(extraOpts_.itemColors_[i]);
            if (i < extraOpts_.outlineColors_.size())
                outlines.push_back(extraOpts_.outlineColors_[i]);
            if (i < extraOpts_.animateOutlines_.size())
                animates.push_back(extraOpts_.animateOutlines_[i]);
            if (i < extraOpts_.outlineThicknesses_.size())
                thicknesses.push_back(extraOpts_.outlineThicknesses_[i]);
        }
        searchResultIndices_.push_back(i);
    }
    currentPage_ = 0;
    updateMaxPages();
    selections_->setStrings(displays, colors, outlines, animates, thicknesses);
    if (!displays.empty())
    {
        selections_->forceActiveChild(0);
    }
    isDefaultPage_ = true;
    updateNavigationButtons();
}

void SuperMenuText::flipPage(int pageIncrement)
{
    if (currentPage_ + pageIncrement >= 0 && currentPage_ + pageIncrement < maxPages_)
    {
        lastTappedIdx_ = -1;       // Reset locked item when changing pages
        tapLockTime_ = -1.0;       // Reset lock timestamp on page change
        currentPage_ += pageIncrement;
        int startIdx = currentPage_ * itemsPerPage_;
        std::vector<std::string> displays;
        std::vector<Color> colors;
        std::vector<Color> outlines;
        std::vector<bool> animates;
        std::vector<int> thicknesses;
        activeIndices_.clear();
        for (std::size_t i = startIdx; i < std::min(searchResultIndices_.size(), (std::size_t)startIdx + itemsPerPage_); i++)
        {
            int idx = searchResultIndices_[i];
            activeIndices_.push_back(idx);
            displays.push_back(items_[idx].second);
            colors.push_back(extraOpts_.itemColors_[idx]);
            if (idx < extraOpts_.outlineColors_.size())
                outlines.push_back(extraOpts_.outlineColors_[idx]);
            else
                outlines.push_back({0, 0, 0, 0});
            if (idx < extraOpts_.animateOutlines_.size())
                animates.push_back(extraOpts_.animateOutlines_[idx]);
            else
                animates.push_back(false);
            if (idx < extraOpts_.outlineThicknesses_.size())
                thicknesses.push_back(extraOpts_.outlineThicknesses_[idx]);
            else
                thicknesses.push_back(1);
        }
        selections_->setStrings(displays, colors, outlines, animates, thicknesses);
        updateNavigationButtons();
    }
}

void SuperMenuText::search(const std::string& text)
{
    std::vector<std::string> results;
    std::vector<Color> colors;
    std::vector<Color> outlines;
    std::vector<bool> animates;
    std::vector<int> thicknesses;
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
                colors.push_back(extraOpts_.itemColors_[i]);
                if (i < extraOpts_.outlineColors_.size())
                    outlines.push_back(extraOpts_.outlineColors_[i]);
                else
                    outlines.push_back({0, 0, 0, 0});
                if (i < extraOpts_.animateOutlines_.size())
                    animates.push_back(extraOpts_.animateOutlines_[i]);
                else
                    animates.push_back(false);
                if (i < extraOpts_.outlineThicknesses_.size())
                    thicknesses.push_back(extraOpts_.outlineThicknesses_[i]);
                else
                    thicknesses.push_back(1);
                activeIndices_.push_back(i);
            }
            searchResultIndices_.push_back(i);
        }
    }
    updateMaxPages();
    currentPage_ = 0;
    selections_->setStrings(results, colors, outlines, animates, thicknesses);
    selections_->forceActiveChild(0);
    isDefaultPage_ = false;
    updateNavigationButtons();
}

void SuperMenuText::updateMaxPages()
{
    maxPages_ = std::max(1, static_cast<int>(std::ceil(searchResultIndices_.size() / static_cast<double>(itemsPerPage_))));
}

void SuperMenuText::updateNavigationButtons()
{
    bool canGoPrev = currentPage_ > 0;
    bool canGoNext = currentPage_ < maxPages_ - 1;

    if (canGoPrev)
    {
        previousButton_->setCustomOutline({100, 200, 255, 220});
        previousButton_->setAnimateOutline(true);
        previousButton_->setOutlineThickness(1);
    }
    else
    {
        previousButton_->clearCustomOutline();
        previousButton_->setAnimateOutline(false);
    }

    if (canGoNext)
    {
        nextButton_->setCustomOutline({100, 200, 255, 220});
        nextButton_->setAnimateOutline(true);
        nextButton_->setOutlineThickness(1);
    }
    else
    {
        nextButton_->clearCustomOutline();
        nextButton_->setAnimateOutline(false);
    }
}

void SuperMenuText::dealEvent(EngineEvent& e)
{
    if (previousButton_->getState() == NodePress && e.type == EVENT_MOUSE_BUTTON_UP)
    {
        if (canFlipPage()) flipPage(-1);
        previousButton_->setState(NodeNormal);
        previousButton_->setResult(-1);
    }
    else if (nextButton_->getState() == NodePress && e.type == EVENT_MOUSE_BUTTON_UP)
    {
        if (canFlipPage()) flipPage(1);
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
    if (inputBox_)
    {
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

    if (extraOpts_.exitable_)
    {
        switch (e.type)
        {
        case EVENT_MOUSE_BUTTON_UP:
            if (e.button.button == BUTTON_RIGHT)
            {
                result_ = -1;
                setExit(true);
            }

            break;
        case EVENT_KEY_UP:
        {
            if (e.key.key == K_ESCAPE)
            {
                result_ = -1;
                setExit(true);
            }
        }
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
            DrawableItemContext context;
            context.itemId = items_[idx].first;
            context.itemIndex = idx;
            doc->updateScreenWithContext(context);
        }
    }
    else
    {
        for (auto& doc : drawableDocs_)
        {
            doc->updateScreenWithContext({});
        }
    }

    if (selections_->getResult() >= 0)
    {
        int selectionIdx = selections_->getResult();
        int itemIdx = activeIndices_[selectionIdx];

        if (doubleTapMode_)
        {
            // Double-tap mode: first tap locks selection, second tap commits.
            // Guard: require at least kDoubleTapMinIntervalMs between lock and
            // commit so that a burst of events from a single browser gesture
            // (WASM batches touch + synthetic mouse events in one rAF frame)
            // cannot fire both lock and commit at once.
            bool allowCommit = (lastTappedIdx_ == itemIdx)
                && (tapLockTime_ >= 0.0)
                && (Engine::getTicks() - tapLockTime_ >= kDoubleTapMinIntervalMs);

            if (allowCommit)
            {
                // Second tap on same item - commit
                tapLockTime_ = -1.0;
                // Clear the green outline
                auto btn = std::dynamic_pointer_cast<Button>(selections_->getChild(selectionIdx));
                if (btn) btn->clearCustomOutline();

                if (extraOpts_.confirmation_)
                {
                    auto confirm = std::make_shared<MenuText>();
                    confirm->setStrings({ "確認", "取消" });
                    confirm->setFontSize(fontSize_);
                    confirm->setHaveBox(true);
                    confirm->arrange(150, 350, 150, 0);
                    confirm->run();
                    confirm->setExit(true);
                    if (confirm->getResult() != 0)
                    {
                        selections_->forceActiveChild(selectionIdx);
                        selections_->setResult(-1);
                        return;
                    }
                }
                result_ = itemIdx;
                if (result_ >= 0 && !extraOpts_.returnIdxOnly)
                {
                    result_ = items_[result_].first;
                }
                setExit(true);
            }
            else
            {
                // First tap - lock selection, show details
                // Clear previous locked item outline
                if (lastTappedIdx_ >= 0)
                {
                    for (int i = 0; i < activeIndices_.size(); i++)
                    {
                        if (activeIndices_[i] == lastTappedIdx_)
                        {
                            auto btn = std::dynamic_pointer_cast<Button>(selections_->getChild(i));
                            if (btn) btn->clearCustomOutline();
                            break;
                        }
                    }
                }
                lastTappedIdx_ = itemIdx;
                tapLockTime_ = Engine::getTicks();  // record when lock happened
                // Apply green outline to locked item
                auto btn = std::dynamic_pointer_cast<Button>(selections_->getChild(selectionIdx));
                if (btn)
                {
                    btn->setCustomOutline({100, 255, 100, 255});
                    btn->setOutlineThickness(2);
                }
                selections_->forceActiveChild(selectionIdx);
                selections_->setResult(-1);
            }
        }
        else
        {
            // Normal mode: single tap commits
            if (extraOpts_.confirmation_)
            {
                auto confirm = std::make_shared<MenuText>();
                confirm->setStrings({ "確認", "取消" });
                confirm->setFontSize(fontSize_);
                confirm->setHaveBox(true);
                confirm->arrange(150, 350, 150, 0);
                confirm->run();
                confirm->setExit(true);
                if (confirm->getResult() != 0)
                {
                    selections_->forceActiveChild(selectionIdx);
                    selections_->setResult(-1);
                    return;
                }
            }
            result_ = itemIdx;
            if (result_ >= 0 && !extraOpts_.returnIdxOnly)
            {
                result_ = items_[result_].first;
            }
            setExit(true);
        }
    }
}

void SuperMenuText::onEntrance()
{
    if (inputBox_)
    {
        inputBox_->onEntrance();
    }
    RunNode::onEntrance();
}

void SuperMenuText::onExit()
{
    if (inputBox_)
    {
        inputBox_->onExit();
    }
    RunNode::onExit();
}

bool SuperMenuText::canFlipPage()
{
    if (!doubleTapMode_) return true;
    bool allow = lastPageFlipTime_ < 0.0 || (Engine::getTicks() - lastPageFlipTime_ >= kDoubleTapMinIntervalMs);
    if (allow) lastPageFlipTime_ = Engine::getTicks();
    return allow;
}
