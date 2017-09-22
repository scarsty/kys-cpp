#include "Menu.h"
#include"Dialogues.h"
#include "Head.h"
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
	
}


void Menu::addButton(Button* b, int x, int y)
{
	bts.push_back(b);
	//b->setPosition(this->m_nx + x, this->m_ny + y);
}


/*========================================================================================*/

void Menu2::draw() {
	SDL_Color color = { 0, 0, 0, 255 };
	
	for (auto&b : bts)
	{
		b->draw();
	}
	Engine::getInstance()->drawText("fonts/Dialogues.ttf",_title, 20, 5, 5, 255, BP_ALIGN_LEFT, color); //这里有问题，字符无法显示
}
void Menu2::setTitle(std::string str) {
	_title = str;
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