#pragma once
#include "Scene.h"
#include "Menu.h"
#include "Event.h"

class TitleScene : public Scene
{
public:
    TitleScene();
    ~TitleScene();

    void draw() override;
    void dealEvent(BP_Event& e) override;

    bool b_s = true;
    int m_y = 15;
    int speed = 2;
    int state = 0;

    Menu* menu_;
    Menu* menu_load_;

    int count_ = 0;
};

