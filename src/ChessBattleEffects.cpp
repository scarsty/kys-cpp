#include "ChessBattleEffects.h"
#include "GameUtil.h"
#include "battle/BattleEffectRuntimeRegistry.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <format>
#include <print>
#include <set>

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
    {"全場殺內", EffectType::EnemyMpDamageAll}, {"全场杀内", EffectType::EnemyMpDamageAll},
    {"螺旋流血弹", EffectType::SpiralBleedProjectile},
    {"范围追踪弹", EffectType::NearbyTrackingProjectiles},
    {"远程化", EffectType::ForceRangedAttack},
    {"格挡绝招反击", EffectType::CounterUltimateBlock},
    {"单次承伤上限", EffectType::MaxHitPctCurrentHP},
    {"免费刷新", EffectType::FreeRefresh},
    {"选择战场", EffectType::BattleMapChoice},
    {"最低友方治療", EffectType::LowestAllyHeal},
    {"最低友方治疗", EffectType::LowestAllyHeal},
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

namespace
{

std::span<const RoleComboEffectId> asSpan(const std::vector<RoleComboEffectId>& ids)
{
    return { ids.data(), ids.size() };
}

const std::vector<RoleComboEffectId>& emptyEffectIds()
{
    static const std::vector<RoleComboEffectId> empty;
    return empty;
}

std::span<const RoleComboEffectId> lookupIds(
    const std::map<RoleComboEffectLookupKey, std::vector<RoleComboEffectId>>& index,
    Trigger trigger,
    EffectType type)
{
    const auto it = index.find({ trigger, type });
    return it == index.end() ? asSpan(emptyEffectIds()) : asSpan(it->second);
}

std::string defaultMagicEffectsConfigPath()
{
    const std::string gamePath = GameUtil::PATH() + "config/chess_magic_effects.yaml";
    if (std::filesystem::exists(gamePath))
    {
        return gamePath;
    }
    return "config/chess_magic_effects.yaml";
}

const RoleComboAlwaysSummary* lookupSummary(
    const std::map<EffectType, RoleComboAlwaysSummary>& summaries,
    EffectType type)
{
    const auto it = summaries.find(type);
    return it == summaries.end() ? nullptr : &it->second;
}

void updateAlwaysSummary(
    std::map<EffectType, RoleComboAlwaysSummary>& summaries,
    const RoleComboEffectStore& effects,
    RoleComboEffectId id)
{
    assert(id.isValid());
    assert(static_cast<size_t>(id.value) < effects.instances.size());
    const auto& effect = effects.instances[id.value];
    assert(effect.id == id);
    const auto [summaryIt, inserted] = summaries.try_emplace(
        effect.type,
        RoleComboAlwaysSummary{
            effect.value,
            effect.value,
            effect.value2,
            id,
            id,
        });
    if (inserted)
    {
        return;
    }

    auto& summary = summaryIt->second;
    assert(summary.firstId.isValid());
    assert(summary.maxByValueId.isValid());
    summary.sumValue += effect.value;
    summary.maxValue = std::max(summary.maxValue, effect.value);
    summary.maxValue2 = std::max(summary.maxValue2, effect.value2);
    assert(static_cast<size_t>(summary.maxByValueId.value) < effects.instances.size());
    if (effect.value > effects.instances[summary.maxByValueId.value].value)
    {
        summary.maxByValueId = id;
    }
}

void updateStatBonuses(RoleComboStatBonuses& bonuses, const ComboEffect& effect)
{
    switch (effect.type)
    {
    case EffectType::FlatHP:  bonuses.flatHP += effect.value; break;
    case EffectType::FlatATK: bonuses.flatATK += effect.value; break;
    case EffectType::FlatDEF: bonuses.flatDEF += effect.value; break;
    case EffectType::FlatSPD: bonuses.flatSPD += effect.value; break;
    case EffectType::PctHP:   bonuses.pctHP += effect.value; break;
    case EffectType::PctATK:  bonuses.pctATK += effect.value; break;
    case EffectType::PctDEF:  bonuses.pctDEF += effect.value; break;
    case EffectType::PctSPD:  bonuses.pctSPD += effect.value; break;
    case EffectType::TeamFlatHP: bonuses.flatHP += effect.value; break;
    case EffectType::TeamFlatATK: bonuses.flatATK += effect.value; break;
    case EffectType::TeamFlatDEF: bonuses.flatDEF += effect.value; break;
    case EffectType::TeamFlatSPD: bonuses.flatSPD += effect.value; break;
    case EffectType::TeamPctHP: bonuses.pctHP += effect.value; break;
    case EffectType::TeamPctATK: bonuses.pctATK += effect.value; break;
    case EffectType::TeamPctDEF: bonuses.pctDEF += effect.value; break;
    case EffectType::TeamPctSPD: bonuses.pctSPD += effect.value; break;
    case EffectType::FightWinHP:
        bonuses.fightWinGrowthHP += effect.value;
        break;
    case EffectType::FightWinATKDEF:
        bonuses.fightWinGrowthATK += effect.value;
        bonuses.fightWinGrowthDEF += effect.value2 > 0 ? effect.value2 : effect.value;
        break;
    case EffectType::NegPctDEF:
        bonuses.pctDEF -= effect.value;
        break;
    default:
        break;
    }
}

std::string comboEffectLabel(const ComboEffect& eff, bool compact)
{
    auto triggerPrefix = [&]() -> std::string {
        if (compact)
        {
            if (eff.trigger == Trigger::WhileLowHP) return std::format("血<{}%·", eff.triggerValue);
            if (eff.trigger == Trigger::AllyLowHPBurst) return std::format("友血<{}%·狂暴{}幀·", eff.triggerValue, eff.duration);
            if (eff.trigger == Trigger::LastAlive) return "最後存活·";
            if (eff.trigger == Trigger::OnCast && eff.triggerValue < 100) return std::format("出手{}%·", eff.triggerValue);
            if (eff.trigger == Trigger::OnUltimate) return "絕招·";
            if (eff.trigger == Trigger::OnHit && eff.triggerValue < 100) return std::format("擊中{}%·", eff.triggerValue);
            if (eff.trigger == Trigger::OnBeingHit && eff.triggerValue < 100) return std::format("被擊{}%·", eff.triggerValue);
            if (eff.trigger == Trigger::OnShieldBreak && eff.triggerValue > 0 && eff.triggerValue < 100) return std::format("盾爆{}%·", eff.triggerValue);
            if (eff.trigger == Trigger::OnShieldBreak) return "盾爆·";
            return "";
        }
        if (eff.trigger == Trigger::WhileLowHP) return std::format("血量<{}%: ", eff.triggerValue);
        if (eff.trigger == Trigger::AllyLowHPBurst) return std::format("血量<{}%狂暴({}幀): ", eff.triggerValue, eff.duration);
        if (eff.trigger == Trigger::LastAlive) return "最後存活: ";
        if (eff.trigger == Trigger::OnCast && eff.triggerValue < 100) return std::format("{}%出手觸發: ", eff.triggerValue);
        if (eff.trigger == Trigger::OnUltimate) return "絕招觸發: ";
        if (eff.trigger == Trigger::OnHit && eff.triggerValue < 100) return std::format("{}%擊中觸發: ", eff.triggerValue);
        if (eff.trigger == Trigger::OnBeingHit && eff.triggerValue < 100) return std::format("{}%被擊中觸發: ", eff.triggerValue);
        if (eff.trigger == Trigger::OnShieldBreak && eff.triggerValue != 0) return std::format("{}%護盾解除時: ", eff.triggerValue);
        if (eff.trigger == Trigger::OnShieldBreak) return "護盾解除時: ";
        return "";
    };
    auto countSuffix = [&]() -> std::string {
        if (eff.maxCount > 0)
        {
            return compact ? std::format("·{}次", eff.maxCount) : std::format(" ({}次)", eff.maxCount);
        }
        return "";
    };
    auto durationSuffix = [&]() -> std::string {
        if (eff.duration > 0 && eff.trigger != Trigger::AllyLowHPBurst && eff.type != EffectType::KnockbackChance)
        {
            return compact ? std::format("·{}幀", eff.duration) : std::format(" ({}幀)", eff.duration);
        }
        return "";
    };

    std::string desc;
    switch (eff.type)
    {
    case EffectType::FlatHP: desc = compact ? std::format("生+{}", eff.value) : std::format("生命+{}", eff.value); break;
    case EffectType::FlatATK: desc = compact ? std::format("攻+{}", eff.value) : std::format("攻擊+{}", eff.value); break;
    case EffectType::FlatDEF: desc = compact ? std::format("防+{}", eff.value) : std::format("防禦+{}", eff.value); break;
    case EffectType::FlatSPD: desc = compact ? std::format("速+{}", eff.value) : std::format("速度+{}", eff.value); break;
    case EffectType::PctHP: desc = compact ? std::format("生+{}%", eff.value) : std::format("生命+{}%", eff.value); break;
    case EffectType::PctATK: desc = compact ? std::format("攻+{}%", eff.value) : std::format("攻擊+{}%", eff.value); break;
    case EffectType::PctDEF: desc = compact ? std::format("防+{}%", eff.value) : std::format("防禦+{}%", eff.value); break;
    case EffectType::TeamFlatHP: desc = compact ? std::format("全生+{}", eff.value) : std::format("全隊生命+{}", eff.value); break;
    case EffectType::TeamFlatATK: desc = compact ? std::format("全攻+{}", eff.value) : std::format("全隊攻擊+{}", eff.value); break;
    case EffectType::TeamFlatDEF: desc = compact ? std::format("全防+{}", eff.value) : std::format("全隊防禦+{}", eff.value); break;
    case EffectType::TeamFlatSPD: desc = compact ? std::format("全速+{}", eff.value) : std::format("全隊速度+{}", eff.value); break;
    case EffectType::TeamPctHP: desc = compact ? std::format("全生+{}%", eff.value) : std::format("全隊生命+{}%", eff.value); break;
    case EffectType::TeamPctATK: desc = compact ? std::format("全攻+{}%", eff.value) : std::format("全隊攻擊+{}%", eff.value); break;
    case EffectType::TeamPctDEF: desc = compact ? std::format("全防+{}%", eff.value) : std::format("全隊防禦+{}%", eff.value); break;
    case EffectType::TeamPctSPD: desc = compact ? std::format("全速+{}%", eff.value) : std::format("全隊速度+{}%", eff.value); break;
    case EffectType::ActAsCombo: desc = std::format("計作{}", eff.text); break;
    case EffectType::FightWinHP:
        desc = std::format("每勝生命+{}", eff.value);
        break;
    case EffectType::FightWinATKDEF:
        desc = std::format("每勝攻防+{}", eff.value);
        break;
    case EffectType::NegPctDEF: desc = compact ? std::format("防-{}%", eff.value) : std::format("防禦-{}%", eff.value); break;
    case EffectType::FlatDmgReduction: desc = std::format("減傷{}", eff.value); break;
    case EffectType::FlatDmgIncrease: desc = std::format("增傷{}", eff.value); break;
    case EffectType::BlockChance: desc = std::format("{}%格擋", eff.value); break;
    case EffectType::DodgeChance: desc = std::format("{}%閃避", eff.value); break;
    case EffectType::DodgeThenCrit: desc = "閃避後暴擊"; break;
    case EffectType::CritChance: desc = std::format("{}%暴擊", eff.value); break;
    case EffectType::CritMultiplier: desc = compact ? std::format("暴傷+{}%", eff.value) : std::format("暴擊{}%傷害", eff.value); break;
    case EffectType::EveryNthDouble: desc = std::format("每{}次雙倍", eff.value); break;
    case EffectType::ArmorPenChance: desc = std::format("{}%穿甲", eff.value); break;
    case EffectType::ArmorPenPct: desc = compact ? std::format("無視防{}%", eff.value) : std::format("無視%防禦", eff.value); break;
    case EffectType::ArmorPen: desc = std::format("{}%穿甲", eff.value); break;
    case EffectType::Stun: desc = std::format("眩暈({}幀)", eff.value); break;
    case EffectType::KnockbackChance:
    {
        const int distance = eff.value2 > 0 ? eff.value2 : 5;
        const int lockFrames = eff.duration > 0 ? eff.duration : 3;
        desc = compact
            ? std::format("{}%擊退{}/{}幀", eff.value, distance, lockFrames)
            : std::format("{}%擊退{}距離並鎖定{}幀", eff.value, distance, lockFrames);
        break;
    }
    case EffectType::PoisonDOT: desc = eff.value2 ? std::format("中毒{}%×{}次", eff.value, eff.value2) : std::format("中毒{}%", eff.value); break;
    case EffectType::PoisonDmgAmp: desc = std::format("中毒增傷{}%", eff.value); break;
    case EffectType::MPOnHit: desc = std::format("命中回{}MP", eff.value); break;
    case EffectType::HPOnHit: desc = std::format("命中回{}HP", eff.value); break;
    case EffectType::MPDrain: desc = std::format("吸取{}MP", eff.value); break;
    case EffectType::MPRecoveryBonus: desc = std::format("回藍+{}%", eff.value); break;
    case EffectType::SkillDmgPct: desc = compact ? std::format("技傷+{}%", eff.value) : std::format("技能傷害+{}%", eff.value); break;
    case EffectType::SkillReflectPct: desc = compact ? std::format("反彈{}%", eff.value) : std::format("反彈{}%", eff.value); break;
    case EffectType::CDR: desc = std::format("冷卻-{}%", eff.value); break;
    case EffectType::FlatShield: desc = compact ? std::format("全隊盾+{}", eff.value) : std::format("全隊固定護盾+{}", eff.value); break;
    case EffectType::ShieldPctMaxHP: desc = compact ? std::format("護盾{}%生", eff.value) : std::format("護盾{}%生命", eff.value); break;
    case EffectType::ShieldFreezeRes: desc = compact ? std::format("護盾僵抗{}%", eff.value) : std::format("護盾僵直抗性{}%", eff.value); break;
    case EffectType::HealAuraPct: desc = eff.value2 ? (compact ? std::format("治療環{}%/{}幀", eff.value, eff.value2) : std::format("治療光環{}%(每{}幀)", eff.value, eff.value2)) : std::format("治療光環{}%", eff.value); break;
    case EffectType::HealAuraFlat: desc = eff.value2 ? (compact ? std::format("治療環{}/{}幀", eff.value, eff.value2) : std::format("治療光環{}(每{}幀)", eff.value, eff.value2)) : std::format("治療光環{}", eff.value); break;
    case EffectType::HealedATKSPDBoost: desc = std::format("受治療加速{}%", eff.value); break;
    case EffectType::HPRegenPct: desc = eff.value2 ? (compact ? std::format("回血{}%/{}幀", eff.value, eff.value2) : std::format("回血{}%(每{}幀)", eff.value, eff.value2)) : std::format("回血{}%", eff.value); break;
    case EffectType::FreezeReductionPct: desc = std::format("僵直-{}%", eff.value); break;
    case EffectType::ControlImmunityFrames: desc = std::format("僵直吸收{}幀", eff.value); break;
    case EffectType::KillHealPct: desc = std::format("擊殺回{}%血", eff.value); break;
    case EffectType::KillInvincFrames: desc = std::format("擊殺無敵{}幀", eff.value); break;
    case EffectType::PostSkillInvincFrames: desc = std::format("技能後無敵{}幀", eff.value); break;
    case EffectType::DmgReductionPct: desc = compact ? std::format("減傷{}%", eff.value) : std::format("傷害減免{}%", eff.value); break;
    case EffectType::Bloodlust: desc = std::format("擊殺增攻+{}", eff.value); break;
    case EffectType::PctSPD: desc = std::format("速度+{}%", eff.value); break;
    case EffectType::Adaptation: desc = std::format("同敵減傷{}%({}層)", eff.value, eff.value2); break;
    case EffectType::DodgeAdaptation: desc = std::format("同敵閃避{}%({}層)", eff.value, eff.value2); break;
    case EffectType::RampingDmg: desc = std::format("連擊增傷+{}%({}層)", eff.value, eff.value2); break;
    case EffectType::HealBurst: desc = std::format("回血{}%", eff.value); break;
    case EffectType::BleedChance: desc = std::format("{}%流血{}層", eff.value, eff.value2); break;
    case EffectType::PostSkillDash: desc = "絕招後疾退"; break;
    case EffectType::EnemyTopDebuff: desc = std::format("敵方前{}名攻防-{}×存活數", eff.value, eff.value2); break;
    case EffectType::BlinkAttack: desc = "閃現攻擊"; break;
    case EffectType::AllyDeathStatBoost: desc = compact ? std::format("友死增{}攻防", eff.value) : std::format("友死增攻防{}", eff.value); break;
    case EffectType::CloneSummon: desc = eff.value > 1 ? std::format("分身×{}", eff.value) : "分身"; break;
    case EffectType::ProjectileReflect: desc = std::format("{}%彈反", eff.value); break;
    case EffectType::OnSkillTeamHeal: desc = compact ? std::format("群療{}HP", eff.value) : std::format("全隊回{}HP", eff.value); break;
    case EffectType::OnSkillTeamHealPct: desc = compact ? std::format("群療{}%HP", eff.value) : std::format("全隊回{}%HP", eff.value); break;
    case EffectType::DeathPrevention: desc = std::format("鎖血並無敵{}幀", eff.value); break;
    case EffectType::DeathMedical: desc = compact ? std::format("死療{}%", eff.value) : std::format("死亡醫療{}%HP", eff.value); break;
    case EffectType::ForcePullProtect: desc = compact ? std::format("保護挪移{}次", eff.value) : std::format("一場{}次隊友低血挪移並保護", eff.value); break;
    case EffectType::ForcePullExecute: desc = compact ? std::format("處決挪移{}次", eff.value) : std::format("一場{}次敵方殘血挪移", eff.value); break;
    case EffectType::ProjectileBounce: desc = compact ?  std::format("彈道彈射{}次", eff.value) : std::format("彈道{}半径内彈射{}次", eff.value2, eff.value); break;
    case EffectType::Execute: desc = std::format("斩殺<{}%", eff.value); break;
    case EffectType::MPBlock: desc = std::format("封内力回復{}幀", eff.value); break;
    case EffectType::CharmCDRDebuff: desc = std::format("{}%增敵CD{}%", eff.value, eff.value2); break;
    case EffectType::OffensiveCharm: desc = std::format("攻擊傾城{}%", eff.value); break;
    case EffectType::DeathAOE:
        desc = eff.value2 > 0 ? std::format("殉爆{}%最多{}敵", eff.value, eff.value2) : std::format("殉爆{}%", eff.value);
        break;
    case EffectType::ShieldExplosion: desc = "護盾解除"; break;
    case EffectType::TempFlatATK: desc = compact ? std::format("临时攻+{}", eff.value) : std::format("临时攻击+{}", eff.value); break;
    case EffectType::AutoUltimate: desc = "自動絕招"; break;
    case EffectType::MPRestore: desc = std::format("回内力+{}", eff.value); break;
    case EffectType::ShieldOnAllyDeath: desc = compact ? std::format("每{}友死獲盾", eff.value) : std::format("每{}友死獲盾", eff.value); break;
    case EffectType::DamageImmunityAfterFrames: desc = std::format("每{}幀免傷{}幀", eff.value, eff.value2); break;
    case EffectType::AutoUltimateAfterFrames: desc = std::format("每{}幀自動絕招", eff.value); break;
    case EffectType::UltimateExtraProjectiles: desc = compact ? std::format("絕招+{}彈", eff.value) : std::format("絕招額外彈道+{}", eff.value); break;
    case EffectType::BlockFirstHits: desc = compact ? std::format("格擋前{}次", eff.value) : std::format("格擋前{}次攻擊", eff.value); break;
    case EffectType::GoldCoefficient: desc = compact ? std::format("勝利+{}×最高星金", eff.value) : std::format("勝利獲得{}×最高星級金幣", eff.value); break;
    case EffectType::HurtInvincFrames: desc = std::format("受傷後無敵{}幀", eff.value); break;
    case EffectType::DashAttack: desc = "滑步攻擊"; break;
    case EffectType::DashChanceBoost: desc = std::format("滑步率+{}%", eff.value); break;
    case EffectType::MPRatioDmgBoost: desc = compact ? std::format("內力比加傷至{}%", eff.value) : std::format("當前內力比例加傷至{}%", eff.value); break;
    case EffectType::DmgReduceDebuff: desc = compact ? std::format("降傷標記{}%/{}幀", eff.value, eff.value2) : std::format("攻擊標記目標降傷{}%/{}幀", eff.value, eff.value2); break;
    case EffectType::CurrentHPPctBlast: desc = compact ? std::format("全敵現血-{}%", eff.value) : std::format("全敵當前生命-{}%", eff.value); break;
    case EffectType::TeamMPRestore: desc = compact ? std::format("全隊回{}MP", eff.value) : std::format("全隊回復{}MP", eff.value); break;
    case EffectType::EnemyMpDamageAll: desc = compact ? std::format("全敵內-{}", eff.value) : std::format("全場敵人內力-{}", eff.value); break;
    case EffectType::SpiralBleedProjectile: desc = compact ? std::format("擴張螺旋彈+{}層", eff.value) : std::format("發射高速擴張螺旋流血彈並附加{}層流血", eff.value); break;
    case EffectType::NearbyTrackingProjectiles:
    {
        int damagePct = eff.value2 > 0 ? eff.value2 : 40;
        desc = compact ? std::format("近敵追彈{}格/{}%", eff.value, damagePct) : std::format("命中時向{}範圍內敵人各發一枚{}%追蹤彈", eff.value, damagePct);
        break;
    }
    case EffectType::ForceRangedAttack:
    {
        int minDistance = eff.value2 > 0 ? eff.value2 : 6;
        desc = compact ? std::format("武功遠程/距{}/彈速{}%", minDistance, eff.value) : std::format("武功遠程化(至少{}距離)且彈速{}%", minDistance, eff.value);
        break;
    }
    case EffectType::CounterUltimateBlock: desc = compact ? std::format("{}%格擋反絕", eff.value) : std::format("{}%格擋並反擊絕招", eff.value); break;
    case EffectType::MaxHitPctCurrentHP: desc = compact ? std::format("單次承傷<=最大血{}%", eff.value) : std::format("單次承傷不超過最大生命{}%", eff.value); break;
    case EffectType::FreeRefresh: desc = compact ? "免費刷新" : "戰勝後獲得一次免費刷新"; break;
    case EffectType::BattleMapChoice: desc = compact ? "選戰場" : "戰鬥前可選擇戰場"; break;
    case EffectType::LowestAllyHeal:
        desc = eff.value2 > 0
            ? std::format("最低友方治療{}+{}%生命", eff.value, eff.value2)
            : std::format("最低友方治療{}", eff.value);
        break;
    default: desc = std::format("效果({})", eff.value); break;
    }
    return triggerPrefix() + desc + durationSuffix() + countSuffix();
}

}  // namespace

