#include "Talk.h"
#include <string>
#include <iostream>
#include "Engine.h"
#include "PotConv.h"

Talk::Talk()
{
}

Talk::~Talk()
{
}

bool Talk::InitDialogusDate()
{
    return true;
}

void Talk::draw()
{

    if (PrinterEffects)
    {
        int length = talkString.length();
        if (strlength < length)
        {
            if (tempSpeed < 0)
            {
                tempTalk = talkString.substr(0, strlength);
                strlength += 2;
                tempSpeed = speed;
            }
            tempSpeed--;
        }
    }
    else
    {
        tempTalk = talkString;
    }
    Engine::getInstance()->drawText(fontsName, PotConv::cp936toutf8(tempTalk), 20, 5, 5, 255, BP_ALIGN_LEFT, color);

}

void Talk::SetFontsName(const std::string& fontsname)
{
    fontsName = fontsname;
}

void Talk::SetFontsColor(SDL_Color& Color)
{
    color = Color;
}

void Talk::SetDialoguesEffect(bool b)
{
    if (PrinterEffects)
    {
    }
    else
    {
        PrinterEffects = b;
    }

}

void Talk::SetDialoguesNum(int num)
{
    talkString = m_Dialogues.at(num);
}

void Talk::SetDialoguesSpeed(int sp)
{
    speed = sp;
    tempSpeed = speed;
}