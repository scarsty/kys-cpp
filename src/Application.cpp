﻿#include "Application.h"
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
    auto engine = Engine::getInstance();
    engine->setStartWindowSize(1024, 640);
    engine->init();    //引擎初始化之后才能创建纹理
    engine->createAssistTexture(768, 480);

    config();
    TitleScene s;      //开始界面
    s.run();

    return 0;
}

void Application::config()
{
    auto game = GameUtil::getInstance();
    Element::setRefreshInterval(game->getInt("game", "refresh_interval", 25));
    Audio::getInstance()->setVolume(game->getInt("music", "volume", 50));
    Event::getInstance()->setUseScript(game->getInt("game", "use_script", 0));
    Font::getInstance()->setStatMessage(game->getInt("game", "stat_font", 0));
    Engine::getInstance()->setWindowTitle(game->getString("game", "title", "All Heroes in Kam Yung Stories"));
}
