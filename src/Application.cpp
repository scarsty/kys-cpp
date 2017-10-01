#include "Application.h"
#include "TitleScene.h"
#include "Engine.h"

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
    engine->init();
    //初始化之后才能创建纹理
    engine->createAssistTexture(768, 480);

    auto s = new TitleScene(); //开始界面
    s->run();
    return 0;
}

