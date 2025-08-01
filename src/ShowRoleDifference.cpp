﻿#include "ShowRoleDifference.h"
#include "Font.h"
#include "Save.h"

ShowRoleDifference::ShowRoleDifference()
{
    head1_ = std::make_shared<Head>();
    head2_ = std::make_shared<Head>();
    addChild(head1_, 0, 0);
    addChild(head2_, 150, 0);
    //setText("修習成功");    
    setPosition(Engine::getInstance()->getUIWidth() / 2 - 150, 180);
    setTextPosition(0, -30);
}

ShowRoleDifference::~ShowRoleDifference()
{
}

void ShowRoleDifference::draw()
{
    if (role1_ == nullptr || role2_ == nullptr) { return; }

    //if (black_screen_)
    //{
    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    //}
    head1_->setRole(role1_);
    head2_->setRole(role2_);
    head1_->setAlwaysLight(1);
    head2_->setAlwaysLight(1);
    if (role1_ && role2_ && role1_->ID == role2_->ID)
    {
        head1_->setRole(role2_);
        head1_->setPosition(Engine::getInstance()->getUIWidth() / 2 - 150, 50);
        head2_->setRole(nullptr);
    }

    head1_->setVisible(show_head_);
    head2_->setVisible(show_head_);

    auto font = Font::getInstance();
    Color color = { 255, 255, 255, 255 };
    const int font_size = 25;
    int x = x_, y = y_;

    std::string str;

    //showOneDifference(role1_->Name, "姓名 %-7s  -> %-7s", 20, color, x, y);
    showOneDifference(role1_->Level, "等級 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->Exp, "經驗 {:9}     -> {:9}", 20, color, x, y);

    showOneDifference(role1_->PhysicalPower, "體力 {:9}     -> {:9}", 20, color, x, y);

    if (role1_->HP != role2_->HP || role1_->MaxHP != role2_->MaxHP)
    {
        str = std::format("生命 {:4}/{:4}     -> {:4}/{:4}", role1_->HP, role1_->MaxHP, role2_->HP, role2_->MaxHP);
        showOneDifference(role1_->HP, str, 20, color, x, y, 1);
    }
    if (role1_->MP != role2_->MP || role1_->MaxMP != role2_->MaxMP)
    {
        str = std::format("內力 {:4}/{:4}     -> {:4}/{:4}", role1_->MP, role1_->MaxMP, role2_->MP, role2_->MaxMP);
        showOneDifference(role1_->MP, str, 20, color, x, y, 1);
    }

    showOneDifference(role1_->Attack, "攻擊 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->Defence, "防禦 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->Speed, "輕功 {:9}     -> {:9}", 20, color, x, y);

    showOneDifference(role1_->Medicine, "醫療 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->UsePoison, "用毒 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->Detoxification, "解毒 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->AntiPoison, "抗毒 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->AttackWithPoison, "帶毒 {:9}     -> {:9}", 20, color, x, y);

    showOneDifference(role1_->Fist, "拳掌 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->Sword, "御劍 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->Knife, "耍刀 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->Unusual, "特殊 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->HiddenWeapon, "暗器 {:9}     -> {:9}", 20, color, x, y);

    showOneDifference(role1_->Poison, "中毒 {:9}     -> {:9}", 20, color, x, y);

    showOneDifference(role1_->Morality, "道德 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->Fame, "聲望 {:9}     -> {:9}", 20, color, x, y);
    showOneDifference(role1_->IQ, "資質 {:9}     -> {:9}", 20, color, x, y);

    str = "內力陰陽調和";
    if (role2_->MPType == 0) { str = "內力陰"; }
    if (role2_->MPType == 1) { str = "內力陽"; }
    showOneDifference(role1_->MPType, str, 20, color, x, y);
    showOneDifference(role1_->AttackTwice, "雙擊", 20, color, x, y);

    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        if (role2_->MagicID[i] > 0
            && (role1_->MagicID[i] <= 0 || role1_->getRoleShowLearnedMagicLevel(i) != role2_->getRoleShowLearnedMagicLevel(i)))
        {
            str = std::format("武學{}目前修為{}",
                Save::getInstance()->getMagic(role2_->MagicID[i])->Name, role2_->getRoleShowLearnedMagicLevel(i));
            showOneDifference(role1_->MagicLevel[i], str, 20, color, x, y);
        }
    }

    if (y == y_)
    {
        Font::getInstance()->draw("無明显效果", 20, x, y, color);
    }
    //showOneDifference(role1_->Level, "御劍 {:9}     -> {:9}", 20, color, x, y);
    TextBox::draw();
}
