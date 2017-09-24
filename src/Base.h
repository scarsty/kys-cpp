#pragma once
#include <vector>
#include "Engine.h"
#include "TextureManager.h"

#define CONNECT(a,b) a##b

//游戏绘制基础类，凡需要显示画面的，均继承自此
class Base
{
public:
    static std::vector<Base*> base_need_draw_;   //所有需要绘制的内容都存储在这个向量中
    bool visible_ = true;
public:
    Base() {}
    virtual ~Base() {}

    void LOG(const char* format, ...);
    template <class T> void safe_delete(T*& pointer)
    {
        if (pointer)
        { delete pointer; }
        pointer = nullptr;
    }

    static void drawAll();
    static void push(Base* b) { base_need_draw_.push_back(b); }
    static Base* pop();
    static Base* getCurentBase() { return base_need_draw_.at(0); }

    int x_ = 0;
    int y_ = 0;
    int w_ = 0;
    int h_ = 0;
    void setPosition(int x, int y) { x_ = x; y_ = y; }
    void setSize(int w, int h) { w_ = w; h_ = h; }
    bool inSide(int x, int y);

    int full_window_ = 0;                               //不为0时表示当前画面为起始层，低于本画面的层将不予显示

    bool loop_ = true;
    void run();                                         //执行本层

    virtual void backRun() {}                           //一直运行，可以放入总计数器
    virtual void draw() {}                              //如何画本层
    virtual void dealEvent(BP_Event& e) {}              //每个循环中处理事件
    virtual void entrance() {}                          //进入本层的事件，例如绘制亮屏等
    virtual void exit() {}                              //离开本层的事件，例如黑屏等
    virtual void init() {}                              //初始化本层，例如放入一些指针，初值的设定

};

