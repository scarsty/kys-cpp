#include "battle/BattleComboTriggerSystem.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>

using namespace KysChess;
using namespace KysChess::Battle;

namespace
{

AppliedEffectInstance triggeredEffect(EffectType type,
                                      Trigger trigger,
                                      int value,
                                      int triggerValue = 0,
                                      int duration = 0,
                                      int maxCount = 0)
{
    AppliedEffectInstance effect;
    effect.type = type;
    effect.trigger = trigger;
    effect.value = value;
    effect.triggerValue = triggerValue;
    effect.duration = duration;
    effect.maxCount = maxCount;
    return effect;
}

}  // namespace

TEST_CASE("BattleComboTriggerSystem_FrameTriggers_HealBurstActivatesOncePerUnit", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::HealBurst, Trigger::WhileLowHP, 25, 50, 0, 1));

    BattleComboTriggerSystem system;
    auto actions = system.updateFrameTriggers(state, { 40, 100, false });

    REQUIRE(actions.size() == 1);
    CHECK(actions[0].type == BattleComboTriggerActionType::HealPercentSelf);
    CHECK(actions[0].value == 25);
    CHECK(state.effectActivationCounts[0] == 1);

    auto repeated = system.updateFrameTriggers(state, { 30, 100, false });
    CHECK(repeated.empty());
    CHECK(state.effectActivationCounts[0] == 1);
}

TEST_CASE("BattleComboTriggerSystem_FrameTriggers_BroadcastsAllyLowHpTimer", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 40, 35, 90, 2));

    BattleComboTriggerSystem system;
    auto actions = system.updateFrameTriggers(state, { 30, 100, false });

    REQUIRE(actions.size() == 1);
    CHECK(actions[0].type == BattleComboTriggerActionType::BroadcastTriggerTimer);
    CHECK(actions[0].trigger == Trigger::AllyLowHPBurst);
    CHECK(actions[0].durationFrames == 90);
    CHECK(state.effectActivationCounts[0] == 1);

    state.triggerTimers[Trigger::AllyLowHPBurst] = 10;
    auto blockedByTimer = system.updateFrameTriggers(state, { 20, 100, false });
    CHECK(blockedByTimer.empty());
    CHECK(state.effectActivationCounts[0] == 1);
}

TEST_CASE("BattleComboTriggerSystem_FrameRuntime_AdvancesTimersAndEmitsPeriodicEffects", "[battle][combo][unit]")
{
    RoleComboState state;
    state.autoUltimateAfterFrames = 3;
    state.autoUltimateTimer = 1;
    state.hpRegenPct = 12;
    state.hpRegenInterval = 5;
    state.healAuraFlat = 7;
    state.healAuraPct = 9;
    state.healAuraInterval = 5;
    state.healedATKSPDBoostPct = 15;
    state.triggerTimers[Trigger::AllyLowHPBurst] = 2;
    state.rampings.push_back({ 10, 3 });
    state.rampingStacks.push_back(2);
    state.rampingIdleTimers.push_back(0);

    auto events = BattleComboTriggerSystem().advanceFrameRuntime(
        state,
        { 10, 40, 100, true, false });

    REQUIRE(events.size() == 3);
    CHECK(events[0].type == BattleComboFrameRuntimeEventType::AutoUltimateReady);
    CHECK(events[1].type == BattleComboFrameRuntimeEventType::SelfHpRegen);
    CHECK(events[1].value == 12);
    CHECK(events[2].type == BattleComboFrameRuntimeEventType::HealAura);
    CHECK(events[2].value == 7);
    CHECK(events[2].value2 == 9);
    CHECK(events[2].durationFrames == 15);
    CHECK(state.autoUltimateTimer == 3);
    CHECK(state.triggerTimers[Trigger::AllyLowHPBurst] == 1);
    CHECK(state.rampingStacks[0] == 0);
}

TEST_CASE("BattleComboTriggerSystem_FrameRuntime_EmitsFrameTriggers", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::HealBurst, Trigger::WhileLowHP, 25, 50, 0, 1));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 40, 45, 60, 1));

    auto events = BattleComboTriggerSystem().advanceFrameRuntime(
        state,
        { 20, 40, 100, true, false });

    REQUIRE(events.size() == 2);
    CHECK(events[0].type == BattleComboFrameRuntimeEventType::HealPercentSelf);
    CHECK(events[0].value == 25);
    CHECK(events[1].type == BattleComboFrameRuntimeEventType::BroadcastTriggerTimer);
    CHECK(events[1].trigger == Trigger::AllyLowHPBurst);
    CHECK(events[1].durationFrames == 60);
    CHECK(state.effectActivationCounts[0] == 1);
    CHECK(state.effectActivationCounts[1] == 1);
}

