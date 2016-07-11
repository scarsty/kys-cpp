#pragma once
#include "Base.h"
#include "TextureManager.h"

class Scene :public Base
{
public:
	Scene();
	virtual ~Scene();
	virtual void draw() override {}
	virtual void dealEvent(BP_Event& e) override {}
};