std::string comboEffectDesc(const ComboEffect& eff)
{
    return comboEffectLabel(eff, false);
}

std::string comboEffectCompactDesc(const ComboEffect& eff)
{
    return comboEffectLabel(eff, true);
}

const std::map<std::string, EffectType>& ChessBattleEffects::getEffectTypeMap()
{
    return effectTypeMap;
}

RoleComboEffectId BattleEffectState::applyConfiguredEffect(const ComboEffect& effect, int sourceComboId)
{
    return appendEffect(effect, RoleComboEffectOrigin::Configured, sourceComboId);
}

RoleComboEffectId BattleEffectState::grantRuntimeEffect(const ComboEffect& effect, int sourceComboId)
{
    return appendEffect(effect, RoleComboEffectOrigin::RuntimeGrant, sourceComboId);
}

const RoleComboEffectInstance& BattleEffectState::effect(RoleComboEffectId id) const
{
    assert(id.isValid());
    assert(static_cast<size_t>(id.value) < effects_.instances.size());
    const auto& instance = effects_.instances[id.value];
    assert(instance.id == id);
    return instance;
}

std::span<const RoleComboEffectId> BattleEffectState::effectIdsInAppendOrder() const
{
    return asSpan(effects_.idsInAppendOrder);
}

std::span<const RoleComboEffectId> BattleEffectState::effectIds(Trigger trigger, EffectType type) const
{
    return lookupIds(effects_.idsByTriggerAndType, trigger, type);
}

