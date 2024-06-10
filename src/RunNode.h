#pragma once
#include "Engine.h"
#include <functional>
#include <memory>
#include <vector>

#define CONNECT(a, b) a##b

//游戏执行和绘制的基础类，凡需要显示画面或者处理事件的，均继承自此
//为了避免纠结，子类全部使用共享指针
class RunNode : public std::enable_shared_from_this<RunNode>
{
private:
    static std::vector<std::shared_ptr<RunNode>> root_;    //所有需要绘制的内容都存储在这个静态向量中
    static double global_prev_present_ticks_;
    static double refresh_interval_;

private:
    bool is_private_ = false;
    double prev_present_ticks_ = 0;

protected:
    std::vector<std::shared_ptr<RunNode>> childs_;
    bool visible_ = true;
    int result_ = -1;
    int full_window_ = 0;    //不为0时表示当前画面为起始层，此时低于本层的将不予显示，节省资源
    bool exit_ = false;      //子类的过程中设此值为true，即表示下一个循环将退出
    bool running_ = false;

    int stay_frame_ = -1;
    int current_frame_ = 0;

protected:
    int x_ = 0;
    int y_ = 0;
    int w_ = 0;
    int h_ = 0;

    int active_child_ = -1;

    int tag_;

    static int render_message_;

    int deal_event_ = 1;

public:
    RunNode() {}
    virtual ~RunNode();

    static void setRefreshInterval(double i) { refresh_interval_ = i; }
    static double getRefreshInterval() { return refresh_interval_; }

    static int getShowTimes() { return global_prev_present_ticks_ / refresh_interval_; }

    static void drawAll();

    static void addIntoDrawTop(std::shared_ptr<RunNode> element) { root_.push_back(element); }
    static std::shared_ptr<RunNode> removeFromDraw(std::shared_ptr<RunNode> element);

    void addChild(std::shared_ptr<RunNode> element);
    void addChild(std::shared_ptr<RunNode> element, int x, int y);
    template <typename T>
    std::shared_ptr<T> addChild()
    {
        auto p = std::make_shared<T>();
        addChild(p);
        return p;
    }
    template <typename T>
    std::shared_ptr<T> addChild(int x, int y)
    {
        auto p = std::make_shared<T>();
        addChild(p, x, y);
        return p;
    }

    std::shared_ptr<RunNode> getChild(int i) { return childs_[i]; }
    int getChildCount() { return childs_.size(); }
    void removeChild(std::shared_ptr<RunNode> element);
    void clearChilds();

    void setPosition(int x, int y);
    void setSize(int w, int h)
    {
        w_ = w;
        h_ = h;
    }

    void getPosition(int& x, int& y)
    {
        x = x_;
        y = y_;
    }
    void getSize(int& w, int& h)
    {
        w = w_;
        h = h_;
    }

    bool inSide(int x, int y)
    {
        int w, h;
        int w1, h1;
        Engine::getInstance()->getWindowSize(w, h);
        Engine::getInstance()->getStartWindowSize(w1, h1);
        x = x * w1 / w;
        y = y * h1 / h;
        return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_;
    }

