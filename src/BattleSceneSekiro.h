#pragma once
#include "BattleScene.h"
#include "Font.h"
#include "Head.h"
#include "TextureManager.h"
#include "UIKeyConfig.h"
#include <deque>
#include <unordered_map>

class BattleSceneSekiro : public BattleScene
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
    BattleSceneSekiro();
    void setID(int id);
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;     //战场主循环
    virtual void dealEvent2(BP_Event& e) override;    //用于停止自动
    virtual void onEntrance() override;
    virtual void onExit() override;
    virtual void backRun() override {}
    virtual void backRun1();
    void Action(Role* r);
    void AI(Role* r);
    virtual void onPressedCancel() override {}

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
        if (dis == -1) { dis = TILE_W * 1.5; }
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

    virtual int checkResult() override;
    virtual void setRoleInitState(Role* r) override;

    void renderExtraRoleInfo(Role* r, double x, double y);
    Role* findNearestEnemy(int team, Pointf p);
    Role* findFarthestEnemy(int team, Pointf p);
    int calCast(int act_type, int operation_type, Role* r);
    int calCoolDown(int act_type, int operation_type, Role* r);
    void decreaseToZero(int& i)
    {
        if (i > 0) { i--; }
    }
    template <typename T>
    void decreaseToZero(T& i, T v)
    {
        if (i > 0) { i -= v; }
        if (i < 0) { i = 0; }
    }
    void defaultMagicEffect(AttackEffect& ae, Role* r);
    virtual int calRolePic(Role* r, int style, int frame) override;

protected:
    const double MAX_POSTURE = 100;
    Pointf pos_;    //坐标为俯视，而非在画面的位置，其中y需除以2画在上面
    double gravity_ = -4;
    double friction_ = 0.3;

    std::deque<Role*> enemies_;
    std::deque<Role> enemies_obj_;

    std::vector<std::shared_ptr<Head>> heads_;
    std::vector<std::shared_ptr<Head>> head_boss_;

    std::deque<AttackEffect> attack_effects_;
    std::deque<TextEffect> text_effects_;

    bool is_running_ = false;    //主角是否在跑动
    Role* role_ = nullptr;

    UIKeyConfig::Keys keys_;

    int weapon_ = 1;
    int frozen_ = 0;
    int slow_ = 0;
    int shake_ = 0;
    int close_up_ = 0;
    int sword_light_ = 0;
    BP_Color sword_light_color_ = { 255, 255, 255, 255 };
    int switch_magic_ = 0;

    std::unordered_map<std::string, std::function<void(Role* r)>> special_magic_effect_every_frame_;            //每帧
    std::unordered_map<std::string, std::function<void(Role* r)>> special_magic_effect_attack_;                 //发动攻击
    std::unordered_map<std::string, std::function<void(AttackEffect&, Role* r)>> special_magic_effect_beat_;    //被打中

    int easy_block_ = 0;
};

//暂时设计：
//身上可以携带4个物品（或者干脆不要）
//战斗中可以按键切换当前武功（例如十字键上下）
//AI初步设计为兜圈，寻机突进攻击，高手的攻击会快一些
//我方可以看破或者格挡，增加敌人的架势条（使用体力）
