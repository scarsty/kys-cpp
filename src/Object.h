#pragma once
#include "TextureManager.h"

class Object
{
public:
    Texture* tex_ = nullptr;
    bool can_burn_ = false;
    bool can_ = false;

public:
    Texture* getTexture() { return tex_; }
    void calPropertyByTexture();
};

