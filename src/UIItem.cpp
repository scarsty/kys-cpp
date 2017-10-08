#include "UIItem.h"
#include "Save.h"
#include "others\libconvert.h"
#include "Font.h"
#include "MainScene.h"
#include "GameUtil.h"

UIItem::UIItem()
{
    item_buttons_.resize(line_count_ * item_each_line_);
    items_.resize(ITEM_IN_BAG_COUNT);
    for (int i = 0; i < item_buttons_.size(); i++)
    {
        auto b = new Button();
        item_buttons_[i] = b;
        b->setPosition(i % item_each_line_ * 85 + 40, i / item_each_line_ * 85 + 100);
        //b->setTexture("item", Save::getInstance()->getItemByBagIndex(i)->ID);
        addChild(b);
    }
    title_ = new MenuText();
    title_->setStrings({ "劇情", "兵甲", "丹藥", "暗器", "拳經", "劍譜", "刀錄", "奇門", "心法" });
    title_->setFontSize(25);
    title_->arrange(0, 50, 65, 0);

    addChild(title_);

    cursor_ = new TextBox();
    cursor_->setTexture("title", 100);
    addChild(cursor_);
}

UIItem::~UIItem()
{
}

//原分类：0剧情，1装备，2秘笈，3药品，4暗器
//详细分类："0劇情", "1兵甲", "2丹藥", "3暗器", "4拳經", "5劍譜", "6刀錄", "7奇門", "8心法"
int UIItem::getItemDetailType(Item* item)
{
    if (item == nullptr) { return -1; }
    if (item->ItemType == 0)
    {
        return 0;
    }
    else if (item->ItemType == 1)
    {
        return 1;
    }
    else if (item->ItemType == 3)
    {
        return 2;
    }
    else if (item->ItemType == 4)
    {
        return 3;
    }
    else if (item->ItemType == 2)
    {
        auto m = Save::getInstance()->getMagic(item->MagicID);
        if (m)
        {
            //吸取内力类归为8
            if (m->HurtType == 0)
            {
                return m->MagicType + 3;
            }
        }
        return 8;
    }
    //未知的种类当成剧情
    return 0;
}

//注意填充到列表的是在背包中的位置而不是物品id
//返回值为个数
int UIItem::geItemBagIndexByType(int item_type)
{
    std::fill(items_.begin(), items_.end(), -1);
    int k = 0;
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        if (getItemDetailType(Save::getInstance()->getItemByBagIndex(i)) == item_type)
        {
            items_[k] = i;
            k++;
        }
    }
    return k;
}

void UIItem::draw()
{
    showItemProperty(current_item_);
}

void UIItem::dealEvent(BP_Event& e)
{
    int type_item_count = geItemBagIndexByType(title_->getResult());
    //从这里计算出左上角可以取的最大值
    //计算方法：先计算出总行数，减去可见行数，乘以每行成员数
    int max_leftup = ((type_item_count + item_each_line_ - 1) / item_each_line_ - line_count_) * item_each_line_;
    if (max_leftup < 0) { max_leftup = 0; }

    if (e.type == BP_MOUSEWHEEL)
    {
        if (e.wheel.y > 0)
        {
            leftup_index_ -= item_each_line_;
        }
        else if (e.wheel.y < 0)
        {
            leftup_index_ += item_each_line_;
        }
    }
    leftup_index_ = GameUtil::limit(leftup_index_, 0, max_leftup);

    //计算当前指向的物品
    //result_ = -1;
    current_item_ = nullptr;
    current_button_ = nullptr;
    for (int i = 0; i < item_buttons_.size(); i++)
    {
        auto button = item_buttons_[i];
        auto item = Save::getInstance()->getItemByBagIndex(items_[i + leftup_index_]);
        if (item)
        {
            button->setTexture("item", item->ID);
            if (button->getState() == Pass || button->getState() == Press)
            {
                current_item_ = item;
                current_button_ = button;
                int x, y;
                current_button_->getPosition(x, y);
                cursor_->setPosition(x, y);
                //result_ = current_item_->ID;
            }
        }
        else
        {
            button->setTexture("item", -1);
        }
    }
}

