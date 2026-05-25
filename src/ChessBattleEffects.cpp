#include "ChessBattleEffects.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <format>
#include <print>

namespace KysChess
{

static const std::map<std::string, EffectType> effectTypeMap = {
    {"生命加成", EffectType::FlatHP}, {"攻击加成", EffectType::FlatATK},
    {"防御加成", EffectType::FlatDEF}, {"速度加成", EffectType::FlatSPD},
    {"生命百分比", EffectType::PctHP}, {"攻击百分比", EffectType::PctATK},
    {"防御百分比", EffectType::PctDEF}, {"速度百分比", EffectType::PctSPD},
    {"全队生命加成", EffectType::TeamFlatHP}, {"全队攻击加成", EffectType::TeamFlatATK},
    {"全队防御加成", EffectType::TeamFlatDEF}, {"全队速度加成", EffectType::TeamFlatSPD},
    {"全队生命百分比", EffectType::TeamPctHP}, {"全队攻击百分比", EffectType::TeamPctATK},
    {"全队防御百分比", EffectType::TeamPctDEF}, {"全队速度百分比", EffectType::TeamPctSPD},
    {"计作羁绊", EffectType::ActAsCombo},
    {"每勝生命加成", EffectType::FightWinHP}, {"每胜生命加成", EffectType::FightWinHP},
    {"每勝攻防加成", EffectType::FightWinATKDEF}, {"每胜攻防加成", EffectType::FightWinATKDEF},
    {"防御削减", EffectType::NegPctDEF},
    {"固定减伤", EffectType::FlatDmgReduction}, {"固定加伤", EffectType::FlatDmgIncrease}, {"格挡几率", EffectType::BlockChance},
    {"闪避几率", EffectType::DodgeChance}, {"闪避后暴击", EffectType::DodgeThenCrit},
    {"暴击几率", EffectType::CritChance}, {"暴击伤害", EffectType::CritMultiplier},
    {"每N次双倍", EffectType::EveryNthDouble}, {"穿甲几率", EffectType::ArmorPenChance},
    {"穿甲百分比", EffectType::ArmorPenPct}, {"穿甲", EffectType::ArmorPen},
    {"眩晕", EffectType::Stun},
    {"击退几率", EffectType::KnockbackChance}, {"中毒伤害", EffectType::PoisonDOT},
    {"中毒增伤", EffectType::PoisonDmgAmp}, {"命中回蓝", EffectType::MPOnHit},
    {"命中回血", EffectType::HPOnHit},
    {"吸取内力", EffectType::MPDrain}, {"回蓝加成", EffectType::MPRecoveryBonus},
    {"技能伤害", EffectType::SkillDmgPct}, {"技能反弹", EffectType::SkillReflectPct},
    {"冷却缩减", EffectType::CDR}, {"护盾值", EffectType::FlatShield}, {"护盾生命比", EffectType::ShieldPctMaxHP},
    {"护盾僵直抗性", EffectType::ShieldFreezeRes}, {"治疗光环", EffectType::HealAuraPct},
    {"固定治疗光环", EffectType::HealAuraFlat},
    {"受治疗加速", EffectType::HealedATKSPDBoost}, {"生命回复", EffectType::HPRegenPct},
    {"僵直抗性", EffectType::FreezeReductionPct}, {"僵直护盾", EffectType::ControlImmunityFrames}, {"僵直吸收", EffectType::ControlImmunityFrames},
    {"击杀回血", EffectType::KillHealPct}, {"击杀无敌", EffectType::KillInvincFrames},
    {"技能后无敌", EffectType::PostSkillInvincFrames}, {"伤害减免", EffectType::DmgReductionPct},
    {"嗜血", EffectType::Bloodlust}, {"击杀增攻", EffectType::Bloodlust},
    {"适应", EffectType::Adaptation}, {"同敌减伤", EffectType::Adaptation},
    {"连击蓄力", EffectType::RampingDmg}, {"连击增伤", EffectType::RampingDmg},
    {"回血", EffectType::HealBurst},
    {"流血", EffectType::BleedChance},
    {"绝招后退", EffectType::PostSkillDash}, {"敌方攻防削弱", EffectType::EnemyTopDebuff},
    {"闪击", EffectType::BlinkAttack}, {"同袍之死", EffectType::AllyDeathStatBoost},
    {"七截分身", EffectType::CloneSummon}, {"弹反", EffectType::ProjectileReflect},
    {"连锁弹", EffectType::ProjectileBounce},
    {"群体施治", EffectType::OnSkillTeamHeal},
    {"群体施治百分比", EffectType::OnSkillTeamHealPct},
    {"死亡庇护", EffectType::DeathPrevention}, {"死亡医疗", EffectType::DeathMedical},
    {"保护挪移", EffectType::ForcePullProtect},
    {"处决挪移", EffectType::ForcePullExecute}, {"斩杀", EffectType::Execute},
    {"破罡", EffectType::MPBlock}, {"封内", EffectType::MPBlock}, {"倾国倾城", EffectType::CharmCDRDebuff}, {"冷却延长反击", EffectType::CharmCDRDebuff},
    {"攻击倾城", EffectType::OffensiveCharm}, {"攻击冷却延长", EffectType::OffensiveCharm}, {"殉爆", EffectType::DeathAOE},
    {"护盾爆炸", EffectType::ShieldExplosion}, {"临时攻击加成", EffectType::TempFlatATK},
    {"护盾重获", EffectType::ShieldOnAllyDeath}, {"自动绝招", EffectType::AutoUltimate}, {"回内力", EffectType::MPRestore},
    {"周期免伤", EffectType::DamageImmunityAfterFrames}, {"周期绝招", EffectType::AutoUltimateAfterFrames},
    {"绝招追加弹", EffectType::UltimateExtraProjectiles},
    {"初次格挡", EffectType::BlockFirstHits},
    {"金币加成", EffectType::GoldCoefficient},
    {"同敌闪避", EffectType::DodgeAdaptation},
    {"受伤无敌", EffectType::HurtInvincFrames},
    {"滑步攻击", EffectType::DashAttack},
    {"滑步概率提升", EffectType::DashChanceBoost},
    {"当前内力加伤", EffectType::MPRatioDmgBoost},
    {"伤害降低", EffectType::DmgReduceDebuff},
    {"当前生命伤害", EffectType::CurrentHPPctBlast},
    {"全队回内", EffectType::TeamMPRestore},
    {"螺旋流血弹", EffectType::SpiralBleedProjectile},
    {"范围追踪弹", EffectType::NearbyTrackingProjectiles},
    {"远程化", EffectType::ForceRangedAttack},
    {"格挡绝招反击", EffectType::CounterUltimateBlock},
    {"单次承伤上限", EffectType::MaxHitPctCurrentHP},
    {"免费刷新", EffectType::FreeRefresh},
    {"选择战场", EffectType::BattleMapChoice},
};

static const std::map<std::string, Trigger> triggerMap = {
    {"低血量", Trigger::WhileLowHP},
    {"友方低血狂暴", Trigger::AllyLowHPBurst},
    {"最后存活", Trigger::LastAlive},
    {"攻擊出手時概率", Trigger::OnCast},
    {"攻击出手时概率", Trigger::OnCast},
    {"绝招时", Trigger::OnUltimate},
    {"攻击命中时概率", Trigger::OnHit},
    {"被击中时概率", Trigger::OnBeingHit},
    {"护盾爆炸时", Trigger::OnShieldBreak},
};

const std::map<std::string, EffectType>& ChessBattleEffects::getEffectTypeMap()
{
    return effectTypeMap;
}

int sumAlwaysEffectValue(const RoleComboState& state, EffectType type)
{
    int total = 0;
    for (const auto& effect : state.appliedEffects)
    {
        if (effect.type == type && effect.trigger == Trigger::Always)
        {
            total += effect.value;
        }
    }
    return total;
}

int maxAlwaysEffectValue(const RoleComboState& state, EffectType type)
{
    int value = 0;
    for (const auto& effect : state.appliedEffects)
    {
        if (effect.type == type && effect.trigger == Trigger::Always)
        {
            value = std::max(value, effect.value);
        }
    }
    return value;
}

int maxAlwaysEffectValue2(const RoleComboState& state, EffectType type)
{
    int value = 0;
    for (const auto& effect : state.appliedEffects)
    {
        if (effect.type == type && effect.trigger == Trigger::Always)
        {
            value = std::max(value, effect.value2);
        }
    }
    return value;
}

const AppliedEffectInstance* firstAlwaysEffect(const RoleComboState& state, EffectType type)
{
    for (const auto& effect : state.appliedEffects)
    {
        if (effect.type == type && effect.trigger == Trigger::Always)
        {
            return &effect;
        }
    }
    return nullptr;
}

const AppliedEffectInstance* maxAlwaysEffectByValue(const RoleComboState& state, EffectType type)
{
    const AppliedEffectInstance* selected = nullptr;
    for (const auto& effect : state.appliedEffects)
    {
        if (effect.type != type || effect.trigger != Trigger::Always)
        {
            continue;
        }
        if (!selected || effect.value > selected->value)
        {
            selected = &effect;
        }
    }
    return selected;
}

bool ChessBattleEffects::parseEffect(const YAML::Node& eNode, ComboEffect& out, const std::string& context)
{
    auto mark = eNode.Mark();
    auto fail = [&](const std::string& msg) {
        if (!mark.is_null())
            std::print("【效果配置】「{}」(第{}行，第{}列) {}\n", context, mark.line + 1, mark.column + 1, msg);
        else
            std::print("【效果配置】「{}」{}\n", context, msg);
        return false;
    };

    auto readString = [&](const char* key, std::string& dst, bool required) {
        auto node = eNode[key];
        if (!node)
            return !required ? true : fail(std::format("缺少「{}」字段", key));
        try
        {
            dst = node.as<std::string>();
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            return fail(std::format("「{}」字段不是有效字符串: {}", key, ex.what()));
        }
    };

    auto readInt = [&](const char* key, int& dst, bool required) {
        auto node = eNode[key];
        if (!node)
            return !required ? true : fail(std::format("缺少「{}」字段", key));
        try
        {
            dst = node.as<int>();
            return true;
        }
        catch (const YAML::Exception& ex)
        {
            return fail(std::format("「{}」字段不是有效整数: {}", key, ex.what()));
        }
    };

    if (!eNode || !eNode.IsMap())
        return fail("效果节点必须是映射表");

    try
    {
        out = {};

        std::string typeName;
        if (!readString("类型", typeName, true))
            return false;

        auto etIt = effectTypeMap.find(typeName);
        if (etIt == effectTypeMap.end())
            return fail(std::format("效果类型「{}」无法识别", typeName));

        out.type = etIt->second;
        const bool requiresValue = out.type != EffectType::ActAsCombo;
        if (!readInt("数值", out.value, requiresValue))
            return false;
        if (out.type == EffectType::ActAsCombo)
        {
            if (!readString("名称", out.text, true))
                return false;
        }
        if (!readInt("附加参数", out.value2, false))
            return false;
        if (!readInt("持续帧数", out.duration, false))
            return false;

        if (out.type == EffectType::ProjectileBounce)
            out.trigger = Trigger::OnHit;

        if (eNode["触发"])
        {
            std::string trigName;
            if (!readString("触发", trigName, true))
                return false;

            auto trIt = triggerMap.find(trigName);
            if (trIt == triggerMap.end())
                return fail(std::format("触发类型「{}」无法识别", trigName));

            out.trigger = trIt->second;
            if (!readInt("触发参数", out.triggerValue, false))
                return false;
        }

        if (!readInt("次数", out.maxCount, false))
            return false;

        return true;
    }
    catch (const YAML::Exception& ex)
    {
        return fail(std::format("解析效果时发生 YAML 异常: {}", ex.what()));
    }
}

void ChessBattleEffects::applyEffect(RoleComboState& s, const ComboEffect& e, int sourceComboId)
{
    AppliedEffectInstance instance;
    static_cast<ComboEffect&>(instance) = e;
    instance.sourceComboId = sourceComboId;
    s.appliedEffects.push_back(instance);

    if (e.trigger != Trigger::Always)
    {
        s.triggeredEffects.push_back(instance);
        return;
    }
    switch (e.type)
    {
    case EffectType::FlatHP:  s.flatHP += e.value; break;
    case EffectType::FlatATK: s.flatATK += e.value; break;
    case EffectType::FlatDEF: s.flatDEF += e.value; break;
    case EffectType::FlatSPD: s.flatSPD += e.value; break;
    case EffectType::PctHP:   s.pctHP += e.value; break;
    case EffectType::PctATK:  s.pctATK += e.value; break;
    case EffectType::PctDEF:  s.pctDEF += e.value; break;
    case EffectType::PctSPD:  s.pctSPD += e.value; break;
    case EffectType::TeamFlatHP: s.flatHP += e.value; break;
    case EffectType::TeamFlatATK: s.flatATK += e.value; break;
    case EffectType::TeamFlatDEF: s.flatDEF += e.value; break;
    case EffectType::TeamFlatSPD: s.flatSPD += e.value; break;
    case EffectType::TeamPctHP: s.pctHP += e.value; break;
    case EffectType::TeamPctATK: s.pctATK += e.value; break;
    case EffectType::TeamPctDEF: s.pctDEF += e.value; break;
    case EffectType::TeamPctSPD: s.pctSPD += e.value; break;
    case EffectType::ActAsCombo: break;
    case EffectType::FightWinHP:
        s.fightWinGrowthHP += e.value;
        break;
    case EffectType::FightWinATKDEF:
        s.fightWinGrowthATK += e.value;
        s.fightWinGrowthDEF += e.value2 > 0 ? e.value2 : e.value;
        break;
    case EffectType::NegPctDEF: s.pctDEF -= e.value; break;
    case EffectType::FlatDmgReduction: break;
    case EffectType::FlatDmgIncrease: break;
    case EffectType::BlockChance: break;
    case EffectType::DodgeChance: break;
    case EffectType::DodgeThenCrit: break;
    case EffectType::CritChance: break;
    case EffectType::CritMultiplier: break;
    case EffectType::EveryNthDouble: break;
    case EffectType::ArmorPenChance: break;
    case EffectType::ArmorPenPct: break;
    case EffectType::ArmorPen: break;  // handled as triggered effect
    case EffectType::Stun: break;  // handled as triggered effect
    case EffectType::KnockbackChance: break;
    case EffectType::PoisonDOT: break;
    case EffectType::PoisonDmgAmp: break;
    case EffectType::MPOnHit: break;
    case EffectType::HPOnHit: break;
    case EffectType::MPDrain: break;
    case EffectType::MPRecoveryBonus: break;
    case EffectType::SkillDmgPct: break;
    case EffectType::SkillReflectPct: break;
    case EffectType::CDR: break;
    case EffectType::FlatShield: break;
    case EffectType::ShieldPctMaxHP: break;
    case EffectType::ShieldFreezeRes: break;
    case EffectType::HealAuraPct: break;
    case EffectType::HealAuraFlat: break;
    case EffectType::HealedATKSPDBoost: break;
    case EffectType::HPRegenPct: break;
    case EffectType::FreezeReductionPct: break;
    case EffectType::ControlImmunityFrames: break;
    case EffectType::KillHealPct: break;
    case EffectType::KillInvincFrames: break;
    case EffectType::PostSkillInvincFrames: break;
    case EffectType::DmgReductionPct: break;
    case EffectType::Bloodlust: break;
    case EffectType::Adaptation:
        s.adaptations.push_back({e.value, e.value2});
        s.adaptationStacks.push_back({});
        break;
    case EffectType::DodgeAdaptation:
        s.dodgeAdaptations.push_back({e.value, e.value2});
        s.dodgeAdaptationStacks.push_back({});
        break;
    case EffectType::RampingDmg:
        s.rampings.push_back({e.value, e.value2});
        s.rampingStacks.push_back(0);
        s.rampingIdleTimers.push_back(0);
        break;
    case EffectType::HealBurst: break;  // only meaningful as triggered
    case EffectType::BleedChance: break;
    case EffectType::PostSkillDash: break;
    case EffectType::EnemyTopDebuff: break;
    case EffectType::BlinkAttack: break;
    case EffectType::AllyDeathStatBoost: break;
    case EffectType::CloneSummon: break;
    case EffectType::ProjectileReflect: break;
    case EffectType::ProjectileBounce: break;
    case EffectType::OnSkillTeamHeal: break;
    case EffectType::OnSkillTeamHealPct: break;
    case EffectType::DeathPrevention: break;
    case EffectType::DeathMedical: break;
    case EffectType::ForcePullProtect: break;
    case EffectType::ForcePullExecute: break;
    case EffectType::Execute: break;  // handled as triggered effect (OnHit)
    case EffectType::MPBlock: break;  // handled as triggered effect (OnHit)
    case EffectType::CharmCDRDebuff: break;
    case EffectType::OffensiveCharm: break;
    case EffectType::DeathAOE: break;
    case EffectType::ShieldExplosion: break;
    case EffectType::TempFlatATK: break;
    case EffectType::AutoUltimate: break;
    case EffectType::MPRestore: break;
    case EffectType::ShieldOnAllyDeath: break;
    case EffectType::DamageImmunityAfterFrames: break;
    case EffectType::AutoUltimateAfterFrames: break;
    case EffectType::UltimateExtraProjectiles: break;
    case EffectType::BlockFirstHits: break;
    case EffectType::GoldCoefficient: break;
    case EffectType::HurtInvincFrames: break;
    case EffectType::DashAttack: break;
    case EffectType::DashChanceBoost: break;
    case EffectType::MPRatioDmgBoost: break;
    case EffectType::DmgReduceDebuff: break;
    case EffectType::CurrentHPPctBlast: break;
    case EffectType::TeamMPRestore: break;
    case EffectType::SpiralBleedProjectile: break;
    case EffectType::NearbyTrackingProjectiles: break;
    case EffectType::ForceRangedAttack: break;
    case EffectType::CounterUltimateBlock: break;
    case EffectType::MaxHitPctCurrentHP: break;
    case EffectType::FreeRefresh: break;
    case EffectType::BattleMapChoice: break;
    }
}

RoleComboState ChessBattleEffects::makeSummonedCloneState(const RoleComboState& sourceState)
{
    RoleComboState cloneState = sourceState;
    cloneState.effectFrameTimers.clear();

    for (int effectIndex = 0; effectIndex < static_cast<int>(cloneState.appliedEffects.size()); ++effectIndex)
    {
        const auto& effect = cloneState.appliedEffects[effectIndex];
        if (effect.type == EffectType::AutoUltimateAfterFrames && effect.trigger == Trigger::Always && effect.value > 0)
        {
            cloneState.effectFrameTimers[effectIndex] = effect.value;
        }
    }
    return cloneState;
}

void ChessBattleEffects::mergeEffects(std::map<int, RoleComboState>& states,
                                      const std::vector<ComboEffect>& effects,
                                      const std::vector<int>& roleIds,
                                      int sourceComboId)
{
    for (int rid : roleIds)
    {
        auto& s = states[rid];
        for (auto& e : effects)
            applyEffect(s, e, sourceComboId);
    }
}

}  // namespace KysChess
