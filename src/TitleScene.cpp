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
#include "Script.h"
#include "SubScene.h"
#include "TeamMenu.h"
#include "UISave.h"
#include "UIShop.h"

#include "TextBoxRoll.h"
#include "../hanzi2pinyin/Hanz2Piny.h"
#include "PotConv.h"

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
    render_message_ = 1;
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
    Font::getInstance()->draw(GameUtil::VERSION(), 28, 0, 0);
}

void TitleScene::dealEvent(BP_Event& e)
{
    int r = menu_->run();
    if (r == 0)
    {
        Save::getInstance()->load(0);
        //Script::getInstance()->runScript("../game/script/0.lua");
        std::string name = "";
#ifdef _MSC_VER
        auto input = new InputBox("ÕˆÝ”ÈëÐÕÃû£º", 30);
        input->setInputPosition(350, 300);
        input->run();
        if (input->getResult() >= 0)
        {
            name = input->getText();
        }
        delete input;
#else
        name = GameUtil::getInstance()->getString("constant", "name");
#endif
        if (!name.empty())
        {
            auto random_role = new RandomRole();
            random_role->setRole(Save::getInstance()->getRole(0));
            random_role->setRoleName(name);
            if (random_role->runAtPosition(300, 0) == 0)
            {
                MainScene::getInstance()->setManPosition(Save::getInstance()->MainMapX, Save::getInstance()->MainMapY);
                MainScene::getInstance()->setTowards(1);
                MainScene::getInstance()->forceEnterSubScene(GameUtil::getInstance()->getInt("constant", "begin_scene", 70), 19, 20, GameUtil::getInstance()->getInt("constant", "begin_event", -1));
                MainScene::getInstance()->run();
            }
            delete random_role;
        }
    }
    if (r == 1)
    {
        if (menu_load_->run() >= 0)
        {
            //Save::getInstance()->getRole(0)->MagicLevel[0] = 900;    //²âÊÔÓÃ
            //Script::getInstance()->runScript("../game/script/0.lua");
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
    Engine::getInstance()->playVideo("../game/movie/1.mp4");
    Audio::getInstance()->playMusic(16);
    //auto tbr = new TextBoxRoll();

    //TextBoxRoll::TextColorLines texts;
    //for (int i = 0; i <= 10; i++)
    //{
    //    texts.push_back({ { { 0, 0, 0, 255 }, "sb" + std::to_string(250 + i) } });
    //}
    //tbr->setTexts(texts);
    //tbr->setRollLine(5);
    //menu_->addChild(tbr, -100, -100);
}
