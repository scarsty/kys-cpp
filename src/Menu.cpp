#include "Menu.h"
#include"Dialogues.h"
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
void Menu2::draw() {
	SDL_Color color = { 0, 0, 0, 255 };
	Engine::getInstance()->drawText("fonts/Dialogues.ttf", _title, 20, 5, 5, 255, BP_ALIGN_LEFT, color);
	for (auto&b : bts)
	{
		b->draw();
	}
}
void Menu2::setTitle(std::string str) {
	_title = str;
}

void Menu::addButton(Button* b, int x, int y)
{
	bts.push_back(b);
	b->setPosition(this->m_nx + x, this->m_ny + y);
}
bool Menu2::getResult() {
	return _rs;
}
void Menu2::func(BP_Event& e, void* data) {
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

void Menu2::ini() {
	if (!_isIni)
	{
		auto bt1 = new Button();
		bt1->setTexture(_path, _num11, _num12, _num13);
		auto bt2 = new Button();
		bt2->setTexture(_path, _num21, _num22, _num23);
		if (_x < 0)
			_x = 100;
		if (_y < 0)
			_y = 100;
		this->setPosition(_x, _y);
		this->setSize(150,58);
		addButton(bt1, 20, 5);
		addButton(bt2, 20, 29);
		bt1->setSize(110, 24);
		bt2->setSize(110, 24);
		bt1->setFunction(BIND_FUNC(Menu2::func));
		auto m_UI = new UI();
		//m_UI->AddSprite(m_Dialogues);		
		m_UI->AddSprite(this);
		push(m_UI);
		_isIni = true;

	}
}
void Menu2::setButton(std::string str, int num11, int num12, int num13, int num21, int num22, int num23) {
	_path = str;
	_num11 = num11;
	_num12 = num12;
	_num13 = num13;
	if (num21 == -1)
		num21 = num11;
	_num21 = num21;
	_num22 = num22;
	_num23 = num23;
}