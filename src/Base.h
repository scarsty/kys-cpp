#pragma once
#include <vector>
#include "Engine.h"
#include "TextureManager.h"

#define CONNECT(a,b) a##b

//游戏绘制基础类，凡需要显示画面的，均继承自此
class Base
{
public:
    static std::vector<Base*> root_;   //所有需要绘制的内容都存储在这个向量中
    std::vector<Base*> childs_;
    bool visible_ = true;
protected:
    int result_ = -1;
    int full_window_ = 0;              //不为0时表示当前画面为起始层，低于本画面的层将不予显示
    bool loop_ = true;
    //bool auto_erase_ = false;
public:
    Base() {}
    virtual ~Base();

    static void LOG(const char* format, ...);
    template <class T> void safe_delete(T*& pointer)
    {
        if (pointer) { delete pointer; }
        pointer = nullptr;
    }
    template <class T> static void safe_delete(std::vector<T*>& pointer_v)
    {
        for (auto& pointer : pointer_v)
        {
            safe_delete(pointer);
        }
    }
    template <class T> static void safe_delete(std::initializer_list<T**> pointer_v)
    {
        for (auto& pointer : pointer_v)
        {
            safe_delete(*pointer);
        }
    }

    static void drawAll();
    static void push(Base* b) { root_.push_back(b); }
    static Base* pop();
    static Base* getCurentBase() { return root_.at(0); }

    void addChild(Base* b, int x = 0, int y = 0);

    int x_ = 0;
    int y_ = 0;
    int w_ = 0;
    int h_ = 0;
    void setPosition(int x, int y);
    void setSize(int w, int h) { w_ = w; h_ = h; }

    bool inSide(int x, int y)
    {
        return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_;
    }

    int run();                                          //执行本层
    virtual void backRun() {}                           //一直运行，可以放入总计数器
    virtual void draw() {}                              //如何画本层
    virtual void dealEvent(BP_Event& e) {}              //每个循环中处理事件
    virtual void entrance() {}                          //进入本层的事件，例如绘制亮屏等
    virtual void exit() {}                              //离开本层的事件，例如黑屏等

    int getResult() { return result_; }
    void drawSelfAndChilds();
    void setVisible(bool v) { visible_ = v; }

    enum State
    {
        Normal,
        Pass,
        Press,
    };

    State state_ = Normal;   //状态
    State getState() { return state_; }
    void setState(State s) { state_ = s; }
    void checkStateAndEvent(BP_Event &e);

};

