#include "Menu.h"
#include "InputBox.h"
#include "Button.h"
#include "DrawableOnCall.h"

#include <vector>
#include <string>
#include <utility>
#include <functional>
#include <unordered_map>
#include <unordered_set>

class SuperMenuText : public InputBox
{
public:
    SuperMenuText(const std::string& title, int font_size, const std::vector<std::pair<int, std::string>>& allItems, int itemsPerPage);
    virtual ~SuperMenuText() = default;
    void dealEvent(BP_Event& e) override;
    virtual void setInputPosition(int x, int y) override;
    void addDrawableOnCall(DrawableOnCall* doc);
    void setMatchFunction(std::function<bool(const std::string&, const std::string&)> match);

private:
    void defaultPage();
    void flipPage(int pInc);
    void search(const std::string& text);
    void updateMaxPages();
    bool defaultMatch(const std::string& input, const std::string& name);

    Button previous_;
    Button next_;
    int curPage_ = 0;
    int maxPages_ = 1;
    int itemsPerPage_ = 10;
    bool curDefault_ = false;
    MenuText selections_;

    // 所有的
    std::vector<std::pair<int, std::string>> items_;

    // 这是当前给显示的，返回result -> items_[activeIndices[result]] 既是实际选项
    // 只显示一页的activeIndices
    std::vector<int> activeIndices_;
    // 所有搜索结果
    std::vector<int> searchResultIndices_;
    std::vector<DrawableOnCall*> docs_;
	std::function<bool(const std::string&, const std::string&)> matchFunc_;
	
    // 预处理
    std::unordered_map<std::string, std::unordered_set<std::string>> matches_;
};