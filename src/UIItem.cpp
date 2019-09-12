#include "UIItem.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "MainScene.h"
#include "Save.h"
#include "ShowRoleDifference.h"
#include "TeamMenu.h"
#include "convert.h"

UIItem::UIItem()
{
    item_buttons_.resize(line_count_ * item_each_line_);

    for (int i = 0; i < item_buttons_.size(); i++)
    {
        auto b = std::make_shared<Button>();
        addChild(b);
        item_buttons_[i] = b;
        b->setPosition(i % item_each_line_ * 85 + 40, i / item_each_line_ * 85 + 100);
        //b->setTexture("item", Save::getInstance()->getItemByBagIndex(i)->ID);
    }
    title_ = std::make_shared<MenuText>();
    title_->setStrings({ "劇情", "兵甲", "丹藥", "暗器", "拳經", "劍譜", "刀錄", "奇門", "心法" });
    title_->setFontSize(24);
    title_->arrange(0, 50, 64, 0);
    addChild(title_);

    cursor_ = std::make_shared<TextBox>();
    cursor_->setTexture("title", 127);
    cursor_->setVisible(false);
    addChild(cursor_);

    active_child_ = 0;
    getChild(0)->setState(Pass);
}

UIItem::~UIItem()
{
}

void UIItem::setForceItemType(int f)
{
    force_item_type_ = f;
    if (f >= 0)
    {
        title_->setAllChildVisible(false);
        title_->getChild(f)->setVisible(true);
    }
    else
    {
        title_->setAllChildVisible(true);
    }
}

