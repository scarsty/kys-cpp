#pragma once

//这里如果用枚举类型会有一些麻烦，干脆改为整型
enum Towards
{
    Towards_RightUp = 0,
    Towards_RightDown = 1,
    Towards_LeftUp = 2,
    Towards_LeftDown = 3,
    Towards_None
};

struct Point
{
public:
    Point() {}
    Point(int _x, int _y) : x(_x), y(_y) {}
    ~Point() {}
    int x = 0, y = 0;
};
