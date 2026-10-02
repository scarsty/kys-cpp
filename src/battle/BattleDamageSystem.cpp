#include "BattleDamageSystem.h"

#include "../ChessBattleEffectTypes.h"
#include "BattleEffectSystem.h"
#include "BattleHealSystem.h"
#include "BattleMath.h"
#include "BattleResourceRules.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <utility>

namespace KysChess::Battle
{

namespace
{

struct StatusDamageInterceptor
{
    bool blocks{};
    int cap{};
    BattleStatusKind kind{};
    std::uint64_t appliedSequence{};
    EffectExecutionOrderKey order;
};

std::vector<StatusDamageInterceptor> statusDamageInterceptors(
    const BattleStatusEffectState& effects)
{
    std::vector<StatusDamageInterceptor> result;
    for (const auto& contribution : effects.statuses)
    {
        if (!contribution.behavior || contribution.stacks <= 0) continue;
        for (std::uint32_t ruleOrder = 0;
             ruleOrder < contribution.behavior->rules.size();
             ++ruleOrder)
        {
            const auto& rule = contribution.behavior->rules[ruleOrder];
            if (rule.event != EffectEvent::StatusPersistent) continue;
            for (std::uint32_t actionOrder = 0;
                 actionOrder < rule.actions.size();
                 ++actionOrder)
            {
                const auto& action = rule.actions[actionOrder];
                const bool blocks = std::holds_alternative<BlockPositiveDamageAction>(
                    action.value);
                int cap{};
                if (const auto* modifier = std::get_if<ModifyDamageAction>(
                        &action.value);
                    modifier
                    && modifier->operation
                        == DamageModifierOperation::CapSingleHitAtValue)
                {
                    const auto value = effectiveConstantEffectNumberValue(
                        modifier->amount);
                    assert(value && *value > 0);
                    cap = *value;
                }
                if (!blocks && cap == 0) continue;
                assert(contribution.origin);
                assert(contribution.producer);
                result.push_back({
                    .blocks = blocks,
                    .cap = cap,
                    .kind = contribution.kind,
                    .appliedSequence = contribution.appliedSequence,
                    .order = statusBehaviorExecutionOrderKey(
                        contribution.origin->binding,
                        contribution.origin->ruleOrder,
                        contribution.producer->actionOrder,
                        ruleOrder,
                        -1,
                        contribution.appliedSequence,
                        actionOrder),
                });
            }
        }
    }
    std::ranges::stable_sort(result, [](const auto& lhs, const auto& rhs)
    {
        return lhs.order < rhs.order;
    });
    return result;
}

BattleDamageKind effectiveDamageKind(const BattleDamageRequest& request)
{
    if (request.damageKind != BattleDamageKind::Physical)
    {
        return request.damageKind;
    }
    if (request.usingSkill)
    {
        return BattleDamageKind::Skill;
    }
    return BattleDamageKind::Physical;
}

void applyDamageReduction(
    BattleFixed& damage,
    int reductionPct,
    int& remainingDamageBasisPoints)
{
    if (reductionPct <= 0)
    {
        return;
    }
    assert(remainingDamageBasisPoints >= (100 - FinalDamageReductionCapPct) * 100);
    const int requestedRemaining = remainingDamageBasisPoints
        * (100 - std::min(reductionPct, 100)) / 100;
    const int cappedRemaining = std::max(
        (100 - FinalDamageReductionCapPct) * 100,
        requestedRemaining);
    damage = damage.scaled(cappedRemaining, remainingDamageBasisPoints);
    remainingDamageBasisPoints = cappedRemaining;
}

void applySignedDamageDelta(
    BattleFixed& damage,
    int pctDelta,
    int& remainingDamageBasisPoints)
{
    if (pctDelta < 0)
    {
        const int reduction = pctDelta == std::numeric_limits<int>::min()
            ? 100
            : -pctDelta;
        applyDamageReduction(damage, reduction, remainingDamageBasisPoints);
    }
    else if (pctDelta > 0)
    {
        damage = damage.scaledPercentSaturated(
            static_cast<std::int64_t>(100) + pctDelta);
    }
}

void applyTypedDefenderStatusModifiers(
    BattleFixed& damage,
    const BattleStatusPersistentModifiers& statuses,
    int& remainingDamageBasisPoints)
{
    applyDamageReduction(
        damage,
        statuses.damageReductionPct,
        remainingDamageBasisPoints);
    applySignedDamageDelta(
        damage,
        statuses.damageTakenPct,
        remainingDamageBasisPoints);
}

BattleUnitDelta makeBattleUnitDelta(const BattleDamageUnitState& before, const BattleDamageUnitState& after)
{
    BattleUnitDelta delta;
    delta.unitId = after.id;
    delta.hpDelta = after.vitals.hp - before.vitals.hp;
    delta.mpDelta = after.vitals.mp - before.vitals.mp;
    delta.shieldDelta = after.shield - before.shield;
    delta.invincibleDelta = after.invincible - before.invincible;
    delta.attackDelta = after.attack - before.attack;
    delta.aliveChanged = before.alive != after.alive;
    delta.alive = after.alive;
    return delta;
}

void recordBattleDamageEvent(std::vector<BattleDamageEvent>& events,
                             BattleDamageEventType type,
                             int sourceUnitId,
                             int targetUnitId,
                             int value)
{
    events.push_back({ type, BattleDamageStatusType::None, sourceUnitId, targetUnitId, value, 0 });
}

void recordBattleStatusEvent(std::vector<BattleDamageEvent>& events,
                             BattleDamageStatusType statusType,
                             int sourceUnitId,
                             int targetUnitId,
                             int value,
                             int maxValue = 0)
{
    events.push_back({ BattleDamageEventType::StatusApplied, statusType, sourceUnitId, targetUnitId, value, maxValue });
}

BattleResourceUnitState makeBattleResourceUnit(const BattleDamageUnitState& unit)
{
    BattleResourceUnitState resource;
    resource.id = unit.id;
    resource.alive = unit.alive;
    resource.vitals = unit.vitals;
    resource.mpBlocked = unit.mpBlocked;
    resource.mpRecoveryBonusPct = unit.mpRecoveryBonusPct;
    return resource;
}

void writeBattleResourceUnit(BattleDamageUnitState& unit, const BattleResourceUnitState& resource)
{
    unit.vitals.hp = resource.vitals.hp;
    unit.vitals.mp = resource.vitals.mp;
}

BattleHealUnitSnapshot healSnapshot(const BattleDamageUnitState& unit)
{
    return { unit.id, unit.alive, unit.vitals.hp, unit.vitals.maxHp };
}

BattleHealUnitSnapshot healSnapshot(const BattleResourceUnitState& unit)
{
    return { unit.id, unit.alive, unit.vitals.hp, unit.vitals.maxHp };
}

BattleHealRequest damageHealRequest(
    int unitId,
    BattleHealKind kind,
    BattleHealAmount amount)
{
    BattleHealRequest request;
    request.sourceUnitId = unitId;
    request.targetUnitId = unitId;
    request.source = {
        .kind = EffectSourceKind::Combo,
        .sourceId = -1,
        .ownerUnitId = unitId,
        .sourceTeam = -1,
    };
    request.kind = kind;
    request.amount = amount;
    return request;
}

}  // namespace

BattleDamageRuntimeUnit makeBattleDamageRuntimeUnit(const BattleDamageUnitState& unit)
{
    BattleDamageRuntimeUnit runtime;
    runtime.hurtInvincFrames = unit.hurtInvincFrames;
    runtime.dualWieldBlocksRemaining = unit.dualWieldBlocksRemaining;
    runtime.deathPrevention = unit.deathPrevention;
    runtime.deathPreventionUsed = unit.deathPreventionUsed;
    runtime.deathPreventionFrames = unit.deathPreventionFrames;
    runtime.lethalRecovery = unit.lethalRecovery;
    return runtime;
}

BattleDamageTransactionResult BattleDamageSystem::resolveTransaction(const BattleDamageTransactionInput& input, BattleRuntimeRandom* recoveryRandom) const
{
    assert(input.request.attackerUnitId == input.attacker.id);
    assert(input.request.defenderUnitId == input.defender.id);
    assert(input.request.baseDamage >= 0 && input.request.mpDamage >= 0);
    assert(input.request.preResolvedDamage
        || input.request.preResolvedModifierPolicy == BattlePreResolvedModifierPolicy::None);
    assert(input.request.preResolvedDamageReductionBasisPoints >= 0);
    assert(input.request.preResolvedDamageReductionBasisPoints
        <= FinalDamageReductionCapPct * 100);
    assert(input.request.preResolvedDamage
        || input.request.preResolvedDamageReductionBasisPoints == 0);

    BattleDamageTransactionResult result;
    result.attacker = input.attacker;
    result.defender = input.defender;
    result.defenderStatus = input.defenderStatus;
    result.defenderCooldown = input.defenderCooldown;
    result.damageKind = effectiveDamageKind(input.request);
    bool acceptedHit = input.request.acceptedHit;

    if (input.request.baseDamage > 0)
    {
        BattleFixed resolvedDamage = input.request.baseDamage;
        int combinedReductionBasisPoints =
            input.request.preResolvedDamageReductionBasisPoints;
        if (!input.request.preResolvedDamage)
        {
            BattleDamageModifierInput modifierInput;
            modifierInput.damage = input.request.baseDamage;
            modifierInput.damageKind = result.damageKind;
            modifierInput.usingSkill = input.request.usingSkill;
            modifierInput.ignoreDefense = input.request.ignoreDefense;
            modifierInput.attacker = input.attackerModifiers;
            modifierInput.defender = input.defenderModifiers;
            modifierInput.defenderUnit = result.defender;

            BattleStatusSystem statusSystem({});
            if (input.attackerStatus.id == input.attacker.id)
            {
                const auto attackerStatuses = statusSystem.persistentModifiers(input.attackerStatus.effects);
                modifierInput.attacker.skillDamagePct = battleSaturatedAdd(
                    modifierInput.attacker.skillDamagePct,
                    attackerStatuses.skillDamagePct);
            }
            if (result.defenderStatus.id == input.defender.id)
            {
                const auto defenderStatuses = statusSystem.persistentModifiers(result.defenderStatus.effects);
                modifierInput.defender.damageReductionPct = battleSaturatedAdd(
                    modifierInput.defender.damageReductionPct,
                    defenderStatuses.damageReductionPct);
                modifierInput.defender.damageTakenIncreasePct = battleSaturatedAdd(
                    modifierInput.defender.damageTakenIncreasePct,
                    defenderStatuses.damageTakenPct);
                modifierInput.defender.poisoned = modifierInput.defender.poisoned
                    || result.defenderStatus.effects.has(BattleStatusKind::Poison);
            }

            auto modified = applyModifiers(modifierInput);

            resolvedDamage = modified.damage;
            combinedReductionBasisPoints = modified.combinedDamageReductionBasisPoints;
        }
        else if (input.request.preResolvedModifierPolicy
                 == BattlePreResolvedModifierPolicy::DefenderTypedStatuses)
        {
            assert(result.defenderStatus.id == input.defender.id);
            const auto statuses = BattleStatusSystem({}).persistentModifiers(result.defenderStatus.effects);
            int remainingDamageBasisPoints = 10'000 - combinedReductionBasisPoints;
            applyTypedDefenderStatusModifiers(
                resolvedDamage,
                statuses,
                remainingDamageBasisPoints);
            combinedReductionBasisPoints = 10'000 - remainingDamageBasisPoints;
        }
        int remainingDamageBasisPoints = 10'000 - combinedReductionBasisPoints;
        applySignedDamageDelta(
            resolvedDamage,
            input.liveOutgoingDamagePctDelta,
            remainingDamageBasisPoints);
        result.combinedDamageReductionBasisPoints = 10'000 - remainingDamageBasisPoints;
        int resolvedDamageValue = resolvedDamage.toInt();
        result.resolvedDamageBeforeDefense = resolvedDamageValue;
        result.executed = input.request.canExecute
            && shouldExecute({
                result.defender.vitals.hp,
                result.defender.vitals.maxHp,
                resolvedDamageValue,
                true,
                input.request.executeThresholdPct,
            });
        if (result.executed)
        {
            result.damageKind = BattleDamageKind::Execute;
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::ExecuteTriggered,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    input.request.executeThresholdPct);
        }

        std::optional<StatusDamageInterceptor> selectedInterceptor;
        BattleStatusSystem statusSystem({});
        if (result.defenderStatus.id == input.defender.id)
        {
            const auto interceptors = statusDamageInterceptors(
                result.defenderStatus.effects);
            const auto block = std::ranges::find_if(
                interceptors,
                &StatusDamageInterceptor::blocks);
            if (block != interceptors.end())
            {
                selectedInterceptor = *block;
            }
            else
            {
                const auto cap = std::ranges::min_element(
                    interceptors,
                    [](const StatusDamageInterceptor& lhs,
                       const StatusDamageInterceptor& rhs)
                    {
                        const int lhsCap = lhs.cap > 0
                            ? lhs.cap
                            : std::numeric_limits<int>::max();
                        const int rhsCap = rhs.cap > 0
                            ? rhs.cap
                            : std::numeric_limits<int>::max();
                        if (lhsCap != rhsCap) return lhsCap < rhsCap;
                        if (lhs.appliedSequence != rhs.appliedSequence)
                            return lhs.appliedSequence < rhs.appliedSequence;
                        return lhs.order < rhs.order;
                    });
                if (cap != interceptors.end() && cap->cap > 0)
                {
                    selectedInterceptor = *cap;
                }
            }
        }

        BattleDamageDefenseInput defenseInput;
        defenseInput.damage = resolvedDamageValue;
        defenseInput.executed = result.executed;
        defenseInput.defenderWasInvincible = result.defender.invincible > 0;
        defenseInput.defender = result.defender;
        defenseInput.blockByStatusLayer = selectedInterceptor
            && selectedInterceptor->blocks;
        defenseInput.singleHitCap = selectedInterceptor
            ? selectedInterceptor->cap
            : 0;
        defenseInput.remainingDamageBasisPoints =
            10'000 - result.combinedDamageReductionBasisPoints;
        defenseInput.absorptionLayers = input.absorptionLayers;
        auto defense = resolveDefense(defenseInput);
        result.defender = defense.defender;
        result.shieldAbsorbed = defense.shieldAbsorbed;
        result.absorptionReceipts = std::move(defense.absorptionReceipts);
        result.combinedDamageReductionBasisPoints =
            10'000 - defense.remainingDamageBasisPoints;
        result.blockedByInvincible = defense.blockedByInvincible;
        result.blockedByDualWield = defense.blockedByDualWield;
        result.blockedByDamageLayer = defense.blockedByDamageLayer;
        result.singleHitCapConsumed = defense.singleHitCapConsumed;
        result.singleHitCapped = defense.singleHitCapped;
        if (defense.blockedByDamageLayer || defense.singleHitCapConsumed)
        {
            auto consumed = statusSystem.consume(
                result.defenderStatus,
                {
                    .kind = selectedInterceptor->kind,
                    .filter = {
                        .holderUnitId = result.defenderStatus.id,
                        .appliedSequence = selectedInterceptor->appliedSequence,
                    },
                });
            assert(consumed.consumed);
            result.defenseStatusConsumed = BattleStatusConsumptionReceipt{
                consumed.target.id, std::move(consumed.consumedStatus), consumed.remainingStacks };
            result.defenderStatus = std::move(consumed.target);
        }
        acceptedHit = !defense.blockedByInvincible
            && !defense.blockedByDualWield
            && !defense.blockedByDamageLayer;
        resolvedDamageValue = defense.damage;

        if (defense.shieldAbsorbed > 0)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::ShieldAbsorbed,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    defense.shieldAbsorbed);
        }
        if (defense.blockedByInvincible)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::BlockedByInvincible,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    0);
        }
        if (defense.blockedByDualWield)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::BlockedByDualWield,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    0);
        }
        if (defense.blockedByDamageLayer)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::BlockedByDamageLayer,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    0);
        }
        if (defense.singleHitCapped)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::SingleHitCapped,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    defense.singleHitCap);
        }

        int hpBeforeDamage = result.defender.vitals.hp;
        int hpDamage = resolvedDamageValue;
        if (result.executed)
        {
            hpDamage = std::max(hpDamage, result.defender.vitals.hp);
        }

        if (input.redirectHpDamage && !result.executed)
        {
            result.redirectedHpDamage = hpDamage;
            hpDamage = 0;
        }
        auto taken = applyDamageTaken(result.defender, hpDamage, input.request.triggersDefenseEffects, recoveryRandom);
        result.defender = taken.defender;
        result.finalHpDamage = std::max(0, hpBeforeDamage - result.defender.vitals.hp);
        result.hurtInvincGranted = taken.hurtInvincGranted;
        result.deathPrevented = taken.deathPrevented;
        result.invincibilityGranted = taken.invincibilityGranted;
        result.killed = taken.died;
        result.recoveryTested = taken.recoveryTested;
        result.recoverySucceeded = taken.recoverySucceeded;

        if (result.finalHpDamage > 0)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::DamageApplied,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    result.finalHpDamage);
        }
        if (taken.deathPrevented)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::DeathPrevented,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    taken.invincibilityGranted);
        }
        if (taken.died)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::UnitDied,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    0);
        }
    }

    if (input.request.mpDamage > 0)
    {
        int mpBefore = result.defender.vitals.mp;
        int mpDamage = std::min(input.request.mpDamage, std::max(0, result.defender.vitals.mp));
        result.defender.vitals.mp -= mpDamage;
        result.finalMpDamage = mpBefore - result.defender.vitals.mp;
        if (result.finalMpDamage > 0)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::MpDamageApplied,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    result.finalMpDamage);
        }
    }

    if (acceptedHit)
    {
        const bool canApplyStatusEffects = input.defender.invincible <= 0;
        if (input.request.mpOnHit > 0 || input.request.hpOnHit > 0 || input.request.mpDrain > 0)
        {
            const auto attackerBeforeResources = healSnapshot(result.attacker);
            auto resources = applyOnHitResources({
                makeBattleResourceUnit(result.attacker),
                makeBattleResourceUnit(result.defender),
                input.request.mpOnHit,
                input.request.hpOnHit,
                input.request.mpDrain,
                input.attackerHealModifiers,
            });
            if (resources.heal)
            {
                auto attackerAfterHeal = attackerBeforeResources;
                attackerAfterHeal.hp = resources.heal->hpAfter;
                result.resolvedHeals.push_back({
                    .result = *resources.heal,
                    .sourceBefore = attackerBeforeResources,
                    .targetBefore = attackerBeforeResources,
                    .targetAfter = attackerAfterHeal,
                });
            }
            int attackerHpBefore = result.attacker.vitals.hp;
            int attackerMpBefore = result.attacker.vitals.mp;
            int defenderMpBefore = result.defender.vitals.mp;
            writeBattleResourceUnit(result.attacker, resources.attacker);
            writeBattleResourceUnit(result.defender, resources.target);
            if (result.attacker.vitals.hp > attackerHpBefore)
            {
                recordBattleDamageEvent(result.events,
                                        BattleDamageEventType::HpRestored,
                                        input.request.attackerUnitId,
                                        input.request.attackerUnitId,
                                        result.attacker.vitals.hp - attackerHpBefore);
            }
            if (result.attacker.vitals.mp > attackerMpBefore)
            {
                recordBattleDamageEvent(result.events,
                                        BattleDamageEventType::MpRestored,
                                        input.request.attackerUnitId,
                                        input.request.attackerUnitId,
                                        result.attacker.vitals.mp - attackerMpBefore);
            }
            if (defenderMpBefore > result.defender.vitals.mp)
            {
                recordBattleDamageEvent(result.events,
                                        BattleDamageEventType::MpDrained,
                                        input.request.attackerUnitId,
                                        input.request.defenderUnitId,
                                        defenderMpBefore - result.defender.vitals.mp);
            }
        }

        if (input.request.cooldownExtendPct > 0)
        {
            auto cooldown = extendActiveCooldown(result.defenderCooldown, input.request.cooldownExtendPct);
            result.defenderCooldown = cooldown.unit;
            result.cooldownDelta = cooldown.after - cooldown.before;
            if (cooldown.increased)
            {
                recordBattleDamageEvent(result.events,
                                        BattleDamageEventType::CooldownExtended,
                                        input.request.attackerUnitId,
                                        input.request.defenderUnitId,
                                        result.cooldownDelta);
            }
        }

        if (canApplyStatusEffects && input.request.bleedStacks > 0)
        {
            assert(input.defenderStatus.id == input.request.defenderUnitId);
            assert(input.request.bleedProducer);
            auto bleed = applyBleed(result.defenderStatus,
                                    *input.request.bleedProducer,
                                    input.request.bleedStacks,
                                    input.request.bleedMaxStacks);
            result.defenderStatus = bleed.target;
            if (bleed.applied)
            {
                recordBattleStatusEvent(result.events,
                                        BattleDamageStatusType::Bleed,
                                        input.request.attackerUnitId,
                                        input.request.defenderUnitId,
                                        bleed.value,
                                        input.request.bleedMaxStacks);
            }
        }

        auto applyControlFrames = [&](BattleDamageStatusType statusType, int requestedFrames)
        {
            if (!canApplyStatusEffects || requestedFrames <= 0)
            {
                return;
            }
            assert(input.defenderStatus.id == input.request.defenderUnitId);
            BattleStatusApplyRequest request;
            request.kind = BattleStatusKind::Stun;
            request.sourceUnitId = input.request.attackerUnitId;
            request.durationFrames = requestedFrames;
            request.stack = EffectStackPolicy::Independent;
            request.targetHasShield = result.defender.shield > 0;
            request.controlLowHpImmunityPct = input.request.frozenLowHpImmunityPct;
            auto applied = BattleStatusSystem({}).apply(result.defenderStatus, request);
            result.defenderStatus = std::move(applied.target);
            if (applied.applied)
            {
                recordBattleStatusEvent(result.events,
                                        statusType,
                                        input.request.attackerUnitId,
                                        input.request.defenderUnitId,
                                        applied.value);
            }
        };
        applyControlFrames(BattleDamageStatusType::Hitstun, input.request.hitstunFrames);
        applyControlFrames(BattleDamageStatusType::Stun, input.request.stunFrames);

    }

    for (auto& event : result.events)
    {
        event.damageKind = result.damageKind;
    }
    result.attackerDelta = makeBattleUnitDelta(input.attacker, result.attacker);
    result.defenderDelta = makeBattleUnitDelta(input.defender, result.defender);
    return result;
}

