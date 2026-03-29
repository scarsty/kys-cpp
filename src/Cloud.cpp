#include "Cloud.h"
#include "Random.h"
#include "Scene.h"
#include "TextureManager.h"

void Cloud::initRand()
{
    RandomDouble r;
    position_.x = r.rand_int(max_X_);
    position_.y = r.rand_int(max_Y_);
    speed_x_ = 1 + r.rand_int(3);
    speed_y_ = 0;
    num_ = r.rand_int(num_style_);
    alpha_ = 64 + r.rand_int(192);
    color_ = { (uint8_t)(r.rand_int(256)), (uint8_t)(r.rand_int(256)), (uint8_t)(r.rand_int(256)), 255 };
}

void Cloud::setPositionOnScreen(int x, int y, int center_x, int center_y)
{
    x_ = position_.x - (-y * Scene::TILE_W + x * Scene::TILE_W + max_X_ / 2 - center_x);
    y_ = position_.y - (y * Scene::TILE_H + x * Scene::TILE_H + Scene::TILE_H - center_y);
}

void Cloud::draw()
{
    TextureManager::RenderInfo info;
    info.c = color_;
    info.alpha = alpha_;
    info.c.a = alpha_;
    info.color_v.resize(4, info.c);
    info.brightness_v = { 1, 0, 0, 0 };
    TextureManager::getInstance()->renderTexture("cloud", num_, x_, y_, info);
}

void Cloud::flow()
{
    position_.x += speed_x_;
    position_.y += speed_y_;
    auto p = position_;
    if (p.x < 0 || p.x > max_X_ || p.y < 0 || p.y > max_Y_)
    {
        initRand();
    }
    if (p.x < 0) { position_.x = max_X_; }
    if (p.x > max_X_) { position_.x = 0; }
    if (p.y < 0) { position_.y = max_Y_; }
    if (p.y > max_Y_) { position_.y = 0; }
}
