#include "ChessBattleEffects.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cassert>
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

}  // namespace

const std::map<std::string, EffectType>& ChessBattleEffects::getEffectTypeMap()
{
    return effectTypeMap;
}

RoleComboEffectId RoleComboState::applyConfiguredEffect(const ComboEffect& effect, int sourceComboId)
{
    return appendEffect(effect, RoleComboEffectOrigin::Configured, sourceComboId);
}

RoleComboEffectId RoleComboState::grantRuntimeEffect(const ComboEffect& effect, int sourceComboId)
{
    return appendEffect(effect, RoleComboEffectOrigin::RuntimeGrant, sourceComboId);
}

const RoleComboEffectInstance& RoleComboState::effect(RoleComboEffectId id) const
{
    assert(id.isValid());
    assert(static_cast<size_t>(id.value) < effects_.instances.size());
    const auto& instance = effects_.instances[id.value];
    assert(instance.id == id);
    return instance;
}

std::span<const RoleComboEffectId> RoleComboState::effectIdsInAppendOrder() const
{
    return asSpan(effects_.idsInAppendOrder);
}

std::span<const RoleComboEffectId> RoleComboState::effectIds(Trigger trigger, EffectType type) const
{
    return lookupIds(effects_.idsByTriggerAndType, trigger, type);
}

std::span<const RoleComboEffectId> RoleComboState::idsFromCombo(int sourceComboId) const
{
    const auto it = effects_.idsBySourceComboId.find(sourceComboId);
    return it == effects_.idsBySourceComboId.end() ? asSpan(emptyEffectIds()) : asSpan(it->second);
}

const RoleComboStatBonuses& RoleComboState::statBonuses() const
{
    return effects_.statBonuses;
}

bool RoleComboState::isRuntimeGranted(RoleComboEffectId id) const
{
    return effect(id).origin == RoleComboEffectOrigin::RuntimeGrant;
}

bool RoleComboState::canActivateTriggeredEffect(RoleComboEffectId id) const
{
    const auto& comboEffect = effect(id);
    assert(comboEffect.trigger != Trigger::Always);
    const auto runtime = runtime_.byEffect.find(id);
    const int activated = runtime == runtime_.byEffect.end() ? 0 : runtime->second.activationCount;
    return comboEffect.maxCount <= 0 || activated < comboEffect.maxCount;
}

void RoleComboState::recordTriggeredEffectActivation(RoleComboEffectId id)
{
    const auto& comboEffect = effect(id);
    assert(comboEffect.trigger != Trigger::Always);
    ++runtime_.byEffect[id].activationCount;
}

int RoleComboState::triggeredEffectActivationCount(RoleComboEffectId id) const
{
    const auto runtime = runtime_.byEffect.find(id);
    return runtime == runtime_.byEffect.end() ? 0 : runtime->second.activationCount;
}

bool RoleComboState::hasTriggeredEffectActivations() const
{
    return std::ranges::any_of(runtime_.byEffect, [](const auto& entry)
        {
            return entry.second.activationCount > 0;
        });
}

bool RoleComboState::hasActiveTriggerTimer(ComboTriggerTimerKey key) const
{
    const auto timer = runtime_.triggerTimers.find(key);
    return timer != runtime_.triggerTimers.end() && timer->second > 0;
}

bool RoleComboState::ownsTriggerTimer(ComboTriggerTimerKey key) const
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

void RoleComboState::extendTriggerTimer(ComboTriggerTimerKey key, int durationFrames)
{
    assert(durationFrames > 0);
    auto& timer = runtime_.triggerTimers[key];
    timer = std::max(timer, durationFrames);
}

void RoleComboState::advanceTriggerTimersOneFrame()
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

void RoleComboState::setLastAliveForComboRuntime(bool lastAlive)
{
    runtime_.lastAliveFlag = lastAlive;
}

bool RoleComboState::lastAliveForComboRuntime() const
{
    return runtime_.lastAliveFlag;
}

void RoleComboState::seedAutoUltimateFrameTimers()
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

bool RoleComboState::advanceAutoUltimateFrameTimer(RoleComboEffectId id, int intervalFrames)
{
    return advanceEffectFrameTimer(id, intervalFrames);
}

int RoleComboState::effectFrameTimerFrames(RoleComboEffectId id) const
{
    const auto runtime = runtime_.byEffect.find(id);
    return runtime == runtime_.byEffect.end() ? 0 : runtime->second.frameTimer;
}

int RoleComboState::triggerTimerFrames(ComboTriggerTimerKey key) const
{
    const auto timer = runtime_.triggerTimers.find(key);
    return timer == runtime_.triggerTimers.end() ? 0 : timer->second;
}

void RoleComboState::setTypePending(EffectType type, bool value)
{
    runtime_.byType[type].pending = value;
}

