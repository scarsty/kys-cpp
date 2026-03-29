#pragma once
#include "Menu.h"

class UISave : public MenuText
{
public:
    UISave();
    ~UISave();

    enum class Slot
    {
        Auto = 4,
    };

private:
    int mode_ = 0;  //0为读档，1为存档
public:
    void setMode(int m) { mode_ = m; }
    void refreshEntries();

    void onEntrance() override;
    virtual void onPressedOK() override;

    static bool load(int r);
    static void save(int r);
    static void autoSave() { save(static_cast<int>(Slot::Auto)); }
    static bool loadAuto() { return load(static_cast<int>(Slot::Auto)); }
};