std::span<const RoleComboEffectId> BattleEffectState::idsFromCombo(int sourceComboId) const
{
    const auto it = effects_.idsBySourceComboId.find(sourceComboId);
    return it == effects_.idsBySourceComboId.end() ? asSpan(emptyEffectIds()) : asSpan(it->second);
}

const RoleComboStatBonuses& BattleEffectState::statBonuses() const
{
    return effects_.statBonuses;
}

bool BattleEffectState::isRuntimeGranted(RoleComboEffectId id) const
{
    return effect(id).origin == RoleComboEffectOrigin::RuntimeGrant;
}

bool BattleEffectState::canActivateTriggeredEffect(RoleComboEffectId id) const
{
    const auto& comboEffect = effect(id);
    assert(comboEffect.trigger != Trigger::Always);
    const auto runtime = runtime_.byEffect.find(id);
    const int activated = runtime == runtime_.byEffect.end() ? 0 : runtime->second.activationCount;
    return comboEffect.maxCount <= 0 || activated < comboEffect.maxCount;
}

void BattleEffectState::recordTriggeredEffectActivation(RoleComboEffectId id)
{
    const auto& comboEffect = effect(id);
    assert(comboEffect.trigger != Trigger::Always);
    ++runtime_.byEffect[id].activationCount;
}

