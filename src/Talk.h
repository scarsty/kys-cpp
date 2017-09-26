#pragma once
#include "Base.h"
#include <vector>
#include <string>

class Talk :
    public Base
{
public:
    Talk();
    ~Talk();
    static std::vector<std::string> m_Dialogues;    //对话全部读取到向量中
    bool InitDialogusDate();
    //  string GBKToUTF8(const string& strGBK);  放head去了
    void draw();
    void SetFontsName(const std::string& fontsName);
    void SetFontsColor(SDL_Color& color);
    void SetDialoguesNum(int num);
    void SetDialoguesEffect(bool b);
    void SetDialoguesSpeed(int speed);

private:
    std::vector<int> m_idxLen;
    std::string fontsName, talkString;
    SDL_Color color;
    int speed = 0;
    int tempSpeed = 0;
    int strlength = 0;
    bool PrinterEffects = false;
    std::string tempTalk = "";
};