TEST_CASE("BattleComboTriggerSystem_SkillFinishedRuntime_EmitsPostSkillInvincibility", "[battle][combo][unit]")
{
    RoleComboState state;
    state.postSkillInvincFrames = 8;

    auto events = BattleComboTriggerSystem().collectSkillFinishedRuntimeEvents(state, true);

    REQUIRE(events.size() == 1);
    CHECK(events[0].type == BattleComboFrameRuntimeEventType::PostSkillInvincibility);
    CHECK(events[0].value == 8);

    CHECK(BattleComboTriggerSystem().collectSkillFinishedRuntimeEvents(state, false).empty());
}

TEST_CASE("BattleComboTriggerSystem_TeamHeal_CollectsMatchingHookAndCountsActivations", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::OnSkillTeamHeal, Trigger::OnHit, 20, 50, 0, 1));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::OnSkillTeamHealPct, Trigger::OnUltimate, 15, 0, 0, 1));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 10));

    BattleComboTriggerSystem system;
    auto hit = system.collectTeamHeal(state, Trigger::OnHit, []() { return 10.0; });

    CHECK(hit.flatHeal == 20);
    CHECK(hit.pctHeal == 0);
    REQUIRE(hit.activatedEffectIndices.size() == 1);
    CHECK(hit.activatedEffectIndices[0] == 0);
    CHECK(state.effectActivationCounts[0] == 1);

    auto repeatedHit = system.collectTeamHeal(state, Trigger::OnHit, []() { return 10.0; });
    CHECK(repeatedHit.flatHeal == 0);
    CHECK(repeatedHit.pctHeal == 0);

    auto ultimate = system.collectTeamHeal(state, Trigger::OnUltimate, []() { return 99.0; });
    CHECK(ultimate.flatHeal == 0);
    CHECK(ultimate.pctHeal == 15);
    REQUIRE(ultimate.activatedEffectIndices.size() == 1);
    CHECK(ultimate.activatedEffectIndices[0] == 1);
}

TEST_CASE("BattleComboTriggerSystem_TeamHeal_ChanceGateDoesNotConsumeMaxCount", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::OnSkillTeamHeal, Trigger::OnHit, 20, 50, 0, 1));

    BattleComboTriggerSystem system;
    auto missed = system.collectTeamHeal(state, Trigger::OnHit, []() { return 60.0; });
    CHECK(missed.flatHeal == 0);
    CHECK(state.effectActivationCounts[0] == 0);

    auto hit = system.collectTeamHeal(state, Trigger::OnHit, []() { return 49.0; });
    CHECK(hit.flatHeal == 20);
    CHECK(state.effectActivationCounts[0] == 1);
}

TEST_CASE("BattleComboTriggerSystem_TriggeredTeamHeal_CollectsFlatAndPercentFromBattleHook", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::OnSkillTeamHeal, Trigger::OnHit, 12, 100));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::OnSkillTeamHealPct, Trigger::OnHit, 8, 100));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 4, 100));

    auto result = BattleComboTriggerSystem().collectTriggeredTeamHeal(
        state,
        { BattleComboTriggerHook::DamageDealt, 1, 2 },
        []() { return 0.0; });

    CHECK(result.flatHeal == 12);
    CHECK(result.pctHeal == 8);
    REQUIRE(result.activatedEffectIndices.size() == 2);
    CHECK(result.activatedEffectIndices[0] == 0);
    CHECK(result.activatedEffectIndices[1] == 1);
    CHECK(state.effectActivationCounts[0] == 1);
    CHECK(state.effectActivationCounts[1] == 1);
    CHECK(state.effectActivationCounts.count(2) == 0);
}

