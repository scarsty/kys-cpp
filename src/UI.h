#pragma once
#include "Base.h"
#include <vector>
class UI:
	public Base
{
public:
	UI();
	~UI();
	void draw() override;
	std::vector<UI*> bts;
	void AddSprite(UI*);
	void dealEvent(BP_Event& e) override;
};

