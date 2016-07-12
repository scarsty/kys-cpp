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

	const int maxX = 17280;
	const int maxY = 8640;

	enum { numTexture = 10 };

	void initRand()
	{
		position.x = rand() % maxX;
		position.y = rand() % maxY;
		speed = 1 + rand() % 3;
		//auto t = MyTexture2D::getSelfPointer(MyTexture2D::Cloud, rand() % numTexture);
		//this->setTexture(t->getTexture());
		//t->setToSprite(this, Point(0, 0));
		//this->setOpacity(128 + rand() % 128);
		//this->setColor(Color3B(rand() % 256, rand() % 256, rand() % 256));
	}
	void setPositionOnScreen(int x, int y, int Center_X, int Center_Y)
	{
		//this->setPosition(position.x - (-x * 18 + y * 18 + maxX / 2 - Center_X), -position.y + (x * 9 + y * 9 + 9 - Center_Y));
		//this->setPosition(324, 200);
	}
	void changePosition()
	{
		position.x += speed;
		if (position.x > maxX)
		{
			position.x = 0;
		}
	}

	static void initTextures()
	{
		// 			//ÔÆµÄÌùÍ¼
		// 			char* path = "cloud";
		// 			MyTexture2D::tex[MyTexture2D::Cloud][1].resize(numTexture);
		// 			for (int i = 0; i < numTexture; i++)
		// 			{
		// 				MyTexture2D::addImage(path, MyTexture2D::Cloud, i, 0, 0, 1, true, true);
		// 			}

	}
};

