#include "BattleCursor.h"
#include "BattleScene.h"
#include "Save.h"

BattleCursor::BattleCursor(BattleScene* b)
{
    head_selected_ = std::make_shared<Head>();
    addChild(head_selected_);
    ui_status_ = std::make_shared<UIStatus>();
    ui_status_->setVisible(false);
    ui_status_->setShowButton(false);
    addChild(ui_status_, 300, 0);
    battle_scene_ = b;
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
    if (battle_scene_ == nullptr)
    {
        return;
    }
    if (!role_->isAuto())
    {
        int x = -1, y = -1;
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
                Scene::getTowardsPosition(battle_scene_->selectX(), battle_scene_->selectY(), tw, &x, &y);
            }
        }
        if (e.type == BP_MOUSEMOTION)
        {
            //线型的特殊处理一下
            if (magic_ && magic_->AttackAreaType == 1)
            {
                int tw = battle_scene_->getTowardsByMouse(e.motion.x, e.motion.y);
                Scene::getTowardsPosition(role_->X(), role_->Y(), tw, &x, &y);
            }
            else
            {
                auto p = battle_scene_->getMousePosition(e.motion.x, e.motion.y, role_->X(), role_->Y());
                x = p.x;
                y = p.y;
            }
        }
        setCursor(x, y);
    }
}

void BattleCursor::setCursor(int x, int y)
{
    if (battle_scene_->canSelect(x, y))
    {
        battle_scene_->setSelectPosition(x, y);
        if (head_selected_->getVisible())
        {
            head_selected_->setRole(battle_scene_->getRoleLayer()->data(x, y));
        }
        //ui的设定
        if (ui_status_->getVisible())
        {
            ui_status_->setRole(battle_scene_->getRoleLayer()->data(x, y));
        }
    }
    if (mode_ == Move)
    {
    }
    else if (mode_ == Action)
    {
        battle_scene_->calEffectLayer(role_, battle_scene_->selectX(), battle_scene_->selectY(), magic_, level_index_);
    }
}

void BattleCursor::onEntrance()
{
    if (battle_scene_ == nullptr)
    {
        setExit(true);
        setVisible(!exit_);
        return;
    }

    int w, h;
    Engine::getInstance()->getWindowSize(w, h);
    head_selected_->setPosition(w - 400, h - 150);
    battle_scene_->towards_ = role_->FaceTowards;

    if (role_->isAuto() || role_->Networked)
    {
        int x = -1, y = -1;

        if (mode_ == Move)
        {
            if (role_->Networked)
            {
                x = role_->Network_MoveX;
                y = role_->Network_MoveY;
            }
            else
            {
                x = role_->AI_MoveX;
                y = role_->AI_MoveY;
            }
        }
        else if (mode_ == Action)
        {
            if (role_->Networked)
            {
                x = role_->Network_ActionX;
                y = role_->Network_ActionY;
            }
            else
            {
                x = role_->AI_ActionX;
                y = role_->AI_ActionY;
            }
        }

        setResult(0);
        setExit(true);
        setVisible(!exit_);
        setCursor(x, y);
    }
}
