#include "Cloud.h"

void Cloud::initRand()
{
    position_.x = rand() % max_X;
    position_.y = rand() % max_Y;
    speed_ = 1 + rand() % 3;
    num_ = rand() % num_style_;
    alpha_ = 128 + rand() % 128;
    color_ = { (uint8_t)(rand() % 256), (uint8_t)(rand() % 256), (uint8_t)(rand() % 256), 255 };
}

void Cloud::setPositionOnScreen(int x, int y, int Center_X, int Center_Y)
{
    x_ = position_.x - (-y * 18 + x * 18 + max_X / 2 - Center_X);
    y_ = position_.y - (y * 9 + x * 9 + 9 - Center_Y);
}

void Cloud::draw()
{
    TextureManager::getInstance()->renderTexture("cloud", num_, x_, y_, color_, alpha_);
    position_.x += speed_;
    if (position_.x > max_X)
    {
        position_.x = 0;
    }
}
