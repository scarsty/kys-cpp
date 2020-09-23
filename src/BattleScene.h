#pragma once
#include "BattleCursor.h"
#include "BattleMap.h"
#include "BattleMenu.h"
#include "Point.h"
#include "Random.h"
#include "Scene.h"

// WinSock.h(Event.cpp, SubScene.cpp, TitleScene.cpp) 和asio有冲突
// 仅在BattleScene.cpp中include BattleNetwork
// 所有调用网络连接的地方需自行include BattleNetwork (先BattleNetwork后windows.h)
class BattleNetwork;

class BattleScene : public Scene
{
public:
    static RandomDouble rng_;

    BattleScene();
    BattleScene(int id);
    virtual ~BattleScene();
    void setID(int id);

    //继承自基类的函数
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;     //战场主循环
    virtual void dealEvent2(BP_Event& e) override;    //用于停止自动
    virtual void onEntrance() override;
    virtual void onExit() override;
    virtual void backRun() override;

protected:
    int battle_id_ = 0;
    BattleInfo* info_;

    Save* save_;

    std::vector<Role*> battle_roles_;    //所有参战角色
    std::vector<Role*> friends_;         //开始就参战的我方角色，用来计算失败经验
    Role* acting_role_ = nullptr;        //当前正在行动中的角色

    std::shared_ptr<BattleActionMenu> battle_menu_;    //战斗行动菜单
    std::shared_ptr<BattleCursor> battle_cursor_;      //战斗时的光标
    std::shared_ptr<Head> head_self_;                  //头像

    //地面层，建筑层，选择层（负值为不可选，0和正值为可选），效果层
    MapSquareInt earth_layer_, building_layer_, select_layer_, effect_layer_;

    //角色层
    MapSquare<Role*> role_layer_;

    int select_type_ = 0;    //0-其他，1-选移动目标，2-选行动目标（好像没被用到）

    //光标选择的位置
    int select_x_ = 0, select_y_ = 0;

    //以下画图用
    int action_frame_ = 0;
    int action_type_ = -1;
    int show_number_y_ = 0;
    int effect_id_ = -1;
    int effect_frame_ = 0;
    uint8_t dead_alpha_ = 255;
    static const int animation_delay_ = 2;

    // 动画，或者需要多写一坨if else等
    std::function<void()> actionAnimation_ = nullptr;

    bool fail_exp_ = false;    //输后是否有经验

    int semi_real_ = 0;    //是否半即时

    std::unique_ptr<BattleNetwork> network_;    // 网络连接

public:
    void setSelectPosition(int x, int y)    //设置选择的坐标
    {
        select_x_ = x;
        select_y_ = y;
    }

    const std::vector<Role*>& getBattleRoles() { return battle_roles_; }
    MapSquare<Role*>* getRoleLayer() { return &role_layer_; }
    int selectX() { return select_x_; }
    int selectY() { return select_y_; }

    virtual void setHaveFailExp(bool b) { fail_exp_ = b; }    //是否输了也有经验

    virtual void readBattleInfo();                                          //读取战场人物的数据
    virtual void setRoleInitState(Role* r);                                 //初始化人物的属性
    virtual void setFaceTowardsNearest(Role* r, bool in_effect = false);    //以离得最近的敌人设置面向，参数为是否敌人在自己行动效果中
    virtual void readFightFrame(Role* r);                                   //读取人物行动帧数
    virtual void sortRoles();                                               //角色排序
    virtual void resetRolesAct();                                           //设置所有人未行动过
    virtual int calMoveStep(Role* r);                                       //计算可移动步数(考虑装备)
    virtual int calActionStep(int ability) { return ability / 15 + 1; }     //依据能力值计算行动的范围步数
    virtual int calRolePic(Role* r, int style = -1, int frame = 0);         //依据动作帧数计算角色的贴图编号

    //计算可以被选择的范围，会改写选择层
    //mode含义：0-移动，受步数和障碍影响；1攻击用毒医疗等仅受步数影响；2查看状态，全都能选；3仅能选直线的格子；4仅能选自己
    virtual void calSelectLayer(Role* r, int mode, int step = 0);
    virtual void calSelectLayer(int x, int y, int team, int mode, int step = 0);

