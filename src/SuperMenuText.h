#pragma once
#include "Button.h"
#include "DrawableOnCall.h"
#include "InputBox.h"
#include "Menu.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class SuperMenuText : public InputBox
{
public:
    // Backward-compatible constructor
    SuperMenuText(const std::string& title, int fontSize, const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage);

    // New constructor with color support
    SuperMenuText(const std::string& title, int fontSize, const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage, const std::vector<Color>& itemColors);

    virtual ~SuperMenuText() = default;
    void dealEvent(EngineEvent& e) override;
    virtual void setInputPosition(int x, int y) override;
    void addDrawableOnCall(std::shared_ptr<DrawableOnCall> doc);
    void setMatchFunction(std::function<bool(const std::string&, const std::string&)> match);

private:
    void defaultPage();
    void flipPage(int pageIncrement);
    void search(const std::string& text);
    void updateMaxPages();
    bool defaultMatch(const std::string& input, const std::string& name);

    std::shared_ptr<Button> previousButton_, nextButton_;
    int currentPage_ = 0;
    int maxPages_ = 1;
    int itemsPerPage_ = 10;
    bool isDefaultPage_ = false;
    std::shared_ptr<MenuText> selections_;

    std::vector<std::pair<int, std::string>> items_;
    std::vector<Color> itemColors_;

    std::vector<int> activeIndices_;
    std::vector<int> searchResultIndices_;
    std::vector<std::shared_ptr<DrawableOnCall>> drawableDocs_;
    std::function<bool(const std::string&, const std::string&)> matchFunction_;

    std::unordered_map<std::string, std::unordered_set<std::string>> matches_;
};