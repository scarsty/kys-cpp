#include "Application.h"
#include "TitleScene.h"
#include "Engine.h"
#include "Random.h"
#include "Option.h"
#include "Audio.h"

Application::Application()
{
}

Application::~Application()
{
}

int Application::run()
{
    config();
    auto engine = Engine::getInstance();
    engine->setStartWindowSize(1024, 640);
    engine->init();                       //引擎初始化之后才能创建纹理

    engine->createAssistTexture(768, 480);

    auto s = new TitleScene();            //开始界面
    s->run();
    delete s;

    return 0;
}

void Application::config()
{
    RandomClassical::srand();
    auto option = Option::getInstance();
    option->loadIniFile("../game/config/kysmod.ini");
    option->loadSaveValues();
    Element::setRefreshInterval(option->getInt("game", "refresh_interval", 25));
    Audio::getInstance()->setVolume(option->getInt("game", "volume", 50));
    Event::getInstance()->setUseScript(option->getInt("game", "use_script", 0));
}

