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
    full_window_ = 1;
}

TitleScene::~TitleScene()
{

}

void TitleScene::draw()
{
    TextureManager::getInstance()->renderTexture("resource/title", 0, 0, 0);
}

void TitleScene::dealEvent(BP_Event& e)
{
    //auto b1 = new Button("resource/title", 3, 4, 5);
    ////m->addButton(b, 20, 30);
    //auto b2 = new Button("resource/title", 6, 7, 8);
    ////m->addButton(b, 20, 60);
    //auto b3 = new Button("resource/title", 9, 10, 11);

    //int r = Menu::menu({ b1,b2,b3 }, 20, 20);
    //pop();
    //m->addButton(b, 20, 90);
}

//void TitleScene::OnStart()
//{
//BattleData::getInstance()->isLoad();
//}

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
//      //pop();
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
//      //fprintf(stderr, "test dialogues %s", Dialogues::m_Dialogues.at(1).c_str());
//    }
//    BattleData::getInstance()->isLoad();
//}

void TitleScene::init()
{
    int r;
    auto m = new Menu(&r);
    m->tex_ = TextureManager::getInstance()->loadTexture("resource/title", 17);
    push(m);
    auto b = new Button("resource/title", 3, 4, 5);    
    m->addButton(b, 20, 30);
    b = new Button("resource/title", 6, 7, 8);
    m->addButton(b, 20, 60);
    b = new Button("resource/title", 9, 10, 11);
    m->addButton(b, 20, 90);
    printf("%d\n",r);
}

