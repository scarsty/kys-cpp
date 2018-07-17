#pragma once
#include "Scene.h"
#include "Menu.h"
#include "Event.h"
#include "UISave.h"

class TitleScene : public Scene
{
public:
    TitleScene();
    ~TitleScene();

    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;

    virtual void onEntrance() override;

    Menu menu_;
    UISave menu_load_;

    int count_ = 0;
    int head_id_ = 0;

    int head_x_, head_y_;
};

