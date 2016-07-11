#include "Button.h"



Button::Button()
{
}


Button::~Button()
{
}

void Button::setTexture(const string& path, int num)
{
	this->path = path;
	this->num = num;
	this->x = x;
	this->y = y;
}

void Button::draw()
{
	TextureManager::getInstance()->copyTexture(path.c_str(), num, x, y);
}
