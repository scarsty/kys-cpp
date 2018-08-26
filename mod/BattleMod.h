#pragma once

#include "Random.h"
#include "BattleScene.h"
#include "BattleConfig.h"

#define YAML_CPP_DLL
#include "yaml-cpp/yaml.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#include <memory>
#include <functional>

// 先偷懒放一起
namespace BattleMod {

class BattleModifier : public BattleScene {
    
private:
    const std::string PATH = "../game/config/battle.yaml";

    // 严禁复制string
    std::vector<std::string> strPool_;

    std::vector<BattleMod::SpecialEffect> effects_;
    std::vector<BattleStatus> battleStatus_;

    // 武功效果，不想碰Magic这个类，修改的东西太多
    // 主动武功效果，必须使用这个武功才行
    EffectsTable atkMagic_;
    // 被动挨打的时候的效果
    EffectsTable defMagic_;
    // 集气特效
    EffectsTable speedMagic_;
    // 回合/行动特效，仅在行动后发动，还有一个在行动前的慢慢添加
    EffectsTable turnMagic_;

    // 人物 攻击特效
    EffectsTable atkRole_;
    // 人物 挨打特效
    EffectsTable defRole_;
    // 人物 集气特效
    EffectsTable speedRole_;
    EffectsTable turnRole_;

    // 所有人物共享
    Effects atkAll_;
    Effects defAll_;
    Effects speedAll_;
    Effects turnAll_;


    // 管理特效
    // 攻击特效管理器只有一个，在攻击结束后清空
    EffectManager atkEffectManager_;
    // 防御特效管理器只有一个，在挨打结束后清空
    EffectManager defEffectManager_;
    // 集气特效 现在也只需要一个了！
    EffectManager speedEffectManager_;
    // 回合 也一个！
    EffectManager turnEffectManager_;

    std::unordered_map<int, BattleStatusManager> battleStatusManager_;

    // simulation对calMagicHurt很不友好
    bool simulation_ = false;
    // 多重攻击（别称 连击 左右互搏）
    int multiAtk_ = 0;

    void init();
    Variable readVariable(const YAML::Node& node);
    std::unique_ptr<Adder> readAdder(const YAML::Node& node);
    VariableParam readVariableParam(const YAML::Node& node);
    Conditions readConditions(const YAML::Node& node);
    Condition readCondition(const YAML::Node& node);
    std::unique_ptr<ProccableEffect> readProccableEffect(const YAML::Node& node);
    EffectParamPair readEffectParamPair(const YAML::Node& node);
    std::vector<EffectParamPair> readEffectParamPairs(const YAML::Node& node);
    void readIntoMapping(const YAML::Node& node, BattleMod::Effects& effects);

    std::vector<EffectIntsPair> tryProcAndAddToManager(const Effects& list, EffectManager& manager,
                                                       const Role* attacker, const Role* defender, const Magic* wg);
    std::vector<EffectIntsPair> tryProcAndAddToManager(int id, const EffectsTable& table, EffectManager& manager,
                                                       const Role* attacker, const Role* defender, const Magic* wg);

    void applyStatusFromParams(const std::vector<int>& params, Role* target);
    void setStatusFromParams(const std::vector<int>& params, Role* target);

public:

    // rng 就应该是个全局变量
    static RandomDouble rng;

    // 我决定这不是一个singleton，不用重开游戏就可以测试
    BattleModifier();
    BattleModifier(int id);

    void dealEvent(BP_Event & e) override;


    // 暂且考虑修改这些函数
    virtual void setRoleInitState(Role* r) override;

    virtual void actUseMagic(Role* r);           //武学

    virtual void useMagic(Role* r, Magic* m);

    virtual int calMagicHurt(Role* r1, Role* r2, Magic* magic);
    virtual int calMagiclHurtAllEnemies(Role* r, Magic* m, bool simulation = false);    //计算全部人物的伤害
    // virtual void showNumberAnimation();
    // 这个是新加的
    virtual Role* semiRealPickOrTick();

    void showMagicNames(const std::vector<std::reference_wrapper<const std::string>>& names);

};

}