BattleDamageModifierResult BattleDamageSystem::applyModifiers(const BattleDamageModifierInput& input) const
{
    BattleFixed damage = input.damage;
    int remainingDamageBasisPoints = 10'000;

    if ((input.usingSkill || input.damageKind == BattleDamageKind::Skill)
        && input.attacker.skillDamagePct != 0)
    {
        applySignedDamageDelta(
            damage,
            input.attacker.skillDamagePct,
            remainingDamageBasisPoints);
    }

    damage += BattleFixed::fromInteger(input.attacker.flatDamageIncrease);

    const bool ignoresDefense = input.ignoreDefense || input.damageKind == BattleDamageKind::Pure;
    if (!ignoresDefense)
    {
        damage -= BattleFixed::fromInteger(input.defender.flatDamageReduction);
    }
    // 固定減傷可完全抵消傷害，後續倍率不可放大負值。
    damage = std::max(BattleFixed{}, damage);
    if (input.defender.damageReductionPct > 0)
    {
        applyDamageReduction(
            damage,
            input.defender.damageReductionPct,
            remainingDamageBasisPoints);
    }

    if (input.defender.poisoned && input.attacker.poisonDamageAmpPct > 0)
    {
        damage = damage.scaledPercentSaturated(
            static_cast<std::int64_t>(100) + input.attacker.poisonDamageAmpPct);
    }
    if (input.defender.damageTakenIncreasePct != 0)
    {
        applySignedDamageDelta(
            damage,
            input.defender.damageTakenIncreasePct,
            remainingDamageBasisPoints);
    }

    BattleDamageModifierResult result;
    result.damage = damage;
    result.combinedDamageReductionBasisPoints = 10'000 - remainingDamageBasisPoints;
    if (input.defender.maxHitPctMaxHp > 0 && result.damage > BattleFixed{})
    {
        int maxHit = std::max(1, input.defenderUnit.vitals.maxHp * input.defender.maxHitPctMaxHp / 100);
        if (result.damage > BattleFixed::fromInteger(maxHit))
        {
            result.damage = BattleFixed::fromInteger(maxHit);
            result.maxHitCapped = true;
            result.maxHitPct = input.defender.maxHitPctMaxHp;
        }
    }
    return result;
}

