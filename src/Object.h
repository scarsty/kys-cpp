#pragma once
#include "TextureManager.h"

enum class ObjectMaterial
{
    Water,
    Wood,
    Stone,
};

class Object
{
public:
    TextureWarpper* tex_ = nullptr;

    int can_walk_ = 0;
    ObjectMaterial material_ = ObjectMaterial::Stone;

public:
    TextureWarpper* getTexture() { return tex_; }
    void calPropertyByTexture();
};