    int getResult() { return result_; }
    void setResult(int r) { result_ = r; }
    bool getVisible() { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    void setDealEvent(int d) { deal_event_ = d; }

    //状态
    enum State
    {
        NodeNormal,
        NodePass,
        NodePress,
    };

    enum Direct
    {
        DIrectNone,
        DirectLeft,
        DirectUp,
        DirectRight,
        DirectDown,
    };

    int state_ = NodeNormal;
    int getState() { return state_; }
    void setState(int s) { state_ = s; }

    int getTag() { return tag_; }
    void setTag(int t) { tag_ = t; }

    //static void clearEvent(BP_Event& e) { e.type = BP_FIRSTEVENT; }
    static std::shared_ptr<RunNode> getCurrentTopDraw() { return root_.back(); }

    void setAllChildState(int s);
    void setAllChildVisible(bool v);

    int findNextVisibleChild(int i0, Direct direct);
    int findFristVisibleChild();

    void setExit(bool e) { exit_ = e; }
    bool isRunning() { return running_; }

    void exitWithResult(int r)
    {
        setExit(true);
        result_ = r;
    }

    int getActiveChildIndex() { return active_child_; }
    void forceActiveChild();
    void forceActiveChild(int index)
    {
        active_child_ = index;
        forceActiveChild();
    }
    void checkActiveToResult();

    //通常来说，部分与操作无关的逻辑放入draw和dealEvent都问题不大，但是建议draw中仅有绘图相关的操作

    virtual void backRun() {}                  //节点在root中就运行，可以放入总计数器
    virtual void draw() {}                     //如何画本节点
    virtual void dealEvent(BP_Event& e) {}     //处理事件，会一直执行，相当于主循环体
    virtual void dealEvent2(BP_Event& e) {}    //处理事件，执行模式和动画模式都会被执行，可用于制动
    virtual void onEntrance() {}               //进入本节点的事件，例如亮屏等
    virtual void onExit() {}                   //离开本节点的事件，例如黑屏等

    virtual void onPressedOK() {}        //按下回车或鼠标左键的事件，子类视情况继承或者留空
    virtual void onPressedCancel() {}    //按下esc或鼠标右键的事件，子类视情况继承或者留空

    void setStayFrame(int s) { stay_frame_ = s; }
    void checkFrame();

    bool isPressOK(BP_Event& e)
    {
        return (e.type == BP_KEYUP && (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE))
            || (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_LEFT)
            || (e.type == BP_CONTROLLERBUTTONUP && e.cbutton.button == BP_CONTROLLER_BUTTON_A);
    }
    bool isPressCancel(BP_Event& e)
    {
        return (e.type == BP_KEYUP && e.key.keysym.sym == BPK_ESCAPE)
            || (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_RIGHT)
            || (e.type == BP_CONTROLLERBUTTONUP && e.cbutton.button == BP_CONTROLLER_BUTTON_B);
    }

private:
    void drawSelfChilds();
    void checkStateSelfChilds(BP_Event& e, bool check_event = false);
    void backRunSelfChilds();
    void dealEventSelfChilds(bool check_event = false);

    bool isSpecialEvent(BP_Event& e);    //是否为游戏需要处理的类型
    int checkChildState();
    void checkSelfState(BP_Event& e);
    static void present();

public:
    int run(bool in_root = true);    //执行本层
    int runAtPosition(int x = 0, int y = 0, bool in_root = true)
    {
        setPosition(x, y);
        return run(in_root);
    }
    static void exitAll(int begin = 0);    //设置从begin开始的全部节点状态为退出
    int drawAndPresent(int times = 1, std::function<void(void*)> func = nullptr, void* data = nullptr);

    template <class T>
    static std::shared_ptr<T> getPointerFromRoot()
    {
        for (int i = root_.size() - 1; i >= 0; i--)
        {
            std::shared_ptr<T> ptr = std::dynamic_pointer_cast<T>(root_[i]);
            if (ptr) { return ptr; }
        }
        return nullptr;
    }

    bool checkPrevTimeElapsed(int64_t ms)
    {
        auto t = Engine::getTicks();
        if (t - prev_present_ticks_ >= ms)
        {
            prev_present_ticks_ = t;
            return true;
        }
        return false;
    }

    //每个节点应自行定义返回值，
    //需要普通退出功能的子节点，请使用下面两个宏，如退出的形式不同请自行实现
    //注意子类的子类可能会出现继承关系，需视情况处理
#define DEFAULT_OK_EXIT \
    virtual void onPressedOK() override { exitWithResult(0); }
#define DEFAULT_CANCEL_EXIT \
    virtual void onPressedCancel() override { exitWithResult(-1); }
};
