#include "TextureManager.h"

TextureManager TextureManager::tm;

TextureManager::TextureManager()
{
}


TextureManager::~TextureManager()
{
}

void TextureManager::copyTexture(const std::string& path, int num, int x, int y)
{
	auto engine = Engine::getInstance();
	auto &v = tm.map[path.c_str()];
	if (v.size() == 0)
	{
		v.resize(9999);
	}
	if (!v[num].loaded)
	{
		v[num].tex = engine->loadImage(path + "/" + to_string(num) + ".png");
		engine->queryTexture(v[num].tex, &(v[num].w), &(v[num].h));
		v[num].loaded = true;
	}
	engine->renderCopy(v[num].tex, x + v[num].dx, y + v[num].dy, v[num].w, v[num].h);
}
