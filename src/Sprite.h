#pragma once
#include "UI.h"
#include <string>
class Sprite :
	public UI
	
{
public:
	Sprite(std::string &);
	~Sprite();
	void draw();
	void setPositionAndSize(int x, int y,int w =-1,int h = -1); 
private:
	std::string fileName;
	int ImageX = 0, ImageY = 0, ImageW = -1, ImageH = -1;
};

