#pragma once

#include "Random.h"
#include "BattleScene.h"

#include "yaml-cpp/yaml.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#include <memory>

// 先偷懒放一起
namespace BattleMod {
    typedef int Param;
    typedef std::vector<Param> Params;

    // 单个特效，描述特效功能
    class SpecialEffect {
    public:
        SpecialEffect(int id, const std::string& desc);
        int getId() const;
        std::string getDescription() const;
    private:
        int id_;
        const std::string description_;
    };

    // 特效绑定参数
    class EffectParamPair {
    public:
        EffectParamPair(const SpecialEffect& effect, Param p, const std::string& desc);
        // TODO 再加多参数的。。。 顺便试试variadic template
        Param getParam0();
        const Params& getParams();
        // 叠加方式可不同，现在就都一样吧
        virtual EffectParamPair & operator+=(const EffectParamPair & rhs);


        const SpecialEffect& effect;
        // 特效解说(显示文字?)
        const std::string description;

    protected:
        // 可以有多个参数
        // 现在每个参数是个int，可以继续配置
        // 比如根据武功/人物等级增加等
        // 到时候改上面的typedef
        Params params_;
    };

    class ProccableEffect {
    public:
        virtual ~ProccableEffect() {};
        // 这里可以添加其他条件检测
        // 比如武功组合 是否修炼其他武功
        // 是否拥有物品，天书数量，内力属性等等等等
        // proc返回触发的效果，nullptr为触发失败
        virtual EffectParamPair* proc(RandomDouble&) = 0;
    };

    enum class ProcProbability {
        random = 0,
        distributed = 1
    };

    // 单个计算
    class EffectSingle : public ProccableEffect {
    public:
        static const ProcProbability type = ProcProbability::random;
        EffectSingle(int p, EffectParamPair epp);
        EffectParamPair* proc(RandomDouble&) override;
    private:
        int percentProb_;
        EffectParamPair effectPair_;
    };

    // 一组特效，可以再继承，其他group类
    class EffectWeightedGroup : public ProccableEffect {
    public:
        static const ProcProbability type = ProcProbability::distributed;
        EffectParamPair* proc(RandomDouble&) override;
        EffectWeightedGroup(int total);
        void addProbEPP(int weight, EffectParamPair epp);
    private:
        // 发动参数，和特效
        std::vector<std::pair<int, EffectParamPair>> group_;
        int total_;
    };

    class EffectManager {
    public:
        bool hasEffect(int eid);
        Param getEffectParam0(int eid);
        Params getAllEffectParams(int eid);
        EffectParamPair* getEPP(int eid);
        // 合并管理的特效
        void unionEffects(const EffectManager& other);
        // 现在全部采取一个直接加的效果
        void addEPP(const EffectParamPair& ewp);
        std::size_t size();
        void clear();
    private:
        std::unordered_map<int, EffectParamPair> epps_;
    };

    typedef std::vector<std::unique_ptr<ProccableEffect>> Effects;
    typedef std::unordered_map<int, BattleMod::Effects> EffectsTable;

class BattleModifier : public BattleScene {
    
private:
    const std::string PATH = "../game/config/battle.yaml";
    std::vector<BattleMod::SpecialEffect> effects_;

    // 武功效果，不想碰Magic这个类，修改的东西太多
    // 主动武功效果，必须使用这个武功才行
    EffectsTable activeAtkMagic_;
    // 被动武功效果，这个武功存在，攻击就考虑特效
    EffectsTable passiveAtkMagic_;
    // 被动挨打的时候的效果
    EffectsTable passiveDefMagic_;
    // 武功 拥有既有的全局特效，进入战斗判断一次，一般概率为100%?
    EffectsTable globalMagic_;

    // 人物效果，不想碰Role这个类，修改的东西太多
    // 人物 主动攻击特效
    EffectsTable atkRole_;
    // 人物 被动挨打特效
    EffectsTable defRole_;
    // 人物 参战既有特效，进入战斗判断一次
    EffectsTable globalRole_;


    // 管理特效
    // 攻击特效管理器只有一个，在攻击结束后清空
    EffectManager atkEffectManager_;
    // 防御特效管理器只有一个，在挨打结束后清空
    EffectManager defEffectManager_;
    // 以下这两个最好是扔到Role里面，但是我不想碰那个文件
    // 每个参战人物有一个集气特效管理器，每时序params[1] -= 1，到0时清除
    // 每个参战人也有一个全局特效管理器
    // 这里用pid辨识
    std::unordered_map<int, EffectManager> timeEffectManagers_; // 还没做
    std::unordered_map<int, EffectManager> globalEffectManagers_;

    // simulation对calMagicHurt很不友好
    bool simulation_ = false;

    void init();
    std::unique_ptr<ProccableEffect> readProccableEffect(const YAML::Node& node);
    // TODO 确保EffectParamPair movable
    EffectParamPair readEffectParamPairs(const YAML::Node& node);
    void readIntoMapping(const YAML::Node& node, BattleMod::Effects& effects);
    std::vector<std::string> tryProcAndAddToManager(int id, const EffectsTable& table, EffectManager& manager);

public:
    // 我决定这不是一个singleton，不用重开游戏就可以测试
    BattleModifier();
    BattleModifier(int id);

    // 暂且考虑修改这些函数
    virtual void setRoleInitState(Role* r) override;
    virtual void useMagic(Role* r, Magic* m);
    virtual int calMagicHurt(Role* r1, Role* r2, Magic* magic);
    virtual int calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation = false);    //计算全部人物的伤害
    virtual void showNumberAnimation();
    // 这个是新加的
    virtual Role* semiRealPickOrTick();
};

}