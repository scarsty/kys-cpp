#include "Cloud.h"

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
	num = rand() % numTexture;
	//this->setOpacity(128 + rand() % 128);
	//this->setColor(Color3B(rand() % 256, rand() % 256, rand() % 256));
}

void Cloud::setPositionOnScreen(int x, int y, int Center_X, int Center_Y)
{
	this->x = position.x - (-x * 18 + y * 18 + maxX / 2 - Center_X);
	this->y =position.y - (x * 9 + y * 9 + 9 - Center_Y);
}

void Cloud::changePosition()
{
	position.x += speed;
	if (position.x > maxX)
	{
		position.x = 0;
	}
}



