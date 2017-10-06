#include "BattleOperator.h"
#include "BattleScene.h"
#include "Save.h"

BattleOperator::BattleOperator()
{
    head_selected_ = new Head();
    addChild(head_selected_);
}

BattleOperator::~BattleOperator()
{
}

void BattleOperator::setRoleAndMagic(Role* r, Magic* m /*= nullptr*/, int l /*= 0*/)
{
    role_ = r;
    magic_ = m;
    level_index_ = l;
    head_selected_->setRole(r);
}

void BattleOperator::dealEvent(BP_Event& e)
{
    auto battle_scene = dynamic_cast<BattleScene*>(battle_scene_);
    if (battle_scene == nullptr) { return; }

    if (e.type == BP_KEYDOWN)
    {
        int x = 0, y = 0;
        auto tw = Scene::getTowardsFromKey(e.key.keysym.sym);
        Scene::getTowardsPosition(battle_scene->select_x_, battle_scene->select_y_, tw, &x, &y);
        if (battle_scene->canSelect(x, y))
        {
            battle_scene->setSelectPosition(x, y);
            if (head_selected_->getVisible())
            {
                int r = battle_scene->role_layer_.data(x, y);
                head_selected_->setRole(Save::getInstance()->getRole(r));
            }
        }
        if (mode_ == Move)
        {
        }
        else if (mode_ == Action)
        {
            battle_scene->calEffectLayer(role_, magic_, level_index_);
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
    int w, h;
    Engine::getInstance()->getPresentSize(w, h);
    head_selected_->setPosition(w - 400, h - 150);
}
