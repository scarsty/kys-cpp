#include "Audio.h"
#include "BattleScene.h"
#include "Button.h"
#include "Event.h"
#include "MainScene.h"
#include "Menu.h"
#include "ParticleExamples.h"
#include "Random.h"
#include "RandomRole.h"
#include "Script.h"
#include "SubScene.h"
#include "TeamMenu.h"
#include "TitleScene.h"
#include "UISave.h"
#include "UIShop.h"

TitleScene::TitleScene()
{
    full_window_ = 1;
    menu_ = new Menu();
    menu_->setPosition(400, 250);
    auto b = new Button("title", 3, 23, 23);
    menu_->addChild(b, 20, 0);
    b = new Button("title", 4, 24, 24);
    menu_->addChild(b, 20, 50);
    b = new Button("title", 6, 26, 26);
    menu_->addChild(b, 20, 100);
    menu_load_ = new UISave();
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
    int alpha = 255 - abs(255 - count_ % 510);
    count_++;
    if (alpha == 0)
    {
        RandomDouble r;
        head_id_ = r.rand_int(115);
        head_x_ = r.rand_int(1024 - 150);
        head_y_ = r.rand_int(640 - 150);
    }
    TextureManager::getInstance()->renderTexture("head", head_id_, head_x_, head_y_, { 255, 255, 255, 255 }, alpha);
}

void TitleScene::dealEvent(BP_Event& e)
{
    int r = menu_->run();
    if (r == 0)
    {
        Save::getInstance()->load(0);
        Script::getInstance()->runScript("../game/script/0.lua");
        auto random_role = new RandomRole();
        random_role->setRole(Save::getInstance()->getRole(0));
        if (random_role->runAtPosition(300, 0) == 0)
        {
            MainScene::getIntance()->setManPosition(Save::getInstance()->MainMapX, Save::getInstance()->MainMapY);
            MainScene::getIntance()->forceEnterSubScene(70, 19, 20);
            MainScene::getIntance()->setTowards(1);
            MainScene::getIntance()->run();
        }
    }
    if (r == 1)
    {
        if (menu_load_->run() >= 0)
        {
            //Save::getInstance()->getRole(0)->MagicLevel[0] = 900;    //²âÊÔÓÃ
            //Script::getInstance()->runScript("../game/script/0.lua");
            MainScene::getIntance()->run();
        }
    }
    if (r == 2)
    {
        setExit(true);
    }
}

void TitleScene::onEntrance()
{
    Engine::getInstance()->playVideo("");
    Audio::getInstance()->playMusic(16);
    auto c1 = new ParticleFire();
    addChild(c1);
    c1->setPosition(50, 630);
    c1->init();
    auto c2 = new ParticleFire();
    addChild(c2);
    c2->setPosition(1024 - 50, 630);
    c2->init();
    //c->initWithTotalParticles(100);
}
