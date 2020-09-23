#pragma once
#include "Engine.h"
#include "ZipFile.h"
#include <map>
#include <vector>

//图片纹理类，用字串和编号来索引
//一个纹理最多可以包含10张子纹理，会随机被贴出，模拟动态效果
//每组图片的偏移用两个int16[n][2]来保存，需要注意的是，UI元素使用的图片最好其偏移都设为0，否则计算鼠标范围时会有一些问题，使用起来也比较麻烦

struct Texture
{
    enum
    {
        SUB_TEXTURE_COUNT = 10,
    };

    BP_Texture* tex[SUB_TEXTURE_COUNT] = { nullptr };
    int w = 0, h = 0, dx = 0, dy = 0;
    bool loaded = false;
    int count = 1;
    int prev_show;
    void setTex(BP_Texture* t)
    {
        destory();
        tex[0] = t;
        count = 1;
        loaded = true;
        Engine::getInstance()->queryTexture(t, &w, &h);
    }
    BP_Texture* getTexture(int i = 0) { return tex[i]; }

private:
    void destory()
    {
        for (int i = 0; i < SUB_TEXTURE_COUNT; i++)
        {
            if (tex[i])
            {
                Engine::destroyTexture(tex[i]);
            }
        }
    }
};

struct TextureGroup : public std::vector<Texture*>
{
    friend class TextureManager;

public:
    int inited_ = 0;
    ZipFile zip_;
    ZipFile* getZip() { return &zip_; }
    std::string path_;
    const std::string& getPath() { return path_; }
    std::string getFileContent(const std::string& filename);

protected:
    void init(const std::string& path, int load_from_path, int load_all);
    void loadTexture(int num, Texture* t);
};

class TextureManager
{
private:
    TextureManager();
    virtual ~TextureManager();
    std::string path_ = "../game/resource/";
    int load_from_path_ = 0;    //0 - 先尝试读取zip，如没有则读取目录；1 - 不尝试读取zip，直接读取目录
    int load_all_ = 0;
    std::map<const std::string, TextureGroup> map_;

public:
    static TextureManager* getInstance()
    {
        static TextureManager tm;
        return &tm;
    }

public:
    void renderTexture(Texture* tex, BP_Rect r,
        BP_Color c = { 255, 255, 255, 255 }, uint8_t alpha = 255);
    void renderTexture(const std::string& path, int num, BP_Rect r,
        BP_Color c = { 255, 255, 255, 255 }, uint8_t alpha = 255);

    void renderTexture(Texture* tex, int x, int y,
        BP_Color c = { 255, 255, 255, 255 }, uint8_t alpha = 255, double zoom_x = 1, double zoom_y = 1);
    void renderTexture(const std::string& path, int num, int x, int y,
        BP_Color c = { 255, 255, 255, 255 }, uint8_t alpha = 255, double zoom_x = 1, double zoom_y = 1);

    Texture* loadTexture(const std::string& path, int num);
    int getTextureGroupCount(const std::string& path);
    TextureGroup* getTextureGroup(const std::string& path);
    void setLoadFromPath(int l) { load_from_path_ = l; }
    void setLoadAll(int l) { load_all_ = l; }
};
