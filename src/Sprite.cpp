#include "Sprite.h"
#include "Engine.h"
#include <string>
using namespace std;

Sprite::Sprite(string &filename)
{
	fileName = filename;
}


Sprite::~Sprite()
{
}

void Sprite::setPositionAndSize(int x, int y,int w /* =-1 */,int h /* = -1 */)
{
	ImageX = x;
	ImageY = y;
	ImageW = w;
	ImageH = h;
}

void Sprite::draw()
{
	if (fileName=="")
	{
		fprintf(stderr, "file's Name is empty \n");
	}
	else
	{
		auto engine = Engine::getInstance();
		auto tex = engine->loadImage(fileName);
		if (tex==nullptr)
		{
			fprintf(stderr, "Can not find file %s \n",fileName.c_str());
		}
		else
		{
			int w, h;
			engine->queryTexture(tex, &w, &h);
			if (ImageW == -1)
			{
				ImageW = w;
			}
			if (ImageH ==-1)
			{
				ImageH = h;
			}
			engine->renderCopy(tex,ImageX,ImageY,ImageW,ImageH);
		}
	}
}