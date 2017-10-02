#include "BattleMenu.h"
#include "Save.h"

BattleMenu::BattleMenu()
{
    head_ = new Head();
    addChild(head_);
}

BattleMenu::~BattleMenu()
{
}

void BattleMenu::setRole(int role_id)
{
    head_->setRole(Save::getInstance()->getRole(role_id));
}

void BattleMenu::setRole(Role* role)
{
    head_->setRole(role);
}
