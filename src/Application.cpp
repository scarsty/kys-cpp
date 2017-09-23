#pragma once
#include "Application.h"
#include "TitleScene.h"

Application::Application()
{
}

Application::~Application()
{
}

/*
* 简要的函数说明文字 
*  @param [in] param1 参数1说明
*  @param [out] param2 参数2说明
*  @return 返回值说明
*/


/**
*  主程序首先进入场景的入口
*  @param [in] viod
*  @return True
*/
int Application::launch()
{
    auto engine = Engine::getInstance();
    engine->init();
    BP_Event e;  ///< 事件结构对象
    auto s = new TitleScene(); //开始界面
    s->run();
    return 0;
}

/**
*  主程序的绘制逻辑
*  @param [in] 事件结构
*  @return void
*/
void Application::mainLoop(BP_Event& e)
{

}
