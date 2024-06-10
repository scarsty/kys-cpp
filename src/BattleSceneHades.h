#pragma once
#include "BattleScene.h"
#include "Font.h"
#include "Head.h"
#include "TextureManager.h"
#include "UIKeyConfig.h"
#include <deque>
#include <unordered_map>

//j轻攻击，i重攻击，m闪身
//每场战斗可以选择从4种武学中选择轻重
//硬直，判定范围，威力，消耗体力，在不攻击的时候可以回复体力

class BattleSceneHades : public BattleScene
{
    struct AttackEffect
    {
        Pointf Pos;
        Pointf Velocity, Acceleration;
        Role* Attacker = nullptr;         //攻击者
        std::map<Role*, int> Defender;    //每人只能被一个特效击中一次
        Magic* UsingMagic = nullptr;
        Item* UsingHiddenWeapon = nullptr;
        int Frame = 0;                 //当前帧数
        int TotalFrame = 1;            //总帧数，当前帧数超过此值就移除此效果
        int TotalEffectFrame = 1;      //效果总帧数
        int OperationType = -1;        //攻击类型
        std::string Path;              //效果贴图路径
        Role* FollowRole = nullptr;    //一直保持在角色身上
        int Weaken = 0;                //弱化程度，减掉
        double Strengthen = 1;         //强化程度，相乘
        int Track = 0;                 //是否追踪
        int Through = 0;               //是否贯穿，即击中敌人后可以不消失
        int NoHurt = 0;                //是否无伤害
        void setEft(int num)
        {
            setPath(fmt1::format("eft/eft{:03}", num));
        }
        void setPath(const std::string& p)
        {
            Path = p;
            TotalEffectFrame = TextureManager::getInstance()->getTextureGroupCount(Path);
        }
    };

    struct TextEffect
    {
        Pointf Pos;
        std::string Text;
        int Size = 15;
        int Frame = 0;
        BP_Color Color;
        int Type = 0;    //0-缓缓向上, 1-原地不动
        void set(const std::string& text, BP_Color c, Role* r)
        {
            Text = text;
            Color = c;
            if (r)
            {
                Pos = r->Pos;
                Pos.x -= 7.5 * Font::getTextDrawSize(Text);
                Pos.y -= 50;
            }
        }
    };

public:
    BattleSceneHades();
    BattleSceneHades(int id);
    virtual ~BattleSceneHades();
    void setID(int id);

    //继承自基类的函数
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;     //战场主循环
    virtual void dealEvent2(BP_Event& e) override;    //用于停止自动
    virtual void onEntrance() override;
    virtual void onExit() override;
    virtual void backRun() override {}
    virtual void backRun1();
    void Action(Role* r);
    void AI(Role* r);
    virtual void onPressedCancel() override;

protected:
    Pointf pos_;    //坐标为俯视，而非在画面的位置，其中y需除以2画在上面
    double gravity_ = -4;
    double friction_ = 0.1;

    UIKeyConfig::Keys keys_;

    std::deque<AttackEffect> attack_effects_;
    std::deque<TextEffect> text_effects_;

    std::deque<Role*> enemies_;
    std::deque<Role> enemies_obj_;

    std::vector<std::shared_ptr<Head>> heads_;
    std::vector<std::shared_ptr<Head>> head_boss_;

    bool is_running_ = false;    //主角是否在跑动
    Role* role_ = nullptr;       //主角
    Role* dying_ = nullptr;
    int weapon_ = 1;
    int frozen_ = 0;
    int slow_ = 0;
    int shake_ = 0;
    int close_up_ = 0;

    std::shared_ptr<Menu> menu_;
    std::vector<std::shared_ptr<Button>> button_magics_;
    std::shared_ptr<Button> button_item_;
    std::shared_ptr<TextBox> show_auto_;

    std::unordered_map<std::string, std::function<void(Role* r)>> special_magic_effect_every_frame_;            //每帧
    std::unordered_map<std::string, std::function<void(Role* r)>> special_magic_effect_attack_;                 //发动攻击
    std::unordered_map<std::string, std::function<void(AttackEffect&, Role* r)>> special_magic_effect_beat_;    //被打中

    bool canWalk45(int x, int y)
    {
        if (isOutLine(x, y) || isBuilding(x, y) || isWater(x, y))
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    bool canWalk90(int x, int y)
    {
        auto p = pos90To45(x, y);
        return canWalk45(p.x, p.y);
    }
    bool canWalk90(Pointf p, Role* r, int dis = -1)
    {
        if (r->Pos.z > 1) { return true; }
        if (dis == -1) { dis = TILE_W; }
        for (auto r1 : battle_roles_)
        {
            if (r1 == r || r1->Dead) { continue; }
            double dis1 = EuclidDis(p, r1->Pos);
            if (dis1 < dis)
            {
                double dis0 = EuclidDis(r->Pos, r1->Pos);
                if (dis0 >= dis1) { return false; }
            }
        }
        auto p45 = pos90To45(p.x, p.y);
        return canWalk45(p45.x, p45.y);
    }
    virtual bool canWalk(int x, int y) override { return canWalk45(x, y); }

    void renderExtraRoleInfo(Role* r, double x, double y);
    //int calHurt(Role* r0, Role* r1);
    virtual int checkResult() override;
    virtual void setRoleInitState(Role* r) override;
    Role* findNearestEnemy(int team, Pointf p);
    Role* findFarthestEnemy(int team, Pointf p);
    int calCast(int act_type, int operation_type, Role* r);
    int calCoolDown(int act_type, int operation_type, Role* r);
    void decreaseToZero(int& i)
    {
        if (i > 0) { i--; }
    }
    void defaultMagicEffect(AttackEffect& ae, Role* r);
    virtual int calRolePic(Role* r, int style, int frame) override;

    virtual int calMagicHurt(Role* r1, Role* r2, Magic* magic, int dis = -1)
    {
        //计算武学对单人的伤害
        //注意原公式中距离为1是无衰减的
        //即时战斗，降低每一次攻击的威力，否则打得太快了
        return BattleScene::calMagicHurt(r1, r2, magic, 1) / 20.0;
    }

    void makeSpecialMagicEffect();
};