//原分类：0剧情，1装备，2秘笈，3药品，4暗器
//详细分类："0劇情", "1兵甲", "2丹藥", "3暗器", "4拳經", "5劍譜", "6刀錄", "7奇門", "8心法"
int UIItem::getItemDetailType(Item* item)
{
    if (item == nullptr)
    {
        return -1;
    }
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

void UIItem::geItemsByType(int item_type)
{
    available_items_.clear();
    for (int i = 0; i < ITEM_IN_BAG_COUNT; i++)
    {
        auto item = Save::getInstance()->getItemByBagIndex(i);
        if (getItemDetailType(item) == item_type)
        {
            available_items_.push_back(item);
        }
    }
}

void UIItem::checkCurrentItem()
{
    //强制停留在某类物品
    if (force_item_type_ >= 0)
    {
        //title_.setResult(force_item_type_);
        title_->forceActiveChild(force_item_type_);
    }
    int active = title_->getActiveChildIndex();
    title_->getChild(active)->setState(Pass);
    geItemsByType(active);
    int type_item_count = available_items_.size();
    //从这里计算出左上角可以取的最大值
    //计算方法：先计算出总行数，减去可见行数，乘以每行成员数
    max_leftup_ = ((type_item_count + item_each_line_ - 1) / item_each_line_ - line_count_) * item_each_line_;
    if (max_leftup_ < 0)
    {
        max_leftup_ = 0;
    }

    leftup_index_ = GameUtil::limit(leftup_index_, 0, max_leftup_);

    //计算被激活的按钮
    std::shared_ptr<Button> current_button{ nullptr };
    for (int i = 0; i < item_buttons_.size(); i++)
    {
        auto button = item_buttons_[i];
        int index = i + leftup_index_;
        auto item = getAvailableItem(index);
        if (item)
        {
            button->setTexture("item", item->ID);
        }
        else
        {
            button->setTexture("item", -1);
        }
        if (button->getState() == Pass || button->getState() == Press)
        {
            current_button = button;
            //result_ = current_item_->ID;
        }
    }

    //计算被激活的按钮对应的物品
    current_item_ = nullptr;
    if (current_button)
    {
        int x, y;
        current_button->getPosition(x, y);
        current_item_ = Save::getInstance()->getItem(current_button->getTexutreID());
        //让光标显示出来
        if (current_button->getState() == Pass)
        {
            x += 2;
        }
        if (current_button->getState() == Press)
        {
            y += 2;
        }
        cursor_->setPosition(x, y);
        cursor_->setVisible(true);
    }
    else
    {
        cursor_->setVisible(false);
    }
}

Item* UIItem::getAvailableItem(int i)
{
    if (i >= 0 && i < available_items_.size())
    {
        return available_items_[i];
    }
    return nullptr;
}

void UIItem::dealEvent(BP_Event& e)
{
    checkCurrentItem();
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

    //此处处理键盘响应  未完成
    if (focus_ == 1)
    {
        if (e.type == BP_KEYDOWN)
        {
            switch (e.key.keysym.sym)
            {
            case BPK_LEFT:
                if (active_child_ > 0)
                {
                    active_child_--;
                }
                else
                {
                    if (leftup_index_ > 0)
                    {
                        leftup_index_ -= item_each_line_;
                        active_child_ = item_each_line_ - 1;
                    }
                }
                break;
            case BPK_RIGHT:
                if (active_child_ < item_each_line_ * line_count_ - 1)
                {
                    active_child_++;
                }
                else
                {
                    leftup_index_ += item_each_line_;
                    if (leftup_index_ <= max_leftup_)
                    { active_child_ = item_each_line_ * (line_count_ - 1); }
                }
                break;
            case BPK_UP:
                if (active_child_ < item_each_line_ && leftup_index_ == 0)
                {
                    focus_ = 0;
                }
                else if (active_child_ < item_each_line_)
                {
                    leftup_index_ -= item_each_line_;
                }
                else
                {
                    active_child_ -= item_each_line_;
                }
                break;
            case BPK_DOWN:
                if (active_child_ < item_each_line_ * (line_count_ - 1))
                {
                    active_child_ += item_each_line_;
                }
                else
                {
                    leftup_index_ += item_each_line_;
                }
                break;
            default:
                break;
            }
            forceActiveChild();
        }
        title_->setDealEvent(0);
        title_->setAllChildState(Normal);
    }
    if (focus_ == 0)
    {
        title_->setDealEvent(1);
        if (e.type == BP_KEYUP && e.key.keysym.sym == BPK_DOWN)
        {
            focus_ = 1;
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
    auto str = convert::formatString("%-14s%5d", item->Name, Save::getInstance()->getItemCountInBag(current_item_->ID));
    Font::getInstance()->draw(str, 24, x_ + 10, y_ + 370, { 255, 255, 255, 255 });
    Font::getInstance()->draw(item->Introduction, 20, x_ + 10, y_ + 400, { 255, 255, 255, 255 });

    int x = 10, y = 430;
    int size = 20;

    //以下显示物品的属性
    BP_Color c = { 255, 215, 0, 255 };

    //特别判断罗盘
    if (item->isCompass())
    {
        int man_x, man_y;
        MainScene::getInstance()->getManPosition(man_x, man_y);
        auto str = convert::formatString("當前坐標 %d, %d", man_x, man_y);
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

    showOneProperty(item->AddMedicine, "醫療%+d", size, c, x, y);
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
    y += size + 10;    //换行
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

    showOneProperty(item->NeedMedicine, "醫療%d", size, c, x, y);
    showOneProperty(item->NeedUsePoison, "用毒%d", size, c, x, y);
    showOneProperty(item->NeedDetoxification, "解毒%d", size, c, x, y);

    showOneProperty(item->NeedFist, "拳掌%d", size, c, x, y);
    showOneProperty(item->NeedSword, "御劍%d", size, c, x, y);
    showOneProperty(item->NeedKnife, "耍刀%d", size, c, x, y);
    showOneProperty(item->NeedUnusual, "特殊兵器%d", size, c, x, y);
    showOneProperty(item->NeedHiddenWeapon, "暗器%d", size, c, x, y);

    showOneProperty(item->NeedIQ, "資質%d", size, c, x, y);

    showOneProperty(item->NeedExp, "基礎經驗%d", size, c, x, y);

    if (item->NeedMaterial >= 0)
    {
        std::string str = "耗費";
        str += Save::getInstance()->getItem(item->NeedMaterial)->Name;
        showOneProperty(1, str, size, c, x, y);
    }

    x = 10;
    y += size + 10;
    for (int i = 0; i < 5; i++)
    {
        int make = item->MakeItem[i];
        if (make >= 0)
        {
            std::string str = Save::getInstance()->getItem(make)->Name;
            //str += " %d";
            showOneProperty(item->MakeItemCount[i], str, size, c, x, y);
        }
    }
}

void UIItem::showOneProperty(int v, std::string format_str, int size, BP_Color c, int& x, int& y)
{
    if (v != 0)
    {
        std::string str;
        int parameter_count = convert::extractFormatString(format_str).size();
        if (parameter_count == 1)
        {
            str = convert::formatString(format_str.c_str(), v);
        }
        else if (parameter_count==0)
        {
            str = format_str;
        }
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

void UIItem::onPressedOK()
{
    current_item_ = nullptr;
    for (int i = 0; i < item_buttons_.size(); i++)
    {
        auto button = item_buttons_[i];
        if (button->getState() == Press)
        {
            auto item = getAvailableItem(i + leftup_index_);
            current_item_ = item;
        }
    }

    if (current_item_ == nullptr)
    {
        return;
    }

    //在使用剧情物品的时候，返回一个结果，主UI判断此时可以退出
    if (current_item_->ItemType == 0)
    {
        result_ = current_item_->ID;
    }

    if (select_user_)
    {
        if (current_item_->ItemType == 3)
        {
            auto team_menu=std::make_shared<TeamMenu>();
            team_menu->setItem(current_item_);
            team_menu->setText(convert::formatString("誰要使用%s", current_item_->Name));
            team_menu->run();
            auto role = team_menu->getRole();
            if (role)
            {
                Role r = *role;
                GameUtil::useItem(role, current_item_);
                auto df=std::make_shared<ShowRoleDifference>(&r, role);
                df->setText(convert::formatString("%s服用%s", role->Name, current_item_->Name));
                df->run();
                Event::getInstance()->addItemWithoutHint(current_item_->ID, -1);
            }
        }
        else if (current_item_->ItemType == 1 || current_item_->ItemType == 2)
        {
            auto team_menu = std::make_shared<TeamMenu>();
            team_menu->setItem(current_item_);
            auto format_str = "誰要修煉%s";
            if (current_item_->ItemType == 1)
            {
                format_str = "誰要裝備%s";
            }
            team_menu->setText(convert::formatString(format_str, current_item_->Name));
            team_menu->run();
            auto role = team_menu->getRole();
            if (role)
            {
                GameUtil::equip(role, current_item_);
            }
        }
        else if (current_item_->ItemType == 4)
        {
            //似乎不需要特殊处理
        }
    }
    setExit(true);    //用于战斗时。平时物品栏不是以根节点运行，设置这个没有作用
}

void UIItem::onPressedCancel()
{
    current_item_ = nullptr;
    exitWithResult(-1);
}