bool RoleComboState::typePending(EffectType type) const
{
    const auto runtime = runtime_.byType.find(type);
    return runtime != runtime_.byType.end() && runtime->second.pending;
}

bool RoleComboState::consumeTypePending(EffectType type)
{
    auto& pending = runtime_.byType[type].pending;
    const bool value = pending;
    pending = false;
    return value;
}

bool RoleComboState::typeToggle(EffectType type) const
{
    const auto runtime = runtime_.byType.find(type);
    return runtime != runtime_.byType.end() && runtime->second.toggle;
}

bool RoleComboState::consumeTypeToggle(EffectType type)
{
    auto& toggle = runtime_.byType[type].toggle;
    const bool value = toggle;
    toggle = !toggle;
    return value;
}

bool RoleComboState::advanceEffectCounter(RoleComboEffectId id, int threshold)
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

void RoleComboState::setEffectFrameTimer(RoleComboEffectId id, int frames)
{
    assert(frames >= 0);
    runtime_.byEffect[id].frameTimer = frames;
}

bool RoleComboState::advanceEffectFrameTimer(RoleComboEffectId id, int intervalFrames)
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

RoleComboStackChange RoleComboState::recordEffectStack(RoleComboEffectId id, int maxStacks, int pctPerStack)
{
    assert(maxStacks > 0);
    auto& stacks = runtime_.byEffect[id].stacks;
    const int beforeStacks = stacks;
    stacks = std::min(stacks + 1, maxStacks);
    return { pctPerStack, stacks, stacks > beforeStacks };
}

RoleComboStackChange RoleComboState::recordEffectStackAgainst(
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

void RoleComboState::setEffectIdleTimer(RoleComboEffectId id, int frames)
{
    assert(frames >= 0);
    runtime_.byEffect[id].idleTimer = frames;
}

void RoleComboState::advanceEffectIdleTimers(std::span<const RoleComboEffectId> ids)
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

int RoleComboState::effectStacks(RoleComboEffectId id) const
{
    const auto runtime = runtime_.byEffect.find(id);
    return runtime == runtime_.byEffect.end() ? 0 : runtime->second.stacks;
}

int RoleComboState::effectIdleTimer(RoleComboEffectId id) const
{
    const auto runtime = runtime_.byEffect.find(id);
    return runtime == runtime_.byEffect.end() ? 0 : runtime->second.idleTimer;
}

int RoleComboState::effectStacksAgainst(RoleComboEffectId id, int unitId) const
{
    const auto runtime = runtime_.byEffect.find(id);
    if (runtime == runtime_.byEffect.end())
    {
        return 0;
    }
    const auto stacks = runtime->second.stacksByUnit.find(unitId);
    return stacks == runtime->second.stacksByUnit.end() ? 0 : stacks->second;
}

int RoleComboState::setEnemyTopDebuffApplied(int desired)
{
    const int delta = desired - runtime_.enemyTopDebuffApplied;
    runtime_.enemyTopDebuffApplied = desired;
    return delta;
}

int RoleComboState::dodgeAdaptationBonusAgainst(int attackerUnitId) const
{
    assert(attackerUnitId >= 0);
    int bonus = 0;
    for (const auto& [id, descriptor] : effects_.dodgeAdaptations)
    {
        bonus += effectStacksAgainst(id, attackerUnitId) * descriptor.pctPerStack;
    }
    return bonus;
}

int RoleComboState::sumAlways(EffectType type) const
{
    const auto* summary = lookupSummary(effects_.alwaysByType, type);
    return summary ? summary->sumValue : 0;
}

int RoleComboState::maxAlways(EffectType type) const
{
    const auto* summary = lookupSummary(effects_.alwaysByType, type);
    return summary ? summary->maxValue : 0;
}

int RoleComboState::maxAlwaysValue2(EffectType type) const
{
    const auto* summary = lookupSummary(effects_.alwaysByType, type);
    return summary ? summary->maxValue2 : 0;
}

bool RoleComboState::hasAlways(EffectType type) const
{
    return lookupSummary(effects_.alwaysByType, type) != nullptr;
}

const RoleComboEffectInstance* RoleComboState::firstAlways(EffectType type) const
{
    const auto* summary = lookupSummary(effects_.alwaysByType, type);
    if (!summary)
    {
        return nullptr;
    }
    assert(summary->firstId.isValid());
    return &effect(summary->firstId);
}

const RoleComboEffectInstance* RoleComboState::maxAlwaysByValue(EffectType type) const
{
    const auto* summary = lookupSummary(effects_.alwaysByType, type);
    if (!summary)
    {
        return nullptr;
    }
    assert(summary->maxByValueId.isValid());
    return &effect(summary->maxByValueId);
}

bool RoleComboState::hasComboApplied(int comboId) const
{
    return !idsFromCombo(comboId).empty();
}

RoleComboEffectId RoleComboState::appendEffect(
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
