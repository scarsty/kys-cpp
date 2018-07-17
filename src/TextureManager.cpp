#include "File.h"
#include "TextureManager.h"

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

void TextureManager::renderTexture(Texture* tex, BP_Rect r, BP_Color c, uint8_t alpha)
{
    if (tex && tex->tex[0])
    {
        auto engine = Engine::getInstance();
        int i = rand() % tex->count;
        engine->setColor(tex->tex[i], c, alpha);
        engine->renderCopy(tex->tex[i], r.x - tex->dx, r.y - tex->dy, r.w, r.h);
    }
}

void TextureManager::renderTexture(const std::string& path, int num, BP_Rect r, BP_Color c, uint8_t alpha)
{
    auto tex = loadTexture(path, num);
    renderTexture(tex, r, c, alpha);
}

void TextureManager::renderTexture(Texture* tex, int x, int y, BP_Color c, uint8_t alpha, double zoom_x, double zoom_y)
{
    auto engine = Engine::getInstance();
    if (tex && tex->tex[0])
    {
        int i = rand() % tex->count;
        engine->setColor(tex->tex[i], c, alpha);
        engine->renderCopy(tex->tex[i], x - tex->dx, y - tex->dy, tex->w * zoom_x, tex->h * zoom_y);
    }
}

void TextureManager::renderTexture(const std::string& path, int num, int x, int y, BP_Color c, uint8_t alpha, double zoom_x, double zoom_y)
{
    auto tex = loadTexture(path, num);
    renderTexture(tex, x, y, c, alpha, zoom_x, zoom_y);
}

Texture* TextureManager::loadTexture(const std::string& path, int num)
{
    auto p = path_ + path;
    auto& v = getInstance()->map_[path];
    //纹理组信息
    if (getTextureGroupCount(path) == 0)
    {
        return nullptr;
    }
    //纹理信息
    if (num < 0 || num >= v.size())
    {
        return nullptr;
    }
    auto& t = v[num];
    if (!t->loaded)
    {
        loadTexture2(path, num, t);
        Engine::getInstance()->setTextureAlphaMod(t->getTexture(), SDL_BLENDMODE_BLEND);
    }
    return t;
}

int TextureManager::getTextureGroupCount(const std::string& path)
{
    auto& v = getInstance()->map_[path];

    if (v.empty())
    {
        initialTextureGroup(path);
    }

    if (v.size() == 1 && v[0] == nullptr)
    {
        return 0;
    }
    else
    {
        return v.size();
    }
}

void TextureManager::initialTextureGroup(const std::string& path, bool load_all)
{
    auto p = path_ + path;
    auto& v = getInstance()->map_[path];
    //纹理组信息
    //不存在的纹理组也会有一个vector存在，但是里面只有一个空指针
    if (v.empty())
    {
        std::vector<short> offset;
        File::readFileToVector((p + "/index.ka").c_str(), offset);
        if (offset.size() == 0)
        {
            v.resize(1);
            v[0] = nullptr;
            return;
        }
        v.resize(offset.size() / 2);
        for (int i = 0; i < v.size(); i++)
        {
            v[i] = new Texture();
            v[i]->dx = offset[i * 2];
            v[i]->dy = offset[i * 2 + 1];
        }
        printf("Load texture group from path: %s, find %d textures\n", p.c_str(), v.size());
    }
    if (load_all)
    {
        auto engine = Engine::getInstance();
        for (int i = 0; i < v.size(); i++)
        {
            loadTexture2(path, i, v[i]);
        }
    }
}

//这个内部使用
void TextureManager::loadTexture2(const std::string& path, int num, Texture* t)
{
    auto p = path_ + path;
    if (!t->loaded)
    {
        //printf("Load texture %s, %d\n", p.c_str(), num);
        t->tex[0] = Engine::getInstance()->loadImage(p + "/" + std::to_string(num) + ".png");
        if (t->tex[0])
        {
            t->count = 1;
        }
        else
        {
            for (int i = 0; i < Texture::SUB_TEXTURE_COUNT; i++)
            {
                t->tex[i] = Engine::getInstance()->loadImage(p + "/" + std::to_string(num) + "_" + std::to_string(i) + ".png");
                if (t->tex[i] == nullptr)
                {
                    t->count = i;
                    break;
                }
            }
        }
        Engine::getInstance()->queryTexture(t->tex[0], &t->w, &t->h);
        t->loaded = true;
    }
}