int BattleDamageSystem::resolveMagicBaseDamage(const BattleMagicBaseDamageInput& input) const
{
    const BattleFixed attack = BattleFixed::fromInteger(input.attackerAttack)
        + BattleFixed::fromRatio(input.magicPower, 3);
    const BattleFixed combined = attack + input.defenderDefense;
    if (combined <= BattleFixed{})
    {
        return 1;
    }

    int damage = attack.multipliedBy(attack).dividedBy(combined).scaled(1, 4).toInt();
    damage += input.randomVariance;
    return std::max(1, damage);
}

BattleAttackPotencySnapshot BattleDamageSystem::snapshotAttackPotency(
    int effectiveAttack,
    int magicPower) const
{
    assert(effectiveAttack >= 0);
    assert(magicPower >= 0);
    return { effectiveAttack, magicPower };
}

int BattleDamageSystem::resolveAttackPotencyAgainstDefender(
    const BattleAttackPotencySnapshot& potency,
    BattleFixed defenderDefense,
    int randomVariance) const
{
    return resolveMagicBaseDamage({
        potency.effectiveAttack,
        potency.magicPower,
        defenderDefense,
        randomVariance,
    });
}

BattleHitShapeResult BattleDamageSystem::shapeHitDamage(const BattleHitShapeInput& input) const
{
    assert(input.baseDamage >= BattleFixed{});
    assert(input.totalFrame > 0);
    assert(input.frame >= 0);
    assert(input.strengthPct >= 0);

    BattleFixed damage = input.baseDamage;
    damage -= BattleFixed::fromInteger(input.projectileCancelDamage);
    damage = damage.scaled(input.strengthPct, 100);
    const int falloffNumerator = input.totalFrame * 10 - input.frame * 3;
    assert(falloffNumerator >= 0);
    damage = damage.scaled(falloffNumerator, input.totalFrame * 10);

    const auto facingArc = classifyBattleFacing(
        input.impactPosition - input.defenderPosition,
        input.defenderFacing);
    if (facingArc == BattleFacingArc::Side)
    {
        damage = damage.scaled(120, 100);
    }
    else if (facingArc == BattleFacingArc::Back)
    {
        damage = damage.scaled(150, 100);
    }

    damage = damage.scaled(battleOperationDamagePct(input.operationType), 100);
    BattleHitShapeResult result;
    if (input.operationType == BattleOperationType::Dash)
    {
        damage = damage.scaled(2, 3);
        result.frozenFrames = 5;
    }

    if (input.usingSkill)
    {
        const int actDiff = std::clamp(
            input.attackerActProperty - input.defenderActProperty,
            -60,
            60);
        damage = damage.scaled(400 + actDiff, 400);
    }

    result.damage = damage;
    result.knockbackStrength = input.operationType == BattleOperationType::RangedProjectile ? 0.5 : 2.0;
    result.knockbackVelocityCap = input.operationType == BattleOperationType::RangedProjectile ? 1.5 : 3.0;
    return result;
}

