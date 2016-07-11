#pragma once
#include "Base.h"
#include "Button.h"

class Menu :
	public Base
{
public:
	Menu();
	virtual ~Menu();

	std::vector<Button*> bts;

	void draw() override;
	void dealEvent(BP_Event& e) override;

	void addButton(Button* b, int x, int y);

};

