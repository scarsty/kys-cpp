#include "ChessBattleEffects.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <print>

namespace KysChess
{

static const std::map<std::string, EffectType> effectTypeMap = {
    {"生命加成", EffectType::FlatHP}, {"攻击加成", EffectType::FlatATK},
    {"防御加成", EffectType::FlatDEF}, {"速度加成", EffectType::FlatSPD},
    {"生命百分比", EffectType::PctHP}, {"攻击百分比", EffectType::PctATK},
    {"防御百分比", EffectType::PctDEF}, {"防御削减", EffectType::NegPctDEF},
    {"固定减伤", EffectType::FlatDmgReduction}, {"格挡几率", EffectType::BlockChance},
    {"闪避几率", EffectType::DodgeChance}, {"闪避后暴击", EffectType::DodgeThenCrit},
    {"暴击几率", EffectType::CritChance}, {"暴击伤害", EffectType::CritMultiplier},
    {"每N次双倍", EffectType::EveryNthDouble}, {"穿甲几率", EffectType::ArmorPenChance},
    {"穿甲百分比", EffectType::ArmorPenPct}, {"穿甲", EffectType::ArmorPen},
    {"眩晕", EffectType::Stun},
    {"击退几率", EffectType::KnockbackChance}, {"中毒伤害", EffectType::PoisonDOT},
    {"中毒增伤", EffectType::PoisonDmgAmp}, {"命中回蓝", EffectType::MPOnHit},
    {"吸取内力", EffectType::MPDrain}, {"回蓝加成", EffectType::MPRecoveryBonus},
    {"技能伤害", EffectType::SkillDmgPct}, {"技能反弹", EffectType::SkillReflectPct},
    {"冷却缩减", EffectType::CDR}, {"护盾生命比", EffectType::ShieldPctMaxHP},
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
    {"流血", EffectType::BleedChance}, {"流血持续", EffectType::BleedPersist},
    {"踏雪", EffectType::PostSkillDash}, {"敌方攻防削弱", EffectType::EnemyTopDebuff},
    {"闪击", EffectType::BlinkAttack}, {"同袍之死", EffectType::AllyDeathStatBoost},
    {"七截分身", EffectType::CloneSummon}, {"弹反", EffectType::ProjectileReflect},
    {"无视防御", EffectType::IgnoreDefense}, {"群体施治", EffectType::OnSkillTeamHeal},
    {"死亡庇护", EffectType::DeathPrevention}, {"保护挪移", EffectType::ForcePullProtect},
    {"处决挪移", EffectType::ForcePullExecute}, {"斩杀", EffectType::Execute},
    {"破罡", EffectType::MPBlock}, {"倾国倾城", EffectType::CharmCDRDebuff},
    {"攻击倾城", EffectType::OffensiveCharm}, {"殉爆", EffectType::DeathAOE},
    {"护盾爆炸", EffectType::ShieldExplosion}, {"护盾重获", EffectType::ShieldOnAllyDeath},
    {"周期免伤", EffectType::DamageImmunityAfterFrames}, {"周期绝招", EffectType::AutoUltimateAfterFrames},
    {"初次格挡", EffectType::BlockFirstHits},
    {"金币加成", EffectType::GoldCoefficient},
};

static const std::map<std::string, Trigger> triggerMap = {
    {"低血量", Trigger::WhileLowHP},
    {"友方低血狂暴", Trigger::AllyLowHPBurst},
    {"最后存活", Trigger::LastAlive},
    {"攻击命中时概率", Trigger::OnHit},
};

const std::map<std::string, EffectType>& ChessBattleEffects::getEffectTypeMap()
{
    return effectTypeMap;
}

bool ChessBattleEffects::parseEffect(const YAML::Node& eNode, ComboEffect& out, const std::string& context)
{
    auto typeName = eNode["类型"].as<std::string>();
    auto etIt = effectTypeMap.find(typeName);
    if (etIt == effectTypeMap.end())
    {
        std::print("【效果配置】「{}」效果类型「{}」无法识别\n", context, typeName);
        return false;
    }
    out.type = etIt->second;
    out.value = eNode["数值"].as<int>();
    if (eNode["附加参数"]) out.value2 = eNode["附加参数"].as<int>();
    if (eNode["触发"])
    {
        auto trigName = eNode["触发"].as<std::string>();
        auto trIt = triggerMap.find(trigName);
        if (trIt == triggerMap.end())
        {
            std::print("【效果配置】「{}」触发类型「{}」无法识别\n", context, trigName);
            return false;
        }
        out.trigger = trIt->second;
        if (eNode["触发参数"]) out.triggerValue = eNode["触发参数"].as<int>();
        if (eNode["持续帧数"]) out.duration = eNode["持续帧数"].as<int>();
    }
    if (eNode["次数"]) out.maxCount = eNode["次数"].as<int>();
    return true;
}

void ChessBattleEffects::applyEffect(RoleComboState& s, const ComboEffect& e)
{
    if (e.trigger != Trigger::Always)
    {
        s.triggeredEffects.push_back(e);
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
    case EffectType::NegPctDEF: s.pctDEF -= e.value; break;
    case EffectType::FlatDmgReduction: s.flatDmgReduction += e.value; break;
    case EffectType::BlockChance: s.blockChancePct += e.value; break;
    case EffectType::DodgeChance: s.dodgeChancePct += e.value; break;
    case EffectType::DodgeThenCrit: s.dodgeThenCrit = true; break;
    case EffectType::CritChance: s.critChancePct += e.value; break;
    case EffectType::CritMultiplier: s.critMultiplier = std::max(s.critMultiplier, e.value); break;
    case EffectType::EveryNthDouble: if (e.value > 0) s.everyNthDoubles.push_back(e.value); break;
    case EffectType::ArmorPenChance: s.armorPenChancePct += e.value; break;
    case EffectType::ArmorPenPct: s.armorPenPct = std::max(s.armorPenPct, e.value); break;
    case EffectType::ArmorPen: break;  // handled as triggered effect
    case EffectType::Stun: break;  // handled as triggered effect
    case EffectType::KnockbackChance: s.knockbackChancePct += e.value; break;
    case EffectType::PoisonDOT: s.poisonDOTPct += e.value; if (e.value2) s.poisonDuration = std::max(s.poisonDuration, e.value2 * 30); break;
    case EffectType::PoisonDmgAmp: s.poisonDmgAmpPct += e.value; break;
    case EffectType::MPOnHit: s.mpOnHit += e.value; break;
    case EffectType::MPDrain: s.mpDrain += e.value; break;
    case EffectType::MPRecoveryBonus: s.mpRecoveryBonusPct += e.value; break;
    case EffectType::SkillDmgPct: s.skillDmgPct += e.value; break;
    case EffectType::SkillReflectPct: s.skillReflectPct = std::max(s.skillReflectPct, e.value); break;
    case EffectType::CDR: s.cdrPct += e.value; break;
    case EffectType::ShieldPctMaxHP: s.shieldPctMaxHP += e.value; break;
    case EffectType::ShieldFreezeRes: s.shieldFreezeResPct += e.value; break;
    case EffectType::HealAuraPct: s.healAuraPct = std::max(s.healAuraPct, e.value); if (e.value2) s.healAuraInterval = e.value2; break;
    case EffectType::HealAuraFlat: s.healAuraFlat = std::max(s.healAuraFlat, e.value); if (e.value2) s.healAuraInterval = e.value2; break;
    case EffectType::HealedATKSPDBoost: s.healedATKSPDBoostPct += e.value; break;
    case EffectType::HPRegenPct: s.hpRegenPct += e.value; if (e.value2) s.hpRegenInterval = e.value2; break;
    case EffectType::FreezeReductionPct: s.freezeReductionPct += e.value; break;
    case EffectType::ControlImmunityFrames: s.controlImmunityFrames += e.value; break;
    case EffectType::KillHealPct: s.killHealPct += e.value; break;
    case EffectType::KillInvincFrames: s.killInvincFrames = std::max(s.killInvincFrames, e.value); break;
    case EffectType::PostSkillInvincFrames: s.postSkillInvincFrames = std::max(s.postSkillInvincFrames, e.value); break;
    case EffectType::DmgReductionPct: s.dmgReductionPct += e.value; break;
    case EffectType::Bloodlust: s.bloodlustATKPerKill += e.value; break;
    case EffectType::Adaptation:
        s.adaptations.push_back({e.value, e.value2});
        s.adaptationStacks.push_back({});
        break;
    case EffectType::RampingDmg:
        s.rampings.push_back({e.value, e.value2});
        s.rampingStacks.push_back(0);
        s.rampingIdleTimers.push_back(0);
        break;
    case EffectType::HealBurst: break;  // only meaningful as triggered
    case EffectType::BleedChance:
        s.bleedChancePct += e.value;
        if (e.value2 > 0) s.bleedMaxStacks = e.value2;
        break;
    case EffectType::BleedPersist: s.bleedPersist = true; break;
    case EffectType::PostSkillDash:
        s.postSkillDash = true;
        s.postSkillDashFrames = std::max(s.postSkillDashFrames, e.value);
        break;
    case EffectType::EnemyTopDebuff:
        s.enemyTopDebuffCount = e.value;
        s.enemyTopDebuffValue = e.value2;
        break;
    case EffectType::BlinkAttack: s.blinkAttack = true; break;
    case EffectType::AllyDeathStatBoost: s.allyDeathStatBoost += e.value; break;
    case EffectType::CloneSummon: s.cloneSummonCount = std::max(s.cloneSummonCount, e.value); break;
    case EffectType::ProjectileReflect: s.projectileReflectPct += e.value; break;
    case EffectType::IgnoreDefense: s.ignoreDefense = true; break;
    case EffectType::OnSkillTeamHeal: s.onSkillTeamHeal = std::max(s.onSkillTeamHeal, e.value); break;
    case EffectType::DeathPrevention:
        s.deathPrevention = true;
        s.deathPreventionFrames = std::max(s.deathPreventionFrames, e.value);
        break;
    case EffectType::ForcePullProtect: s.forcePullProtect = true; break;
    case EffectType::ForcePullExecute: s.forcePullExecute = true; break;
    case EffectType::Execute: break;  // handled as triggered effect (OnHit)
    case EffectType::MPBlock: break;  // handled as triggered effect (OnHit)
    case EffectType::CharmCDRDebuff:
        s.charmCDRChancePct = e.value;
        s.charmCDRAmountPct = e.value2;
        break;
    case EffectType::OffensiveCharm: s.offensiveCharm = true; break;
    case EffectType::DeathAOE:
        s.deathAOEPct = std::max(s.deathAOEPct, e.value);
        if (e.value2 > 0) s.deathAOEStunFrames = e.value2;
        break;
    case EffectType::ShieldExplosion: s.shieldExplosionPct = std::max(s.shieldExplosionPct, e.value); break;
    case EffectType::ShieldOnAllyDeath: s.shieldOnAllyDeathCount = e.value; break;
    case EffectType::DamageImmunityAfterFrames:
        s.damageImmunityAfterFrames = e.value;
        s.damageImmunityDuration = e.value2;
        break;
    case EffectType::AutoUltimateAfterFrames: s.autoUltimateAfterFrames = e.value; break;
    case EffectType::BlockFirstHits: s.blockFirstHitsCount = e.value; break;
    case EffectType::GoldCoefficient: s.goldCoefficient = e.value; break;
    }
}

void ChessBattleEffects::mergeEffects(std::map<int, RoleComboState>& states,
                                      const std::vector<ComboEffect>& effects,
                                      const std::vector<int>& roleIds)
{
    for (int rid : roleIds)
    {
        auto& s = states[rid];
        for (auto& e : effects)
            applyEffect(s, e);
    }
}

}  // namespace KysChess
