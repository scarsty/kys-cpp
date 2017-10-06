#pragma once
#include <vector>
#include "Engine.h"
#include "TextureManager.h"

#define CONNECT(a,b) a##b

//游戏执行和绘制的基础类，凡需要显示画面或者处理事件的，均继承自此
class Element
{
private:
    static std::vector<Element*> root_;   //所有需要绘制的内容都存储在这个静态向量中
    static int prev_present_ticks_;
protected:
    std::vector<Element*> childs_;
    bool visible_ = true;
    int result_ = -1;
    int full_window_ = 0;              //不为0时表示当前画面为起始层，此时低于本层的将不予显示，节省资源
    bool exit_ = true;                 //子类的过程中设此值为true，即表示下一个循环将退出
protected:
    int x_ = 0;
    int y_ = 0;
    int w_ = 0;
    int h_ = 0;
public:
    Element() {}
    virtual ~Element();

    static void drawAll();

    static void addOnRootTop(Element* element) { root_.push_back(element); }
    static Element* removeFromRoot(Element* element);

    void addChild(Element* element);
    void addChild(Element* element, int x, int y);
    Element* getChild(int i) { return childs_[i]; }
    int getChildCount() { return childs_.size(); }
    void removeChild(Element* element);
    void clearChilds();   //不推荐

    void setPosition(int x, int y);
    void setSize(int w, int h) { w_ = w; h_ = h; }

    void getPosition(int& x, int& y) { x = x_; y = y_; }
    void getSize(int& w, int& h) { w = w_; h = h_; }

    bool inSide(int x, int y)
    {
        return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_;
    }

    int run(bool in_root = true);                       //执行本层
    int runAtPosition(int x = 0, int y = 0, bool in_root = true) { setPosition(x, y); return run(in_root); }

    virtual void backRun() {}                           //一直运行，可以放入总计数器
    virtual void draw() {}                              //如何画本层
    virtual void dealEvent(BP_Event& e) {}              //每个循环中处理事件
    virtual void onEntrance() {}                          //进入本层的事件，例如绘制亮屏等
    virtual void onExit() {}                              //离开本层的事件，例如黑屏等

    int getResult() { return result_; }
    void drawSelfAndChilds();

    bool getVisible() { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    enum State
    {
        Normal,
        Pass,
        Press,
    };

    int state_ = Normal;   //状态
    int getState() { return state_; }
    void setState(int s) { state_ = s; }
    void checkStateAndEvent(BP_Event& e);

    void checkEventAndPresent(int max_delay=25, bool check_event = false);

    static void clearEvent(BP_Event& e) { e.type = BP_FIRSTEVENT; }
    static Element* getCurrentTopDraw() { return root_.back(); }

    void setAllChildState(int s);
    //void setChildState(int i, int s);
    //int getChildState(int i);

    int findNextVisibleChild(int i0, int direct);

    void setExit(bool e) { exit_ = e; }
    bool isRunning() { return !exit_; }

    //按下回车或鼠标左键的事件
    virtual void pressedOK() {}
    //按下esc或鼠标右键的事件
    virtual void pressedCancel() {}

    void exitWithResult(int r) { setExit(true); result_ = r; }

    //需要普通退出功能的子节点，请复制这两个过去，如退出的形式不同请自行实现
    //virtual void pressedOK() override { exitWithResult(0); }
    //virtual void pressedCancel() override { exitWithResult(-1); }
};

