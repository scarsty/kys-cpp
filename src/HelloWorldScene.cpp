#include "HelloWorldScene.h"
#include "Menu.h"

HelloWorldScene::HelloWorldScene()
{
	push(this);
	auto m = new Menu();
	m->setPosition(100, 100);
	for (int i = 0; i < 3; i++)
	{
		auto b = new Button();
		b->setTexture("title", i + 107);
		m->addButton(b, 0, i*30);
	}
}

HelloWorldScene::~HelloWorldScene()
{

}

void HelloWorldScene::draw()
{
	TextureManager::getInstance()->copyTexture("title", 0, 0, 0);
}

void HelloWorldScene::dealEvent(BP_Event &e)
{

}



