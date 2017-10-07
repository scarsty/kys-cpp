#pragma once
#include "Element.h"
#include "Button.h"
#include "Menu.h"
#include "Types.h"

class UIItem : public Element
{
public:
    UIItem();
    ~UIItem();

    //这里注意，用来显示物品图片的按钮的纹理编号实际就是物品编号
    std::vector<Button*> item_buttons_;
    std::vector<int> items_;  //当前种类的物品列表
    int leftup_index_ = 0;  //左上角第一个物品在当前种类列表中的索引

    const int item_each_line_ = 7;
    const int line_count_ = 3;

    MenuText* title_ = nullptr;

    int getItemDetailType(Item* item);

    int geItemBagIndexByType(int item_type);

    void draw() override;
    void dealEvent(BP_Event& e) override;

    void showItemProperty(Item* item);
    void showOneProperty(int v, std::string format_str, int size, BP_Color c, int& x, int& y);

    Item* current_item_ = nullptr;

};

