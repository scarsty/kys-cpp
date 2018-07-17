#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "MainScene.h"
#include "Save.h"
#include "ShowRoleDifference.h"
#include "TeamMenu.h"
#include "UIItem.h"
#include "libconvert.h"

UIItem::UIItem()
{
    item_buttons_.resize(line_count_ * item_each_line_);

    for (int i = 0; i < item_buttons_.size(); i++)
    {
        auto b = new Button();
        item_buttons_[i] = b;
        b->setPosition(i % item_each_line_ * 85 + 40, i / item_each_line_ * 85 + 100);
        //b->setTexture("item", Save::getInstance()->getItemByBagIndex(i)->ID);
        addChild(b);
    }
    title_ = new MenuText();
    title_->setStrings({ "����", "����", "��ˎ", "����", "ȭ��", "���V", "���", "���T", "�ķ�" });
    title_->setFontSize(24);
    title_->arrange(0, 50, 64, 0);
    addChild(title_);

    cursor_ = new TextBox();
    cursor_->setTexture("title", 127);
    cursor_->setVisible(false);
    addChild(cursor_);
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

//ԭ���ࣺ0���飬1װ����2���ţ�3ҩƷ��4����
//��ϸ���ࣺ"0����", "1����", "2��ˎ", "3����", "4ȭ��", "5���V", "6���", "7���T", "8�ķ�"
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
            //��ȡ�������Ϊ8
            if (m->HurtType == 0)
            {
                return m->MagicType + 3;
            }
        }
        return 8;
    }
    //δ֪�����൱�ɾ���
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
    //ǿ��ͣ����ĳ����Ʒ
    if (force_item_type_ >= 0)
    {
        //title_->setResult(force_item_type_);
        title_->forcePassChild(force_item_type_);
    }
    geItemsByType(title_->getPassChildIndex());
    int type_item_count = available_items_.size();
    //�������������Ͻǿ���ȡ�����ֵ
    //���㷽�����ȼ��������������ȥ�ɼ�����������ÿ�г�Ա��
    int max_leftup = ((type_item_count + item_each_line_ - 1) / item_each_line_ - line_count_) * item_each_line_;
    if (max_leftup < 0)
    {
        max_leftup = 0;
    }

    leftup_index_ = GameUtil::limit(leftup_index_, 0, max_leftup);

    //���㱻����İ�ť
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
            current_button_ = button;
            //result_ = current_item_->ID;
        }
    }

    //���㱻����İ�ť��Ӧ����Ʒ
    current_item_ = nullptr;
    if (current_button_)
    {
        int x, y;
        current_button_->getPosition(x, y);
        current_item_ = Save::getInstance()->getItem(current_button_->getTexutreID());
        //�ù����ʾ����
        if (current_button_->getState() == Pass)
        {
            x += 2;
        }
        if (current_button_->getState() == Press)
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
}

