#include "BattleOperator.h"
#include "BattleScene.h"

BattleOperator::BattleOperator()
{
}


BattleOperator::~BattleOperator()
{
}

void BattleOperator::dealEvent(BP_Event& e)
{
    auto battle_scene = dynamic_cast<BattleScene*>(battle_scene_);
    if (battle_scene == nullptr) { return; }

    if (mode_ == Move)
    {
        if (e.type == BP_KEYDOWN)
        {
            int x, y;
            auto tw = Scene::getTowardsFromKey(e.key.keysym.sym);
            Scene::getTowardsPosition(battle_scene->select_x_, battle_scene->select_y_, tw, &x, &y);
            if (battle_scene->canSelect(x, y))
            {
                battle_scene->setSelectPosition(x, y);
            }
        }
        if (e.type == BP_KEYUP)
        {
            if (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE)
            {
                setExit(true);
            }
        }
    }
}
