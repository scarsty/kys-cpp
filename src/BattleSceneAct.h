#pragma once
#include <deque>

#include "BattleScene.h"
#include "Camera.h"
#include "Font.h"
#include "PaperPresentation.h"
#include "TextureManager.h"
#include "UIKeyConfig.h"
#include <cmath>

//在即时战斗场景中，使用的是物理坐标，在画面上显示时y方向需要除以2
//x和y方向的最大值都是地面的宽度，即TILE_W * 64 * 2
//物理坐标到像素坐标的转换：x_pix = x_phy, y_pix = y_phy /2 - z_phy
//之后再根据主角所在的位置，计算出应该将整个像素纹理的哪个部分贴到屏幕上

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
            setPath(std::format("eft/eft{:03}", num));
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
        Pointf PaperPos;
        std::string Text;
        int Size = 30;
        int Frame = 0;
        int PaperRise = 0;
        Color color;
        int Type = 0;    //0-缓缓向上, 1-原地不动

        void set(const std::string& text, Color c, Role* r)
        {
            Text = text;
            color = c;
            if (r)
            {
                PaperPos = r->Pos;
                Pos = r->Pos;
                Pos.x -= 7.5 * Font::getTextDrawSize(Text);
                Pos.y -= 50;
            }
        }
    };

    Pointf pos_;    //坐标为俯视，而非在画面的位置，其中y需除以2画在上面
    float gravity_ = -4;
    float friction_ = 0.1;

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
    int paper_shake_ = 0;
    int close_up_ = 0;
    int sword_light_ = 0;
    Color sword_light_color_ = { 255, 255, 255, 255 };

    std::unordered_map<std::string, std::function<void(Role* r)>> special_magic_effect_every_frame_;            //每帧
    std::unordered_map<std::string, std::function<void(Role* r)>> special_magic_effect_attack_;                 //发动攻击
    std::unordered_map<std::string, std::function<void(AttackEffect&, Role* r)>> special_magic_effect_beat_;    //被打中

    void setID(int id);

    void backRun() override {}

    bool usePaperPresentation() const;
    void initializePaperPresentation();
    void drawPaperPresentation();
    void handlePaperPresentationEvent(EngineEvent& e);
    Role* findNearestEnemy(int team, Pointf p);

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

protected:
    struct ClassicDrawInfo
    {
        std::string path;
        int num = 0;
        Pointf p;
        Color color{ 255, 255, 255, 255 };
        uint8_t alpha = 255;
        int rot = 0;
        int rot2 = 0;
        int shadow = 0;
        uint8_t white = 0;
        int breathless = 0;
        int draw_turn = 1;
    };

    void drawClassicPresentation();
    virtual void adjustClassicRoleDrawInfo(Role* role, ClassicDrawInfo& info) {}
    virtual void drawClassicExtraEffects() {}
    virtual int swordLightYOffset() const { return 0; }
    int realTowardsToCameraFaceTowards(const Pointf& dir, const Pointf& view_dir,
        const Pointf& paper_right, int current_face_towards);
    Pointf getPaperMoveDirection(float input_right, float input_forward) const;

    Pointf camera_pos_ = { 1500, 1500, 200 };
    Pointf camera_focus_ = { 1500, 1500, 0 };
    float camera_angle_ = M_PI / 2;
    float camera_distance_ = 0;
    float free_camera_distance_ = 400;
    float free_camera_pitch_ = std::atan(200.0f / 400.0f);
    float camera_height_ = 200;
    bool camera_locked_ = false;
    PaperPresentation paper_presentation_;
    Camera camera_;
};
