#include "Texture.h"
#include "File.h"

Texture Texture::tm;

Texture::Texture()
{
}


Texture::~Texture()
{
}

void Texture::copyTexture(const std::string& path, int num, int x, int y)
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
			v[i].dx = *(short*)(s + i * 4);
			v[i].dy = *(short*)(s + i * 4 + 2);
		}
		delete s;
		fprintf(stderr, "load textures info %s, %d\n", path.c_str(), l);
	}
	auto &t = v[num];
	if (!t.loaded)
	{
		fprintf(stderr, "load texture %s, %d\n", path.c_str(), num);
		t.tex[0] = engine->loadImage(path + "/" + to_string(num) + ".png");
		if (t.tex[0])
		{

		}
		else
		{
			for (int i = 0; i < 10; i++)
			{
				t.tex[i] = engine->loadImage(path + "/" + to_string(num) + "_" + to_string(i) + ".png");
				if (!t.tex[i])
				{
					t.count = i;
					break;
				}
			}
		}
		engine->queryTexture(t.tex[0], &t.w, &t.h);
		t.loaded = true;
	}
	if(t.count)
	engine->renderCopy(t.tex[rand() % t.count], x - t.dx, y - t.dy, t.w, t.h);
}
