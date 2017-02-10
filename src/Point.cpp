#include "Point.h"
#include <cmath>

Point::Point(int _x, int _y) :
    x(_x), y(_y)
{

}

Point::Point()
{
    x = y = step = g = h = f = Gx = Gy = 0;
    parent = nullptr;
    for (int i = 0; i < 4; i++)
    {
        child[i] = nullptr;
    }
    towards = LeftDown;
}

Point::~Point()
{
}

void Point::delTree(Point* root)
{
    if (root == nullptr)                // 空子树，直接返回
    {
        return;
    }
    else                            // 非空子树
    {
        delTree(root->child[0]);    // 删除子树
        delTree(root->child[1]);
        delTree(root->child[2]);
        delTree(root->child[3]);
        delete (root);                // 释放根节点
    }
}

int Point::Heuristic(int Fx, int Fy)                        //manhattan估价函数
{
    h = (abs(x - Fx) + abs(y - Fy)) * 10;
    return h;
}