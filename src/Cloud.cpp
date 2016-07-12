#include "Cloud.h"

std::vector<Cloud*> Cloud::cloudVector;

Cloud::Cloud()
{
}


Cloud::~Cloud()
{
}

void Cloud::initRand()
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

void Cloud::setPositionOnScreen(int x, int y, int Center_X, int Center_Y)
{
	//this->setPosition(position.x - (-x * 18 + y * 18 + maxX / 2 - Center_X), -position.y + (x * 9 + y * 9 + 9 - Center_Y));
	//this->setPosition(324, 200);
}

void Cloud::changePosition()
{
	position.x += speed;
	if (position.x > maxX)
	{
		position.x = 0;
	}
}

void Cloud::initTextures()
{
	// 			//ÔÆµÄÌùÍ¼
	// 			char* path = "cloud";
	// 			MyTexture2D::tex[MyTexture2D::Cloud][1].resize(numTexture);
	// 			for (int i = 0; i < numTexture; i++)
	// 			{
	// 				MyTexture2D::addImage(path, MyTexture2D::Cloud, i, 0, 0, 1, true, true);
	// 			}
}
