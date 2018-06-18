#include "Cloud.h"
#include "Random.h"

void Cloud::initRand()
{
    RandomDouble r;
    position_.x = r.rand_int(max_X);
    position_.y = r.rand_int(max_Y);
    speed_ = 1 + r.rand_int(3);
    num_ = r.rand_int(num_style_);
    alpha_ = 64 + r.rand_int(192);
    color_ = { (uint8_t)(r.rand_int(256)), (uint8_t)(r.rand_int(256)), (uint8_t)(r.rand_int(256)), 255 };
}

void Cloud::setPositionOnScreen(int x, int y, int Center_X, int Center_Y)
{
    x_ = position_.x - (-y * 18 + x * 18 + max_X / 2 - Center_X);
    y_ = position_.y - (y * 9 + x * 9 + 9 - Center_Y);
}

void Cloud::draw()
{
    TextureManager::getInstance()->renderTexture("cloud", num_, x_, y_, color_, alpha_);
}

void Cloud::flow()
{
    position_.x += speed_;
    if (position_.x > max_X)
    {
        initRand();
        position_.x = 0;
    }
}