int BattleEffectState::triggeredEffectActivationCount(RoleComboEffectId id) const
{
    const auto runtime = runtime_.byEffect.find(id);
    return runtime == runtime_.byEffect.end() ? 0 : runtime->second.activationCount;
}

bool BattleEffectState::hasTriggeredEffectActivations() const
{
    return std::ranges::any_of(runtime_.byEffect, [](const auto& entry)
        {
            return entry.second.activationCount > 0;
        });
}

bool BattleEffectState::hasActiveTriggerTimer(ComboTriggerTimerKey key) const
{
    const auto timer = runtime_.triggerTimers.find(key);
    return timer != runtime_.triggerTimers.end() && timer->second > 0;
}

bool BattleEffectState::ownsTriggerTimer(ComboTriggerTimerKey key) const
{
    for (RoleComboEffectId id : idsFromCombo(key.sourceComboId))
    {
        if (effect(id).trigger == key.trigger)
        {
            return true;
        }
    }
    return false;
}

void BattleEffectState::extendTriggerTimer(ComboTriggerTimerKey key, int durationFrames)
{
    assert(durationFrames > 0);
    auto& timer = runtime_.triggerTimers[key];
    timer = std::max(timer, durationFrames);
}

void BattleEffectState::advanceTriggerTimersOneFrame()
{
    for (auto& timerEntry : runtime_.triggerTimers)
    {
        auto& timer = timerEntry.second;
        if (timer > 0)
        {
            --timer;
        }
    }
}

