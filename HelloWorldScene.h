#pragma once
#include "Scene.h"
#include "Dialogues.h"
#include "Event.h"

class HelloWorldScene : public Scene
{
public:
	HelloWorldScene();
	~HelloWorldScene();

	void draw() override;
	void dealEvent(BP_Event &e) override;
	
	void func(BP_Event &e, void* data);
	bool b_s = true;
	int m_y = 15;
	int speed = 2;
	int state = 0;
	void init() override;

	Dialogues * m_Dialogues = new Dialogues();
};

