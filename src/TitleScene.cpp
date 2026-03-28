#include "TitleScene.h"
#include "Audio.h"
#include "BattleScene.h"
#include "Button.h"
#include "Font.h"
#include "GameUtil.h"
#include "INIReader.h"
#include "InputBox.h"
#include "MainScene.h"
#include "Menu.h"
#include "Random.h"
#include "RandomRole.h"
#include "ScenePreloader.h"
#include "SubScene.h"
#include "UISave.h"
#include "DrawableOnCall.h"
#include "Video.h"
#include "Weather.h"
#include "ChessModHook.h"
#include "filefunc.h"
#include <cstdlib>

#ifdef __EMSCRIPTEN__
extern "C" void notify_fonts_loaded();
#endif

TitleScene::TitleScene()
{
    full_window_ = 1;
    battle_mode_ = GameUtil::getInstance()->getInt("game", "battle_mode");
    // Text-only menu: 3 items, each 4 CJK chars at size 28 = 112px wide
    // Spacing 172px (112 + 60 gap), total span 456px, centered: x = (1280-456)/2 = 412
    auto mt = std::make_shared<MenuText>();
    mt->setFontSize(36);
    mt->setHaveBox(true);
    mt->setStrings({"重新開始", "載入進度", "離開遊戲"});
    mt->setPosition(412, 500);
    mt->arrange(0, 0, 172, 0);
    for (auto& c : mt->getChilds())
    {
        auto* btn = dynamic_cast<Button*>(c.get());
        if (btn)
        {
            btn->setTextOnly(true);
            btn->setHaveBox(false);
            btn->setTextColor({ 200, 50, 50, 255 });
        }
    }
    menu_ = mt;
    menu_load_ = std::make_shared<UISave>();
    menu_load_->setPosition(500, 500);
    //render_message_ = 1;

    if (battle_mode_ == 2)
    {
        // auto pe1 = std::make_shared<ParticleExample>();
        // pe1->setStyle(ParticleExample::FIRE);
        // addChild(pe1);
        // pe1->setPosition(490, 80);
        // pe1->setSize(20, 20);
    }
    else if (battle_mode_ == 3)
    {
        auto pe1 = std::make_shared<ParticleExample>();
        pe1->setPosition(Engine::getInstance()->getWindowWidth() * 0.15, 0);
        pe1->setStyle(ParticleExample::RAIN);    //先设置位置，再设置样式，有var的问题
        pe1->setTotalParticles(2000);
        pe1->setPosVar({ float(Engine::getInstance()->getWindowWidth() * 0.85), -50 });
        pe1->resetSystem();
        pe1->setEmissionRate(200);
        pe1->setGravity({ 200, 50 });
        addChild(pe1);
    }
    //调试用代码
    //Save::getInstance()->load(5);
    RandomDouble rand;
    int k = rand.rand() * 139;
    k = 1;
    //Event::getInstance()->tryBattle(k, 0);

    //auto  p=std::make_shared<RunNodeFromJson>("../game/Scene.json");
    //addChild(p);
}

TitleScene::~TitleScene()
{
}

void TitleScene::draw()
{
    auto engine = Engine::getInstance();
    int w = engine->getUIWidth();
    int h = engine->getUIHeight();
    engine->fillColor({ 0, 0, 0, 255 }, 0, 0, w, h);

    // Chess piece (king) in ASCII art — each char is 8px wide at size 16
    const char* chess[] = {
        "    +    ",
        "   +++   ",
        "    +    ",
        "  +---+  ",
        " | . . | ",
        " |  .  | ",
        " | . . | ",
        "  +---+  ",
        " /     \\ ",
        "/       \\",
        "+-------+",
        "|       |",
        "+-------+",
        " \\_____/ ",
        "/_______\\",
    };

    int chessSize = 16;
    int chessW = 9 * (chessSize / 2);    // 9 chars wide, ASCII = half size
    int chessX = (w - chessW) / 2;
    int chessY = 120;
    Color gold = { 200, 180, 100, 255 };
    for (int i = 0; i < 15; i++)
    {
        Font::getInstance()->draw(chess[i], chessSize, chessX, chessY + i * chessSize, gold, 255);
    }

    // Title: "金群自走棋" in red, centered
    int titleSize = 48;
    int titleW = 5 * titleSize;    // 5 CJK chars, each = titleSize width
    int titleX = (w - titleW) / 2;
    int titleY = chessY + 15 * chessSize + 30;
    Font::getInstance()->draw("金群自走棋", titleSize, titleX, titleY, { 220, 40, 40, 255 }, 255);

    // Version string below title
    Font::getInstance()->draw(GameUtil::VERSION(), 20, titleX, titleY + titleSize + 16, { 150, 150, 150, 255 }, 255);

#ifdef __EMSCRIPTEN__
    Font::getInstance()->draw("推荐使用 Edge / Firefox / Chrome / Opera 浏览器", 16, 10, 10, { 180, 180, 180, 180 }, 255);
    if (!overlay_dismissed_)
    {
        overlay_dismissed_ = true;
        notify_fonts_loaded();
    }
#endif
}

void TitleScene::dealEvent(EngineEvent& e)
{
    int r = menu_->run();
    if (r == 0)
    {
        Engine::getInstance()->gameControllerRumble(50, 50, 500);
        Save::getInstance()->load(0);
        {
            MainScene::getInstance()->setManPosition(Save::getInstance()->MainMapX, Save::getInstance()->MainMapY);
            MainScene::getInstance()->setTowards(1);
            int s = 0, x = 0, y = 0, ev = -1;
            KysChess::ChessModHook::overrideNewGame(s, x, y, ev);
            MainScene::getInstance()->forceEnterSubScene(s, x, y, ev);
            ScenePreloader::showPromptAndPreload("加載中...", []() {
                ScenePreloader::preloadSubSceneAssets(53);
            });
            MainScene::getInstance()->run();
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
        exit(0);    //强制退出，否则在Android下可能退不完全
    }
}

void TitleScene::onEntrance()
{
    Video v(Engine::getInstance()->getWindow());
    v.playVideo(GameUtil::PATH() + "movie/1.mp4");
    Audio::getInstance()->playMusic(16);

#ifdef __EMSCRIPTEN__
    bool hasAutoSave = Save::getInstance()->checkSaveFileExist(static_cast<int>(UISave::Slot::Auto));
    if (hasAutoSave)
    {
        auto prompt = std::make_shared<MenuText>(std::vector<std::string>{"  是  ", "  否  "});
        prompt->setFontSize(32);
        prompt->arrange(0, 0, 200, 0);
        prompt->setPosition(440, 400);
        auto label = std::make_shared<TextBox>();
        label->setText("檢測到自動存檔，是否繼續？");
        label->setFontSize(32);
        label->setHaveBox(false);
        label->setTextColor({255, 220, 80});
        prompt->addChild(label, -200, -60);
        auto bg = std::make_shared<DrawableOnCall>([](DrawableOnCall*) {
            Engine::getInstance()->fillColor({0, 0, 0, 180}, 200, 300, 880, 180);
        });
        RunNode::addIntoDrawTop(bg);
        int r = prompt->run();
        RunNode::removeFromDraw(bg);
        if (r == 0)
        {
            if (UISave::loadAuto())
            {
                MainScene::getInstance()->run();
                return;
            }
        }
    }
#endif
}
