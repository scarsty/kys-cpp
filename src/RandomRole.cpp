#include "RandomRole.h"
#include "Random.h"

RandomRole::RandomRole()
{
    setShowButton(false);
    button_ok_ = addChild<Button>(350, 55);
    button_ok_->setText("確定");
    head_ = addChild<Head>(-290, 100);
}

RandomRole::~RandomRole()
{
}

void RandomRole::onPressedOK()
{
    if (button_ok_->getState() == Press)
    {
        result_ = 0;
        setExit(true);
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
    fmt::print("IQ is %d\n", role_->IQ);
}

void RandomRole::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 192 }, 0, 0, -1, -1);
    head_->setRole(role_);
    UIStatus::draw();
}
