#include "UI.h"



UI::UI()
{
}


UI::~UI()
{
	for (auto b : bts)
	{
		safe_delete(b);
	}
}

void UI::draw()
{
	for (auto&b : bts)
	{
		b->draw();
	}
}

void UI::AddSprite(UI*b)
{
	bts.push_back(b);
}

void UI::dealEvent(BP_Event& e)
{
	bts.back()->dealEvent(e);
	/*for (auto&b : bts)
	{
		b->dealEvent(e);
	}*/
}