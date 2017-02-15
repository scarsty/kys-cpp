#include "HelloWorldScene.h"
#include "Menu.h"
#include "MainMap.h"
#include "BattleData.h"
#include "Dialogues.h"
#include "UI.h"

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

void HelloWorldScene::dealEvent(BP_Event& e)
{

}

void HelloWorldScene::func(BP_Event& e, void* data)
{
    auto i = *(int*)(data);
    //Engine::getInstance()->showMessage((to_string(i)).c_str());
    if (i == 2)
    { e.type = BP_QUIT; }
    if (i == 0)
    {
        Save::getInstance()->LoadR(0);
        auto m = new MainMap();
		pop();
        push(m);
        SDL_FlushEvents(SDL_QUIT, SDL_MOUSEWHEEL);
    }
    if (i == 1)
    {
        /*Save::getInstance()->LoadR(0);
        auto m = new MainMap();
        push(m);
        SDL_FlushEvents(SDL_QUIT, SDL_MOUSEWHEEL);*/
		
		//fprintf(stderr, "test dialogues %s", Dialogues::m_Dialogues.at(1).c_str());
    }
    BattleData::getInstance()->load();
}

void HelloWorldScene::init()
{
	auto m_Dialogues = new Dialogues();
	m_Dialogues->InitDialogusDate();
	m_Dialogues->SetFontsName("fonts/Dialogues.ttf");
	SDL_Color color = { 0, 0, 0, 255 };
	m_Dialogues->SetFontsColor(color);
	
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

	auto m_UI = new UI();
	m_UI->AddSprite(m_Dialogues);
	m_UI->AddSprite(m);
	push(m_UI);
	
}

