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

void TextureManager::renderTexture(const std::string& path, int num, int x, int y)
{
    auto tex = loadTexture(path, num);
    renderTexture(tex, x, y);
}

void TextureManager::renderTexture(Texture* tex, int x, int y)
{
    auto engine = Engine::getInstance();
    if (tex)
    {
        engine->renderCopy(tex->tex[rand() % tex->count], x - tex->dx, y - tex->dy, tex->w, tex->h);
    }
}

Texture* TextureManager::loadTexture(const std::string& path, int num)
{
    auto engine = Engine::getInstance();
    auto& v = texture_manager_.map_[path.c_str()];
    //纹理组信息
    if (v.size() == 0)
    {
        unsigned char* s;
        int l = 0;
        File::readFile((path + "/index.ka").c_str(), &s, &l);
        if (l == 0) { return nullptr; }
        l /= 4;
        v.resize(l);
        for (int i = 0; i < l; i++)
        {
            v[i] = new Texture();
            v[i]->dx = *(short*)(s + i * 4);
            v[i]->dy = *(short*)(s + i * 4 + 2);
        }
        delete s;
        printf("Load texture group from path: %s, find %d textures\n", path.c_str(), l);
    }

    //纹理信息
    auto& t = v[num];
    if (!t->loaded)
    {
        printf("Load texture %s, %d\n", path.c_str(), num);
        t->tex[0] = engine->loadImage(path + "/" + std::to_string(num) + ".png");
        if (t->tex[0])
        {

        }
        else
        {
            for (int i = 0; i < 10; i++)
            {
                t->tex[i] = engine->loadImage(path + "/" + std::to_string(num) + "_" + std::to_string(i) + ".png");
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

/**
*  通过路径读取图片地址
*  @param [in] 文件路径，x坐标，y坐标
*  @return
*/

void TextureManager::LoadImageByPath(const std::string& strPath, int x, int y)
{
    //if (!strPath.empty())
    //{
    //    auto engine = Engine::getInstance();
    //    auto& v = texture_manager_.map[strPath.c_str()];
    //    v.resize(1);
    //    auto& t = v[0];
    //    if (!t.loaded)
    //    {
    //        t.tex[0] = engine->loadImage(strPath);
    //        engine->queryTexture(t.tex[0], &t.w, &t.h);
    //        t.loaded = true;
    //    }
    //    if (t.count)
    //    { engine->renderCopy(t.tex[0], x - t.dx, y - t.dy, t.w, t.h); }
    //}
}