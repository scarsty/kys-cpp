#pragma once
#include "Engine.h"
#include <map>
#include <vector>

//图片纹理类，用字串和编号来索引
//一个纹理最多可以包含10张子纹理，会随机被贴出，模拟动态效果
//每组图片的偏移用两个int16[n][2]来保存，需要注意的是，UI元素使用的图片最好其偏移都设为0，否则计算鼠标范围时会有一些问题，使用起来也比较麻烦

class TextureManager
{
private:
    TextureManager();
    virtual ~TextureManager();
    static TextureManager texture_manager_;
    std::string path_ = "../game/resource/";

    enum
    {
        SUB_TEXTURE_COUNT = 10,
    };

    struct Texture
    {
        BP_Texture* tex[SUB_TEXTURE_COUNT];
        int w = 0, h = 0, dx = 0, dy = 0;
        bool loaded = false;
        int count = 1;
        Texture()
        {
            for (int i = 0; i < SUB_TEXTURE_COUNT; tex[i++] = nullptr);
        }
        ~Texture() { destory(); }
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

public:
    enum Type
    {
        MainMap = 0,
        Scene,
        Battle,
        Cloud,
        MaxType
    };

    std::map<const std::string, std::vector<Texture*>> map_;

    static TextureManager* getInstance() { return &texture_manager_; }

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
private:
    void initialTextureGroup(const std::string& path, bool load_all = false);
    void loadTexture2(const std::string& path, int num, Texture* t);
};

