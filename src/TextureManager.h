#pragma once
#include "Engine.h"
#include <map>
#include <vector>

class TextureManager 
{
public:
	TextureManager();
	virtual ~TextureManager();

	enum Type
	{
		MainMap = 0,
		Scene = 1,
		Battle = 2,
		Cloud = 3,
		MaxType
	};

	typedef struct
	{
		BP_Texture* tex = nullptr;
		int w = 0, h = 0, dx = 0, dy = 0;
		bool loaded = false;
	}Texture;

	std::map<const char*, std::vector<Texture>> map;
	static TextureManager tm;

	static TextureManager* getInstance() { return &tm; }
	void copyTexture(const std::string& path, int num, int x, int y);

};

