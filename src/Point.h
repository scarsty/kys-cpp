#pragma once

//这里如果用枚举类型会有一些麻烦，干脆改为整型
enum Towards
{
    Towards_LeftUp = 0,
    Towards_RightUp = 1,
    Towards_LeftDown = 2,
    Towards_RightDown = 3,
    Towards_None
};

struct Point
{
public:
    Point() {}
    Point(int _x, int _y):x(_x), y(_y) {}
    ~Point() {}
    int x = 0, y = 0;
};

struct PointEx : public Point
{
    PointEx();
    ~PointEx() {}

    int step = 0;
    int g = 0, h = 0, f = 0;
    int Gx = 0, Gy = 0;

    int towards;
    PointEx* parent;
    PointEx* child[4];

    void delTree(PointEx*);

    bool lessthan(const PointEx* myPoint) const
    {
        return f > myPoint->f;                                              //重载比较运算符
    }
    int Heuristic(int Fx, int Fy);
};

class Compare
{
public:
    bool operator()(PointEx* point1, PointEx* point2)
    {
        return point1->lessthan(point2);
    }
};

