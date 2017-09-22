#pragma once
#include "Engine.h"
#include <map>
#include <vector>

class Texture
{
private:
    Texture();
    virtual ~Texture();
    static Texture texture_manager_;
public:
    enum Type
    {
        MainMap = 0,
        Scene,
        Battle,
        Cloud,
        MaxType
    };

    struct TextureInfo
    {
        BP_Texture* tex[10];
        int w = 0, h = 0, dx = 0, dy = 0;
        bool loaded = false;
        int count = 1;
        TextureInfo()
        {
            for (int i = 0; i < 10; tex[i++] = nullptr);
        }
    };

    std::map<const std::string, std::vector<TextureInfo>> map;

    static Texture* getInstance() { return &texture_manager_; }
    void renderTexture(const std::string& path, int num, int x, int y);

	//add by xiaowu for ∂¡»°Õº∆¨
	void LoadImageByPath(const std::string& strPath, int x, int y);
	//add end

};

