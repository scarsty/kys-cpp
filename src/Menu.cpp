#include "Menu.h"



Menu::Menu()
{
	push(this);
}


Menu::~Menu()
{
}

void Menu::draw()
{
	for (auto&b : bts )
	{
		b->draw();
	}
}

void Menu::dealEvent(BP_Event& e)
{
	switch (e.type)
	{
	case BP_MOUSEMOTION:
		if (e.button.x > 100)
			pop();
		break;
	}

}

void Menu::addButton(Button* b, int x, int y)
{
	bts.push_back(b);
	b->setPosition(this->x + x, this->y + y);
}