TEST_CASE("BattleComboTriggerSystem_PendingSkillTeamHeal_ConsumesPendingBaseHealAndUltimateTrigger", "[battle][combo][unit]")
{
    RoleComboState state;
    state.onSkillTeamHealPending = true;
    state.onSkillTeamHeal = 5;
    state.onSkillTeamHealPct = 7;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::OnSkillTeamHeal, Trigger::OnUltimate, 11, 100));

    auto result = BattleComboTriggerSystem().collectPendingSkillTeamHeal(
        state,
        { BattleComboTriggerHook::AfterSkillCast, 3, -1 },
        []() { return 0.0; });

    CHECK(result.flatHeal == 16);
    CHECK(result.pctHeal == 7);
    CHECK_FALSE(state.onSkillTeamHealPending);
    CHECK(state.effectActivationCounts[0] == 1);
}

TEST_CASE("BattleComboTriggerSystem_OnHitComboCommands_FilterNearbyTrackingWhenSuppressed", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::MPBlock, Trigger::OnHit, 4, 100));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::NearbyTrackingProjectiles, Trigger::OnHit, 80, 100));
    state.triggeredEffects.back().value2 = 30;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::FlatShield, Trigger::OnHit, 12, 100));

    auto suppressed = BattleComboTriggerSystem().collectOnHitComboCommands(
        state,
        { BattleComboTriggerHook::DamageDealt, 1, 2 },
        true,
        []() { return 0.0; });

    REQUIRE(suppressed.size() == 2);
    CHECK(suppressed[0].type == BattleOnHitComboCommandType::MpBlock);
    CHECK(suppressed[0].value == 4);
    CHECK(suppressed[1].type == BattleOnHitComboCommandType::FlatShield);
    CHECK(suppressed[1].value == 12);
    CHECK(state.effectActivationCounts[0] == 1);
    CHECK(state.effectActivationCounts[1] == 0);
    CHECK(state.effectActivationCounts[2] == 1);

    auto allowed = BattleComboTriggerSystem().collectOnHitComboCommands(
        state,
        { BattleComboTriggerHook::DamageDealt, 1, 2 },
        false,
        []() { return 0.0; });

    REQUIRE(allowed.size() == 3);
    CHECK(allowed[1].type == BattleOnHitComboCommandType::NearbyTrackingProjectiles);
    CHECK(allowed[1].value == 80);
    CHECK(allowed[1].value2 == 30);
    CHECK(state.effectActivationCounts[1] == 1);
}

TEST_CASE("BattleComboTriggerSystem_ChanceEffects_FilterTypesAndRecordOnlyActivatedEffects", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::ShieldExplosion, Trigger::OnShieldBreak, 30, 75, 0, 1));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnShieldBreak, 20, 100));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::AutoUltimate, Trigger::OnShieldBreak, 1, 50));

    BattleComboTriggerSystem system;
    auto missed = system.collectChanceEffects(
        state,
        Trigger::OnShieldBreak,
        { EffectType::ShieldExplosion, EffectType::AutoUltimate },
        []() { return 80.0; });
    CHECK(missed.empty());
    CHECK(state.effectActivationCounts[0] == 0);

    auto activated = system.collectChanceEffects(
        state,
        Trigger::OnShieldBreak,
        { EffectType::ShieldExplosion, EffectType::AutoUltimate },
        []() { return 10.0; });
    REQUIRE(activated.size() == 2);
    CHECK(activated[0].effect.type == EffectType::ShieldExplosion);
    CHECK(activated[1].effect.type == EffectType::AutoUltimate);
    CHECK(state.effectActivationCounts[0] == 1);
    CHECK(state.effectActivationCounts[1] == 0);
    CHECK(state.effectActivationCounts[2] == 1);

    auto capped = system.collectChanceEffects(
        state,
        Trigger::OnShieldBreak,
        { EffectType::ShieldExplosion },
        []() { return 10.0; });
    CHECK(capped.empty());
}

TEST_CASE("BattleComboTriggerSystem_ChanceEffects_CallerCanRecordAfterSceneActionSucceeds", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::MPRestore, Trigger::OnShieldBreak, 25, 100, 0, 1));

    BattleComboTriggerSystem system;
    auto candidates = system.collectChanceEffects(
        state,
        Trigger::OnShieldBreak,
        { EffectType::MPRestore },
        []() { return 0.0; },
        BattleComboActivationRecording::CallerRecords);

    REQUIRE(candidates.size() == 1);
    CHECK(state.effectActivationCounts[0] == 0);

    system.recordActivation(state, static_cast<size_t>(candidates[0].effectIndex));
    CHECK(state.effectActivationCounts[0] == 1);
}

