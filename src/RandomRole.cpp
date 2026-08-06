#include "RandomRole.h"
#include "Random.h"
#include "GameUtil.h"

RandomRole::RandomRole()
{
    setShowButton(false);
    setShowRandomProperties(true);
    action_menu_ = std::make_shared<Menu>();
    action_menu_->setPosition(185, 675);
    action_menu_->setSize(270, 30);
    button_random_ = action_menu_->addChild<Button>(0, 0);
    button_random_->setText("隨機");
    button_ok_ = action_menu_->addChild<Button>(90, 0);
    button_ok_->setText("確定");
    button_cancel_ = action_menu_->addChild<Button>(180, 0);
    button_cancel_->setText("取消");
    addChild(action_menu_);
    head_ = addChild<Head>(-290, 100);
}

RandomRole::~RandomRole()
{
}

void RandomRole::onEntrance()
{
    randomizeRole();
    action_menu_->onEntrance();
}

void RandomRole::randomizeRole()
{
    if (role_ == nullptr)
    {
        return;
    }

    RandomDouble r;
    role_->MaxHP = 25 + r.rand_int(26);
    role_->HP = role_->MaxHP;
    role_->MaxMP = 25 + r.rand_int(26);
    role_->MP = role_->MaxMP;
    role_->MPType = r.rand_int(2);
    role_->IncLife = 1 + r.rand_int(10);
    role_->Attack = 25 + r.rand_int(6);
    role_->Speed = 25 + r.rand_int(6);
    role_->Defence = 25 + r.rand_int(6);
    role_->Medicine = 25 + r.rand_int(6);
    role_->UsePoison = 25 + r.rand_int(6);
    role_->Detoxification = 25 + r.rand_int(6);
    role_->Fist = 25 + r.rand_int(6);
    role_->Sword = 25 + r.rand_int(6);
    role_->Knife = 25 + r.rand_int(6);
    role_->Unusual = 25 + r.rand_int(6);
    role_->HiddenWeapon = 25 + r.rand_int(6);
    role_->IQ = 1 + r.rand_int(100);
    for (auto& e:role_->EquipMagic)
    {
        e = role_->MagicID[0];
    }
}

void RandomRole::onPressedOK()
{
    int action = action_menu_->getResult();
    if (action < 0)
    {
        return;
    }

    action_menu_->setExit(false);
    action_menu_->setResult(-1);
    if (action == 0)
    {
        randomizeRole();
        action_menu_->onEntrance();
    }
    else if (action == 1)
    {
        exitWithResult(0);
    }
    else if (action == 2)
    {
        exitWithResult(-1);
    }
}

void RandomRole::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 192 }, 0, 0, -1, -1);
    head_->setRole(role_);
    UIStatus::draw();
}
