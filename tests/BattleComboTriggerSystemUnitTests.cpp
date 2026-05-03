#include "battle/BattleComboTriggerSystem.h"

#include <catch2/catch_test_macros.hpp>

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
