#include "TextureManager.h"
#include "File.h"
#include "RunNode.h"
#include "convert.h"

std::string TextureGroup::getFileContent(const std::string& filename)
{
    if (!inited_)
    {
        return "";
    }
    if (zip_.opened())
    {
        return zip_.readEntryName(filename);
    }
    else
    {
        return convert::readStringFromFile(path_ + "/" + filename);
    }
}

void TextureGroup::init(const std::string& path, int load_from_path, int load_all)
{
    //纹理组信息
    if (!inited_)
    {
        path_ = path;
        inited_ = 1;
        if (!load_from_path)
        {
            zip_.openFile(path_ + ".zip");
        }
        std::vector<short> offset;
        if (zip_.opened())
        {
            std::string index_ka = zip_.readEntryName("index.ka");
            offset.resize(index_ka.size() / 2);
            memcpy(offset.data(), index_ka.data(), offset.size() * 2);
        }
        else
        {
            File::readFileToVector((path_ + "/index.ka").c_str(), offset);
        }
        resize(offset.size() / 2);
        for (int i = 0; i < size(); i++)
        {
            (*this)[i] = new Texture();
            (*this)[i]->dx = offset[i * 2];
            (*this)[i]->dy = offset[i * 2 + 1];
        }
        if (zip_.opened())
        {
            fmt::print("Load texture group from file: {}.zip, {} textures\n", path_, size());
        }
        else
        {
            fmt::print("Load texture group from path: {}, {} textures\n", path_, size());
        }
    }
    if (load_all)
    {
        auto engine = Engine::getInstance();
        for (int i = 0; i < size(); i++)
        {
            loadTexture(i, (*this)[i]);
        }
    }
}

//这个内部使用
void TextureGroup::loadTexture(int num, Texture* t)
{
    if (!t->loaded)
    {
        //fmt::print("Load texture %s, %d\n", p.c_str(), num);
        if (zip_.opened())
        {
            t->tex[0] = Engine::getInstance()->loadImageFromMemory(zip_.readEntryName(std::to_string(num) + ".png"));
        }
        else
        {
            t->tex[0] = Engine::getInstance()->loadImage(path_ + "/" + std::to_string(num) + ".png");
        }
        if (t->tex[0])
        {
            t->count = 1;
        }
        else
        {
            for (int i = 0; i < Texture::SUB_TEXTURE_COUNT; i++)
            {
                if (zip_.opened())
                {
                    t->tex[i] = Engine::getInstance()->loadImageFromMemory(zip_.readEntryName(std::to_string(num) + "_" + std::to_string(i) + ".png"));
                }
                else
                {
                    t->tex[i] = Engine::getInstance()->loadImage(path_ + "/" + std::to_string(num) + "_" + std::to_string(i) + ".png");
                }
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
        int i = 0;
        if (tex->count > 1)
        {
            int now = RunNode::getShowTimes();
            //此处同时模拟随机的水面和大场景的瀑布
            if (now == tex->prev_show)
            {
                //若本张图在一帧中再次出现则更换一个贴图
                i = rand() % tex->count;
            }
            else
            {
                //若本张图在一帧中首次出现则顺序贴图
                i = now % tex->count;
            }
            tex->prev_show = now;
        }
        c.a = alpha;
        engine->setColor(tex->tex[i], c);
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
    if (tex && tex->tex[0])
    {
        renderTexture(tex, { x, y, int(tex->w * zoom_x), int(tex->h * zoom_y) }, c, alpha);
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
        v.loadTexture(num, t);
        Engine::getInstance()->setTextureAlphaMod(t->getTexture(), SDL_BLENDMODE_BLEND);
    }
    return t;
}

int TextureManager::getTextureGroupCount(const std::string& path)
{
    auto& v = getInstance()->map_[path];
    if (!v.inited_)
    {
        v.init(path_ + "/" + path, load_from_path_, load_all_);
    }
    return v.size();
}

TextureGroup* TextureManager::getTextureGroup(const std::string& path)
{
    auto& v = getInstance()->map_[path];
    if (!v.inited_)
    {
        v.init(path_ + "/" + path, load_from_path_, load_all_);
    }
    return &v;
}
