#include "Button.h"



Button::Button()
{
}


Button::~Button()
{
}

void Button::setTexture(const string& path, int num1, int num2 /*= -1*/, int num3 /*= -1*/)
{
	this->path = path;
	this->num1 = num1;
	this->num2 = num2;
	this->num3 = num3;
}

void Button::draw()
{
	if (state == 0)
	{
		TextureManager::getInstance()->copyTexture(path, num1, x, y);
	}
	else
	{
		if (num2 >= 0)
		{
			TextureManager::getInstance()->copyTexture(path, num2, x, y);
		}
		else
		{
			TextureManager::getInstance()->copyTexture(path, num1, x+1, y+1);
		}
	}
}