void BattleEffectState::setLastAliveForComboRuntime(bool lastAlive)
{
    runtime_.lastAliveFlag = lastAlive;
}

bool BattleEffectState::lastAliveForComboRuntime() const
{
    return runtime_.lastAliveFlag;
}

void BattleEffectState::seedAutoUltimateFrameTimers()
{
    for (auto& [id, state] : runtime_.byEffect)
    {
        state.frameTimer = 0;
    }
    for (RoleComboEffectId id : effectIds(Trigger::Always, EffectType::AutoUltimateAfterFrames))
    {
        const auto& comboEffect = effect(id);
        if (comboEffect.value > 0)
        {
            setEffectFrameTimer(id, comboEffect.value);
        }
    }
}

bool BattleEffectState::advanceAutoUltimateFrameTimer(RoleComboEffectId id, int intervalFrames)
{
    return advanceEffectFrameTimer(id, intervalFrames);
}

int BattleEffectState::effectFrameTimerFrames(RoleComboEffectId id) const
{
    const auto runtime = runtime_.byEffect.find(id);
    return runtime == runtime_.byEffect.end() ? 0 : runtime->second.frameTimer;
}

int BattleEffectState::triggerTimerFrames(ComboTriggerTimerKey key) const
{
    const auto timer = runtime_.triggerTimers.find(key);
    return timer == runtime_.triggerTimers.end() ? 0 : timer->second;
}

