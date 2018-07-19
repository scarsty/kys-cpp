#include "File.h"
#include "MainScene.h"
#include "Save.h"
#include "SubScene.h"
#include "UI.h"
#include "UISave.h"
#include "libconvert.h"
#include "Event.h"

UISave::UISave()
{
    std::vector<std::string> strings;
    auto get_save_time = [](int i) -> std::string
    {
        auto str = File::getFileTime(Save::getFilename(i, 'r'));
        if (str.empty())
        {
            str = "--------------------";
        }
        return str;
    };
    for (int i = 0; i <= 10; i++)
    {
        auto str = convert::formatString("�M��%02d  %s", i, get_save_time(i).c_str());
        strings.push_back(str);
    }
    auto str = convert::formatString("�Ԅәn  %s", get_save_time(AUTO_SAVE_ID).c_str());
    strings.push_back(str);
    setStrings(strings);
    childs_[0]->setVisible(false);    //���ν���0
    forceActiveChild(1);
    arrange(0, 0, 0, 28);
}

UISave::~UISave()
{
}

void UISave::onEntrance()
{
    //�浵ʱ�����Զ���
    if (mode_ == 1)
    {
        childs_.back()->setVisible(false);
    }
}

void UISave::onPressedOK()
{
    activeIndexToResult();
    if (result_ >= 0)
    {
        if (mode_ == 0 && Save::checkSaveFileExist(result_))
        {
            load(result_);
            setExit(true);
        }
        if (mode_ == 1)
        {
            Event::getInstance()->arrangeBag();    //�浵ʱ��������Ʒ����
            save(result_);
            setExit(true);
        }
    }
}

void UISave::load(int r)
{
    auto sub_scene = getPointerFromRoot<SubScene>();    //����֪���ڲ����ӳ�����
    auto save = Save::getInstance();
    auto main_scene = MainScene::getInstance();
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
    auto sub_scene = getPointerFromRoot<SubScene>();    //����֪���ڲ����ӳ�����
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
