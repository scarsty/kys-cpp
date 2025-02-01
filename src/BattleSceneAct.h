#pragma once
#include <deque>

#include "BattleScene.h"
#include "Font.h"
#include "TextureManager.h"
#include "UIKeyConfig.h"

class BattleSceneAct : public BattleScene
{
public:
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
        Color color;
        int Type = 0;    //0-缓缓向上, 1-原地不动

        void set(const std::string& text, Color c, Role* r)
        {
            Text = text;
            color = c;
            if (r)
            {
                Pos = r->Pos;
                Pos.x -= 7.5 * Font::getTextDrawSize(Text);
                Pos.y -= 50;
            }
        }
    };

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

    std::unordered_map<std::string, std::function<void(Role* r)>> special_magic_effect_every_frame_;            //每帧
    std::unordered_map<std::string, std::function<void(Role* r)>> special_magic_effect_attack_;                 //发动攻击
    std::unordered_map<std::string, std::function<void(AttackEffect&, Role* r)>> special_magic_effect_beat_;    //被打中

    void setID(int id);

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

    static void decreaseToZero(int& i)
    {
        if (i > 0) { i--; }
    }

    template <typename T>
    static void decreaseToZero(T& i, T v)
    {
        if (i > 0) { i -= v; }
        if (i < 0) { i = 0; }
    }
};
