#include "Application.h"
#include "Audio.h"
#include "Engine.h"
#include "Event.h"
#include "Font.h"
#include "GameUtil.h"
#include "INIReader.h"
#include "TextureManager.h"
#include "TitleScene.h"
#include "Types.h"
#include "UIKeyConfig.h"

Application::Application()
{
}

Application::~Application()
{
}

int Application::run()
{
    auto engine = Engine::getInstance();
    engine->setStartWindowSize(1280, 720);
    engine->init();
    //引擎初始化之后才能创建纹理
    engine->createAssistTexture(800, 450);

    config();

    auto s = std::make_shared<TitleScene>();    //开始界面
    s->run();

    return 0;
}

void Application::config()
{
    auto game = GameUtil::getInstance();
    //RunNode::setRefreshInterval(game->getReal("game", "refresh_interval", 16));
    Audio::getInstance()->setVolume(game->getInt("music", "volume", 50));
    Audio::getInstance()->setVolumeWav(game->getInt("music", "volumewav", 50));
    Event::getInstance()->setUseScript(game->getInt("game", "use_script", 0));
    Font::getInstance()->setStatMessage(game->getInt("game", "stat_font", 0));
    Font::getInstance()->setSimplified(game->getInt("game", "simplified_chinese", 1));
    Engine::getInstance()->setWindowTitle(game->getString("game", "title", "All Heroes in Kam Yung Stories"));
    TextureManager::getInstance()->setLoadFromPath(game->getInt("game", "png_from_path", 0));
    TextureManager::getInstance()->setLoadAll(game->getInt("game", "load_all_png", 0));
    UIKeyConfig::readFromString(game->getString("game", "key", ""));
    Scene::setKeyWalkDealy(game->getInt("game", "walk_speed", 20));

    Role::setMaxValue();
    Role::setLevelUpList();
    Item::setSpecialItems();
}
