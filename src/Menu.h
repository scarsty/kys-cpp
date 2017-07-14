#pragma once
#include "UI.h"
#include "Button.h"

/**
* @file		 Menu.h
* @brief	 ≤Àµ•¿‡
* @author    xiaowu

*/

class Menu :
	public UI
{
public:
	Menu();
	virtual ~Menu();

	std::vector<Button*> bts;

	void draw();
	void dealEvent(BP_Event& e) override;

	void addButton(Button* b, int x, int y);

	int disappear = 0;
	void setDisappear(int d) { disappear = d; }

};

class Menu2 :public Menu
{
public:
	void draw();
	bool getResult();
	void func(BP_Event& e, void* data);
	void setButton(std::string str, int num11, int num12 = -1, int num13 = -1, int num21=-1, int num22 = -1, int num23 = -1);
	void ini();
	void setTitle(std::string str);
	void setPosition(int x, int y) { _x = x; _y = y; };
private:
	std::string _path,_title;
	int _num11, _num12, _num13, _num21, _num22, _num23;
	bool _rs = false;
	bool _isIni = false;
	int _x = -1, _y = -1;
};