BattleDamageRequest BattleDamageSystem::makeScriptedHitRequest(
    const BattleScriptedHitRequestInput& input) const
{
    assert(input.defenderUnitId >= 0);
    assert(input.stunFrames >= 0);
    assert(input.bleedStacks >= 0);
    assert(input.bleedMaxStacks >= 0);
    assert(input.bleedStacks == 0 || input.bleedProducer);

    BattleDamageRequest request;
    request.attackerUnitId = input.attackerUnitId;
    request.defenderUnitId = input.defenderUnitId;
    request.acceptedHit = true;
    request.stunFrames = input.stunFrames;
    request.bleedStacks = input.bleedStacks;
    request.bleedMaxStacks = input.bleedMaxStacks;
    request.bleedProducer = input.bleedProducer;
    return request;
}

BattleDamageDefenseResult BattleDamageSystem::resolveDefense(const BattleDamageDefenseInput& input) const
{
    BattleDamageDefenseResult result;
    result.damage = input.damage;
    result.defender = input.defender;
    result.remainingDamageBasisPoints = input.remainingDamageBasisPoints;
    assert(result.remainingDamageBasisPoints >= (100 - FinalDamageReductionCapPct) * 100);
    assert(result.remainingDamageBasisPoints <= 10'000);

    if (!input.executed && input.defenderWasInvincible)
    {
        result.damage = 0;
        result.blockedByInvincible = true;
        return result;
    }

    if (!input.executed
        && result.damage > 0
        && result.defender.dualWieldBlocksRemaining > 0)
    {
        result.damage = 0;
        result.defender.dualWieldBlocksRemaining--;
        result.blockedByDualWield = true;
        return result;
    }

    if (!input.executed && result.damage > 0 && input.blockByStatusLayer)
    {
        result.damage = 0;
        result.blockedByDamageLayer = true;
        return result;
    }

    if (!input.executed && result.damage > 0 && input.singleHitCap > 0)
    {
        result.singleHitCapConsumed = true;
        result.singleHitCap = input.singleHitCap;
        if (result.damage > input.singleHitCap)
        {
            result.damage = input.singleHitCap;
            result.singleHitCapped = true;
        }
    }

    std::uint64_t previousAbsorptionSequence{};
    for (const auto& layer : input.absorptionLayers)
    {
        assert(layer.sequence > previousAbsorptionSequence);
        assert(layer.absorbedPct >= 0 && layer.absorbedPct <= 100);
        previousAbsorptionSequence = layer.sequence;

        int absorbed{};
        if (!input.executed && result.damage > 0)
        {
            const int requestedRemaining = result.remainingDamageBasisPoints
                * (100 - layer.absorbedPct) / 100;
            const int cappedRemaining = std::max(
                (100 - FinalDamageReductionCapPct) * 100,
                requestedRemaining);
            absorbed = static_cast<int>(
                static_cast<std::int64_t>(result.damage)
                * (result.remainingDamageBasisPoints - cappedRemaining)
                / result.remainingDamageBasisPoints);
            result.damage -= absorbed;
            result.remainingDamageBasisPoints = cappedRemaining;
        }
        result.absorptionReceipts.push_back({ layer.sequence, absorbed });
    }

    if (result.defender.shield > 0 && result.damage > 0)
    {
        int shieldBefore = result.defender.shield;
        int absorbed = std::min(result.defender.shield, result.damage);
        result.defender.shield -= absorbed;
        result.damage -= absorbed;
        result.shieldAbsorbed = absorbed;
        result.shieldBroken = shieldBefore > 0 && result.defender.shield == 0;
    }

    return result;
}

BattleDamageTakenResult BattleDamageSystem::applyDamageTaken(
    BattleDamageUnitState defender,
    int damage,
    bool triggersDefenseEffects,
    BattleRuntimeRandom* recoveryRandom) const
{
    BattleDamageTakenResult result;
    result.defender = defender;
    if (damage <= 0 || !result.defender.alive)
    {
        return result;
    }

    result.defender.vitals.hp -= damage;
    if (triggersDefenseEffects && result.defender.vitals.hp > 0 && result.defender.hurtInvincFrames > 0)
    {
        result.defender.invincible += result.defender.hurtInvincFrames;
        result.hurtInvincGranted = true;
        result.invincibilityGranted = result.defender.hurtInvincFrames;
    }

    if (result.defender.vitals.hp <= 0)
    {
        if (result.defender.deathPrevention && !result.defender.deathPreventionUsed)
        {
            result.defender.deathPreventionUsed = true;
            result.defender.vitals.hp = 1;
            int frames = result.defender.deathPreventionFrames > 0 ? result.defender.deathPreventionFrames : 100;
            result.defender.invincible += frames;
            result.deathPrevented = true;
            result.invincibilityGranted = frames;
        }
        else
        {
            auto& recovery = result.defender.lethalRecovery;
            if (recovery && !recovery->used)
            {
                assert(recoveryRandom);
                recovery->used = true;
                result.recoveryTested = true;
                result.recoverySucceeded = recoveryRandom->nextInt(100) < recovery->chancePercent;
                if (result.recoverySucceeded)
                {
                    result.defender.vitals.hp = std::min(recovery->survivalHp, result.defender.vitals.maxHp);
                    result.defender.invincible += recovery->invincibleFrames;
                    result.invincibilityGranted = recovery->invincibleFrames;
                    return result;
                }
            }
            result.defender.vitals.hp = 0;
            result.defender.alive = false;
            result.died = true;
        }
    }

    return result;
}

BattleCooldownIncreaseResult BattleDamageSystem::extendActiveCooldown(BattleCooldownState unit, int pct) const
{
    BattleCooldownIncreaseResult result;
    result.unit = unit;
    result.before = unit.cooldown;
    result.after = unit.cooldown;

    bool canExtend = unit.alive
        && unit.cooldown > 0
        && unit.haveAction
        && unit.operationType != BattleOperationType::None
        && unit.actType >= 0
        && pct > 0;
    if (!canExtend)
    {
        return result;
    }

    int baseCooldown = std::max(1, unit.cooldownMax);
    int extension = std::max(1, (baseCooldown * pct + 99) / 100);
    int cap = baseCooldown + extension;
    if (unit.cooldown >= cap)
    {
        return result;
    }

    result.unit.cooldown = std::min(cap, unit.cooldown + extension);
    result.unit.cooldownMax = std::max(unit.cooldownMax, result.unit.cooldown);
    result.after = result.unit.cooldown;
    result.increased = result.after > result.before;
    return result;
}

bool BattleDamageSystem::shouldExecute(const BattleExecuteInput& input) const
{
    if (input.thresholdPct <= 0 || input.maxHp <= 0 || input.pendingDamage <= 0)
    {
        return false;
    }

    int projectedHp = input.projectedHpBeforeDamage;
    if (input.appliesHpDamage)
    {
        projectedHp -= input.pendingDamage;
    }
    return projectedHp * 100 < input.maxHp * input.thresholdPct;
}

BattleOnHitResourceResult BattleDamageSystem::applyOnHitResources(const BattleOnHitResourceInput& input) const
{
    assert(input.attacker.id >= 0);
    assert(input.target.id >= 0);
    assert(input.attacker.vitals.maxHp >= 0);
    assert(input.attacker.vitals.maxMp >= 0);
    assert(input.target.vitals.maxHp >= 0);
    assert(input.target.vitals.maxMp >= 0);
    assert(input.mpOnHit >= 0);
    assert(input.hpOnHit >= 0);
    assert(input.mpDrain >= 0);
    assert(input.attacker.mpRecoveryBonusPct >= 0);

    BattleOnHitResourceResult result;
    result.attacker = input.attacker;
    result.target = input.target;
    if (!result.attacker.alive)
    {
        return result;
    }

    if (input.hpOnHit > 0)
    {
        const auto request = damageHealRequest(
            result.attacker.id,
            BattleHealKind::OnHit,
            fixedHealAmount(input.hpOnHit));
        const auto snapshot = healSnapshot(result.attacker);
        const auto heal = resolveHeal(request, snapshot, snapshot, input.healModifiers);
        result.heal = heal;
        result.attacker.vitals.hp = heal.hpAfter;
        result.hpHealed = heal.appliedAmount;
    }

    if (input.mpDrain > 0 && result.target.alive)
    {
        int drained = std::min(input.mpDrain, std::max(0, result.target.vitals.mp));
        result.target.vitals.mp -= drained;
        result.mpDrained = drained;
    }

    int mpGain = adjustedMpRestore(result.attacker.mpBlocked,
                                   result.attacker.mpRecoveryBonusPct,
                                   input.mpOnHit + result.mpDrained);
    if (mpGain > 0)
    {
        int before = result.attacker.vitals.mp;
        result.attacker.vitals.mp = std::min(result.attacker.vitals.maxMp, result.attacker.vitals.mp + mpGain);
        result.mpRestored = result.attacker.vitals.mp - before;
    }

    return result;
}

BattleStatusApplyResult BattleDamageSystem::applyBleed(
    BattleStatusUnitState target,
    const BattleStatusProducerProvenance& provenance,
    int stacks,
    int maxStacks) const
{
    assert(target.id >= 0);
    assert(stacks > 0);
    assert(maxStacks > 0);

    BattleStatusApplyRequest request;
    request.kind = BattleStatusKind::Bleed;
    request.producer = provenance.producer;
    request.producerFamily = provenance.producerFamily;
    request.behavior = makeRuntimeBleedStatusBehavior();
    request.sourceUnitId = provenance.sourceUnitId;
    request.stacks = stacks;
    request.stack = EffectStackPolicy::AddStack;
    request.targetTotalLimit = maxStacks;
    request.origin = provenance.origin;
    return BattleStatusSystem({}).apply(std::move(target), request);
}

int combineBattleBlockChancePct(int baseChancePct, int liveAreaChancePct)
{
    return std::clamp(baseChancePct + liveAreaChancePct, 0, 100);
}

}  // namespace KysChess::Battle
