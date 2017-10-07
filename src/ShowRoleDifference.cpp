#include "ShowRoleDifference.h"
#include "Font.h"
#include "others/libconvert.h"

ShowRoleDifference::ShowRoleDifference()
{
    head1_ = new Head();
    addChild(head1_);
    head2_ = new Head();
    addChild(head2_, 400, 0);
}


ShowRoleDifference::~ShowRoleDifference()
{
}

void ShowRoleDifference::draw()
{
    head1_->setRole(role1_);
    head2_->setRole(role2_);
    if (role1_ && role2_ && role1_->ID == role2_->ID)
    {
        head2_->setRole(nullptr);
    }
    //TextureManager::getInstance()->renderTexture("head", role_->HeadID, x_, y_);

    auto font = Font::getInstance();
    BP_Color color = { 255, 255, 255, 255 };
    const int font_size = 25;
    int x = 100, y = 100;

    //showOneDifference(role1_->Name, "姓名 %-7s  -> %-7s", 20, color, x, y);

    showOneDifference(role1_->Level, "等級 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->Exp, "經驗 %7d   -> %7d", 20, color, x, y);

    showOneDifference(role1_->PhysicalPower, "體力 %7d   -> %7d", 20, color, x, y);

    showOneDifference(role1_->HP, role1_->MaxHP, "生命 %3d/%3d   -> %3d/%3d", 20, color, x, y);
    showOneDifference(role1_->MP, role1_->MaxMP, "內力 %3d/%3d   -> %3d/%3d", 20, color, x, y);


    showOneDifference(role1_->Attack, "攻擊 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->Defence, "防禦 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->Speed, "輕功 %7d   -> %7d", 20, color, x, y);

    showOneDifference(role1_->Medcine, "醫療 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->UsePoison, "用毒 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->Detoxification, "解毒 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->AntiPoison, "抗毒 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->AttackWithPoison, "帶毒 %7d   -> %7d", 20, color, x, y);



    showOneDifference(role1_->Fist, "拳掌 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->Sword, "御劍 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->Knife, "耍刀 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->Unusual, "特殊 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->HiddenWeapon, "暗器 %7d   -> %7d", 20, color, x, y);

    showOneDifference(role1_->Poison, "中毒 %7d   -> %7d", 20, color, x, y);

    showOneDifference(role1_->Morality, "道德 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->Fame, "聲望 %7d   -> %7d", 20, color, x, y);
    showOneDifference(role1_->IQ, "資質 %7d   -> %7d", 20, color, x, y);

    auto str = "內力陰陽調和";

    if (role2_->MPType == 0)
    {
        str = "內力陰";
    }
    if (role2_->MPType == 1)
    {
        str = "內力陽";
    }

    showOneDifference(role1_->MPType, str, 20, color, x, y);
    showOneDifference(role1_->AttackTwice, "雙擊", 20, color, x, y);

    if (y == 100)
    { Font::getInstance()->draw("無變化", 20, x, y, color); }
    //showOneDifference(role1_->Level, "御劍 %7d   -> %7d", 20, color, x, y);
}

