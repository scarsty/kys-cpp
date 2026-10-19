#include "UISave.h"
#include "Event.h"
#include "MainScene.h"
#include "ScenePreloader.h"
#include "Save.h"
#include "ChessSaveFile.h"
#include "ChessTalent.h"
#include "DisplayText.h"
#include "SubScene.h"
#include "UI.h"
#include "ChessUiStyle.h"
#include "ChessUiCommon.h"
#include "Button.h"
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
    auto get_slot_display = [](int i) -> SlotDisplay
    {
        auto filename = Save::getInstance()->getFilename(i, '\0');
        SlotDisplay display{.timestamp = filefunc::getFileTime(filename)};
        display.status = display.timestamp.empty() ? "空檔" : "已儲存";
        if (const auto slot = KysChess::readChessSaveSlotFile(filename))
        {
            const auto talent = KysChess::parseChessTalent(slot->checkpoint.replay.header.talent);
            if (talent) display.status = std::format("{} · 第{}關", KysChess::chessTalentName(*talent), slot->checkpoint.state.fight + 1);
        }
        return display;
    };
    for (int i = 0; i <= 3; i++)
    {
        slotDisplay_[i] = get_slot_display(i);
        strings.push_back(std::format("進度{:02}", i));
    }
    slotDisplay_[static_cast<int>(Slot::Auto)] = get_slot_display(static_cast<int>(Slot::Auto));
    strings.push_back("自動檔");
    strings.push_back("返回");
    setStrings(strings);
    setFontSize(24);
    setFlatStyle(928, 64);
    setPosition(176, 128);
    childs_[0]->setVisible(false);    //屏蔽进度0
    forceActiveChild(1);
    arrange(0, 0, 0, 64);
    auto back = std::static_pointer_cast<Button>(childs_.back());
    back->setFlatStyle(120, 44);
    back->setFontSize(20);
    back->setFlatIcon(KysChess::ChessUiStyle::Icon::Left);
    back->setPosition(Engine::getInstance()->getUIWidth() - 152, 16);
}

void UISave::onEntrance()
{
    refreshEntries();
    //存档时屏蔽自动档
    if (mode_ == 1)
    {
        childs_[static_cast<int>(Slot::Auto)]->setVisible(false);
    }
}

void UISave::draw()
{
    KysChess::ChessUiStyle::page(mode_ == 1 ? "儲存進度" : "載入進度", "選擇存檔",
        KysChess::ChessUiStyle::Type::pageTitle);
    KysChess::ChessUiStyle::panel(160, 96, 960, 496);
    for (int i = 1; i <= static_cast<int>(Slot::Auto); ++i)
    {
        if (mode_ == 1 && i == static_cast<int>(Slot::Auto)) continue;
        const int y = 148 + (i - 1) * 64;
        KysChess::ChessUiStyle::label(slotDisplay_[i].status, 20,
            420, y,
            slotDisplay_[i].status == "空檔" ? KysChess::ChessUiStyle::muted : KysChess::ChessUiStyle::ink,
            260);
        KysChess::ChessUiStyle::label(slotDisplay_[i].timestamp, 18,
            744, y + 2, KysChess::ChessUiStyle::muted, 340);
    }
}

void UISave::onPressedOK()
{
    checkActiveToResult();
    if (result_ == static_cast<int>(Slot::Auto) + 1)
    {
        exitWithResult(-1);
        return;
    }
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
    bool loadSucceeded = false;
    ScenePreloader::showPromptAndPreload("讀取中...", [save, r, &loadSucceeded]() {
        loadSucceeded = save->load(r);
        if (loadSucceeded)
        {
            ScenePreloader::preloadSubSceneAssets(53);
            if (save->InSubMap >= 0 && save->InSubMap != 53)
            {
                ScenePreloader::preloadSubSceneAssets(save->InSubMap);
            }
        }
    });
    if (loadSucceeded)
    {
        main_scene->setManPosition(save->MainMapX, save->MainMapY);
        main_scene->setTowards(save->FaceTowards);
        if (save->InSubMap >= 0)
        {
            if (sub_scene)
            {
                sub_scene->forceJumpSubScene(save->InSubMap, save->SubMapX, save->SubMapY);
                sub_scene->setTowards(save->FaceTowards);
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
        KysChess::showChessMessage("存檔讀取失敗！");
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
        save->FaceTowards = sub_scene->towards_;
    }
    else
    {
        save->InSubMap = -1;
        save->FaceTowards = main_scene->towards_;
    }
    save->save(r);
}
