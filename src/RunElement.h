#pragma once
#include "Engine.h"
#include "TextureManager.h"
#include <functional>
#include <memory>
#include <vector>

#define CONNECT(a, b) a##b

//��Ϸִ�кͻ��ƵĻ����࣬����Ҫ��ʾ������ߴ����¼��ģ����̳��Դ�
class RunElement
{
private:
    static std::vector<RunElement*> root_;    //������Ҫ���Ƶ����ݶ��洢�������̬������
    static int prev_present_ticks_;
    static int refresh_interval_;

private:
    bool is_private_ = false;

protected:
    std::vector<std::shared_ptr<RunElement>> childs_;
    bool visible_ = true;
    int result_ = -1;
    int full_window_ = 0;    //��Ϊ0ʱ��ʾ��ǰ����Ϊ��ʼ�㣬��ʱ���ڱ���Ľ�������ʾ����ʡ��Դ
    bool exit_ = false;      //����Ĺ��������ֵΪtrue������ʾ��һ��ѭ�����˳�
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
    RunElement() {}
    virtual ~RunElement();

    static void setRefreshInterval(int i) { refresh_interval_ = i; }
    static int getRefreshInterval() { return refresh_interval_; }

    static int getShowTimes() { return prev_present_ticks_ / refresh_interval_; }

    static void drawAll();

    static void addOnRootTop(RunElement* element) { root_.push_back(element); }
    static RunElement* removeFromRoot(RunElement* element);

    //Լ�������в���ʹ��new�������࣬����ʹ�������ģ��
    void addChild(RunElement* element);
    void addChild(RunElement* element, int x, int y);
    template <class T>
    T* addChild()
    {
        T* c = new T();
        c->is_private_ = true;
        addChild(c);
        return c;
    }
    template <class T>
    T* addChild(int x, int y)
    {
        T* c = new T();
        c->is_private_ = true;
        addChild(c, x, y);
        return c;
    }

    std::shared_ptr<RunElement> getChild(int i) { return childs_[i]; }
    int getChildCount() { return childs_.size(); }
    void removeChild(RunElement* element);
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
        return x > x_ && x < x_ + w_ && y > y_ && y < y_ + h_;
    }

    int getResult() { return result_; }
    void setResult(int r) { result_ = r; }
    bool getVisible() { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    void setDealEvent(int d) { deal_event_ = d; }

    //״̬
    enum State
    {
        Normal,
        Pass,
        Press,
    };

    enum Direct
    {
        None,
        Left,
        Up,
        Right,
        Down
    };

    int state_ = Normal;
    int getState() { return state_; }
    void setState(int s) { state_ = s; }

    int getTag() { return tag_; }
    void setTag(int t) { tag_ = t; }

    //static void clearEvent(BP_Event& e) { e.type = BP_FIRSTEVENT; }
    static RunElement* getCurrentTopDraw() { return root_.back(); }

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

    //ͨ����˵������������޹ص��߼�����draw��dealEvent�����ⲻ�󣬵��ǽ���draw�н��л�ͼ��صĲ���

    virtual void backRun() {}                  //�ڵ���root�о����У����Է����ܼ�����
    virtual void draw() {}                     //��λ����ڵ�
    virtual void dealEvent(BP_Event& e) {}     //�����¼�����һֱִ�У��൱����ѭ����
    virtual void dealEvent2(BP_Event& e) {}    //�����¼���ִ��ģʽ�Ͷ���ģʽ���ᱻִ�У��������ƶ�
    virtual void onEntrance() {}               //���뱾�ڵ���¼�������������
    virtual void onExit() {}                   //�뿪���ڵ���¼������������

    virtual void onPressedOK() {}        //���»س������������¼�������������̳л�������
    virtual void onPressedCancel() {}    //����esc������Ҽ����¼�������������̳л�������

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
    void drawSelfChilds();
    void checkStateSelfChilds(BP_Event& e, bool check_event = false);
    void backRunSelfChilds();
    void dealEventSelfChilds(bool check_event = false);

    bool isSpecialEvent(BP_Event& e);    //�Ƿ�Ϊ��Ϸ��Ҫ���������
    int checkChildState();
    void checkSelfState(BP_Event& e);
    static void present();

public:
    int run(bool in_root = true);    //ִ�б���
    int runAtPosition(int x = 0, int y = 0, bool in_root = true)
    {
        setPosition(x, y);
        return run(in_root);
    }
    static void exitAll(int begin = 0);    //���ô�begin��ʼ��ȫ���ڵ�״̬Ϊ�˳�
    int drawAndPresent(int times = 1, std::function<void(void*)> func = nullptr, void* data = nullptr);

    template <class T>
    static T* getPointerFromRoot()
    {
        for (int i = root_.size() - 1; i >= 0; i--)
        {
            T* ptr = dynamic_cast<T*>(root_[i]);
            if (ptr) { return ptr; }
        }
        return nullptr;
    }

    //ÿ���ڵ�Ӧ���ж��巵��ֵ��
    //��Ҫ��ͨ�˳����ܵ��ӽڵ㣬��ʹ�����������꣬���˳�����ʽ��ͬ������ʵ��
    //ע�������������ܻ���ּ̳й�ϵ�������������
#define DEFAULT_OK_EXIT virtual void onPressedOK() override { exitWithResult(0); }
#define DEFAULT_CANCEL_EXIT virtual void onPressedCancel() override { exitWithResult(-1); }
};
