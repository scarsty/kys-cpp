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
    int w, h;
    Engine::getInstance()->getWindowSize(w, h);
    menu_.setStrings({ "重新開始", "載入進度", "離開游戲" });

    menu_.arrange(0, 0, 0, 40);
    int fontSize = 24;
    menu_.setPosition(w / 2 - 2 * fontSize, h / 2 - fontSize);
    menu_.setFontSize(fontSize);

    menu_load_.setPosition(500, 300);
    render_message_ = 1;
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
    int r = menu_.run();
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
#else
        name = GameUtil::getInstance()->getString("constant", "name");
#endif

        if (name.empty()) {
            return;
        }

        int initX = 230;
        int initY = 90;

        std::vector<std::pair<int, std::string>> iqs;
        for (int i = 1; i <= 100; i++) {
            iqs.emplace_back(i, std::to_string(i));
        }
        SuperMenuText smt("請輸入資質：", 30, iqs, 10);
        smt.setInputPosition(initX, initY);

        auto createIntText = [](Role * role, const std::string& s,
            BP_Color color1, BP_Color color2)
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

        // 這個有點奇怪，不過先這樣吧
        auto drawStatus = [this, createIntText, initX, initY](DrawableOnCall * d) {
            int iq = d->getID();
            if (iq == -1) return;
            int x = initX + 270;
            int y = initY + 20;
            BP_Rect rect{ x - 25, y - 25, 280, 400 };
            TextureManager::getInstance()->renderTexture("title", 17, rect, { 255, 255, 255, 255 }, 255);
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
            BP_Color c1{ 0,0,0,255 };
            BP_Color c2{ 220, 20, 60, 255 };  // crimson
            texts.push_back(createIntText(role, "生命最大值", c1, c2));
            texts.push_back(createIntText(role, "內力最大值", c1, c2));
            texts.push_back(createIntText(role, "攻擊力", c1, c2));
            texts.push_back(createIntText(role, "防禦力", c1, c2));
            texts.push_back(createIntText(role, "輕功", c1, c2));
            texts.push_back({ { c1, "兵器值     " }, {c2, "30" } });
            texts.push_back({});
            texts.push_back({ {c1, "不可修煉秘籍" } });
            for (auto item : Save::getInstance()->getItems()) {
                // 秘籍
                if (item->ItemType == 2) {
                    if (!GameUtil::attributeTest(role->IQ, item->NeedIQ)) {
                        texts.push_back({ {c2, item->Name } });
                    }
                }
            }
            TextBoxRoll tbr;
            tbr.setTexts(texts);
            tbr.setPosition(x, y);
            tbr.draw();
        };

        auto statusDraw = new DrawableOnCall(drawStatus);
        smt.addDrawableOnCall(statusDraw);
        // TODO 加入Drawable顯示數據
        while (smt.getResult() == -1)
        {
            smt.run();
        }
        // iq = smt.getResult();

        int x = initX + 270;
        int y = initY + 20;

        auto drawUnusable = [createIntText, x, y](DrawableOnCall * d) {
            BP_Rect rect{ x - 28, y - 25, 400, 400 };
            TextureManager::getInstance()->renderTexture("title", 17, rect, { 255, 255, 255, 255 }, 255);
        };

        auto update = [](DrawableOnCall * d) {
            int nlType = d->getID();
            if (nlType == -1) return;
            auto role = Save::getInstance()->getRole(0);
            role->MPType = nlType;
            // 瞎搞之
            TextBoxRoll* tbr1 = dynamic_cast<TextBoxRoll*>(d->getChild(0));
            TextBoxRoll* tbr2 = dynamic_cast<TextBoxRoll*>(d->getChild(1));
            if (!tbr1 || !tbr2) return;
            std::vector<std::vector<std::pair<BP_Color, std::string>>> texts1;
            std::vector<std::vector<std::pair<BP_Color, std::string>>> texts2;
            BP_Color c1{ 0,0,0,255 };
            BP_Color c2{ 220, 20, 60, 255 };  // crimson
            texts1.push_back({ {c1, "不可修煉秘籍" } });
            texts2.push_back({ {c1, "" } });
            for (auto item : Save::getInstance()->getItems()) {
                // 秘籍
                if (item->ItemType == 2) {
                    if (!GameUtil::mpTypeTest(role->MPType, item->NeedMPType)) {
                        if (texts1.size() < 17) {
                            texts1.push_back({ {c2, item->Name } });
                        }
                        else {
                            texts2.push_back({ {c2, item->Name } });
                        }
                    }
                }
            }
            tbr1->setTexts(texts1);
            tbr2->setTexts(texts2);
        };

        std::vector<std::pair<int, std::string>> nlTypes;
        nlTypes.emplace_back(0, "陰性内力");
        nlTypes.emplace_back(1, "陽性内力");
        SuperMenuText smtNlType("内力屬性：", 30, nlTypes, 10);
        smtNlType.setInputPosition(initX, initY);
        auto unusableDraw = new DrawableOnCall(drawUnusable);
        auto tbr1 = new TextBoxRoll();
        tbr1->setPosition(x, y);
        auto tbr2 = new TextBoxRoll();
        tbr2->setPosition(x + 120, y);
        unusableDraw->addChild(tbr1);
        unusableDraw->addChild(tbr2);
        unusableDraw->setPostUpdate(update);
        smtNlType.addDrawableOnCall(unusableDraw);
        while (smtNlType.getResult() == -1)
        {
            smtNlType.run();
        }

        RandomRole status;
        status.setRole(Save::getInstance()->getRole(0));
        status.setRoleName(name);
        status.runAtPosition(300, 0);

        MainScene::getInstance()->setManPosition(Save::getInstance()->MainMapX, Save::getInstance()->MainMapY);
        MainScene::getInstance()->setTowards(1);
        MainScene::getInstance()->forceEnterSubScene(GameUtil::getInstance()->getInt("constant", "begin_scene", 70), 19, 20, GameUtil::getInstance()->getInt("constant", "begin_event", -1));
        MainScene::getInstance()->run();

    }
    if (r == 1)
    {
        if (menu_load_.run() >= 0)
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
    /*
    auto tbr = new TextBoxRoll();
    TextBoxRoll::TextColorLines texts;
    for (int i = 0; i <= 10; i++)
    {
        texts.push_back({ { { 0, 0, 0, 255 },"sb" + std::to_string(250 + i) } });
    }
    tbr->setTexts(texts);
    tbr->setRollLine(5);
    menu_->addChild(tbr, -100, -100);
    */
}
