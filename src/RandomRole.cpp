#include "RandomRole.h"
#include "Random.h"

RandomRole::RandomRole()
{
    setShowButton(false);

    // button_ok_ = new Button();
    // button_ok_->setText("´_¶¨");
    // addChild(button_ok_, 350, 55);

    head_ = new Head();
    addChild(head_, -290, 100);
}

RandomRole::~RandomRole()
{
}


void RandomRole::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 192 }, 0, 0, -1, -1);
    head_->setRole(role_);
    UIStatus::draw();
}
