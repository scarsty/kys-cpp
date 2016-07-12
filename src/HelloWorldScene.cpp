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
		b->setSize(100,30);
		b->setFunction(BIND_FUNC(HelloWorldScene::func));
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


void HelloWorldScene::func(BP_Event &e, void* data)
{
	auto i = *(int*)(data);
	Engine::getInstance()->showMessage((to_string(i)).c_str());
	if (i == 2)
		e.type = BP_QUIT;
}