void UIItem::showItemProperty(Item* item)
{
    if (item == nullptr)
    {
        return;
    }
    //��Ʒ��������
    auto str = convert::formatString("%-14s%5d", item->Name, Save::getInstance()->getItemCountInBag(current_item_->ID));
    Font::getInstance()->draw(str, 24, x_ + 10, y_ + 370, { 255, 255, 255, 255 });
    Font::getInstance()->draw(item->Introduction, 20, x_ + 10, y_ + 400, { 255, 255, 255, 255 });

    int x = 10, y = 430;
    int size = 20;

    //������ʾ��Ʒ������
    BP_Color c = { 255, 215, 0, 255 };

    //�ر��ж�����
    if (item->isCompass())
    {
        int man_x, man_y;
        MainScene::getInstance()->getManPosition(man_x, man_y);
        auto str = convert::formatString("��ǰ���� %d, %d", man_x, man_y);
        showOneProperty(1, str, size, c, x, y);
    }

    //������Ʒ��������ʾ��
    if (item->ItemType == 0)
    {
        return;
    }

    //Font::getInstance()->draw("Ч����", size, x_ + x, y_ + y, c);
    //y += size + 10;

    showOneProperty(item->AddHP, "����%+d", size, c, x, y);
    showOneProperty(item->AddMaxHP, "��������%+d", size, c, x, y);
    showOneProperty(item->AddMP, "����%+d", size, c, x, y);
    showOneProperty(item->AddMaxMP, "��������%+d", size, c, x, y);
    showOneProperty(item->AddPhysicalPower, "�w��%+d", size, c, x, y);
    showOneProperty(item->AddPoison, "�ж�%+d", size, c, x, y);

    showOneProperty(item->AddAttack, "����%+d", size, c, x, y);
    showOneProperty(item->AddSpeed, "�p��%+d", size, c, x, y);
    showOneProperty(item->AddDefence, "���R%+d", size, c, x, y);

    showOneProperty(item->AddMedcine, "�t��%+d", size, c, x, y);
    showOneProperty(item->AddUsePoison, "�ö�%+d", size, c, x, y);
    showOneProperty(item->AddDetoxification, "�ⶾ%+d", size, c, x, y);
    showOneProperty(item->AddAntiPoison, "����%+d", size, c, x, y);

    showOneProperty(item->AddFist, "ȭ��%+d", size, c, x, y);
    showOneProperty(item->AddSword, "����%+d", size, c, x, y);
    showOneProperty(item->AddKnife, "ˣ��%+d", size, c, x, y);
    showOneProperty(item->AddUnusual, "�������%+d", size, c, x, y);
    showOneProperty(item->AddHiddenWeapon, "����%+d", size, c, x, y);

    showOneProperty(item->AddKnowledge, "����%+d", size, c, x, y);
    showOneProperty(item->AddMorality, "����%+d", size, c, x, y);
    showOneProperty(item->AddAttackWithPoison, "���􎧶�%+d", size, c, x, y);

    showOneProperty(item->ChangeMPType == 2, "�����{��", size, c, x, y);
    showOneProperty(item->AddAttackTwice == 1, "�p��", size, c, x, y);

    auto magic = Save::getInstance()->getMagic(item->MagicID);
    if (magic)
    {
        auto str = convert::formatString("������W%s", magic->Name);
        showOneProperty(1, str, size, c, x, y);
    }

    //������ʾ��Ʒ����

    //ҩƷ�Ͱ����಻������ʾ��
    if (item->ItemType == 3 || item->ItemType == 4)
    {
        return;
    }

    x = 10;
    y += size + 10;    //����
    c = { 224, 170, 255, 255 };
    //Font::getInstance()->draw("����", size, x_ + x, y_ + y, c);
    //y += size + 10;
    auto role = Save::getInstance()->getRole(item->OnlySuitableRole);
    if (role)
    {
        auto str = convert::formatString("�H�m��%s", role->Name);
        showOneProperty(1, str, size, c, x, y);
        return;
    }

    showOneProperty(item->NeedMP, "����%d", size, c, x, y);
    showOneProperty(item->NeedAttack, "����%d", size, c, x, y);
    showOneProperty(item->NeedSpeed, "�p��%d", size, c, x, y);

    showOneProperty(item->NeedMedcine, "�t��%d", size, c, x, y);
    showOneProperty(item->NeedUsePoison, "�ö�%d", size, c, x, y);
    showOneProperty(item->NeedDetoxification, "�ⶾ%d", size, c, x, y);

    showOneProperty(item->NeedFist, "ȭ��%d", size, c, x, y);
    showOneProperty(item->NeedSword, "����%d", size, c, x, y);
    showOneProperty(item->NeedKnife, "ˣ��%d", size, c, x, y);
    showOneProperty(item->NeedUnusual, "�������%d", size, c, x, y);
    showOneProperty(item->NeedHiddenWeapon, "����%d", size, c, x, y);

    showOneProperty(item->NeedIQ, "�Y�|%d", size, c, x, y);

    showOneProperty(item->NeedExp, "���A���%d", size, c, x, y);

    if (item->NeedMaterial >= 0)
    {
        std::string str = "���M";
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
        auto str = convert::formatString(format_str.c_str(), v);
        //�����ǲ��ǳ�����
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

    //��ʹ�þ�����Ʒ��ʱ�򣬷���һ���������UI�жϴ�ʱ�����˳�
    if (current_item_->ItemType == 0)
    {
        result_ = current_item_->ID;
    }

    if (select_user_)
    {
        if (current_item_->ItemType == 3)
        {
            TeamMenu team_menu;
            team_menu.setItem(current_item_);
            team_menu.setText(convert::formatString("�lҪʹ��%s", current_item_->Name));
            team_menu.run();
            auto role = team_menu.getRole();
            if (role)
            {
                Role r = *role;
                GameUtil::useItem(role, current_item_);
                ShowRoleDifference df(&r, role);
                df.setText(convert::formatString("%s����%s", role->Name, current_item_->Name));
                df.run();
                Event::getInstance()->addItemWithoutHint(current_item_->ID, -1);
            }
        }
        else if (current_item_->ItemType == 1 || current_item_->ItemType == 2)
        {
            TeamMenu team_menu;
            team_menu.setItem(current_item_);
            auto format_str = "�lҪ�ޟ�%s";
            if (current_item_->ItemType == 1)
            {
                format_str = "�lҪ�b��%s";
            }
            team_menu.setText(convert::formatString(format_str, current_item_->Name));
            team_menu.run();
            auto role = team_menu.getRole();
            if (role)
            {
                GameUtil::equip(role, current_item_);
            }
        }
        else if (current_item_->ItemType == 4)
        {
            //�ƺ�����Ҫ���⴦��
        }
    }
    setExit(true);    //����ս��ʱ��ƽʱ��Ʒ�������Ը��ڵ����У��������û������
}

void UIItem::onPressedCancel()
{
    current_item_ = nullptr;
    exitWithResult(-1);
}
