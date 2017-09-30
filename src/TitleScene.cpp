#include "TitleScene.h"
#include "Menu.h"
#include "MainMap.h"
#include "BattleData.h"
#include "Event.h"
#include "SubMap.h"
#include "Button.h"
#include "Audio.h"

TitleScene::TitleScene()
{
    full_window_ = 1;
    menu_ = new Menu();
    menu_->setTexture(TextureManager::getInstance()->loadTexture("title", 17));
    menu_->setPosition(400, 250);
    auto b = new Button("title", 3, 23, 23);
    menu_->addChildOnPosition(b, 20, 0);
    b = new Button("title", 4, 24, 24);
    menu_->addChildOnPosition(b, 20, 50);
    b = new Button("title", 6, 26, 26);
    menu_->addChildOnPosition(b, 20, 100);
    menu_load_ = new MenuText({ "载入进度一", "載入進度二", "載入進度3" });
    menu_load_->setPosition(500, 300);

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
    int r = menu_->run();
    if (r == 0)
    {

    }
    if (r == 1)
    {
        auto s = new SubMap(2);
        s->run();
        auto m = new MainMap();
        m->run();
        //menu_load_->run();
    }
    if (r == 2)
    {
        loop_ = false;
    }
}

void TitleScene::entrance()
{
    Save::getInstance()->LoadR(2);
    Audio::getInstance()->playMusic(3);
    //Audio::getInstance()->playESound(1);
    auto m = new MainMap();
    m->run();
}

