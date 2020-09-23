#pragma once
#include "Head.h"
#include "Menu.h"
#include "UIItem.h"

class BattleScene;

//注意，AI选择行动的行为也在这里面
class BattleActionMenu : public MenuText
{
public:
    BattleActionMenu(BattleScene* b);
    virtual ~BattleActionMenu();

    //virtual void onEntrance() override;

    Role* role_ = nullptr;
    void setRole(Role* r);
    int runAsRole(Role* r)
    {
        setRole(r);
        return run();    //据说这里有潜在问题
    }

    void dealEvent(BP_Event& e) override;

    BattleScene* battle_scene_ = nullptr;

    int autoSelect(Role* role);

    MapSquareInt distance_layer_;

    void calDistanceLayer(int x, int y, int max_step = 64);

    struct AIAction
    {
        int Action;
        double point = 0;
        int MoveX, MoveY;
        int ActionX, ActionY;
        Magic* magic = nullptr;
        Item* item = nullptr;
    };

    void setAIActionToRole(AIAction& aa, Role* role)
    {
        role->AI_Action = aa.Action;
        role->AI_MoveX = aa.MoveX;
        role->AI_MoveY = aa.MoveY;
        role->AI_ActionX = aa.ActionX;
        role->AI_ActionY = aa.ActionY;
        role->AI_Magic = aa.magic;
        role->AI_Item = aa.item;
    }

    void getFarthestToAll(Role* role, std::vector<Role*> roles, int& x, int& y);
    void getNearestPosition(int x0, int y0, int& x, int& y);
    Role* getNearestRole(Role* role, std::vector<Role*> roles);
    void calAIActionNearest(Role* r2, AIAction& aa, Role* r_temp = nullptr);
    int calNeedActionDistance(AIAction& aa);
};

class BattleMagicMenu : public MenuText
{
public:
    BattleMagicMenu() {}
    virtual ~BattleMagicMenu() {}

    //virtual void onEntrance() override;

    Role* role_ = nullptr;
    Magic* magic_ = nullptr;
    void setRole(Role* r);
    int runAsRole(Role* r)
    {
        setRole(r);
        return run();
    }

    Magic* getMagic() { return magic_; }
    void onEntrance() override;

    virtual void onPressedOK() override;
    virtual void onPressedCancel() override
    {
        magic_ = nullptr;
        exitWithResult(-1);
    }
};

class BattleItemMenu : public UIItem
{
public:
    BattleItemMenu();
    virtual ~BattleItemMenu() {}

private:
    Role* role_ = nullptr;

public:
    void setRole(Role* r) { role_ = r; }
    Role* getRole() { return role_; }

    void addItem(Item* item, int count);

    void onEntrance() override;

    std::vector<Item*> getAvaliableItems();
    static std::vector<Item*> getAvaliableItems(Role* role, int type);
};
