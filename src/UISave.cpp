#include "UISave.h"
#include "Event.h"
#include "MainScene.h"
#include "ScenePreloader.h"
#include "Save.h"
#include "SubScene.h"
#include "UI.h"
#include "filefunc.h"

UISave::UISave()
{
    refreshEntries();
}

UISave::~UISave()
{
}

void UISave::refreshEntries()
{
    std::vector<std::string> strings;
    auto get_save_time = [](int i) -> std::string
    {
        auto filename = Save::getInstance()->getFilename(i, '\0');
        auto str = filefunc::getFileTime(filename);
        if (str.empty())
        {
            str = "-------------------";
        }
        return str;
    };
    for (int i = 0; i <= 3; i++)
    {
        auto str = std::format("進度{:02}  {}", i, get_save_time(i));
        strings.push_back(str);
    }
    auto str = std::format("自動檔  {}", get_save_time(static_cast<int>(Slot::Auto)));
    strings.push_back(str);
    setStrings(strings);
    setFontSize(32);
    childs_[0]->setVisible(false);    //屏蔽进度0
    forceActiveChild(1);
    arrange(0, 0, 0, 41);
}

void UISave::onEntrance()
{
    refreshEntries();
    //存档时屏蔽自动档
    if (mode_ == 1)
    {
        childs_.back()->setVisible(false);
    }
}

void UISave::onPressedOK()
{
    checkActiveToResult();
    if (result_ >= 0)
    {
        if (mode_ == 0)
        {
            setExit(load(result_));
        }
        if (mode_ == 1)
        {
            Event::getInstance()->arrangeBag();    //存档时会整理物品背包
            save(result_);
            setExit(true);
        }
    }
}

bool UISave::load(int r)
{
    auto sub_scene = getPointerFromRoot<SubScene>();    //可以知道在不在子场景中
    auto save = Save::getInstance();
    auto main_scene = MainScene::getInstance();
    if (save->load(r))
    {
        ScenePreloader::showPromptAndPreload("加載中...", [targetSubmap = save->InSubMap]() {
            ScenePreloader::preloadSubSceneAssets(53);
            if (targetSubmap >= 0 && targetSubmap != 53)
            {
                ScenePreloader::preloadSubSceneAssets(targetSubmap);
            }
        });
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
        return true;
    }
    else
    {
        auto text = std::make_shared<TextBox>();
        text->setText("存檔讀取失敗!");
        text->setFontSize(24);
        text->setTextColor({255, 0, 0});
        text->runAtPosition(Engine::getInstance()->getUIWidth() / 2 - 100, 620);
    }
    return false;
}

void UISave::save(int r)
{
    auto sub_scene = getPointerFromRoot<SubScene>();    //可以知道在不在子场景中
    auto save = Save::getInstance();
    auto main_scene = MainScene::getInstance();
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
