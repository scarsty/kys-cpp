#pragma once
#include <vector>
#include "Engine.h"
#include "TextureManager.h"
#include <functional>

#define CONNECT(a,b) a##b

//游戏执行和绘制的基础类，凡需要显示画面或者处理事件的，均继承自此
class Element
{
private:
    static std::vector<Element*> root_;   //所有需要绘制的内容都存储在这个静态向量中
    static int prev_present_ticks_;
    const int max_delay_ = 25;
protected:
    std::vector<Element*> childs_;
    bool visible_ = true;
    int result_ = -1;
    int full_window_ = 0;              //不为0时表示当前画面为起始层，此时低于本层的将不予显示，节省资源
    bool exit_ = false;                 //子类的过程中设此值为true，即表示下一个循环将退出
    bool running_ = false;

    int stay_frame_ = -1;
    int current_frame_ = 0;
protected:
    int x_ = 0;
    int y_ = 0;
    int w_ = 0;
    int h_ = 0;

    int pass_child_ = -1;
    int press_child_ = -1;

    int tag_;

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
    void clearChilds();

    void setPosition(int x, int y);
    void setSize(int w, int h) { w_ = w; h_ = h; }

    void getPosition(int& x, int& y) { x = x_; y = y_; }
    void getSize(int& w, int& h) { w = w_; h = h_; }

    bool inSide(int x, int y)
    {
        return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_;
    }

    int getResult() { return result_; }
    void setResult(int r) { result_ = r; }
    bool getVisible() { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    //状态
    enum State
    {
        Normal,
        Pass,
        Press,
    };

    int state_ = Normal;
    int getState() { return state_; }
    void setState(int s) { state_ = s; }

    int getTag() { return tag_; }
    void setTag(int t) { tag_ = t; }

    static void clearEvent(BP_Event& e) { e.type = BP_FIRSTEVENT; }
    static Element* getCurrentTopDraw() { return root_.back(); }

    void setAllChildState(int s);
    void setAllChildVisible(bool v);

    int findNextVisibleChild(int i0, int direct);
    int findFristVisibleChild();

    void setExit(bool e) { exit_ = e; }
    bool isRunning() { return running_; }

    void exitWithResult(int r) { setExit(true); result_ = r; }

    int getPassChild() { return pass_child_; }
    void forcePassChild();
    int getPressChild() { return press_child_; }
    void pressToResult() { result_ = press_child_; }

    //通常来说，部分与操作无关的逻辑放入draw和dealEvent都问题不大，但是建议draw中仅有绘图相关的操作

    virtual void backRun() {}                                  //一直运行，可以放入总计数器
    virtual void draw() {}                                     //如何画本节点
    virtual void dealEvent(BP_Event& e) {}                     //每个循环中处理事件，在子节点需要执行动画时可以不被进行
    virtual void dealEvent2(BP_Event& e) {}                    //每个循环中处理事件，任何时候都会被执行，可用于制动
    virtual void onEntrance() {}                               //进入本节点的事件，例如亮屏等
    virtual void onExit() {}                                   //离开本节点的事件，例如黑屏等

    virtual void onPressedOK() {}                             //按下回车或鼠标左键的事件，子类视情况继承或者留空
    virtual void onPressedCancel() {}                         //按下esc或鼠标右键的事件，子类视情况继承或者留空

    void setStayFrame(int s) { stay_frame_ = s; }
    void checkFrame();

    bool isPressOK(BP_Event& e)
    {
        return (e.type == BP_KEYUP && (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE))
            || (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_LEFT);
    }
    bool isPressCancel(BP_Event& e)
    {
        return (e.type == BP_KEYUP && e.key.keysym.sym == BPK_ESCAPE)
            || (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_RIGHT);
    }

private:
    void drawSelfAndChilds();
    void checkStateAndEvent(BP_Event& e);
    void checkEventAndPresent(bool check_event = false);
    void checkChildState();
    void checkSelfState(BP_Event& e);

public:
    int run(bool in_root = true);                       //执行本层
    int runAtPosition(int x = 0, int y = 0, bool in_root = true) { setPosition(x, y); return run(in_root); }
    static void exitAll(int begin = 0);                 //设置从begin开始的全部节点状态为退出
    int drawAndPresent(int times = 1, std::function<void(void*)> func = nullptr, void* data = nullptr);

    template <class T> static T* getPointerFromRoot()
    {
        for (int i = root_.size() - 1; i >= 0; i--)
        {
            T* ptr = dynamic_cast<T*>(root_[i]);
            if (ptr) { return ptr; }
        }
        return nullptr;
    }


    //每个节点应自行定义返回值，
    //需要普通退出功能的子节点，请使用下面两个宏，如退出的形式不同请自行实现
    //注意子类的子类可能会出现继承关系，需视情况处理
#define DEFAULT_OK_EXIT virtual void onPressedOK() override { exitWithResult(0); }
#define DEFAULT_CANCEL_EXIT virtual void onPressedCancel() override { exitWithResult(-1); }
};

