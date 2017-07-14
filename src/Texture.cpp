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
    auto& v = tm.map[path.c_str()];
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
    auto& t = v[num];
    if (!t.loaded)
    {
        fprintf(stderr, "load texture %s, %d\n", path.c_str(), num);
        t.tex[0] = engine->loadImage(path + "/" + std::to_string(num) + ".png");
        if (t.tex[0])
        {

        }
        else
        {
            for (int i = 0; i < 10; i++)
            {
                t.tex[i] = engine->loadImage(path + "/" + std::to_string(num) + "_" + std::to_string(i) + ".png");
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
    if (t.count)
        engine->renderCopy(t.tex[rand() % t.count], x - t.dx, y - t.dy, t.w, t.h); //这里使用了随机数应该是水面的特效，这里之前和读取图像写到了一起，以后有机会改进一下
}

/**
*  通过路径读取图片地址

*  @param [in] 文件路径，x坐标，y坐标

*  @return 
*/

void Texture::LoadImageByPath(const std::string& strPath, int x, int y)
{
	if (!strPath.empty())
	{
		auto engine = Engine::getInstance();
		auto& v = tm.map[strPath.c_str()];
		v.resize(1);
		auto& t = v[0];
		if (!t.loaded)
		{
			t.tex[0] = engine->loadImage(strPath);
			engine->queryTexture(t.tex[0], &t.w, &t.h);
			t.loaded = true;
		}
		if (t.count)
			engine->renderCopy(t.tex[0], x - t.dx, y - t.dy, t.w, t.h);
	}
}