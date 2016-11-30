#include "HelloWorldScene.h"
#include "Menu.h"
#include "MainMap.h"

HelloWorldScene::HelloWorldScene()
{

}

HelloWorldScene::~HelloWorldScene()
{

}

void HelloWorldScene::draw()
{
	Texture::getInstance()->copyTexture("title", 0, 0, 0);
}

void HelloWorldScene::dealEvent(BP_Event &e)
{
		
}


void HelloWorldScene::func(BP_Event &e, void* data)
{
	auto i = *(int*)(data);
	//Engine::getInstance()->showMessage((to_string(i)).c_str());
	if (i == 2)
		e.type = BP_QUIT;
	if (i == 0)
	{
		Save::getInstance()->LoadR(0);
		auto m = new MainMap();
		push(m);
		SDL_FlushEvents(SDL_QUIT, SDL_MOUSEWHEEL);
	}
	if (i == 1)
	{
		Save::getInstance()->LoadR(0);
		auto m = new MainMap();
		push(m);
		SDL_FlushEvents(SDL_QUIT, SDL_MOUSEWHEEL);
	}
}

void HelloWorldScene::init()
{
	auto m = new Menu();
	m->setPosition(100, 100);
	for (int i = 0; i < 3; i++)
	{
		auto b = new Button();
		b->setTexture("title", i + 107);
		m->addButton(b, 0, i * 30);
		b->setSize(100, 30);
		b->setFunction(BIND_FUNC(HelloWorldScene::func));
	}
	push(m);
}

