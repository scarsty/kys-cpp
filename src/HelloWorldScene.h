#pragma once
#include "Scene.h"

class HelloWorldScene : public Scene
{
public:
	HelloWorldScene();
	~HelloWorldScene();

	void draw() override;
	void dealEvent(BP_Event &e) override;

	void func(BP_Event &e, void* data);
	int state = 0;
};

