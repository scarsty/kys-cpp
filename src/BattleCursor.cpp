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

    int x = -1, y = -1;
    if (!role_->isAuto())
    {
        if (e.type == BP_KEYDOWN)
        {
            int tw = battle_scene_->getTowardsByKey(e.key.keysym.sym);
            //线型的特殊处理一下
            if (magic_ && magic_->AttackAreaType == 1)
            {
                Scene::getTowardsPosition(role_->X(), role_->Y(), tw, &x, &y);
            }
            else
            {
                Scene::getTowardsPosition(battle_scene_->select_x_, battle_scene_->select_y_, tw, &x, &y);
            }
        }
    }
    else
    {
        if (mode_ == Move)
        {
            x = role_->AI_MoveX;
            y = role_->AI_MoveY;
            setResult(0);
            setExit(true);
        }
        else if (mode_ == Action)
        {
            x = role_->AI_ActionX;
            y = role_->AI_ActionY;
            setResult(0);
            setExit(true);
        }
    }

    if (battle_scene_->canSelect(x, y))
    {
        battle_scene_->setSelectPosition(x, y);
        if (head_selected_->getVisible())
        {
            int r = battle_scene_->role_layer_->data(x, y);
            head_selected_->setRole(Save::getInstance()->getRole(r));
        }
        //ui的设定
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
        battle_scene_->calEffectLayer(role_, battle_scene_->select_x_, battle_scene_->select_y_, magic_, level_index_);
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
    battle_scene_->towards_ = role_->FaceTowards;
}
