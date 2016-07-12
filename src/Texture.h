#pragma once
#include "Engine.h"
#include <map>
#include <vector>

class Texture 
{
public:
	Texture();
	virtual ~Texture();

	enum Type
	{
		MainMap = 0,
		Scene = 1,
		Battle = 2,
		Cloud = 3,
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
	static Texture tm;

	static Texture* getInstance() { return &tm; }
	void copyTexture(const std::string& path, int num, int x, int y);

};

