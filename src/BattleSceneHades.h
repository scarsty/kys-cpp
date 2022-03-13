﻿#pragma once
#include "BattleScene.h"
#include "BattleMap.h"
#include "Head.h"
#include "Save.h"
#include "UIKeyConfig.h"
#include <deque>
#include <unordered_map>

#include "Font.h"

//j轻攻击，i重攻击，m闪身
//每场战斗可以选择从4种武学中选择轻重
//硬直，判定范围，威力，消耗体力，在不攻击的时候可以回复体力

struct AttackEffect
{
    Pointf Pos;
    Pointf Velocity, Acceleration;
    Role* Attacker = nullptr;    //攻击者
    std::map<Role*, int> Defender;    //每人只能被一个特效击中一次
    Magic* UsingMagic = nullptr;
    Item* UsingHiddenWeapon = nullptr;
    int Frame = 0;    //当前帧数
    int TotalFrame = 1;    //总帧数，当前帧数超过此值就移除此效果
    int TotalEffectFrame = 1;    //效果总帧数
    int OperationType = -1;    //攻击类型
    std::string Path;    //效果贴图路径
    Role* FollowRole = nullptr;    //一直保持在角色身上
    int Weaken = 0;    //弱化程度，减掉
    double Strengthen = 1;    //强化程度，相乘
    int Track = 0;    //是否追踪
    int Through = 0;    //是否贯穿，即击中敌人后可以不消失
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

class BattleSceneHades : public BattleScene
{
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

    UIKeyConfig::Keys keys_;

    std::deque<AttackEffect> attack_effects_;
    std::deque<TextEffect> text_effects_;

    std::deque<Role*> enemies_;
    std::deque<Role> enemies_obj_;

    std::vector<std::shared_ptr<Head>> heads_;
    std::vector<std::shared_ptr<Head>> head_boss_;

    bool is_running_ = false;   //主角是否在跑动
    Role* role_ = nullptr;    //主角
    Role* dying_ = nullptr;    //主角
    int weapon_ = 1;
    int frozen_ = 0;
    int slow_ = 0;
    int shake_ = 0;

    std::shared_ptr<Menu> menu_;
    std::vector<std::shared_ptr<Button>> equip_magics_;

    std::unordered_map<std::string, std::function<int(Role* r)>> special_magic_effect_every_frame_;    //每帧
    std::unordered_map<std::string, std::function<int(Role* r)>> special_magic_effect_attack_;    //发动攻击
    std::unordered_map<std::string, std::function<int(AttackEffect&, Role* r)>> special_magic_effect_beat_;    //被打中

    Pointf pos45To90(int x, int y)    //45度坐标转为直角
    {
        Pointf p;
        p.x = -y * TILE_W + x * TILE_W + COORD_COUNT * TILE_W;
        p.y = y * TILE_W + x * TILE_W;
        return p;
    }

    Point pos90To45(double x, double y)    //直角坐标转为45度
    {
        x -= COORD_COUNT * TILE_W;
        Point p;
        p.x = round(((x) / TILE_W + (y) / TILE_W) / 2);
        p.y = round(((-x) / TILE_W + (y) / TILE_W) / 2);
        return p;
    }

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
    virtual bool canWalk(int x, int y) override { return canWalk45(x, y); }
    bool canWalk90(int x, int y)
    {
        auto p = pos90To45(x, y);
        return canWalk45(p.x, p.y);
    }
    bool canWalk90(Pointf p, Role* r, int dis = -1)
    {
        if (r->Pos.z > 20) { return true; }
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

    double EuclidDis(double x, double y)
    {
        return sqrt(x * x + y * y);
    }

    double EuclidDis(Pointf& p1, Pointf p2)
    {
        return EuclidDis(p1.x - p2.x, p1.y - p2.y);
    }
    void norm(double& x, double& y, double n0 = 1)
    {
        auto n = sqrt(x * x + y * y);
        if (n > 0)
        {
            x *= n0 / n;
            y *= n0 / n;
        }
    }

    void renderExtraRoleInfo(Role* r, double x, double y);
    //int calHurt(Role* r0, Role* r1);
    virtual int checkResult() override;
    virtual void setRoleInitState(Role* r) override;
    Role* findNearestEnemy(int team, Pointf p);
    Role* findFarthestEnemy(int team, Pointf p);
    int calCast(int act_type, int operation_type, Role* r);
    int calCoolDown(int act_type, int operation_type, Role* r);
    void decreaseToZero(int& i) { if (i > 0) { i--; } }
    int defaultMagicEffect(AttackEffect& ae, Role* r);
    int calRolePic(Role* r, int style, int frame) override;




    void makeSpecialMagicEffect();
};


