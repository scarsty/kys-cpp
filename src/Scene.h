#pragma once
#include "Element.h"
#include "Point.h"

//����ͼ���ӳ�����ս���������̳д���
class Scene : public Element
{
public:
    Scene();
    virtual ~Scene();
    virtual void draw() override {}
    virtual void dealEvent(BP_Event& e) override {}

    //BP_Texture* earth_texture_ = nullptr;

    int render_center_x_ = 0;
    int render_center_y_ = 0;

    const int TILE_W = 18;    //Сͼ���СX
    const int TILE_H = 9;     //Сͼ���СY

    //ȷ����Ұʹ��
    int view_width_region_ = 0;
    int view_sum_region_ = 0;

    void calViewRegion();

    int total_step_ = 0;        //������·�ļ���
    BP_Keycode pre_pressed_;    //������·���ϴΰ���

    int man_x_, man_y_;
    int mouse_event_x_ = -1, mouse_event_y_ = -1;    //�����·ʱ������Ŀ�꣬����Ϊ�¼��������
    int cursor_x_ = 0, cursor_y_ = 0;
    int towards_ = 0;    //���򣬹���һ������
    int step_ = 0;
    int man_pic_ = 0;

    int rest_time_ = 0;    //ֹͣ������ʱ��

    int COORD_COUNT = 0;

    int first_step_delay_ = 5;

    void setManPosition(int x, int y)
    {
        man_x_ = x;
        man_y_ = y;
    }
    void getManPosition(int& x, int& y)
    {
        x = man_x_;
        y = man_y_;
    }
    void setManPic(int pic) { man_pic_ = pic; }

    void checkWalk(int x, int y, BP_Event& e);    //һЩ�������֣�δ���

    Point getPositionOnRender(int x, int y, int view_x, int view_y);
    Point getPositionOnWindow(int x, int y, int view_x, int view_y);
    int calTowards(int x1, int y1, int x2, int y2);
    void setTowards(int t) { towards_ = t; }

    int calDistance(int x1, int y1, int x2, int y2) { return abs(x1 - x2) + abs(y1 - y2); }
    int calBlockTurn(int x, int y, int layer) { return 4 * (128 * (x + y) + x) + layer; }

    void changeTowardsByKey(BP_Keycode key);
    int getTowardsByKey(BP_Keycode key);
    int getTowardsByMouse(int mouse_x, int mouse_y);

    //��ȡ����һ�������
    static void getTowardsPosition(int x0, int y0, int tw, int* x1, int* y1);

    virtual bool canWalk(int x, int y) { return false; }
    virtual bool isOutScreen(int x, int y) { return false; }

    std::vector<Point> way_que_;    //ջ(·��ջ)

    Point getMousePosition(int mouse_x, int mouse_y, int view_x, int view_y);
    Point getMousePosition(int view_x, int view_y);
    void calCursorPosition(int x, int y);

    void FindWay(int Mx, int My, int Fx, int Fy);

    void lightScene();
    void darkScene();

    bool isOutLine(int x, int y);

    void setMouseEventPoint(int x, int y)
    {
        mouse_event_x_ = x;
        mouse_event_y_ = y;
    }
};