    //计算武学可选择的范围
    virtual void calSelectLayerByMagic(int x, int y, int team, Magic* magic, int level_index);

    //计算效果层，x，y，或者r为选择的中心点，即人所在的位置
    virtual void calEffectLayer(Role* r, int select_x, int select_y, Magic* m = nullptr, int level_index = 0) { calEffectLayer(r->X(), r->Y(), select_x, select_y, m, level_index); }
    virtual void calEffectLayer(int x, int y, int select_x, int select_y, Magic* m = nullptr, int level_index = 0);

    //所在坐标是否有效果
    virtual bool haveEffect(int x, int y) { return effect_layer_.data(x, y) >= 0; }

    //r2是不是在效果层里面，且会被r1的效果打中
    virtual bool inEffect(Role* r1, Role* r2);

    virtual bool canSelect(int x, int y);

    virtual void walk(Role* r, int x, int y, Towards t);

    virtual bool canWalk(int x, int y) override;
    virtual bool isBuilding(int x, int y);
    virtual bool isWater(int x, int y);
    virtual bool isRole(int x, int y);
    virtual bool isOutScreen(int x, int y) override;
    virtual bool isNearEnemy(int team, int x, int y);    //是否x，y上的人物与team不一致

    virtual int calRoleDistance(Role* r1, Role* r2) { return calDistance(r1->X(), r1->Y(), r2->X(), r2->Y()); }                     //计算距离
    virtual int calDistanceRound(int x1, int x2, int y1, int y2) { return sqrt((x1 - y1) * (x1 - y1) + (x2 - y2) * (x2 - y2)); }    //计算欧氏距离

    virtual Role* getSelectedRole();    //获取恰好在选择点的角色

    virtual void action(Role* r);    //行动主控

    virtual void actMove(Role* r);                         //移动
    virtual void actUseMagic(Role* r);                     //武学
    virtual void actUseMagicSub(Role* r, Magic* magic);    //选择武功后，使用
    virtual void actUsePoison(Role* r);                    //用毒
    virtual void actDetoxification(Role* r);               //解毒
    virtual void actMedicine(Role* r);                     //医疗
    virtual void actUseHiddenWeapon(Role* r);              //暗器
    virtual void actUseDrug(Role* r);                      //吃药
    virtual void actWait(Role* r);                         //等待
    virtual void actStatus(Role* r);                       //状态
    virtual void actAuto(Role* r);                         //自动
    virtual void actRest(Role* r);                         //休息

    virtual void moveAnimation(Role* r, int x, int y);                                 //移动动画
    virtual void useMagicAnimation(Role* r, Magic* m);                                 //使用武学动画
    virtual void actionAnimation(Role* r, int style, int effect_id, int shake = 0);    //行动动画

    virtual int calMagicHurt(Role* r1, Role* r2, Magic* magic);                         //计算武学对单人的伤害
    virtual int calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation = false);    //计算全部人物的伤害

    virtual int calHiddenWeaponHurt(Role* r1, Role* r2, Item* item);    //计算暗器伤害

    virtual void showMagicName(std::string name);    //显示武学名

    //显示数字
    virtual void showNumberAnimation(
        int delay = animation_delay_,                                       // 跑几次
        bool floating = true,                                               // 文字是否悬浮
        const std::vector<std::pair<int&, int>>& animated_changes = {});    // 是否渐变某些变量

    virtual void renderExtraRoleInfo(Role* r, int x, int y);    // 在人物上，显示血条等

    virtual void clearDead();              //清除被击退的角色
    virtual void poisonEffect(Role* r);    //中毒效果

    virtual int getTeamMateCount(int team);    //获取队员数目
    virtual int checkResult();                 //检查结果
    virtual void calExpGot();                  //计算经验

    //设置网络连接
    virtual void setupNetwork(std::unique_ptr<BattleNetwork> net, int battle_id = 67);
    virtual void setupRolePosition(Role* r, int team, int x, int y);
    virtual void receiveAction(Role* r);
    virtual void sendAction(Role* r);
};
