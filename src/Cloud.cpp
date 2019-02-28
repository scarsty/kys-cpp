#include "Cloud.h"
#include "Random.h"

void Cloud::initRand(bool init_pos)
{
    RandomDouble r;
    if (init_pos)
    {
        position_.x = r.rand_int(max_X);
        position_.y = r.rand_int(max_Y);
    }
    speed_x_ = 1 + r.rand_int(3);
    speed_y_ = 0;
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
    position_.x += speed_x_;
    position_.y += speed_y_;
    if (position_.x < 0 || position_.x > max_X || position_.y < 0 || position_.y > max_Y)
    {
        initRand(false);
        position_.x = abs(position_.x % max_X);
        position_.y = abs(position_.y % max_Y);
    }
}
