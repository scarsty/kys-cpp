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
					LOG("%d\n", i);
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
	b->setPosition(this->m_nx + x, this->m_ny + y);
}
bool Menu2::getResult() {
	return _rs;
}
void Menu2::fun(BP_Event& e, void* data) {
	auto i = *(int*)(data);
	if (i == 0)
	{
		_rs = true;
	}
	if (i == 1)
	{
		_rs = false;
	}
	
}
void Menu2::draw()
{
	auto bt1 = new Button();
	bt1->setTexture(path, _num11, _num12,_num13);
	auto bt2 = new Button();
	bt2->setTexture(path, _num21, _num22, _num23);
	addButton(bt1, 0, 0);
	addButton(bt2, 100, 0);
	bt1->setSize(110, 24);
	bt2->setSize(110, 24);
	bt1->setFunction(BIND_FUNC(Menu2::getResult));
	for (auto&b : bts)
	{
		b->draw();
	}
}
void Menu2::setButton(std::string str, int num11, int num12 = -1, int num13 = -1, int num21 = -1, int num22 = -1, int num23 = -1) {
	path = str;
	_num11 = num11;
	_num12 = num12;
	_num13 = num13;
	if (num21 == -1)
		num21 = num11;
	_num21 = num21;
	_num22 = num22;
	_num23 = num23;
}