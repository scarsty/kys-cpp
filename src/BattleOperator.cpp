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
    }
    else if (mode_ == Action)
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
            battle_scene->calEffectLayer(role_, magic_, level_index_);
        }
    }

    if (e.type == BP_KEYUP)
    {
        if (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE)
        {
            result_ = 0;
            setExit(true);
        }
        if (e.key.keysym.sym == BPK_ESCAPE)
        {
            result_ = -1;
            setExit(true);
        }
    }
}

void BattleOperator::dealMoveEvent(BP_Event& e)
{


}

void BattleOperator::dealActionEvent(BP_Event& e)
{

}

void BattleOperator::onEntrance()
{

}
