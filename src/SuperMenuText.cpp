#include "SuperMenuText.h"
#include "PotConv.h"
#include <cmath>
#include <algorithm>

SuperMenuText::SuperMenuText(const std::string& title, int font_size, const std::vector<std::string>& allItems, int itemsPerPage) :
    InputBox(title, font_size), items_(allItems), itemsPerPage_(itemsPerPage)
{
    previous_ = new Button();
    previous_->setText("上一頁PgUp");
    next_ = new Button();
    next_->setText("下一頁PgDown");

    addChild(previous_);
    addChild(next_);
    selections_ = new MenuText();
    addChild(selections_);
    setAllChildState(Normal);
    defaultPage();
}

void SuperMenuText::setInputPosition(int x, int y)
{
    InputBox::setInputPosition(x, y);
    selections_->setPosition(x, y + font_size_ * 3);
    previous_->setPosition(x, y - font_size_ * 1.5);
    next_->setPosition(x + font_size_ * 5, y - font_size_ * 1.5);
}

void SuperMenuText::defaultPage() {
    if (curDefault_) return;
    std::vector<std::string> displays;
    searchResultIndices_.clear();
    for (int i = 0; i < items_.size(); i++) {
        if (i < std::min((std::size_t)itemsPerPage_, items_.size())) {
            activeIndices_.push_back(i);
            displays.push_back(items_[i]);
        }
        searchResultIndices_.push_back(i);
    }
    curPage_ = 0;
    updateMaxPages();
    selections_->setStrings(displays);
    if (displays.size() != 0)
        selections_->forceActiveChild(0);
    curDefault_ = true;
}

void SuperMenuText::flipPage(int pInc)
{
    if (curPage_ + pInc >= 0 && curPage_ + pInc < maxPages_) {
        // 允许你翻页！
        curPage_ += pInc;
        int startIdx = curPage_ * itemsPerPage_;
        std::vector<std::string> displays;
        activeIndices_.clear();
        for (std::size_t i = startIdx; i < std::min(searchResultIndices_.size(), (std::size_t)startIdx + itemsPerPage_); i++) {
            activeIndices_.push_back(searchResultIndices_[i]);
            displays.push_back(items_[searchResultIndices_[i]]);
        }
        selections_->setStrings(displays);
    }
    // curDefault_ = false;
}

void SuperMenuText::search(const std::string& text) {
    std::vector<std::string> results;
    activeIndices_.clear();
    searchResultIndices_.clear();
    // 只返回itemsPerPage_数量
    for (int i = 0; i < items_.size(); i++) {
        auto& opt = items_[i];
        if (opt.size() < text.size()) {
            continue;
        }
        auto size = std::min(opt.size(), text.size());
        if (memcmp(text.c_str(), opt.c_str(), size) == 0) {
            if (results.size() < itemsPerPage_) {
                results.emplace_back(opt);
                activeIndices_.push_back(i);
            }
            searchResultIndices_.push_back(i);
        }
    }
    updateMaxPages();
    curPage_ = 0;
    selections_->setStrings(results);
    curDefault_ = false;
}

void SuperMenuText::updateMaxPages()
{
    maxPages_ = std::ceil((searchResultIndices_.size() / (double)itemsPerPage_));
}

void SuperMenuText::dealEvent(BP_Event & e)
{   
    // get不到result 为何
    // 不知道这玩意儿在干嘛，瞎搞即可
    if (previous_->getState() == Press && e.type == BP_MOUSEBUTTONUP) {
        flipPage(-1);
        previous_->setState(Normal);
        previous_->setResult(-1);
    }
    else if (next_->getState() == Press && e.type == BP_MOUSEBUTTONUP) {
        flipPage(1);
        next_->setState(Normal);
        next_->setResult(-1);
    }

    bool research = false;
    // 为什么switch自动缩进是这样
    switch (e.type)
    {
    case BP_TEXTINPUT:
    {
        auto converted = ccConv_.Convert(e.text.text);
        converted = PotConv::conv(converted, "utf-8", "cp936");
        text_ += converted;
        research = true;
        break;
    }
    case BP_TEXTEDITING:
    {
        break;
    }
    case BP_KEYDOWN: {
        if (e.key.keysym.sym == BPK_BACKSPACE)
        {
            if (text_.size() >= 1)
            {
                text_.pop_back();
                if (text_.size() >= 1 && uint8_t(text_.back()) >= 128)
                {
                    text_.pop_back();
                }
            }
            research = true;
        }
        break;
    }
    case BP_KEYUP: {
        switch (e.key.keysym.sym) {
        case BPK_PAGEUP:
        {
            flipPage(-1);
            break;
        }
        case BPK_PAGEDOWN:
        {
            flipPage(1);
            break;
        }
        }
        break;
    }
    }

    if (text_.empty()) {
        defaultPage();
    }

    if (research) {
        search(text_);
    }

    if (selections_->getResult() >= 0) {
        selections_->setExit(false);
        auto selected = selections_->getResultString();
        // 如果文字一样的话，直接当确定
        // 这里有个致命问题，我还没想好怎么解决，晚点再说
        // 我似乎忘了是什么问题
        if (text_ == selected) {
            result_ = activeIndices_[selections_->getResult()];
            setExit(true);
        }
        else {
            text_ = selected;
            search(selected);
        }
        selections_->setResult(-1);
    }
}
