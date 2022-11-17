#include "TitleScene.h"
#include "Audio.h"
#include "BattleScene.h"
#include "Button.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "INIReader.h"
#include "InputBox.h"
#include "MainScene.h"
#include "Menu.h"
#include "Random.h"
#include "RandomRole.h"
#include "RunNodeFromJson.h"
#include "Script.h"
#include "SubScene.h"
#include "TeamMenu.h"
#include "UISave.h"
#include "UIShop.h"

#include "../others/Hanz2Piny.h"
#include "PotConv.h"

#include "TextBoxRoll.h"
#include "ZipFile.h"

TitleScene::TitleScene()
{
    full_window_ = 1;
    menu_ = std::make_shared<Menu>();
    menu_->setPosition(560, 600);
    menu_->addChild<Button>(-180, 0)->setTexture("title", 3, 23, 23);
    menu_->addChild<Button>(20, 0)->setTexture("title", 4, 24, 24);
    menu_->addChild<Button>(220, 0)->setTexture("title", 6, 26, 26);
    menu_load_ = std::make_shared<UISave>();
    menu_load_->setPosition(500, 300);
    //render_message_ = 1;
    pe_ = std::make_shared<ParticleExample>();
    pe_->setStyle(ParticleExample::FIRE);
    pe_->setPosition(490, 80);
    pe_->setSize(20, 20);
    addChild(pe_);

    //Save::getInstance()->load(5);
    //RandomDouble rand;
    //int k = rand.rand() * 139;
    //k = 100;
    //Event::getInstance()->tryBattle(k, 0);

    //auto  p=std::make_shared<RunNodeFromJson>("../game/Scene.json");
    //addChild(p);
}

TitleScene::~TitleScene()
{
}

void TitleScene::draw()
{
    int count = count_ / 20;
    TextureManager::getInstance()->renderTexture("title", 153, 0, 0);
    int alpha = 255 - abs(255 - count_ % 510);
    count_++;
    if (alpha == 0)
    {
        RandomDouble r;
        head_id_ = r.rand_int(115);
        head_x_ = r.rand_int(1280 - 150);
        head_y_ = r.rand_int(800 - 150);
    }
    TextureManager::getInstance()->renderTexture("head", head_id_, head_x_, head_y_, { 255, 255, 255, 255 }, alpha);
    //TextureManager::getInstance()->renderTexture("title", 150, 240, 150, { 255,255,255,255 }, 255, 0.3, 0.3);
    Font::getInstance()->draw(GameUtil::VERSION(), 28, 0, 0);
}

void TitleScene::dealEvent(BP_Event& e)
{
    int r = menu_->run();
    if (r == 0)
    {
        Engine::getInstance()->gameControllerRumble(50 , 50, 500);
        Save::getInstance()->load(0);
        //Script::getInstance()->runScript(GameUtil::PATH()+"script/0.lua");
        std::string name = "";
        auto input = std::make_shared<InputBox>("請輸入姓名：", 30);
        input->setInputPosition(350, 300);
        input->run();
        if (input->getResult() >= 0)
        {
            name = input->getText();
        }
        if (!name.empty())
        {
            auto random_role = std::make_shared<RandomRole>();
            random_role->setRole(Save::getInstance()->getRole(0));
            random_role->setRoleName(name);
            if (random_role->runAtPosition(300, 0) == 0)
            {
                MainScene::getInstance()->setManPosition(Save::getInstance()->MainMapX, Save::getInstance()->MainMapY);
                MainScene::getInstance()->setTowards(1);
                int s = GameUtil::getInstance()->getInt("constant", "begin_scene", 70);
                int x = GameUtil::getInstance()->getInt("constant", "begin_sx", 19);
                int y = GameUtil::getInstance()->getInt("constant", "begin_sy", 20);
                MainScene::getInstance()->forceEnterSubScene(s, x, y, GameUtil::getInstance()->getInt("constant", "begin_event", -1));
                MainScene::getInstance()->run();
            }
        }
    }
    if (r == 1)
    {
        if (menu_load_->run() >= 0)
        {
            //Save::getInstance()->getRole(0)->MagicLevel[0] = 900;    //测试用
            //Script::getInstance()->runScript(GameUtil::PATH()+"script/0.lua");
            MainScene::getInstance()->run();
        }
    }
    if (r == 2)
    {
        setExit(true);
    }
}

void TitleScene::onEntrance()
{
    Engine::getInstance()->playVideo(GameUtil::PATH() + "movie/1.mp4");
    Audio::getInstance()->playMusic(16);
}
