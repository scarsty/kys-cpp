#include "UISave.h"
#include "Save.h"
#include "File.h"
#include "others/libconvert.h"
#include "MainScene.h"
#include "SubScene.h"
#include "UI.h"

UISave::UISave()
{
    std::vector<std::string> strings;
    for (int i = 0; i <= 10; i++)
    {
        auto str = convert::formatString("進度%02d  %s", i, File::getFileTime(Save::getFilename(i, 'r')).c_str());
        strings.push_back(str);
    }
    auto str = convert::formatString("自動檔  %s", File::getFileTime(Save::getFilename(AUTO_SAVE_ID, 'r')).c_str());
    strings.push_back(str);
    setStrings(strings);
    childs_[0]->setVisible(false); //屏蔽进度0
    arrange(0, 0, 0, 28);
}

UISave::~UISave()
{
}

void UISave::onEntrance()
{
    //存档时屏蔽自动档
    if (mode_ == 1)
    {
        childs_.back()->setVisible(false);
    }
}

void UISave::onPressedOK()
{
    pressIndexToResult();
    if (result_ >= 0)
    {
        if (mode_ == 0 && Save::checkSaveFileExist(result_))
        {
            load(result_);
            setExit(true);
        }
        if (mode_ == 1)
        {
            save(result_);
            setExit(true);
        }
    }
}

void UISave::load(int r)
{
    auto sub_scene = getPointerFromRoot<SubScene>();  //可以知道在不在子场景中
    auto save = Save::getInstance();
    auto main_scene = MainScene::getIntance();
    save->load(r);
    main_scene->setManPosition(save->MainMapX, save->MainMapY);
    if (save->InSubMap >= 0)
    {
        if (sub_scene)
        {
            sub_scene->forceJumpSubScene(save->InSubMap, save->SubMapX, save->SubMapY);
        }
        else
        {
            main_scene->forceEnterSubScene(save->InSubMap, save->SubMapX, save->SubMapY);
        }
    }
    else
    {
        if (sub_scene)
        {
            sub_scene->forceExit();
        }
    }
    UI::getInstance()->setExit(true);
}

void UISave::save(int r)
{
    auto sub_scene = getPointerFromRoot<SubScene>();  //可以知道在不在子场景中
    auto save = Save::getInstance();
    auto main_scene = MainScene::getIntance();
    main_scene->getManPosition(save->MainMapX, save->MainMapY);
    if (sub_scene)
    {
        sub_scene->getManPosition(save->SubMapX, save->SubMapY);
        save->InSubMap = sub_scene->getMapInfo()->ID;
    }
    else
    {
        save->InSubMap = -1;
    }
    save->save(r);
}
