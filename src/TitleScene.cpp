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
#include "SuperMenuText.h"
#include "TeamMenu.h"
#include "UISave.h"
#include "UIShop.h"
#include "OpenCCConverter.h"
#include "SaveFieldTranslate.h"

#include "TextBoxRoll.h"

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
        int iq = 100;
#ifdef _MSC_VER
        InputBox input("請輸入姓名：", 30);
        input.setInputPosition(350, 300);
        input.run();
        if (input.getResult() >= 0)
        {
            name = input.getText();
        }
        
        std::vector<std::pair<int, std::string>> iqs;
        for (int i = 1; i <= 100; i++) {
            iqs.emplace_back(i, std::to_string(i));
        }
        SuperMenuText smt("請輸入資質：", 30, iqs, 10);
		smt.setInputPosition(180, 80);

		auto createIntText = [](Role * role, const std::string& s, 
			BP_Color color1 = { 255,218,185,255 }, BP_Color color2 = { 240,248,255,255 }) 
		{
			std::vector<std::pair<BP_Color, std::string>> line;
			std::string padded = s;
			while (padded.size() < 10) {
				padded += " ";
			}
			padded += " ";
			line.emplace_back(color1, padded);
			line.emplace_back(color2, RoleTranslate::getVal(role, s));
			return line;
		};

		auto drawStatus = [this, createIntText](DrawableOnCall * d) {
			int iq = d->getID();
			if (iq == -1) return;
			int x = 550;
			int y = 100;
			RandomDouble r;
			auto role = Save::getInstance()->getRole(0);
			role->IQ = iq;
			role->MaxHP = 500 - 2 * role->IQ;
			role->HP = role->MaxHP;
			role->MaxMP = 250 + role->IQ * 5;
			role->MP = role->MaxMP;
			role->MPType = r.rand_int(2);
			role->Attack = 20 + std::ceil(role->IQ / 2.0);
			role->Speed = 30;
			role->Defence = 20 + std::floor(role->IQ / 2.0);
			role->Medicine = 40;
			role->UsePoison = 40;
			role->Detoxification = 30;
			role->Fist = 30;
			role->Sword = 30;
			role->Knife = 30;
			role->Unusual = 30;
			role->HiddenWeapon = 30;

			std::vector<std::vector<std::pair<BP_Color, std::string>>> texts;
			texts.push_back(createIntText(role, "生命最大值"));
			texts.push_back(createIntText(role, "内力最大值"));
			texts.push_back(createIntText(role, "攻擊力"));
			texts.push_back(createIntText(role, "防禦力"));
			texts.push_back(createIntText(role, "輕功"));
			texts.push_back({ {BP_Color{255,218,185,255}, "兵器值    " }, {BP_Color{240,248,255,255}, "30" } });
			texts.push_back({});
			texts.push_back({ {BP_Color{255,218,185,255}, "不可修煉秘籍" } });
			for (auto item : Save::getInstance()->getItems()) {
				// 秘籍
				if (item->ItemType == 2) {
					if (!GameUtil::canUseItem(role, item)) {
						texts.push_back({ {BP_Color{240,248,255,255}, item->Name } });
					}
				}
			}

		};

		auto status_ui = new DrawableOnCall(drawStatus);
		smt.addDrawableOnCall(status_ui);
        // TODO 加入Drawable顯示數據
        while (smt.getResult() == -1)
        {
            smt.run();
        }
        iq = smt.getResult();

#else
        name = GameUtil::getInstance()->getString("constant", "name");
        iq = GameUtil::getInstance()->getInt("constant", "iq", 100);
#endif
        if (!name.empty())
        {
            RandomRole random_role;
            auto role = Save::getInstance()->getRole(0);
            role->IQ = iq;
            random_role.setRole(role);
            random_role.setRoleName(name);
            if (random_role.runAtPosition(300, 0) == 0)
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
            //Save::getInstance()->getRole(0)->MagicLevel[0] = 900;    //测试用
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
    auto tbr = new TextBoxRoll();

    TextBoxRoll::TextColorLines texts;
    for (int i = 0; i <= 10; i++)
    {
        texts.push_back({ { { 0, 0, 0, 255 },"sb" + std::to_string(250 + i) } });
    }
    tbr->setTexts(texts);
    tbr->setRollLine(5);
    menu_->addChild(tbr, -100, -100);
}