TEST_CASE("BattleComboTriggerSystem_Hooks_CoverBattleRuntimeEvents", "[battle][combo][unit]")
{
    std::vector<BattleComboTriggerHook> hooks = {
        BattleComboTriggerHook::FrameTick,
        BattleComboTriggerHook::BeforeCast,
        BattleComboTriggerHook::AfterSkillCast,
        BattleComboTriggerHook::AttackLaunched,
        BattleComboTriggerHook::ProjectileHitEnemy,
        BattleComboTriggerHook::ProjectileHitAllyOrSource,
        BattleComboTriggerHook::DamageTaken,
        BattleComboTriggerHook::DamageDealt,
        BattleComboTriggerHook::ShieldBreak,
        BattleComboTriggerHook::UnitDeath,
        BattleComboTriggerHook::AllyDeath,
        BattleComboTriggerHook::BattleStart,
        BattleComboTriggerHook::BattleEnd,
    };

    CHECK(hooks.size() == 13);
}

TEST_CASE("BattleComboTriggerSystem_TriggerEvents_EmitInEffectOrder", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 12, 100));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::MPBlock, Trigger::OnHit, 30, 50));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::DmgReductionPct, Trigger::OnBeingHit, 25, 100));

    std::vector<double> rolls = { 0.0, 49.0 };
    size_t nextRoll = 0;
    auto events = BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, 7, 9 },
        { EffectType::Stun, EffectType::MPBlock },
        [&]() { return rolls[nextRoll++]; });

    REQUIRE(events.size() == 2);
    CHECK(events[0].hook == BattleComboTriggerHook::ProjectileHitEnemy);
    CHECK(events[0].sourceUnitId == 7);
    CHECK(events[0].targetUnitId == 9);
    CHECK(events[0].effectIndex == 0);
    CHECK(events[0].effect.type == EffectType::Stun);
    CHECK(events[1].effectIndex == 1);
    CHECK(events[1].effect.type == EffectType::MPBlock);
    CHECK(nextRoll == 2);
}

TEST_CASE("BattleComboTriggerSystem_TriggerEvents_SameSeedProducesSameEventStream", "[battle][combo][unit]")
{
    auto makeState = []() {
        RoleComboState state;
        state.triggeredEffects.push_back(
            triggeredEffect(EffectType::Stun, Trigger::OnHit, 12, 100));
        state.triggeredEffects.push_back(
            triggeredEffect(EffectType::MPBlock, Trigger::OnHit, 30, 55));
        state.triggeredEffects.push_back(
            triggeredEffect(EffectType::Execute, Trigger::OnHit, 15, 20));
        return state;
    };
    auto collectWithSeed = [&](unsigned int seed, RoleComboState& state) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> roll(0.0, 100.0);
        return BattleComboTriggerSystem().collectTriggerEvents(
            state,
            { BattleComboTriggerHook::ProjectileHitEnemy, 4, 8 },
            { EffectType::Stun, EffectType::MPBlock, EffectType::Execute },
            [&]() { return roll(rng); });
    };

    auto firstState = makeState();
    auto secondState = makeState();
    auto first = collectWithSeed(12345, firstState);
    auto second = collectWithSeed(12345, secondState);

    REQUIRE(first.size() == second.size());
    REQUIRE_FALSE(first.empty());
    for (size_t i = 0; i < first.size(); ++i)
    {
        CHECK(first[i].hook == second[i].hook);
        CHECK(first[i].sourceUnitId == second[i].sourceUnitId);
        CHECK(first[i].targetUnitId == second[i].targetUnitId);
        CHECK(first[i].effectIndex == second[i].effectIndex);
        CHECK(first[i].effect.type == second[i].effect.type);
        CHECK(first[i].effect.value == second[i].effect.value);
    }
    CHECK(firstState.effectActivationCounts == secondState.effectActivationCounts);
}

