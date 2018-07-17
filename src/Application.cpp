#include "Application.h"
#include "Audio.h"
#include "Engine.h"
#include "Font.h"
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

    TitleScene s;
    s.run();

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
    Font::getInstance()->setStatMessage(ini.getInt("game", "stat_font", 0));
}
