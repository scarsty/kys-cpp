#pragma once
#include "Element.h"
#include "Button.h"
#include "Menu.h"

class UIItem : public Element
{
public:
    UIItem();
    ~UIItem();

    //这里注意，用来显示物品图片的按钮的纹理编号实际就是物品编号
    std::vector<Button*> item_buttons_;
    std::vector<int> items_;  //当前种类的物品列表
    int leftup_index_=0;  //左上角第一个物品在当前种类列表中的索引

    MenuText* title_ = nullptr;

};

