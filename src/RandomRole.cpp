#include "RandomRole.h"
#include "Random.h"

RandomRole::RandomRole()
{
    setShowButton(false);

    button_ok_ = new Button();
    button_ok_->setText("´_¶¨");
    addChild(button_ok_, 350, 55);
    head_ = new Head();
    addChild(head_, -290, 100);
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
    role_->MaxHP = 500 - 2 * role_->IQ;
    role_->HP = role_->MaxHP;
    role_->MaxMP = 250 + role_->IQ * 5;
    role_->MP = role_->MaxMP;
    role_->MPType = r.rand_int(2);
    role_->IncLife = 1 + r.rand_int(10);
    role_->Attack = 20 + std::ceil(role_->IQ / 2.0);
    role_->Speed = 30;
    role_->Defence = 20 + std::floor(role_->IQ / 2.0);
    role_->Medicine = 40;
    role_->UsePoison = 40;
    role_->Detoxification = 30;
    role_->Fist = 30;
    role_->Sword = 30;
    role_->Knife = 30;
    role_->Unusual = 30;
    role_->HiddenWeapon = 30;
    printf("IQ is %d\n", role_->IQ);
}

void RandomRole::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 192 }, 0, 0, -1, -1);
    head_->setRole(role_);
    UIStatus::draw();
}
