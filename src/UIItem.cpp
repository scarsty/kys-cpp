#include "UIItem.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "MainScene.h"
#include "Save.h"
#include "ShowRoleDifference.h"
#include "TeamMenu.h"

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
    title_->setDealEvent(0);
    title_->setLRStyle(1);
    addChild(title_);

    cursor_ = std::make_shared<TextBox>();
    cursor_->setTexture("title", 127);
    cursor_->setVisible(false);
    addChild(cursor_);

    active_child_ = 0;
    getChild(0)->setState(NodePass);
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
    title_->getChild(active)->setState(NodePass);
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
        if (button->getState() == NodePass || button->getState() == NodePress)
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
        if (current_button->getState() == NodePass)
        {
            x += 2;
        }
        if (current_button->getState() == NodePress)
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
                    {
                        active_child_ = item_each_line_ * (line_count_ - 1);
                    }
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
        title_->setAllChildState(NodeNormal);
    }
    if (focus_ == 0)
    {
        title_->setDealEvent(1);
        if (e.type == BP_KEYUP && e.key.keysym.sym == BPK_DOWN)
        {
            focus_ = 1;
        }
    }
    if (e.type == BP_CONTROLLERBUTTONDOWN)
    {
        title_->setDealEvent(1);
        switch (e.cbutton.button)
        {
        case BP_CONTROLLER_BUTTON_DPAD_LEFT:
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
        case BP_CONTROLLER_BUTTON_DPAD_RIGHT:
            if (active_child_ < item_each_line_ * line_count_ - 1)
            {
                active_child_++;
            }
            else
            {
                leftup_index_ += item_each_line_;
                if (leftup_index_ <= max_leftup_)
                {
                    active_child_ = item_each_line_ * (line_count_ - 1);
                }
            }
            break;
        case BP_CONTROLLER_BUTTON_DPAD_UP:
            if (active_child_ < item_each_line_ && leftup_index_ == 0)
            {
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
        case BP_CONTROLLER_BUTTON_DPAD_DOWN:
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
}

void UIItem::showItemProperty(Item* item)
{
    if (item == nullptr)
    {
        return;
    }
    //物品名和数量
    Font::getInstance()->draw(item->Name, 24, x_ + 10, y_ + 370, { 255, 255, 255, 255 });
    Font::getInstance()->draw(std::to_string(Save::getInstance()->getItemCountInBag(current_item_->ID)), 24, x_ + 260, y_ + 370, { 255, 255, 255, 255 });
    Font::getInstance()->draw(item->Introduction, 20, x_ + 10, y_ + 400, { 255, 255, 255, 255 });

    int x = 10, y = 430;
    int size = 20;
    int l;

    //以下显示物品的属性
    BP_Color c = { 255, 215, 0, 255 };

    //特别判断罗盘
    if (item->isCompass())
    {
        int man_x, man_y;
        MainScene::getInstance()->getManPosition(man_x, man_y);
        auto str = fmt1::format("當前坐標 {}, {}", man_x, man_y);
        addOneProperty(str, 1);
        y = showAddedProperty(size, c, x, y);
    }

    //剧情物品不继续显示了
    if (item->ItemType == 0)
    {
        return;
    }

    //Font::getInstance()->draw("效果：", size, x_ + x, y_ + y, c);
    //y += size + 10;

    addOneProperty("生命{:+}", item->AddHP);
    addOneProperty("生命上限{:+}", item->AddMaxHP);
    addOneProperty("內力{:+}", item->AddMP);
    addOneProperty("內力上限{:+}", item->AddMaxMP);
    addOneProperty("體力{:+}", item->AddPhysicalPower);
    addOneProperty("中毒{:+}", item->AddPoison);

    addOneProperty("攻擊{:+}", item->AddAttack);
    addOneProperty("輕功{:+}", item->AddSpeed);
    addOneProperty("防禦{:+}", item->AddDefence);

    addOneProperty("醫療{:+}", item->AddMedicine);
    addOneProperty("用毒{:+}", item->AddUsePoison);
    addOneProperty("解毒{:+}", item->AddDetoxification);
    addOneProperty("抗毒{:+}", item->AddAntiPoison);

    addOneProperty("拳掌{:+}", item->AddFist);
    addOneProperty("御劍{:+}", item->AddSword);
    addOneProperty("耍刀{:+}", item->AddKnife);
    addOneProperty("特殊兵器{:+}", item->AddUnusual);
    addOneProperty("暗器{:+}", item->AddHiddenWeapon);

    addOneProperty("作弊{:+}", item->AddKnowledge);
    addOneProperty("道德{:+}", item->AddMorality);
    addOneProperty("攻擊帶毒{:+}", item->AddAttackWithPoison);

    addOneProperty("內力調和", int(item->ChangeMPType == 2));
    addOneProperty("雙擊", int(item->AddAttackTwice == 1));

    auto magic = Save::getInstance()->getMagic(item->MagicID);
    if (magic)
    {
        auto str = fmt1::format("習得武學{}", magic->Name);
        addOneProperty(str, 1);
    }
    l = showAddedProperty(size, c, x, y);
    //以下显示物品需求

    //药品和暗器类不继续显示了
    if (item->ItemType == 3 || item->ItemType == 4)
    {
        return;
    }

    y += l * size + 10;    //换行
    c = { 224, 170, 255, 255 };
    //Font::getInstance()->draw("需求：", size, x_ + x, y_ + y, c);
    //y += size + 10;
    auto role = Save::getInstance()->getRole(item->OnlySuitableRole);
    if (role)
    {
        auto str = fmt1::format("僅適合{}", role->Name);
        addOneProperty(str, 1);
    }

    addOneProperty("內力{}", item->NeedMP);
    addOneProperty("攻擊{}", item->NeedAttack);
    addOneProperty("輕功{}", item->NeedSpeed);

    addOneProperty("醫療{}", item->NeedMedicine);
    addOneProperty("用毒{}", item->NeedUsePoison);
    addOneProperty("解毒{}", item->NeedDetoxification);

    addOneProperty("拳掌{}", item->NeedFist);
    addOneProperty("御劍{}", item->NeedSword);
    addOneProperty("耍刀{}", item->NeedKnife);
    addOneProperty("特殊兵器{}", item->NeedUnusual);
    addOneProperty("暗器{}", item->NeedHiddenWeapon);

    addOneProperty("資質{}", item->NeedIQ);

    if (item->NeedMPType == 0)
    {
        addOneProperty("陰性內力");
    }
    if (item->NeedMPType == 1)
    {
        addOneProperty("陽性內力");
    }

    addOneProperty("基礎經驗{}", item->NeedExp);

    if (item->NeedMaterial >= 0)
    {
        std::string str = "耗費";
        str += Save::getInstance()->getItem(item->NeedMaterial)->Name;
        addOneProperty(str, 1);
    }

    l = showAddedProperty(size, c, x, y);

    y += l * size + 10;
    c = { 51, 250, 255, 255 };
    for (int i = 0; i < 5; i++)
    {
        int make = item->MakeItem[i];
        if (make >= 0)
        {
            std::string str = Save::getInstance()->getItem(make)->Name;
            //str += " %d";
            addOneProperty(str, item->MakeItemCount[i]);
        }
    }
    showAddedProperty(size, c, x, y);
}

void UIItem::addOneProperty(const std::string& format_str, int v)
{
    if (v != 0)
    {
        properties_.push_back(fmt1::format(format_str.c_str(), v));
    }
}

void UIItem::addOneProperty(const std::string& format_str)
{
    properties_.push_back(format_str);
}

//返回值为行数
int UIItem::showAddedProperty(int size, BP_Color c, int x, int y)
{
    int line = 1;
    for (auto& str : properties_)
    {
        int draw_length = size * Font::getTextDrawSize(str) / 2 + size;
        int x1 = x + draw_length;
        if (x1 > 700)
        {
            x = 10;
            y += size + 5;
            line++;
        }
        Font::getInstance()->draw(str, size, x_ + x, y_ + y, c);
        x += draw_length;
    }
    properties_.clear();
    return line;
}

void UIItem::onPressedOK()
{
    current_item_ = nullptr;
    for (int i = 0; i < item_buttons_.size(); i++)
    {
        auto button = item_buttons_[i];
        if (button->getState() == NodePress)
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
            auto team_menu = std::make_shared<TeamMenu>();
            team_menu->setItem(current_item_);
            team_menu->setText(fmt1::format("誰要使用{}", current_item_->Name));
            team_menu->run();
            auto role = team_menu->getRole();
            if (role)
            {
                Role r = *role;
                role->useItem(current_item_);
                auto df = std::make_shared<ShowRoleDifference>(&r, role);
                df->setText(fmt1::format("{}服用{}", role->Name, current_item_->Name));
                df->run();
                Event::getInstance()->addItemWithoutHint(current_item_->ID, -1);
            }
        }
        else if (current_item_->ItemType == 1 || current_item_->ItemType == 2)
        {
            auto team_menu = std::make_shared<TeamMenu>();
            team_menu->setItem(current_item_);
            auto format_str = "誰要修煉{}";
            if (current_item_->ItemType == 1)
            {
                format_str = "誰要裝備{}";
            }
            team_menu->setText(fmt1::format(format_str, current_item_->Name));
            team_menu->run();
            auto role = team_menu->getRole();
            if (role)
            {
                role->equip(current_item_);
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
