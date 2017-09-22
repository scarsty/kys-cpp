#include "TitleScene.h"
#include "Menu.h"
#include "MainMap.h"
#include "BattleData.h"
#include "Dialogues.h"
#include "UI.h"
#include "Sprite.h"
#include "Event.h"

TitleScene::TitleScene()
{

}

TitleScene::~TitleScene()
{

}

void TitleScene::draw()
{
    Texture::getInstance()->renderTexture("resource/title", 0, 0, 0);
	//string filename = "Dialogues/Picture/1.png";
	//auto sprite = new Sprite(filename);
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
//	sprite->setPositionAndSize(5, m_y);
//	sprite->draw();
	//m_Dialogues->draw();

}

void TitleScene::dealEvent(BP_Event& e)
{

}

void TitleScene::OnStart()
{
	//BattleData::getInstance()->isLoad();
}

//void HelloWorldScene::func()
//{
//    auto i = *(int*)(data);
//    //Engine::getInstance()->showMessage((to_string(i)).c_str());
//    if (i == 2)
//    { e.type = BP_QUIT; }
//    if (i == 0)
//    {
//        Save::getInstance()->LoadR(0);		
//        auto m = new MainMap();
//		//pop();
//        push(m);
//        SDL_FlushEvents(SDL_QUIT, SDL_MOUSEWHEEL);
//    }
//    if (i == 1)
//    {
//        /*Save::getInstance()->LoadR(0);
//        auto m = new MainMap();
//        push(m);
//        SDL_FlushEvents(SDL_QUIT, SDL_MOUSEWHEEL);*/
//		
//		//fprintf(stderr, "test dialogues %s", Dialogues::m_Dialogues.at(1).c_str());
//    }
//    BattleData::getInstance()->isLoad();
//}

void TitleScene::init()
{
	auto b = new Button("menu/0.png", "menu/6.png", "menu/0.png");
	b->setRect(22, 22, 110, 24);
	b->setFunction(BIND_FUNC(TitleScene::OnStart));
	push(b);
	
}

