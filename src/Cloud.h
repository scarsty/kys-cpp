#pragma once
#include "Base.h"
#include "Point.h"

class Cloud :
	public Base
{
public:
	Cloud();
	virtual ~Cloud();

	enum CloudTowards
	{
		Left = 0,
		Right = 1,
		Up = 2,
		Down = 3,
	};

	Point position;
	float speed;

	static std::vector<Cloud*> cloudVector;

	const int maxX = 17280;
	const int maxY = 8640;
	enum { numTexture = 10 };

	void initRand();
	void setPositionOnScreen(int x, int y, int Center_X, int Center_Y);
	void changePosition();
	static void initTextures();
};

