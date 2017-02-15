#pragma once
#include "Base.h"
#include <vector>
#include <string>
using namespace std;
class Dialogues :
	public Base
{
public:
	Dialogues();
	~Dialogues();
	static vector<string> m_Dialogues;    //对话全部读取到向量中
	bool InitDialogusDate();
	string GBKToUTF8(const string& strGBK);
	vector<int> m_idxLen;    

	void draw() override;
	string fontsName, talkString;
	SDL_Color color;
	void SetFontsName(const string& fontsName);
	void SetFontsColor(SDL_Color &color);
};

