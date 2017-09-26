#pragma once
#include "Engine.h"
#include <map>
#include <vector>


struct Texture
{
    BP_Texture* tex[10];
    int w = 0, h = 0, dx = 0, dy = 0;
    bool loaded = false;
    int count = 1;
    Texture()
    {
        for (int i = 0; i < 10; tex[i++] = nullptr);
    }
    ~Texture() { destory(); }
    void setTex(BP_Texture* t)
    {
        destory();
        tex[0] = t;
        count = 1;
        loaded = true;
    }
private:
    void destory()
    {
        for (int i = 0; i < 10; i++)
        {
            if (tex[i])
            {
                Engine::getInstance()->destroyTexture(tex[i]);
            }
        }
    }
};

class TextureManager
{
private:
    TextureManager();
    virtual ~TextureManager();
    static TextureManager texture_manager_;
    std::string path_ = "../game/resource/";
    Engine* engine_;
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
    void renderTexture(const std::string& path, int num, int x, int y, BP_Color c = { 255, 255, 255, 255 }, uint8_t alpha = 255, double zoom = 1);
    void renderTexture(Texture* tex, int x, int y, BP_Color c = { 255, 255, 255, 255 }, uint8_t alpha = 255, double zoom = 1);
    Texture* loadTexture(const std::string& path, int num);

    //add by xiaowu for ∂¡»°Õº∆¨
    void LoadImageByPath(const std::string& strPath, int x, int y);
    //add end

};