void BattleEffectState::setTypePending(EffectType type, bool value)
{
    runtime_.byType[type].pending = value;
}

void BattleEffectState::clearTypePending()
{
    for (auto& entry : runtime_.byType)
    {
        entry.second.pending = false;
    }
}

bool BattleEffectState::typePending(EffectType type) const
{
    const auto runtime = runtime_.byType.find(type);
    return runtime != runtime_.byType.end() && runtime->second.pending;
}

bool BattleEffectState::consumeTypePending(EffectType type)
{
    auto& pending = runtime_.byType[type].pending;
    const bool value = pending;
    pending = false;
    return value;
}

bool BattleEffectState::typeToggle(EffectType type) const
{
    const auto runtime = runtime_.byType.find(type);
    return runtime != runtime_.byType.end() && runtime->second.toggle;
}

bool BattleEffectState::consumeTypeToggle(EffectType type)
{
    auto& toggle = runtime_.byType[type].toggle;
    const bool value = toggle;
    toggle = !toggle;
    return value;
}

bool BattleEffectState::advanceEffectCounter(RoleComboEffectId id, int threshold)
{
    assert(threshold > 0);
    auto& counter = runtime_.byEffect[id].counter;
    ++counter;
    if (counter >= threshold)
    {
        counter = 0;
        return true;
    }
    return false;
}

void BattleEffectState::setEffectFrameTimer(RoleComboEffectId id, int frames)
{
    assert(frames >= 0);
    runtime_.byEffect[id].frameTimer = frames;
}

bool BattleEffectState::advanceEffectFrameTimer(RoleComboEffectId id, int intervalFrames)
{
    assert(intervalFrames > 0);
    int& timer = runtime_.byEffect[id].frameTimer;
    if (timer <= 0)
    {
        timer = intervalFrames;
    }
    --timer;
    if (timer <= 0)
    {
        timer = intervalFrames;
        return true;
    }
    return false;
}

RoleComboStackChange BattleEffectState::recordEffectStack(RoleComboEffectId id, int maxStacks, int pctPerStack)
{
    assert(maxStacks > 0);
    auto& stacks = runtime_.byEffect[id].stacks;
    const int beforeStacks = stacks;
    stacks = std::min(stacks + 1, maxStacks);
    return { pctPerStack, stacks, stacks > beforeStacks };
}

RoleComboStackChange BattleEffectState::recordEffectStackAgainst(
    RoleComboEffectId id,
    int unitId,
    int maxStacks,
    int pctPerStack)
{
    assert(unitId >= 0);
    assert(maxStacks > 0);
    auto& stacks = runtime_.byEffect[id].stacksByUnit[unitId];
    const int beforeStacks = stacks;
    stacks = std::min(stacks + 1, maxStacks);
    return { pctPerStack, stacks, stacks > beforeStacks };
}

