#include "TextureManager.h"
#include "File.h"
TextureManager TextureManager::tm;

TextureManager::TextureManager()
{
}


TextureManager::~TextureManager()
{
}

void TextureManager::copyTexture(const std::string& path, int num, int x, int y)
{
	if (num < 0) return;
	auto engine = Engine::getInstance();
	auto &v = tm.map[path.c_str()];
	if (v.size() == 0)
	{
		unsigned char* s;
		int l = 0;
		File::readFile((path + "/index.ka").c_str(), &s, &l);
		if (l == 0)return;
		l /= 4;
		v.resize(l);
		for (int i = 0; i < l; i++)
		{
			v[num].dx = *(short*)(s + i * 4);
			v[num].dx = *(short*)(s + i * 4 + 2);
		}
		delete s;
	}
	if (!v[num].loaded)
	{
		v[num].tex = engine->loadImage(path + "/" + to_string(num) + ".png");
		engine->queryTexture(v[num].tex, &(v[num].w), &(v[num].h));
		v[num].loaded = true;
	}
	engine->renderCopy(v[num].tex, x + v[num].dx, y + v[num].dy, v[num].w, v[num].h);
}