void UIItem::showItemProperty(Item* item)
{
    if (item == nullptr)
    {
        return;
    }
    //物品名和数量
    auto str = convert::formatString("%s", item->Name);
    Font::getInstance()->draw(str, 25, x_ + 10, y_ + 370, { 255, 255, 255, 255 });
    str = convert::formatString("%5d", Save::getInstance()->getItemCountInBag(current_item_->ID));
    Font::getInstance()->draw(str, 25, x_ + 10 + 25 * 7, y_ + 370, { 255, 255, 255, 255 });

    Font::getInstance()->draw(item->Introduction, 20, x_ + 10, y_ + 400, { 255, 255, 255, 255 });

    int x = 10, y = 430;
    int size = 20;

    //以下显示物品的属性
    BP_Color c = { 255, 215, 0, 255 };

    //特别判断罗盘
    if (item->isCompass())
    {
        int x, y;
        MainScene::getIntance()->getManPosition(x, y);
        auto str = convert::formatString("當前坐標（%d, %d）", x, y);
        showOneProperty(1, str, size, c, x, y);
    }

    //剧情物品不继续显示了
    if (item->ItemType == 0)
    {
        return;
    }

    //Font::getInstance()->draw("效果：", size, x_ + x, y_ + y, c);
    //y += size + 10;

    showOneProperty(item->AddHP, "生命%+d", size, c, x, y);
    showOneProperty(item->AddMaxHP, "生命上限%+d", size, c, x, y);
    showOneProperty(item->AddMP, "內力%+d", size, c, x, y);
    showOneProperty(item->AddMaxMP, "內力上限%+d", size, c, x, y);
    showOneProperty(item->AddPhysicalPower, "體力%+d", size, c, x, y);
    showOneProperty(item->AddPoison, "中毒%+d", size, c, x, y);

    showOneProperty(item->AddAttack, "攻擊%+d", size, c, x, y);
    showOneProperty(item->AddSpeed, "輕功%+d", size, c, x, y);
    showOneProperty(item->AddDefence, "防禦%+d", size, c, x, y);

    showOneProperty(item->AddMedcine, "醫療%+d", size, c, x, y);
    showOneProperty(item->AddUsePoison, "用毒%+d", size, c, x, y);
    showOneProperty(item->AddDetoxification, "解毒%+d", size, c, x, y);
    showOneProperty(item->AddAntiPoison, "抗毒%+d", size, c, x, y);

    showOneProperty(item->AddFist, "拳掌%+d", size, c, x, y);
    showOneProperty(item->AddSword, "御劍%+d", size, c, x, y);
    showOneProperty(item->AddKnife, "耍刀%+d", size, c, x, y);
    showOneProperty(item->AddUnusual, "特殊兵器%+d", size, c, x, y);
    showOneProperty(item->AddHiddenWeapon, "暗器%+d", size, c, x, y);

    showOneProperty(item->AddKnowledge, "作弊%+d", size, c, x, y);
    showOneProperty(item->AddMorality, "道德%+d", size, c, x, y);
    showOneProperty(item->AddAttackWithPoison, "攻擊帶毒%+d", size, c, x, y);

    showOneProperty(item->ChangeMPType == 2, "內力調和", size, c, x, y);
    showOneProperty(item->AddAttackTwice == 1, "雙擊", size, c, x, y);

    auto magic = Save::getInstance()->getMagic(item->MagicID);
    if (magic)
    {
        auto str = convert::formatString("習得武學%s", magic->Name);
        showOneProperty(1, str, size, c, x, y);
    }

    //以下显示物品需求

    //药品和暗器类不继续显示了
    if (item->ItemType == 3 || item->ItemType == 4)
    {
        return;
    }

    x = 10;
    y += size + 10;  //换行
    c = { 224, 170, 255, 255 };
    //Font::getInstance()->draw("需求：", size, x_ + x, y_ + y, c);
    //y += size + 10;
    auto role = Save::getInstance()->getRole(item->OnlySuitableRole);
    if (role)
    {
        auto str = convert::formatString("僅適合%s", role->Name);
        showOneProperty(1, str, size, c, x, y);
        return;
    }

    showOneProperty(item->NeedMP, "內力%d", size, c, x, y);
    showOneProperty(item->NeedAttack, "攻擊%d", size, c, x, y);
    showOneProperty(item->NeedSpeed, "輕功%d", size, c, x, y);

    showOneProperty(item->NeedMedcine, "醫療%d", size, c, x, y);
    showOneProperty(item->NeedUsePoison, "用毒%d", size, c, x, y);
    showOneProperty(item->NeedDetoxification, "解毒%d", size, c, x, y);

    showOneProperty(item->NeedFist, "拳掌%d", size, c, x, y);
    showOneProperty(item->NeedSword, "御劍%d", size, c, x, y);
    showOneProperty(item->NeedKnife, "耍刀%d", size, c, x, y);
    showOneProperty(item->NeedUnusual, "特殊兵器%d", size, c, x, y);
    showOneProperty(item->NeedHiddenWeapon, "暗器%d", size, c, x, y);

    showOneProperty(item->NeedIQ, "資質%d", size, c, x, y);

    showOneProperty(item->NeedExp, "基礎經驗%d", size, c, x, y);
}

void UIItem::showOneProperty(int v, std::string format_str, int size, BP_Color c, int& x, int& y)
{
    if (v != 0)
    {
        auto str = convert::formatString(format_str.c_str(), v);
        //测试是不是出界了
        int draw_length = size * str.size() / 2 + size;
        int x1 = x + draw_length;
        if (x1 > 700)
        {
            x = 10;
            y += size + 5;
        }
        Font::getInstance()->draw(str, size, x_ + x, y_ + y, c);
        x += draw_length;
    }
}

void UIItem::pressedOK()
{
    //仅在使用剧情物品的时候，发出退出的提示
    if (current_item_ && current_item_->ItemType == 0)
    {
        result_ = current_item_->ID;
    }
}
