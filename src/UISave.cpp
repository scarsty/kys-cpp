#include "UISave.h"
#include "Save.h"
#include "File.h"
#include "others/libconvert.h"
#include "MainScene.h"

UISave::UISave()
{
}

UISave::~UISave()
{
}

void UISave::onEntrance()
{
    std::vector<std::string> strings;
    for (int i = 0; i <= 10; i++)
    {
        auto str = convert::formatString("ßM¶È%02d  %s", i, File::getFileTime(Save::getFilename(i, 'r')).c_str());
        strings.push_back(str);
    }
    setStrings(strings);
    childs_[0]->setVisible(false); //ÆÁ±Î½ø¶È0
    arrange(0, 0, 0, 25);
}

void UISave::onPressedOK()
{
    if (mode_ == 0)
    {
        Save::getInstance()->LoadR(result_);        
        MainScene::getIntance()->setManPosition(Save::getInstance()->MainMapX, Save::getInstance()->MainMapY);
    }
    if (mode_ == 1)
    {
        MainScene::getIntance()->getManPosition(Save::getInstance()->MainMapX, Save::getInstance()->MainMapY);
        Save::getInstance()->SaveR(result_);
    }
    setExit(true);
}