void BattleEffectState::setEffectIdleTimer(RoleComboEffectId id, int frames)
{
    assert(frames >= 0);
    runtime_.byEffect[id].idleTimer = frames;
}

void BattleEffectState::advanceEffectIdleTimers(std::span<const RoleComboEffectId> ids)
{
    for (RoleComboEffectId id : ids)
    {
        auto& runtime = runtime_.byEffect[id];
        if (runtime.idleTimer > 0)
        {
            --runtime.idleTimer;
        }
        else
        {
            runtime.stacks = 0;
        }
    }
}

int BattleEffectState::effectStacks(RoleComboEffectId id) const
{
    const auto runtime = runtime_.byEffect.find(id);
    return runtime == runtime_.byEffect.end() ? 0 : runtime->second.stacks;
}

int BattleEffectState::effectIdleTimer(RoleComboEffectId id) const
{
    const auto runtime = runtime_.byEffect.find(id);
    return runtime == runtime_.byEffect.end() ? 0 : runtime->second.idleTimer;
}

int BattleEffectState::effectStacksAgainst(RoleComboEffectId id, int unitId) const
{
    const auto runtime = runtime_.byEffect.find(id);
    if (runtime == runtime_.byEffect.end())
    {
        return 0;
    }
    const auto stacks = runtime->second.stacksByUnit.find(unitId);
    return stacks == runtime->second.stacksByUnit.end() ? 0 : stacks->second;
}

int BattleEffectState::setEnemyTopDebuffApplied(int desired)
{
    const int delta = desired - runtime_.enemyTopDebuffApplied;
    runtime_.enemyTopDebuffApplied = desired;
    return delta;
}

int BattleEffectState::dodgeAdaptationBonusAgainst(int attackerUnitId) const
{
    assert(attackerUnitId >= 0);
    int bonus = 0;
    for (const auto& [id, descriptor] : effects_.dodgeAdaptations)
    {
        bonus += effectStacksAgainst(id, attackerUnitId) * descriptor.pctPerStack;
    }
    return bonus;
}

int BattleEffectState::sumAlways(EffectType type) const
{
    const auto* summary = lookupSummary(effects_.alwaysByType, type);
    return summary ? summary->sumValue : 0;
}

int BattleEffectState::maxAlways(EffectType type) const
{
    const auto* summary = lookupSummary(effects_.alwaysByType, type);
    return summary ? summary->maxValue : 0;
}

int BattleEffectState::maxAlwaysValue2(EffectType type) const
{
    const auto* summary = lookupSummary(effects_.alwaysByType, type);
    return summary ? summary->maxValue2 : 0;
}

bool BattleEffectState::hasAlways(EffectType type) const
{
    return lookupSummary(effects_.alwaysByType, type) != nullptr;
}

const RoleComboEffectInstance* BattleEffectState::firstAlways(EffectType type) const
{
    const auto* summary = lookupSummary(effects_.alwaysByType, type);
    if (!summary)
    {
        return nullptr;
    }
    assert(summary->firstId.isValid());
    return &effect(summary->firstId);
}

const RoleComboEffectInstance* BattleEffectState::maxAlwaysByValue(EffectType type) const
{
    const auto* summary = lookupSummary(effects_.alwaysByType, type);
    if (!summary)
    {
        return nullptr;
    }
    assert(summary->maxByValueId.isValid());
    return &effect(summary->maxByValueId);
}

bool BattleEffectState::hasComboApplied(int comboId) const
{
    return !idsFromCombo(comboId).empty();
}

