#include "TextureManager.h"
#include "GameUtil.h"
#include "RunNode.h"
#include "filefunc.h"
#include "strfunc.h"

void Texture::setTex(BP_Texture* t)
{
    destory();
    tex[0] = t;
    count = 1;
    loaded = true;
    Engine::getInstance()->queryTexture(t, &w, &h);
}

void Texture::load()
{
    if (!loaded)
    {
        loaded = true;
        //fmt1::print("Load texture {}, {}\n", group_info_->path, num_);
        if (group_info_->zip.opened())
        {
            tex[0] = Engine::getInstance()->loadImageFromMemory(group_info_->zip.readEntryName(std::to_string(num_) + ".png"));
        }
        else
        {
            tex[0] = Engine::getInstance()->loadImage(group_info_->path + "/" + std::to_string(num_) + ".png");
        }
        if (tex[0])
        {
            count = 1;
        }
        else
        {
            for (int i = 0; i < Texture::SUB_TEXTURE_COUNT; i++)
            {
                if (group_info_->zip.opened())
                {
                    tex[i] = Engine::getInstance()->loadImageFromMemory(group_info_->zip.readEntryName(std::to_string(num_) + "_" + std::to_string(i) + ".png"));
                }
                else
                {
                    tex[i] = Engine::getInstance()->loadImage(group_info_->path + "/" + std::to_string(num_) + "_" + std::to_string(i) + ".png");
                }
                if (tex[i] == nullptr)
                {
                    count = i;
                    break;
                }
            }
        }
        Engine::getInstance()->queryTexture(tex[0], &w, &h);
        for (auto t : tex)
        {
            Engine::getInstance()->setTextureAlphaMod(t, SDL_BLENDMODE_BLEND);
        }
    }
}

void Texture::createWhiteTexture()
{
    if (tex_white == nullptr)
    {
        if (group_info_->zip.opened())
        {
            tex_white = Engine::getInstance()->loadImageFromMemory(group_info_->zip.readEntryName(std::to_string(num_) + ".png"), 1);
        }
        else
        {
            tex_white = Engine::getInstance()->loadImage(group_info_->path + "/" + std::to_string(num_) + ".png", 1);
        }
    }
}

void Texture::destory()
{
    for (int i = 0; i < SUB_TEXTURE_COUNT; i++)
    {
        if (tex[i])
        {
            Engine::destroyTexture(tex[i]);
        }
    }
}

std::string TextureGroup::getFileContent(const std::string& filename)
{
    if (!inited_)
    {
        return "";
    }
    if (info_.zip.opened())
    {
        return info_.zip.readEntryName(filename);
    }
    else
    {
        return filefunc::readFileToString(info_.path + "/" + filename);
    }
}

void TextureGroup::init(const std::string& path, int load_from_path, int load_all)
{
    //纹理组信息
    if (!inited_)
    {
        inited_ = 1;
        info_.path = path;
        if (!load_from_path)
        {
            info_.zip.openFile(path + ".zip");
        }
        std::vector<short> offset;
        if (info_.zip.opened())
        {
            std::string index_ka = info_.zip.readEntryName("index.ka");
            offset.resize(index_ka.size() / 2);
            memcpy(offset.data(), index_ka.data(), offset.size() * 2);
        }
        else
        {
            filefunc::readFileToVector((info_.path + "/index.ka").c_str(), offset);
        }
        group_.resize(offset.size() / 2);
        for (int i = 0; i < group_.size(); i++)
        {
            group_[i] = new Texture();
            group_[i]->dx = offset[i * 2];
            group_[i]->dy = offset[i * 2 + 1];
            group_[i]->group_info_ = &info_;
            group_[i]->num_ = i;
        }
        if (info_.zip.opened())
        {
            fmt1::print("Load texture group from file: {}.zip, {} textures\n", info_.path, group_.size());
        }
        else
        {
            fmt1::print("Load texture group from path: {}, {} textures\n", info_.path, group_.size());
        }
    }
    if (load_all)
    {
        for (int i = 0; i < group_.size(); i++)
        {
            group_[i]->load();
        }
    }
}

TextureManager::TextureManager()
{
    path_ = GameUtil::PATH() + "resource/";
}

TextureManager::~TextureManager()
{
    for (auto& m : map_)
    {
        for (auto& t : m.second.group_)
        {
            delete t;
        }
    }
}

void TextureManager::renderTexture(Texture* tex, BP_Rect r, BP_Color c, uint8_t alpha, double angle, uint8_t white)
{
    if (tex == nullptr) { return; }
    tex->load();
    if (tex->tex[0] == nullptr) { return; }

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
    if (i < 0)
    {
        i = 0;
    }
    c.a = alpha;
    engine->setColor(tex->tex[i], c);
    engine->renderCopy(tex->tex[i], r.x - tex->dx, r.y - tex->dy, r.w, r.h, angle);
    if (white)
    {
        tex->createWhiteTexture();
        engine->setColor(tex->tex_white, { 255, 255, 255, white });
        engine->renderCopy(tex->tex_white, r.x - tex->dx, r.y - tex->dy, r.w, r.h, angle);
    }
}

void TextureManager::renderTexture(const std::string& path, int num, BP_Rect r, BP_Color c, uint8_t alpha, double angle, uint8_t white)
{
    renderTexture(getTexture(path, num), r, c, alpha, angle, white);
}

void TextureManager::renderTexture(Texture* tex, int x, int y, BP_Color c, uint8_t alpha, double zoom_x, double zoom_y, double angle, uint8_t white)
{
    if (tex)
    {
        tex->load();    //需要纹理尺寸
        renderTexture(tex, { x, y, int(tex->w * zoom_x), int(tex->h * zoom_y) }, c, alpha, angle, white);
    }
}

void TextureManager::renderTexture(const std::string& path, int num, int x, int y, BP_Color c, uint8_t alpha, double zoom_x, double zoom_y, double angle, uint8_t white)
{
    renderTexture(getTexture(path, num), x, y, c, alpha, zoom_x, zoom_y, angle, white);
}

Texture* TextureManager::getTexture(const std::string& path, int num)
{
    auto p = path_ + path;
    auto& v = getInstance()->map_[path];
    //纹理组信息
    if (getTextureGroupCount(path) == 0)
    {
        return nullptr;
    }
    //纹理信息
    if (num < 0 || num >= v.group_.size())
    {
        return nullptr;
    }
    auto& t = v.group_[num];
    return t;
}

int TextureManager::getTextureGroupCount(const std::string& path)
{
    auto& v = getInstance()->map_[path];
    if (!v.inited_)
    {
        v.init(path_ + "/" + path, load_from_path_, load_all_);
    }
    return v.group_.size();
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
