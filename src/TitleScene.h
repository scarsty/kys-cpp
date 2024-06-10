#pragma once
#include "Scene.h"
#include "Menu.h"
#include "UISave.h"

class TitleScene : public Scene
{
public:
    TitleScene();
    ~TitleScene();

    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;

    virtual void onEntrance() override;

    std::shared_ptr<Menu> menu_;
    std::shared_ptr<UISave> menu_load_;

    int count_ = 0;
    int head_id_ = 0;

    int head_x_, head_y_;

    int battle_mode_ = 0;
};

