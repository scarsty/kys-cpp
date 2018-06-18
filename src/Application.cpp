#include "Application.h"
#include "Audio.h"
#include "Engine.h"
#include "GameUtil.h"
#include "INIReader.h"
#include "Random.h"
#include "TitleScene.h"

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
    engine->init();    //引擎初始化之后才能创建纹理

    engine->createAssistTexture(768, 480);

    auto s = new TitleScene();    //开始界面
    s->run();
    delete s;

    return 0;
}

void Application::config()
{
    INIReaderNormal ini;
    ini.loadFile(GameUtil::configFile());
    Role::setMaxValue(&ini);
    Item::setSpecialItems(&ini);
    Element::setRefreshInterval(ini.getInt("game", "refresh_interval", 25));
    Audio::getInstance()->setVolume(ini.getInt("game", "volume", 50));
    Event::getInstance()->setUseScript(ini.getInt("game", "use_script", 0));
}
