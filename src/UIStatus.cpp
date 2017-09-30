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

	Font::getInstance()->draw(convert::formatString("%s", role_->Name), 30, 320, 150, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("等级%5d", role_->Level), 30, 320, 190, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("生命%5d/%5d",role_->HP, role_->MaxHP), 30, 320, 230, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("内力%5d/%5d", role_->MP, role_->MaxMP), 30, 320, 270, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("体力%5d/%5d", role_->IncLife, role_->IncLife), 30, 320, 310, { 255, 255, 255, 255 });//需要修改
	Font::getInstance()->draw(convert::formatString("经验%10d", role_->Exp), 30, 320, 350, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("升级%10d", role_->Exp), 30, 320, 390, { 255, 255, 255, 255 });//需要修改

	Font::getInstance()->draw(convert::formatString("攻击%10d", role_->Attack), 30, 600, 150, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("防御%10d", role_->Defence), 30, 600, 190, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("轻功%10d", role_->Speed), 30, 600, 230, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("移动%10d", role_->Morality), 30, 600, 270, { 255, 255, 255, 255 });//需要修改
	Font::getInstance()->draw(convert::formatString("医疗能力%10d", role_->Medcine), 30, 600, 310, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("用毒能力%10d", role_->UsePoison), 30, 600, 350, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("解毒能力%10d", role_->Detoxification), 30, 600, 390, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("拳掌功夫%10d", role_->Fist), 30, 600, 430, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("御剑能力%10d", role_->Sword), 30, 600, 470, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("耍刀能力%10d", role_->Knife), 30, 600, 510, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("特殊兵器%10d", role_->Unusual), 30, 600, 550, { 255, 255, 255, 255 });
	Font::getInstance()->draw(convert::formatString("暗器技巧%10d", role_->HiddenWeapon), 30, 600, 690, { 255, 255, 255, 255 });


}