TEST_CASE("BattleComboTriggerSystem_TriggerEvents_MaxCountConsumesOnlyOnActivation", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 12, 50, 0, 1));

    BattleComboTriggerSystem system;
    auto missed = system.collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, 1, 2 },
        { EffectType::Stun },
        []() { return 60.0; });
    CHECK(missed.empty());
    CHECK(state.effectActivationCounts[0] == 0);

    auto activated = system.collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, 1, 2 },
        { EffectType::Stun },
        []() { return 40.0; });
    REQUIRE(activated.size() == 1);
    CHECK(state.effectActivationCounts[0] == 1);

    auto capped = system.collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, 1, 2 },
        { EffectType::Stun },
        []() { return 0.0; });
    CHECK(capped.empty());
    CHECK(state.effectActivationCounts[0] == 1);
}

TEST_CASE("BattleComboTriggerSystem_QueryMatchingTriggerEffects_DoesNotConsumeActivationCounts", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::ProjectileBounce, Trigger::OnHit, 2, 40, 0, 1));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Execute, Trigger::OnHit, 15, 25));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnBeingHit, 5, 100));

    auto onHit = BattleComboTriggerSystem().matchingTriggerEffects(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, 1, 2 },
        { EffectType::ProjectileBounce, EffectType::Execute });

    REQUIRE(onHit.size() == 2);
    CHECK(onHit[0].effectIndex == 0);
    CHECK(onHit[0].effect.type == EffectType::ProjectileBounce);
    CHECK(onHit[1].effectIndex == 1);
    CHECK(onHit[1].effect.type == EffectType::Execute);
    CHECK(state.effectActivationCounts.empty());

    auto beingHit = BattleComboTriggerSystem().matchingTriggerEffects(
        state,
        { BattleComboTriggerHook::DamageTaken, 2, 1 },
        { EffectType::Stun });
    REQUIRE(beingHit.size() == 1);
    CHECK(beingHit[0].effectIndex == 2);
    CHECK(beingHit[0].effect.type == EffectType::Stun);
    CHECK(state.effectActivationCounts.empty());
}

TEST_CASE("BattleComboTriggerSystem_TriggerEvents_MapsShieldBreakHook", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::ShieldExplosion, Trigger::OnShieldBreak, 30, 100));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 5, 100));

    auto events = BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ShieldBreak, 2, 2 },
        { EffectType::ShieldExplosion },
        []() { return 0.0; },
        BattleComboActivationRecording::CallerRecords);

    REQUIRE(events.size() == 1);
    CHECK(events[0].hook == BattleComboTriggerHook::ShieldBreak);
    CHECK(events[0].sourceUnitId == 2);
    CHECK(events[0].targetUnitId == 2);
    CHECK(events[0].effect.type == EffectType::ShieldExplosion);
    CHECK(state.effectActivationCounts.empty());
}

TEST_CASE("BattleComboTriggerSystem_ShieldBreakCommands_MapEffectsWithoutRecording", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::ShieldExplosion, Trigger::OnShieldBreak, 30, 100));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::AutoUltimate, Trigger::OnShieldBreak, 1, 100));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::TempFlatATK, Trigger::OnShieldBreak, 14, 100, 45));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::MPRestore, Trigger::OnShieldBreak, 25, 100));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 5, 100));

    auto commands = BattleComboTriggerSystem().collectShieldBreakCommands(
        state,
        { BattleComboTriggerHook::ShieldBreak, 2, 2 },
        []() { return 0.0; });

    REQUIRE(commands.size() == 4);
    CHECK(commands[0].type == BattleShieldBreakCommandType::ShieldExplosion);
    CHECK(commands[0].value == 30);
    CHECK(commands[0].effectIndex == 0);
    CHECK(commands[1].type == BattleShieldBreakCommandType::AutoUltimate);
    CHECK(commands[1].effectIndex == 1);
    CHECK(commands[2].type == BattleShieldBreakCommandType::TempFlatAttack);
    CHECK(commands[2].value == 14);
    CHECK(commands[2].durationFrames == 45);
    CHECK(commands[2].effectIndex == 2);
    CHECK(commands[3].type == BattleShieldBreakCommandType::MpRestore);
    CHECK(commands[3].value == 25);
    CHECK(commands[3].effectIndex == 3);
    CHECK(state.effectActivationCounts.empty());
}

