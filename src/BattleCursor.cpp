#include "BattleCursor.h"
#include "BattleScene.h"
#include "Save.h"

BattleCursor::BattleCursor()
{
    head_selected_ = new Head();
    addChild(head_selected_);
    ui_status_ = new UIStatus();
    ui_status_->setVisible(false);
    ui_status_->setShowButton(false);
    addChild(ui_status_, 300, 0);
}

BattleCursor::~BattleCursor()
{
}

void BattleCursor::setRoleAndMagic(Role* r, Magic* m /*= nullptr*/, int l /*= 0*/)
{
    role_ = r;
    magic_ = m;
    level_index_ = l;
    head_selected_->setRole(r);
}

void BattleCursor::dealEvent(BP_Event& e)
{
    if (battle_scene_ == nullptr) { return; }
    if (e.type == BP_KEYDOWN)
    {
        int x = 0, y = 0;
        battle_scene_->changeTowardsByKey(e.key.keysym.sym);
        Scene::getTowardsPosition(battle_scene_->select_x_, battle_scene_->select_y_, battle_scene_->towards_, &x, &y);
        if (battle_scene_->canSelect(x, y))
        {
            battle_scene_->setSelectPosition(x, y);
            if (head_selected_->getVisible())
            {
                int r = battle_scene_->role_layer_->data(x, y);
                head_selected_->setRole(Save::getInstance()->getRole(r));
            }
            //uiµÄÉè¶¨
            if (ui_status_->getVisible())
            {
                int r = battle_scene_->role_layer_->data(x, y);
                ui_status_->setRole(Save::getInstance()->getRole(r));
            }
        }
        if (mode_ == Move)
        {
        }
        else if (mode_ == Action)
        {
            battle_scene_->calEffectLayer(role_, magic_, level_index_);
        }
    }
}

void BattleCursor::dealMoveEvent(BP_Event& e)
{


}

void BattleCursor::dealActionEvent(BP_Event& e)
{

}

void BattleCursor::onEntrance()
{
    int w, h;
    Engine::getInstance()->getPresentSize(w, h);
    head_selected_->setPosition(w - 400, h - 150);
}
