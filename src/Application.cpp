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
#include "SystemSettings.h"
#include "UIKeyConfig.h"

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
    engine->setUISize(1280, 720);
    engine->init(nullptr, 0, 0, renderer_);
    engine->setWindowTitle(title_);
    engine->addEventWatch([](void*, EngineEvent* e) -> bool
        {
            if (e->type == EVENT_QUIT)
            {
                return true;
            }
            if (e->type == EVENT_DID_ENTER_BACKGROUND)
            {
                EngineEvent queued = *e;
                SDL_PeepEvents(&queued, 1, SDL_ADDEVENT, 0, 0);
                return false;
            }
            if (e->type == EVENT_DID_ENTER_FOREGROUND)
            {
                EngineEvent queued = *e;
                SDL_PeepEvents(&queued, 1, SDL_ADDEVENT, 0, 0);
                return false;
            }
            if (e->type == EVENT_TERMINATING)
            {
                Audio::getInstance()->pauseMusic();
                return false;
            }
            return true;
        },
        nullptr);

    //引擎初始化之后才能创建纹理
    engine->createAssistTexture("scene", 1280, 720);

    auto s = std::make_shared<TitleScene>();    //开始界面
    s->run();

    return 0;
}

void Application::config()
{
    auto game = GameUtil::getInstance();
    SystemSettings::getInstance()->load();
    //RunNode::setRefreshInterval(game->getReal("game", "refresh_interval", 16));
    Event::getInstance()->setUseScript(game->getInt("game", "use_script", 1));
    Font::getInstance()->setStatMessage(game->getInt("game", "stat_font", 0));
    TextureManager::getInstance()->setLoadFromPath(game->getInt("game", "png_from_path", 0));
    TextureManager::getInstance()->setLoadAll(game->getInt("game", "load_all_png", 0));
    UIKeyConfig::readFromString(game->getString("game", "key", ""));
    Scene::setKeyWalkDealy(game->getInt("game", "walk_speed", 20));
    RunNode ::setRenderMessage(game->getInt("game", "render_message", 0));
    Engine::initUIStyle();

    renderer_ = game->getString("game", "renderer", "");
    title_ = game->getString("game", "title", "All Heroes in Kam Yung Stories");

    Role::setMaxValue();
    Role::setLevelUpList();
    Item::setSpecialItems();
}
