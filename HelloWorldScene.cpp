#include "HelloWorldScene.h"
#include "Menu.h"
#include "MainMap.h"
#include "BattleData.h"
#include "Dialogues.h"
#include "UI.h"
#include "Sprite.h"
#include "Event.h"

HelloWorldScene::HelloWorldScene()
{

}

HelloWorldScene::~HelloWorldScene()
{

}

void HelloWorldScene::draw()
{
    Texture::getInstance()->copyTexture("title", 0, 0, 0);
	string filename = "Dialogues/Picture/1.png";
	auto sprite = new Sprite(filename);
	speed--;
	if (speed<0)
	{
		if (m_y <= 25 && m_y>=15)
		{
			if (b_s)
			{
				m_y++;
			}
			else
			{
				m_y--;
			}
			
		}
		if (m_y==25)
		{
			b_s = false;
		}
		if (m_y==15)
		{
			b_s = true;
		}
		

		speed = 2;
	}
	sprite->setPositionAndSize(5, m_y);
	sprite->draw();
	m_Dialogues->draw();

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
		
		EventManager *event = EventManager::getInstance();
		event->initEventData();
		
        auto m = new MainMap();
		//pop();
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
    BattleData::getInstance()->isLoad();
}

void HelloWorldScene::init()
{
	/*auto m_Dialogues = new Dialogues();
	m_Dialogues->InitDialogusDate();
	m_Dialogues->SetFontsName("fonts/Dialogues.ttf");
	SDL_Color color = { 0, 0, 0, 255 };
	m_Dialogues->SetFontsColor(color);
	m_Dialogues->SetDialoguesNum(2);*/
	
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

	string filename = "Dialogues/Picture/1.png";
	auto sprite = new Sprite(filename);
	sprite->setPositionAndSize(10, 20);

	auto m_UI = new UI();
	//m_UI->AddSprite(m_Dialogues);
	//m_UI->AddSprite(sprite);
	m_UI->AddSprite(m);
	push(m_UI);

	m_Dialogues->InitDialogusDate();
	m_Dialogues->SetFontsName("fonts/Dialogues.ttf");
	SDL_Color color = { 0, 0, 0, 255 };
	m_Dialogues->SetFontsColor(color);
	m_Dialogues->SetDialoguesEffect(true);
	m_Dialogues->SetDialoguesSpeed(5);
	m_Dialogues->SetDialoguesNum(2);
	
}

