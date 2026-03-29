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
    virtual void dealEvent(EngineEvent& e) override;

    virtual void onEntrance() override;

private:
    void runExternalSaveFlow();
    int promptExternalSaveSlot();
    int promptExternalSaveAction(int slot);
    void exportExternalSaveSlot(int slot);
    void importExternalSaveSlot(int slot);

public:
    std::shared_ptr<Menu> menu_;
    std::shared_ptr<UISave> menu_load_;

    int count_ = 0;
    int head_id_ = 0;

    int head_x_, head_y_;

    int battle_mode_ = 0;
    bool hide_title_art_for_external_save_ = false;
#ifdef __EMSCRIPTEN__
    bool overlay_dismissed_ = false;
#endif
};
