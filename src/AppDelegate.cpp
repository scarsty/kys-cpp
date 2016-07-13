#pragma once
#include "AppDelegate.h"
#include "HelloWorldScene.h"

AppDelegate::AppDelegate()
{

}

AppDelegate::~AppDelegate()
{
}

bool AppDelegate::applicationDidFinishLaunching()
{
	auto engine = Engine::getInstance();

	engine->init();
	engine->setWindowSize(1024, 480);

	BP_Event e;
	HelloWorldScene h;
	h.push(&h);
	mainLoop(e);
	h.pop();
	return true;
}

void AppDelegate::mainLoop(BP_Event & e)
{
	auto engine = Engine::getInstance();
	auto loop = true;
	while (loop && engine->pollEvent(e) >= 0)
	{
		//从最后一个独占的开始画
		int begin_base = 0;
		for (int i = Base::baseVector.size() - 1; i >= 0; i--)
		{
			if (Base::baseVector[i]->full)
			{
				begin_base = i;
				break;
			}
		}
		for (int i = begin_base; i < Base::baseVector.size(); i++)
		{
			auto &b = Base::baseVector[i];
			if (b->visible)
				b->draw();
		}
		//处理最上层的消息
		if (Base::baseVector.size() > 0)
			Base::baseVector.back()->dealEvent(e);
		switch (e.type)
		{
		case BP_QUIT:
			if (engine->showMessage("Quit"))
				loop = false;
			break;
		default:
			break;
		}
		engine->renderPresent();
		engine->delay(25);
	}
}