RoleComboEffectId BattleEffectState::appendEffect(
    const ComboEffect& comboEffect,
    RoleComboEffectOrigin origin,
    int sourceComboId)
{
    const RoleComboEffectId id{ static_cast<int>(effects_.instances.size()) };

    RoleComboEffectInstance instance;
    static_cast<ComboEffect&>(instance) = comboEffect;
    instance.id = id;
    instance.origin = origin;
    instance.sourceComboId = sourceComboId;
    effects_.instances.push_back(instance);
    effects_.idsInAppendOrder.push_back(id);

    const RoleComboEffectLookupKey key{ comboEffect.trigger, comboEffect.type };
    effects_.idsByTriggerAndType[key].push_back(id);
    if (sourceComboId >= 0)
    {
        effects_.idsBySourceComboId[sourceComboId].push_back(id);
    }

    if (comboEffect.trigger == Trigger::Always)
    {
        updateAlwaysSummary(effects_.alwaysByType, effects_, id);
        updateStatBonuses(effects_.statBonuses, comboEffect);

        if (comboEffect.type == EffectType::Adaptation)
        {
            effects_.adaptations[id] = { comboEffect.value, comboEffect.value2 };
        }
        else if (comboEffect.type == EffectType::DodgeAdaptation)
        {
            effects_.dodgeAdaptations[id] = { comboEffect.value, comboEffect.value2 };
        }
        else if (comboEffect.type == EffectType::RampingDmg)
        {
            effects_.rampings[id] = { comboEffect.value, comboEffect.value2 };
        }
    }

    return id;
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

namespace
{

std::vector<ComboEffect> normalizedMagicEffects(const ComboEffect& effect)
{
    if (effect.type != EffectType::OffensiveCharm)
    {
        return { effect };
    }

    ComboEffect debuff = effect;
    debuff.type = EffectType::CharmCDRDebuff;
    return { effect, debuff };
}

bool printMagicLoadError(const YAML::Node& node, const std::string& context, const std::string& message)
{
    const auto mark = node.Mark();
    if (!mark.is_null())
    {
        std::print("【武功效果】「{}」(第{}行，第{}列) {}\n", context, mark.line + 1, mark.column + 1, message);
    }
    else
    {
        std::print("【武功效果】「{}」{}\n", context, message);
    }
    return false;
}

}  // namespace

bool ChessBattleEffects::parseMagicEffects(
    const YAML::Node& root,
    std::vector<ChessMagicEffectDefinition>& out,
    const std::string& context)
{
    out.clear();
    if (!root || !root.IsMap())
    {
        return printMagicLoadError(root, context, "根節點必須是映射表");
    }

    if (const auto enabled = root["啟用"])
    {
        try
        {
            if (!enabled.as<bool>())
            {
                return true;
            }
        }
        catch (const YAML::Exception& ex)
        {
            return printMagicLoadError(enabled, context, std::format("「啟用」欄位不是有效布林值: {}", ex.what()));
        }
    }

    const auto entries = root["武功效果"];
    if (!entries)
    {
        return printMagicLoadError(root, context, "缺少「武功效果」根節點");
    }
    if (!entries.IsSequence())
    {
        return printMagicLoadError(entries, context, "「武功效果」必須是列表");
    }

    std::set<int> seenMagicIds;
    for (const auto& entryNode : entries)
    {
        if (!entryNode || !entryNode.IsMap())
        {
            return printMagicLoadError(entryNode, context, "武功項目必須是映射表");
        }

        ChessMagicEffectDefinition definition;
        try
        {
            if (!entryNode["武功"])
            {
                return printMagicLoadError(entryNode, context, "缺少「武功」字段");
            }
            definition.magicId = entryNode["武功"].as<int>();
            if (entryNode["名称"])
            {
                definition.name = entryNode["名称"].as<std::string>();
            }
            else if (entryNode["名稱"])
            {
                definition.name = entryNode["名稱"].as<std::string>();
            }
            if (entryNode["用途"])
            {
                definition.purpose = entryNode["用途"].as<std::string>();
            }
        }
        catch (const YAML::Exception& ex)
        {
            return printMagicLoadError(entryNode, context, std::format("武功欄位解析失敗: {}", ex.what()));
        }

        if (definition.magicId < 0)
        {
            return printMagicLoadError(entryNode, context, "「武功」必須是非負整數");
        }
        if (!seenMagicIds.insert(definition.magicId).second)
        {
            return printMagicLoadError(entryNode, context, std::format("武功 {} 重複定義", definition.magicId));
        }

        const auto effectNodes = entryNode["效果"];
        if (!effectNodes || !effectNodes.IsSequence() || effectNodes.size() == 0)
        {
            return printMagicLoadError(entryNode, context, std::format("武功 {} 缺少有效「效果」列表", definition.magicId));
        }

        for (const auto& effectNode : effectNodes)
        {
            ComboEffect parsed;
            if (!parseEffect(effectNode, parsed, std::format("{}:武功{}", context, definition.magicId)))
            {
                return false;
            }
            for (const auto& normalized : normalizedMagicEffects(parsed))
            {
                if (!Battle::isSelectedSkillMagicAllowed(normalized.type, normalized.trigger))
                {
                    return printMagicLoadError(
                        effectNode,
                        context,
                        std::format("武功 {} 的效果沒有第一階段武功作用域", definition.magicId));
                }
                if (!Battle::isUltimateOnlyMagicEffect(normalized.type, normalized.trigger))
                {
                    return printMagicLoadError(
                        effectNode,
                        context,
                        std::format("武功 {} 的效果不符合目前只支援絕招武功效果的策略", definition.magicId));
                }
                definition.effects.push_back(normalized);
            }
        }

        out.push_back(std::move(definition));
    }
    return true;
}

bool ChessBattleEffects::loadMagicEffectsFile(
    const std::string& path,
    std::vector<ChessMagicEffectDefinition>& out)
{
    try
    {
        return parseMagicEffects(YAML::LoadFile(path), out, path);
    }
    catch (const YAML::Exception& ex)
    {
        std::print("【武功效果】讀取「{}」失敗: {}\n", path, ex.what());
        out.clear();
        return false;
    }
}

bool ChessBattleEffects::loadDefaultMagicEffectsFile(std::vector<ChessMagicEffectDefinition>& out)
{
    return loadMagicEffectsFile(defaultMagicEffectsConfigPath(), out);
}

RoleComboState ChessBattleEffects::makeSummonedCloneState(const RoleComboState& sourceState)
{
    RoleComboState cloneState = sourceState;
    cloneState.seedAutoUltimateFrameTimers();
    return cloneState;
}

void ChessBattleEffects::mergeEffects(std::map<int, RoleComboState>& states,
                                      const std::vector<ComboEffect>& effects,
                                      const std::vector<int>& roleIds,
                                      int sourceComboId)
{
    for (int rid : roleIds)
    {
        auto& state = states[rid];
        for (const auto& effect : effects)
        {
            state.applyConfiguredEffect(effect, sourceComboId);
        }
    }
}

}  // namespace KysChess
