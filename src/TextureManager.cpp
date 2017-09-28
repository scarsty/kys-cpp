#include "TextureManager.h"
#include "File.h"

TextureManager TextureManager::texture_manager_;

TextureManager::TextureManager()
{
}

TextureManager::~TextureManager()
{
    for (auto& m : map_)
    {
        for (auto& t : m.second)
        {
            delete t;
        }
    }
}

void TextureManager::renderTexture(const std::string& path, int num, int x, int y, BP_Color c, uint8_t alpha, double zoom)
{
    auto tex = loadTexture(path, num);
    renderTexture(tex, x, y, c, alpha, zoom);
}

void TextureManager::renderTexture(Texture* tex, int x, int y, BP_Color c, uint8_t alpha, double zoom)
{
    auto engine = Engine::getInstance();
    if (tex && tex->tex[0])
    {
        engine->setColor(tex->tex[rand() % tex->count], c, alpha);
        engine->renderCopy(tex->tex[rand() % tex->count], x - tex->dx, y - tex->dy, tex->w * zoom, tex->h * zoom);
    }
}

Texture* TextureManager::loadTexture(const std::string& path, int num)
{
    auto p = path_ + path;
    auto engine = Engine::getInstance();
    auto& v = texture_manager_.map_[path];
    //纹理组信息
    if (v.size() == 0)
    {
        char* s;
        int l = 0;
        File::readFile((p + "/index.ka").c_str(), &s, &l);
        l /= 4;
        if (l == 0)
        {
            v.resize(1);
            v[0] = nullptr;
            return nullptr;
        }
        v.resize(l);
        for (int i = 0; i < l; i++)
        {
            v[i] = new Texture();
            v[i]->dx = *(short*)(s + i * 4);
            v[i]->dy = *(short*)(s + i * 4 + 2);
        }
        delete s;
        printf("Load texture group from path: %s, find %d textures\n", p.c_str(), l);
    }

    //纹理信息
    if (num < 0 || num >= v.size())
    {
        return nullptr;
    }
    auto& t = v[num];
    if (!t->loaded)
    {
        printf("Load texture %s, %d\n", p.c_str(), num);
        t->tex[0] = engine->loadImage(p + "/" + std::to_string(num) + ".png");
        if (t->tex[0])
        {

        }
        else
        {
            for (int i = 0; i < 10; i++)
            {
                t->tex[i] = engine->loadImage(p + "/" + std::to_string(num) + "_" + std::to_string(i) + ".png");
                if (!t->tex[i])
                {
                    t->count = i;
                    break;
                }
            }
        }
        engine->queryTexture(t->tex[0], &t->w, &t->h);
        t->loaded = true;
    }
    return t;
    //这里使用了随机数应该是水面的特效，这里之前和读取图像写到了一起，以后有机会改进一下
    //if (t.count)
    //{ engine->renderCopy(t.tex[rand() % t.count], x - t.dx, y - t.dy, t.w, t.h); }
}
