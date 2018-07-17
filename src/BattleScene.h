#pragma once
#include "BattleCursor.h"
#include "BattleMap.h"
#include "BattleMenu.h"
#include "Point.h"
#include "Random.h"
#include "Scene.h"

class BattleScene : public Scene
{
private:
    Save* save_;
    RandomDouble rand_;

public:
    BattleScene();
    BattleScene(int id);
    ~BattleScene();

    std::vector<Role*> battle_roles_;
    std::vector<Role*> friends_;    //保存开始就参战的人物，用来计算失败经验

    BattleActionMenu* battle_menu_;
    BattleCursor* battle_cursor_;
    Head* head_self_;

    //Head* head_selected_;

    int battle_id_ = 0;
    BattleInfo* info_;
    void setID(int id);

    //地面层，建筑层，选择层（负值为不可选，0和正值为可选）
    MapSquareInt *earth_layer_, *building_layer_, *select_layer_, *effect_layer_;

    //角色层
    MapSquare<Role*>* role_layer_;

    int select_state_ = 0;    //0-其他，1-选移动目标，2-选行动目标

    int select_x_ = 0, select_y_ = 0;
    void setSelectPosition(int x, int y)
    {
        select_x_ = x;
        select_y_ = y;
    }

    //以下画图用
    int action_frame_ = 0;
    int action_type_ = -1;
    int show_number_y_ = 0;
    int effect_id_ = -1;
    int effect_frame_ = 0;
    uint8_t dead_alpha_ = 255;
    const int animation_delay_ = 2;

    bool fail_exp_ = false;

    void setHaveFailExp(bool b) { fail_exp_ = b; }    //是否输了也有经验

    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;     //战场主循环
    virtual void dealEvent2(BP_Event& e) override;    //用于停止自动
    virtual void onEntrance() override;
    virtual void onExit() override;
    virtual void backRun() override;

    void readBattleInfo();                                          //读取战场人物的数据
    void setRoleInitState(Role* r);                                 //初始化人物的属性
    void setFaceTowardsNearest(Role* r, bool in_effect = false);    //以离得最近的敌人设置面向，参数为是否敌人在自己行动效果中
    void readFightFrame(Role* r);                                   //读取人物行动帧数
    void sortRoles();                                               //角色排序
    static bool compareRole(Role* r1, Role* r2);                    //角色排序的规则
    void resetRolesAct();                                           //设置所有人未行动过
    int calMoveStep(Role* r);                                       //计算可移动步数(考虑装备)
    int calActionStep(int ability) { return ability / 15 + 1; }     //依据能力值计算行动的范围步数
    int calRolePic(Role* r, int style = -1, int frame = 0);         //依据动作帧数计算角色的贴图编号

    //计算可以被选择的范围，会改写选择层
    //mode含义：0-移动，受步数和障碍影响；1攻击用毒医疗等仅受步数影响；2查看状态，全都能选；3仅能选直线的格子；4仅能选自己
    void calSelectLayer(Role* r, int mode, int step = 0);
    void calSelectLayer(int x, int y, int team, int mode, int step = 0);

    //计算武学可选择的范围
    void calSelectLayerByMagic(int x, int y, int team, Magic* magic, int level_index);

    //计算效果层，x，y，或者r为选择的中心点，即人所在的位置
    void calEffectLayer(Role* r, int select_x, int select_y, Magic* m = nullptr, int level_index = 0) { calEffectLayer(r->X(), r->Y(), select_x, select_y, m, level_index); }
    void calEffectLayer(int x, int y, int select_x, int select_y, Magic* m = nullptr, int level_index = 0);

    //所在坐标是否有效果
    bool haveEffect(int x, int y) { return effect_layer_->data(x, y) >= 0; }

    //r2是不是在效果层里面，且会被r1的效果打中
    bool inEffect(Role* r1, Role* r2);

    bool canSelect(int x, int y);

    void walk(Role* r, int x, int y, Towards t);

    virtual bool canWalk(int x, int y) override;
    bool isBuilding(int x, int y);
    bool isWater(int x, int y);
    bool isRole(int x, int y);
    bool isOutScreen(int x, int y) override;
    bool isNearEnemy(int team, int x, int y);    //是否x，y上的人物与team不一致

    //计算距离
    int calRoleDistance(Role* r1, Role* r2) { return calDistance(r1->X(), r1->Y(), r2->X(), r2->Y()); }
    int calDistanceRound(int x1, int x2, int y1, int y2) { return sqrt((x1 - y1) * (x1 - y1) + (x2 - y2) * (x2 - y2)); }
    Role* getSelectedRole();    //获取恰好在选择点的角色

    void action(Role* r);    //行动主控

    void actMove(Role* r);
    void actUseMagic(Role* r);
    void actUsePoison(Role* r);
    void actDetoxification(Role* r);
    void actMedcine(Role* r);
    void actUseHiddenWeapon(Role* r);
    void actUseDrag(Role* r);
    void actWait(Role* r);
    void actStatus(Role* r);
    void actAuto(Role* r);
    void actRest(Role* r);

    void moveAnimation(Role* r, int x, int y);                                 //移动动画
    void useMagicAnimation(Role* r, Magic* m);                                 //使用武学动画
    void actionAnimation(Role* r, int style, int effect_id, int shake = 0);    //行动动画
    void showMagicName(std::string name);

    int calMagicHurt(Role* r1, Role* r2, Magic* magic);
    int calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation = false);    //计算全部人物的伤害
    int calHiddenWeaponHurt(Role* r1, Role* r2, Item* item);

    void showNumberAnimation();
    void clearDead();
    void poisonEffect(Role* r);    //中毒效果

    int getTeamMateCount(int team);    //获取队员数目
    int checkResult();
    void calExpGot();    //计算经验
};