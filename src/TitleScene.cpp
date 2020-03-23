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

#include "../others/Hanz2Piny.h"
#include "PotConv.h"
#include "TextBoxRoll.h"
#include "ZipFile.h"

TitleScene::TitleScene()
{
    full_window_ = 1;
    menu_ = std::make_shared<Menu>();
    menu_->setPositionRate(0.22, 0.65);
	menu_->addChild<Button>(0, 20, 180, 150)->setTexture("background", 107, 107, 131);
	menu_->addChild<Button>(200, 20, 180, 150)->setTexture("background", 108, 108, 132);
    menu_->addChild<Button>(400, 20, 180, 150)->setTexture("background", 109, 109, 133);
    menu_load_ = std::make_shared<UISave>();
    menu_load_->setPosition(0.25, 0.55);
    render_message_ = 1;
}

TitleScene::~TitleScene()
{
}

void TitleScene::draw()
{
    int count = count_ / 20;
	int w_width = Engine::getInstance()->getWindowWidth();
	int w_hight = Engine::getInstance()->getWindowHeight();
	TextureManager::getInstance()->renderTexture("background", 0, 0, 0, w_width, w_hight);
    int alpha = 255 - abs(255 - count_ % 510);
    count_++;
    if (alpha == 0)
    {
        RandomDouble r;
        head_id_ = r.rand_int(115);
        head_x_ = r.rand_int(w_width - 150);
        head_y_ = r.rand_int(w_hight - 150);
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
        auto input = std::make_shared<InputBox>("ÕˆÝ”ÈëÐÕÃû£º", 30);
        input->setInputPosition(350, 300);
        input->run();
        if (input->getResult() >= 0)
        {
            name = input->getText();
        }
#else
        name = GameUtil::getInstance()->getString("constant", "name");
#endif
        if (!name.empty())
        {
            auto random_role = std::make_shared<RandomRole>();
            random_role->setRole(Save::getInstance()->getRole(0));
            random_role->setRoleName(name);
            if (random_role->runAtPosition(300, 0) == 0)
            {
                MainScene::getInstance()->setManPosition(Save::getInstance()->MainMapX, Save::getInstance()->MainMapY);
                MainScene::getInstance()->setTowards(1);
                MainScene::getInstance()->forceEnterSubScene(GameUtil::getInstance()->getInt("constant", "begin_scene", 70), 19, 20, GameUtil::getInstance()->getInt("constant", "begin_event", -1));
                MainScene::getInstance()->run();
            }
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
}
