#include "UIStatus.h"
#include "Font.h"
#include "others/libconvert.h"



UIStatus::UIStatus()
{
}


UIStatus::~UIStatus()
{
}

void UIStatus::draw()
{
    if (role_ == nullptr) { return; }

    TextureManager::getInstance()->renderTexture("head", role_->HeadNum, x_, y_);

    auto font = Font::getInstance();
    BP_Color color = { 255, 255, 255, 255 };
    const int font_size = 30;
    font->draw(convert::formatString("%s", role_->Name), font_size, x_ + 20, y_ + 150, color);
    font->draw(convert::formatString("等級%5d", role_->Level), font_size, x_ + 20, y_ + 190, color);
    font->draw(convert::formatString("生命%5d/%5d", role_->HP, role_->MaxHP), font_size, x_ + 20, y_ + 230, color);
    font->draw(convert::formatString("內力%5d/%5d", role_->MP, role_->MaxMP), font_size, x_ + 20, y_ + 270, color);
    font->draw(convert::formatString("體力%5d/%5d", role_->PhysicalPower, 100), font_size, x_ + 20, y_ + 310, color);
    font->draw(convert::formatString("經驗%10d", role_->Exp), font_size, x_ + 20, y_ + 350, color);
    font->draw(convert::formatString("升級%10d", role_->Exp), font_size, x_ + 20, y_ + 390, color);//这个再说

    font->draw(convert::formatString("攻擊%10d", role_->Attack), font_size, x_ + 300, y_ + 150, color);
    font->draw(convert::formatString("防禦%10d", role_->Defence), font_size, x_ + 300, y_ + 190, color);
    font->draw(convert::formatString("輕功%10d", role_->Speed), font_size, x_ + 300, y_ + 230, color);
    //font->draw(convert::formatString("移動%10d", role_->Morality), font_size, x_ + 300, y_ + 270, color);//原版不存在
    font->draw(convert::formatString("醫療能力%10d", role_->Medcine), font_size, x_ + 300, y_ + 310, color);
    font->draw(convert::formatString("用毒能力%10d", role_->UsePoison), font_size, x_ + 300, y_ + 350, color);
    font->draw(convert::formatString("解毒能力%10d", role_->Detoxification), font_size, x_ + 300, y_ + 390, color);
    font->draw(convert::formatString("拳掌功夫%10d", role_->Fist), font_size, x_ + 300, y_ + 430, color);
    font->draw(convert::formatString("御劍能力%10d", role_->Sword), font_size, x_ + 300, y_ + 470, color);
    font->draw(convert::formatString("耍刀能力%10d", role_->Knife), font_size, x_ + 300, y_ + 510, color);
    font->draw(convert::formatString("特殊兵器%10d", role_->Unusual), font_size, x_ + 300, y_ + 550, color);
    font->draw(convert::formatString("暗器技巧%10d", role_->HiddenWeapon), font_size, x_ + 300, y_ + 690, color);


}
