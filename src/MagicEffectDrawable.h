#pragma once

#include "DrawableOnCall.h"
#include "BattleConfig.h"

class MagicEffectDrawable : public DrawableOnCall
{
public:
    MagicEffectDrawable(BattleMod::BattleConfManager& conf, Role * role, Role * opponent, int x, int y);
    virtual void updateScreenWithID(int id);
    virtual void draw();
private:
    BattleMod::BattleConfManager& conf_;
    Role * role_;
    Role * opponent_;
    int x_;
    int y_;
    std::vector<std::vector<std::pair<BP_Color, std::string>>> texts_;
};