#pragma once
class Point
{
public:
    Point();
    Point(int _x, int _y);
    virtual ~Point();

	int x, y, step;
	int g, h, f;
	int Gx, Gy;

	enum Towards
	{
		LeftUp = 0,
		RightUp = 1,
		LeftDown = 2,
		RightDown = 3,
	}towards;
	Point* parent;
	Point* child[4];

	void delTree(Point*);

	bool lessthan(const Point *myPoint) const
	{
		return f > myPoint->f;												//重载比较运算符
	}
	int Heuristic(int Fx, int Fy);



};

