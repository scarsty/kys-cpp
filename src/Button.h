#pragma once
#include "Base.h"

class Button : public Base
{
public:
	Button();
	virtual ~Button();

	void setTexture(const string& path, int num);

	int w, h;

	std::string path;
	int num;

	void draw() override;
	//void dealEvent(BP_Event& e) override;
};

