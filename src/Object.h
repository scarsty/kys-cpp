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
    Texture* tex_ = nullptr;

    int can_walk_ = 0;
    ObjectMaterial material_ = ObjectMaterial::Stone;

public:
    Texture* getTexture() { return tex_; }
    void calPropertyByTexture();
};