TEST_CASE("BattleComboTriggerSystem_FrameTriggerEffects_FilterActiveConditionsWithoutConsumingCounts", "[battle][combo][unit]")
{
    RoleComboState state;
    state.lastAliveFlag = true;
    state.triggerTimers[Trigger::AllyLowHPBurst] = 3;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::PctATK, Trigger::WhileLowHP, 30, 50));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::DmgReductionPct, Trigger::LastAlive, 20));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 10));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 5, 100));

    auto active = BattleComboTriggerSystem().activeFrameTriggerEffects(
        state,
        { 40, 100, state.lastAliveFlag },
        { EffectType::PctATK, EffectType::DmgReductionPct });

    REQUIRE(active.size() == 3);
    CHECK(active[0].effectIndex == 0);
    CHECK(active[0].effect.type == EffectType::PctATK);
    CHECK(active[1].effectIndex == 1);
    CHECK(active[1].effect.type == EffectType::DmgReductionPct);
    CHECK(active[2].effectIndex == 2);
    CHECK(active[2].effect.type == EffectType::PctATK);
    CHECK(state.effectActivationCounts.empty());

    auto inactive = BattleComboTriggerSystem().activeFrameTriggerEffects(
        state,
        { 80, 100, false },
        { EffectType::PctATK, EffectType::DmgReductionPct });
    REQUIRE(inactive.size() == 1);
    CHECK(inactive[0].effectIndex == 2);
}

TEST_CASE("BattleComboTriggerSystem_DodgeResolutionAddsAdaptationStacksAndConsumesExplicitRoll", "[battle][combo][unit]")
{
    RoleComboState state;
    state.dodgeChancePct = 10;
    state.dodgeAdaptations.push_back({ 5, 4 });
    state.dodgeAdaptations.push_back({ 20, 3 });
    state.dodgeAdaptationStacks.resize(2);
    state.dodgeAdaptationStacks[0][7] = 3;
    state.dodgeAdaptationStacks[1][7] = 2;

    auto dodged = BattleComboTriggerSystem().resolveDodge(state, 7, 64.9);
    CHECK(dodged.chancePct == 65);
    CHECK(dodged.dodged);

    auto hit = BattleComboTriggerSystem().resolveDodge(state, 7, 65.0);
    CHECK(hit.chancePct == 65);
    CHECK_FALSE(hit.dodged);
}

TEST_CASE("BattleComboTriggerSystem_DodgeResolutionClampsChance", "[battle][combo][unit]")
{
    RoleComboState state;
    state.dodgeChancePct = 95;
    state.dodgeAdaptations.push_back({ 20, 10 });
    state.dodgeAdaptationStacks.resize(1);
    state.dodgeAdaptationStacks[0][3] = 2;

    auto result = BattleComboTriggerSystem().resolveDodge(state, 3, 99.5);

    CHECK(result.chancePct == 100);
    CHECK(result.dodged);
}

TEST_CASE("BattleComboTriggerSystem_AttackerHitDamage_AppliesCritNthAndRampingState", "[battle][combo][unit]")
{
    RoleComboState state;
    state.critChancePct = 50;
    state.critMultiplier = 200;
    state.everyNthDoubles.push_back(2);
    state.everyNthCounters[2] = 1;
    state.rampings.push_back({ 25, 3 });
    state.rampingStacks.push_back(1);
    state.rampingIdleTimers.push_back(0);
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::PctATK, Trigger::Always, 50));

    auto result = BattleComboTriggerSystem().shapeAttackerHitDamage(
        state,
        { 100.0, 80, 100, false },
        []() { return 10.0; });

    CHECK(result.damage == Catch::Approx(900.0));
    REQUIRE(result.events.size() == 2);
    CHECK(result.events[0].type == BattleAttackerHitDamageEventType::Crit);
    CHECK(result.events[0].value == 200);
    CHECK(result.events[1].type == BattleAttackerHitDamageEventType::RampingStack);
    CHECK(result.events[1].value == 25);
    CHECK(result.events[1].value2 == 2);
    CHECK(state.everyNthCounters[2] == 0);
    CHECK(state.rampingStacks[0] == 2);
    CHECK(state.rampingIdleTimers[0] == 90);
}

