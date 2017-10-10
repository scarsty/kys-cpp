#pragma once
#include "BattleMap.h"
#include "Scene.h"
#include "Point.h"
#include "BattleMenu.h"
#include "BattleCursor.h"

class BattleScene : public Scene
{
private:
    Save* save_;
public:
    BattleScene();
    BattleScene(int id);
    ~BattleScene();

    std::vector<Role*> battle_roles_;

    BattleMenu* battle_menu_;
    BattleCursor* battle_cursor_;
    Head* head_self_;

    //Head* head_selected_;

    int battle_id_ = 0;
    BattleInfo* info_;
    void setID(int id);

    static const int COORD_COUNT = BATTLEMAP_COORD_COUNT;
    //地面层，建筑层，角色层，选择层（负值为不可选，0和正值为可选）
    MapSquare *earth_layer_, *building_layer_, *role_layer_, *select_layer_, *effect_layer_;

    int select_state_ = 0;  //0-其他，1-选移动目标，2-选行动目标

    int select_x_ = 0, select_y_ = 0;
    void setSelectPosition(int x, int y) { select_x_ = x; select_y_ = y; }

    //以下画图用
    int action_frame_ = 0;
    int action_type_ = -1;
    int show_number_y_ = 0;
    int effect_id_ = -1;
    int effect_frame_ = 0;
    uint8_t dead_alpha_ = 255;
    const int animation_delay_ = 2;
    
    bool fail_exp_ = false;

    void setHaveFailExp(bool b) { fail_exp_ = b; }

    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;
    virtual void onEntrance() override;
    virtual void onExit() override;

    void setRoleInitState(Role* r);
    void readFightFrame(Role* r);

    void sortRoles();
    static bool compareRole(Role* r1, Role* r2);
    void resetRolesAct();

    int calMoveStep(Role* r);

    int calRolePic(Role* r, int style = -1, int frame = 0);

    void calSelectLayer(Role* r, int mode, int step = 0);
    void calEffectLayer(Role* r, Magic* m = nullptr, int level_index = 0);

    bool canSelect(int x, int y);

    void walk(Role* r, int x, int y, Towards t);

    virtual bool canWalk(int x, int y) override;
    bool isBuilding(int x, int y);
    bool isOutLine(int x, int y);
    bool isWater(int x, int y);
    bool isRole(int x, int y);
    bool isOutScreen(int x, int y);
    bool isNearEnemy(Role* r, int x, int y);
    int calDistance(Role* r1, Role* r2) { return calDistance(r1->X(), r1->Y(), r2->X(), r2->Y()); }
    int calDistance(int x1, int y1, int x2, int y2) { return abs(x1 - x2) + abs(y1 - y2); }

    Role* getSelectedRole();

    //"移動", "武學", "用毒", "解毒", "醫療", "物品", "等待", "狀態", "自動", "結束"
    void actMove(Role* r);
    void actUseMagic(Role* r);
    void actUsePoison(Role* r);
    void actDetoxification(Role* r);
    void actMedcine(Role* r);
    void actUseItem(Role* r);
    void actWait(Role* r);
    void actStatus(Role* r);
    void actAuto(Role* r);
    void actRest(Role* r);

    void moveAnimation(Role* r, int x, int y);
    void useMagicAnimation(Role* r, Magic* m);
    void actionAnimation(Role* r, int style, int effect_id, int shake = 0);

    int calHurt(Role* r1, Role* r2, Magic* magic);
    int calAllHurt(Role* r, Magic* m);
    void showNumberAnimation();
    void clearDead();
    void poisonEffect(Role* r);

    int getTeamMateCount(int team);
    int checkResult();
    void calExpGot();

};