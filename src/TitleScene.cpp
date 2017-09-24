#include "TitleScene.h"
#include "Menu.h"
#include "MainMap.h"
#include "BattleData.h"
#include "Event.h"

TitleScene::TitleScene()
{
    full_window_ = 1;
}

TitleScene::~TitleScene()
{
    delete menu_;
    delete menu_load_;
}

void TitleScene::draw()
{
    int count = count_ / 20;
    TextureManager::getInstance()->renderTexture("title", 0, 0, 0);
    TextureManager::getInstance()->renderTexture("title", 12, 70, 100);
    TextureManager::getInstance()->renderTexture("title", 10, 70, 100);
    TextureManager::getInstance()->renderTexture("title", 10, 670, 100);

    int alpha = count_ % 256;
    TextureManager::getInstance()->renderTexture("title", 13, 50, 300, { 255, 255, 255, 255 }, alpha);
    TextureManager::getInstance()->renderTexture("head", count % 115, 50, 300, { 255, 255, 255, 255 }, alpha);
    count_++;
}

void TitleScene::dealEvent(BP_Event& e)
{
    Save::getInstance()->LoadR(1);
    menu_->run();
    int r = menu_->getResult();
    if (r == 0)
    {        
        //auto m = new MainMap();
        //m->run();
    }
    if (r == 1)
    {
        menu_load_->run();
    }
    if (r == 2)
    {
        loop_ = false;
    }
}

void TitleScene::init()
{
    menu_ = new Menu();
    //m->tex_ = TextureManager::getInstance()->loadTexture("title", 17);
    menu_->setPosition(400, 250);
    auto b = new Button("title", 3, 23, 23);
    menu_->addButton(b, 20, 0);
    b = new Button("title", 4, 24, 24);
    menu_->addButton(b, 20, 50);
    b = new Button("title", 6, 26, 26);
    menu_->addButton(b, 20, 100);

    menu_load_ = new MenuText({ "载入进度一", "載入進度二", "載入進度3" });
    menu_load_->setPosition(500, 300);


}

