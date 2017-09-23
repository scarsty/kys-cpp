#pragma once
#include <vector>
#include "Engine.h"
#include "TextureManager.h"

#define CONNECT(a,b) a##b

struct auto_ptr
{

};

//游戏绘制基础类，凡需要显示画面的，均继承自此
class Base
{
public:
    static std::vector<Base*> base_vector_;   //所有需要绘制的内容都存储在
    bool visible_ = true;
public:
    Base();
    virtual ~Base();

    virtual void backRun() {}
    virtual void draw() {}
    virtual void dealEvent(BP_Event& e) {}
    virtual void init() {}

    void LOG(const char* format, ...);
    template <class T> void safe_delete(T*& pointer)
    {
        if (pointer)
        { delete pointer; }
        pointer = nullptr;
    }

    static void drawAll();
    static void push(Base* b) { base_vector_.push_back(b); b->init(); }
    static Base* pop();
    static Base* getCurentBase() { return base_vector_.at(0); }

    int x_ = 0;
    int y_ = 0;
    int w_ = 0;
    int h_ = 0;
    void setPosition(int x, int y) { x_ = x; y_ = y; }
    void setSize(int w, int h) { w_ = w; h_ = h; }
    bool inSide(int x, int y);

    int full_window_ = 0;  //不为0时表示当前画面为起始层，低于本画面的层将不予显示

    bool loop_ = true;
    void run();

};

