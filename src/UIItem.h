#pragma once
#include "Button.h"
#include "Menu.h"
#include "Types.h"
#include <set>

class UIItem : public Menu
{
public:
    UIItem();
    ~UIItem();

    //这里注意，用来显示物品图片的按钮的纹理编号实际就是物品编号
    std::vector<std::shared_ptr<Button>> item_buttons_;
    std::shared_ptr<TextBox> cursor_;

    int leftup_index_ = 0;    //左上角第一个物品在当前种类列表中的索引
    int max_leftup_ = 0;

    const int item_each_line_ = 7;
    const int line_count_ = 3;

    std::shared_ptr<MenuText> title_;

    std::set<int> force_item_type_{};

    bool select_user_ = true;

    int focus_ = 0;    //焦点位置：0分类栏，1物品栏

    std::shared_ptr<MenuText> getTitle() { return title_; }

    void setForceItemType(std::set<int> f);

    void setSelectUser(bool s) { select_user_ = s; }

    int getItemDetailType(Item* item);
    Item* getAvailableItem(int i);

    void geItemsByType(int item_type);

    void checkCurrentItem();
    virtual void draw() override { showItemProperty(current_item_); }
    virtual void dealEvent(EngineEvent& e) override;

    void showItemProperty(Item* item);
    std::vector<std::string> properties_;
    void addOneProperty(const std::string& format_str, int v);
    void addOneProperty(const std::string& format_str);
    int showAddedProperty(int size, Color c, int x, int y);

    Item* current_item_ = nullptr;
    std::shared_ptr<Button> current_button_{ nullptr };
    std::shared_ptr<Button> drag_item_{ nullptr };
    std::vector<Item*> available_items_;

    Item* getCurrentItem() { return current_item_; }

    virtual void onPressedOK() override;
    virtual void onPressedCancel() override;

    Role* role_ = nullptr;
    void setRole(Role* r) { role_ = r; }    //只处理适合的物品
    Role* try_role_ = nullptr;
    void setTryRole(Role* r) { try_role_ = r; }    //只处理适合的物品
};