TEST_CASE("BattleComboTriggerSystem_DefenderHitDamage_AppliesReductionAndAdaptationState", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::DmgReductionPct, Trigger::Always, 20));
    state.adaptations.push_back({ 10, 3 });
    state.adaptationStacks.resize(1);
    state.adaptationStacks[0][7] = 1;
    state.dodgeAdaptations.push_back({ 5, 4 });
    state.dodgeAdaptationStacks.resize(1);

    auto result = BattleComboTriggerSystem().shapeDefenderHitDamage(
        state,
        { 100.0, 40, 100, false, 7 });

    CHECK(result.damage == Catch::Approx(64.0));
    REQUIRE(result.events.size() == 2);
    CHECK(result.events[0].type == BattleDefenderHitDamageEventType::DamageAdaptationStack);
    CHECK(result.events[0].value == 10);
    CHECK(result.events[0].value2 == 2);
    CHECK(result.events[1].type == BattleDefenderHitDamageEventType::DodgeAdaptationStack);
    CHECK(result.events[1].value == 5);
    CHECK(result.events[1].value2 == 1);
    CHECK(state.adaptationStacks[0][7] == 2);
    CHECK(state.dodgeAdaptationStacks[0][7] == 1);
}

TEST_CASE("BattleComboTriggerSystem_ExecuteCombo_RecordsOnlyTriggeredExecute", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Execute, Trigger::OnHit, 20, 50, 0, 1));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Execute, Trigger::OnHit, 10, 100, 0, 1));

    auto result = BattleComboTriggerSystem().resolveExecuteCombo(
        state,
        { 4, 8, 50, 200, 15.0, true },
        []() { return 25.0; });

    CHECK(result.executed);
    CHECK(result.thresholdPct == 20);
    CHECK(result.effectIndex == 0);
    CHECK(state.effectActivationCounts[0] == 1);
    CHECK(state.effectActivationCounts.count(1) == 0);
}

TEST_CASE("BattleComboTriggerSystem_DefenderReactions_ResolveReflectAndBlocksInLegacyOrder", "[battle][combo][unit]")
{
    RoleComboState state;
    state.projectileReflectPct = 40;
    state.counterUltimateBlockChancePct = 60;
    state.blockChancePct = 70;

    CHECK(BattleComboTriggerSystem().resolveProjectileReflect(state, true, 39.0));
    CHECK_FALSE(BattleComboTriggerSystem().resolveProjectileReflect(state, true, 40.0));
    CHECK_FALSE(BattleComboTriggerSystem().resolveProjectileReflect(state, false, 0.0));

    std::vector<double> rolls = { 50.0, 69.0 };
    size_t nextRoll = 0;
    auto blocks = BattleComboTriggerSystem().collectDefenderBlockCommands(
        state,
        { false, false },
        [&]() { return rolls[nextRoll++]; });

    REQUIRE(blocks.size() == 2);
    CHECK(blocks[0] == BattleDefenderBlockCommand::CounterUltimateBlock);
    CHECK(blocks[1] == BattleDefenderBlockCommand::Block);
    CHECK(nextRoll == 2);

    CHECK(BattleComboTriggerSystem().collectDefenderBlockCommands(
        state,
        { true, false },
        []() { return 0.0; }).empty());
}

TEST_CASE("BattleComboTriggerSystem_StunCommands_CollectWithoutRecordingActivation", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 12, 100, 0, 1));
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::MPBlock, Trigger::OnHit, 20, 100));

    auto commands = BattleComboTriggerSystem().collectStunCommands(
        state,
        { BattleComboTriggerHook::DamageDealt, 4, 8 },
        []() { return 0.0; });

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].frames == 12);
    CHECK(commands[0].effectIndex == 0);
    CHECK(state.effectActivationCounts.empty());
}

TEST_CASE("BattleComboTriggerSystem_ProjectileBouncePrime_AggregatesBounceEffects", "[battle][combo][unit]")
{
    RoleComboState state;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::ProjectileBounce, Trigger::OnHit, 2, 40));
    state.triggeredEffects.back().value2 = 90;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::ProjectileBounce, Trigger::OnHit, 1, 70));
    state.triggeredEffects.back().value2 = 120;
    state.triggeredEffects.push_back(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 5, 100));

    auto prime = BattleComboTriggerSystem().collectProjectileBouncePrime(
        state,
        { 4, 25, 80 });

    CHECK(prime.count == 3);
    CHECK(prime.chancePct == 100);
    CHECK(prime.rollPct == 25);
    CHECK(prime.range == 120);
    CHECK(state.effectActivationCounts.empty());
}
