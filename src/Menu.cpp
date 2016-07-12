#include "Menu.h"

Menu::Menu()
{

}


Menu::~Menu()
{
	for (auto b : bts)
	{
		safe_delete(b);
	}
}

void Menu::draw()
{
	for (auto&b : bts)
	{
		b->draw();
	}
}

void Menu::dealEvent(BP_Event& e)
{
	switch (e.type)
	{
	case BP_MOUSEMOTION:
		for (auto&b : bts)
		{
			b->state = 0;
			if (b->inSide(e.button.x, e.button.y))
			{
				b->state = 1;
			}
		}
		break;
	case BP_MOUSEBUTTONUP:
		if (e.button.button == BP_BUTTON_LEFT)
		{
			for (int i = 0; i < bts.size(); i++)
			{
				auto &b = bts[i];
				if (b->inSide(e.button.x, e.button.y) && b->state == 1)
				{
					int c = i;
					b->func(e, &c);
					log("%d\n", i);
				}
			}
			if (disappear)
				pop();
		}
		if (e.button.button == BP_BUTTON_RIGHT)
		{
			pop();
		}
		break;
	}
}

void Menu::addButton(Button* b, int x, int y)
{
	bts.push_back(b);
	b->setPosition(this->x + x, this->y + y);
}
