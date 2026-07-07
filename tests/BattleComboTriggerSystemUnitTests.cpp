#include "battle/BattleCore.h"
#include "battle/BattleComboTriggerSystem.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <random>
#include <stdexcept>

using namespace KysChess;
using namespace KysChess::Battle;

namespace
{

ComboEffectSnapshot triggeredEffect(EffectType type,
                                      Trigger trigger,
                                      int value,
                                      int triggerValue = 0,
                                      int duration = 0,
                                      int maxCount = 0,
                                      int sourceComboId = -1)
{
    ComboEffectSnapshot effect;
    effect.type = type;
    effect.trigger = trigger;
    effect.value = value;
    effect.triggerValue = triggerValue;
    effect.duration = duration;
    effect.maxCount = maxCount;
    effect.sourceComboId = sourceComboId;
    return effect;
}

struct ChanceExpectation
{
    int chancePct = 0;
    bool shouldPass = true;
};

BattleRuntimeRandom fixedBattleRandom()
{
    return BattleRuntimeRandom(1u);
}

BattleRuntimeRandom randomForChanceSequence(std::initializer_list<ChanceExpectation> expectations)
{
    for (unsigned int seed = 1; seed < 200000; ++seed)
    {
        BattleRuntimeRandom candidate(seed);
        bool matches = true;
        for (const auto& expectation : expectations)
        {
            const bool actual = expectation.chancePct <= 0
                ? false
                : expectation.chancePct >= 100
                    ? true
                    : candidate.chance(expectation.chancePct);
            if (actual != expectation.shouldPass)
            {
                matches = false;
                break;
            }
        }
        if (matches)
        {
            return BattleRuntimeRandom(seed);
        }
    }

    throw std::runtime_error("Could not find battle random seed for requested chance sequence");
}

}  // namespace

TEST_CASE("BattleComboTriggerSystem_FrameTriggers_HealBurstActivatesOncePerUnit", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::HealBurst, Trigger::WhileLowHP, 25, 50, 0, 1));

    BattleComboTriggerSystem system;
    auto actions = system.updateFrameTriggers(state, { 40, 100, false });

    REQUIRE(actions.size() == 1);
    CHECK(actions[0].type == BattleComboTriggerActionType::HealPercentSelf);
    CHECK(actions[0].value == 25);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);

    auto repeated = system.updateFrameTriggers(state, { 30, 100, false });
    CHECK(repeated.empty());
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
}

TEST_CASE("BattleComboTriggerSystem_FrameTriggers_BroadcastsAllyLowHpTimer", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 40, 35, 90, 2));

    BattleComboTriggerSystem system;
    auto actions = system.updateFrameTriggers(state, { 30, 100, false });

    REQUIRE(actions.size() == 1);
    CHECK(actions[0].type == BattleComboTriggerActionType::BroadcastTriggerTimer);
    CHECK(actions[0].trigger == Trigger::AllyLowHPBurst);
    CHECK(actions[0].timerKey.trigger == Trigger::AllyLowHPBurst);
    CHECK(actions[0].timerKey.sourceComboId == -1);
    CHECK(actions[0].durationFrames == 90);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
    CHECK_FALSE((state.triggerTimerFrames({ Trigger::AllyLowHPBurst, -1 }) > 0));

    state.extendTriggerTimer({ Trigger::AllyLowHPBurst, -1 }, 10);
    auto blockedByTimer = system.updateFrameTriggers(state, { 20, 100, false });
    CHECK(blockedByTimer.empty());
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
}

TEST_CASE("BattleComboTriggerSystem_FrameTriggers_UsesSourceScopedAllyLowHpTimer", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 40, 35, 90, 2), 17);

    BattleComboTriggerSystem system;
    auto actions = system.updateFrameTriggers(state, { 30, 100, false });

    REQUIRE(actions.size() == 1);
    CHECK(actions[0].type == BattleComboTriggerActionType::BroadcastTriggerTimer);
    CHECK(actions[0].timerKey.trigger == Trigger::AllyLowHPBurst);
    CHECK(actions[0].timerKey.sourceComboId == 17);
    CHECK_FALSE((state.triggerTimerFrames({ Trigger::AllyLowHPBurst, 17 }) > 0));

    RoleComboState stateWithDifferentSourceTimer;
    stateWithDifferentSourceTimer.applyConfiguredEffect(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 40, 35, 90, 2), 17);
    stateWithDifferentSourceTimer.extendTriggerTimer({ Trigger::AllyLowHPBurst, 18 }, 10);
    auto differentComboTimer = system.updateFrameTriggers(stateWithDifferentSourceTimer, { 20, 100, false });
    REQUIRE(differentComboTimer.size() == 1);
    CHECK(differentComboTimer[0].timerKey.sourceComboId == 17);

    RoleComboState stateWithMatchingSourceTimer;
    stateWithMatchingSourceTimer.applyConfiguredEffect(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 40, 35, 90, 2), 17);
    stateWithMatchingSourceTimer.extendTriggerTimer({ Trigger::AllyLowHPBurst, 17 }, 10);
    auto matchingComboTimer = system.updateFrameTriggers(stateWithMatchingSourceTimer, { 20, 100, false });
    CHECK(matchingComboTimer.empty());
}

TEST_CASE("BattleComboTriggerSystem_FrameTriggers_DoesNotSuppressSameSourceLowHpEffects", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 40, 35, 90, 1), 17);
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::PctDEF, Trigger::AllyLowHPBurst, 30, 35, 120, 1), 17);

    auto actions = BattleComboTriggerSystem().updateFrameTriggers(state, { 30, 100, false });

    REQUIRE(actions.size() == 2);
    CHECK(actions[0].timerKey.sourceComboId == 17);
    CHECK(actions[1].timerKey.sourceComboId == 17);
    CHECK(actions[1].durationFrames == 120);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 1 }) == 1);
    CHECK_FALSE((state.triggerTimerFrames({ Trigger::AllyLowHPBurst, 17 }) > 0));
}

TEST_CASE("BattleComboTriggerSystem_FrameTriggerEffects_RequireMatchingTimerSource", "[battle][combo][unit]")
{
    RoleComboState state;
    state.extendTriggerTimer({ Trigger::AllyLowHPBurst, 17 }, 3);
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 10, 0, 0, 0), 17);
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::DmgReductionPct, Trigger::AllyLowHPBurst, 20, 0, 0, 0), 18);

    auto active = BattleComboTriggerSystem().activeFrameTriggerEffects(
        state,
        { 90, 100, false },
        { EffectType::PctATK, EffectType::DmgReductionPct });

    REQUIRE(active.size() == 1);
    CHECK(active[0].effect.type == EffectType::PctATK);
    CHECK(active[0].effect.sourceComboId == 17);
}

TEST_CASE("BattleComboTriggerSystem_FrameRuntime_AdvancesTimersAndEmitsPeriodicEffects", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect({ EffectType::AutoUltimateAfterFrames, 1 });
    state.applyConfiguredEffect({ EffectType::HPRegenPct, 12, 5 });
    state.applyConfiguredEffect({ EffectType::HealAuraFlat, 7, 5 });
    state.applyConfiguredEffect({ EffectType::HealAuraPct, 9, 5 });
    state.applyConfiguredEffect({ EffectType::HealedATKSPDBoost, 15 });
    state.extendTriggerTimer({ Trigger::AllyLowHPBurst, -1 }, 2);

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
    CHECK(state.effectFrameTimerFrames(RoleComboEffectId{ 0 }) == 1);
    CHECK(state.triggerTimerFrames({ Trigger::AllyLowHPBurst, -1 }) == 1);
}

TEST_CASE("BattleComboTriggerSystem_FrameRuntime_EmitsFrameTriggers", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::HealBurst, Trigger::WhileLowHP, 25, 50, 0, 1));
    state.applyConfiguredEffect(
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
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 1 }) == 1);
}

TEST_CASE("BattleComboTriggerSystem_TeamHeal_CollectsMatchingHookAndCountsActivations", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::OnSkillTeamHeal, Trigger::OnHit, 20, 50, 0, 1));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::OnSkillTeamHealPct, Trigger::OnUltimate, 15, 0, 0, 1));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 10));

    BattleComboTriggerSystem system;
    auto hitRandom = randomForChanceSequence({ { 50, true } });
    auto hit = system.collectTeamHeal(state, Trigger::OnHit, hitRandom);

    CHECK(hit.flatHeal == 20);
    CHECK(hit.pctHeal == 0);
    REQUIRE(hit.activatedEffectIds.size() == 1);
    CHECK(hit.activatedEffectIds[0] == 0);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);

    auto repeatedRandom = fixedBattleRandom();
    auto repeatedHit = system.collectTeamHeal(state, Trigger::OnHit, repeatedRandom);
    CHECK(repeatedHit.flatHeal == 0);
    CHECK(repeatedHit.pctHeal == 0);

    auto ultimateRandom = fixedBattleRandom();
    auto ultimate = system.collectTeamHeal(state, Trigger::OnUltimate, ultimateRandom);
    CHECK(ultimate.flatHeal == 0);
    CHECK(ultimate.pctHeal == 15);
    REQUIRE(ultimate.activatedEffectIds.size() == 1);
    CHECK(ultimate.activatedEffectIds[0] == 1);
}

TEST_CASE("BattleComboTriggerSystem_TeamHeal_ChanceGateDoesNotConsumeMaxCount", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::OnSkillTeamHeal, Trigger::OnHit, 20, 50, 0, 1));

    BattleComboTriggerSystem system;
    auto missedRandom = randomForChanceSequence({ { 50, false } });
    auto missed = system.collectTeamHeal(state, Trigger::OnHit, missedRandom);
    CHECK(missed.flatHeal == 0);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 0);

    auto hitRandom = randomForChanceSequence({ { 50, true } });
    auto hit = system.collectTeamHeal(state, Trigger::OnHit, hitRandom);
    CHECK(hit.flatHeal == 20);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
}

TEST_CASE("BattleComboTriggerSystem_TriggeredTeamHeal_CollectsFlatAndPercentFromTriggerHook", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::OnSkillTeamHeal, Trigger::OnHit, 12, 100));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::OnSkillTeamHealPct, Trigger::OnHit, 8, 100));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 4, 100));

    auto random = fixedBattleRandom();
    auto result = BattleComboTriggerSystem().collectTriggeredTeamHeal(
        state,
        { BattleComboTriggerHook::DamageDealt, 1, 2 },
        random);

    CHECK(result.flatHeal == 12);
    CHECK(result.pctHeal == 8);
    REQUIRE(result.activatedEffectIds.size() == 2);
    CHECK(result.activatedEffectIds[0] == 0);
    CHECK(result.activatedEffectIds[1] == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 1 }) == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 2 }) == 0);
}

TEST_CASE("BattleComboTriggerSystem_PendingSkillTeamHeal_ConsumesPendingBaseHealAndUltimateTrigger", "[battle][combo][unit]")
{
    RoleComboState state;
    state.setTypePending(EffectType::OnSkillTeamHeal, true);
    state.applyConfiguredEffect({ EffectType::OnSkillTeamHeal, 5 });
    state.applyConfiguredEffect({ EffectType::OnSkillTeamHealPct, 7 });
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::OnSkillTeamHeal, Trigger::OnUltimate, 11, 100));

    auto random = fixedBattleRandom();
    auto result = BattleComboTriggerSystem().collectPendingSkillTeamHeal(
        state,
        { BattleComboTriggerHook::AfterSkillCast, 3, -1, true, true },
        random);

    CHECK(result.flatHeal == 16);
    CHECK(result.pctHeal == 7);
    CHECK_FALSE(state.typePending(EffectType::OnSkillTeamHeal));
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 2 }) == 1);
}

TEST_CASE("BattleComboTriggerSystem_TriggerEvents_FilterNearbyTrackingWhenSuppressed", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::MPBlock, Trigger::OnHit, 4, 100));
    auto tracking = triggeredEffect(EffectType::NearbyTrackingProjectiles, Trigger::OnHit, 80, 100);
    tracking.value2 = 30;
    state.applyConfiguredEffect(tracking);
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::FlatShield, Trigger::OnCast, 12, 100));

    auto suppressedRandom = fixedBattleRandom();
    auto suppressed = BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::DamageDealt, 1, 2 },
        { EffectType::MPBlock },
        suppressedRandom);

    REQUIRE(suppressed.size() == 1);
    CHECK(suppressed[0].effect.type == EffectType::MPBlock);
    CHECK(suppressed[0].effect.value == 4);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 1 }) == 0);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 2 }) == 0);

    auto allowedRandom = fixedBattleRandom();
    auto allowed = BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::DamageDealt, 1, 2 },
        { EffectType::MPBlock, EffectType::NearbyTrackingProjectiles },
        allowedRandom);

    REQUIRE(allowed.size() == 2);
    CHECK(allowed[1].effect.type == EffectType::NearbyTrackingProjectiles);
    CHECK(allowed[1].effect.value == 80);
    CHECK(allowed[1].effect.value2 == 30);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 1 }) == 1);
}

TEST_CASE("BattleComboTriggerSystem_ChanceEffects_FilterTypesAndRecordOnlyActivatedEffects", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::ShieldExplosion, Trigger::OnShieldBreak, 30, 75, 0, 1));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnShieldBreak, 20, 100));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::AutoUltimate, Trigger::OnShieldBreak, 1, 50));

    BattleComboTriggerSystem system;
    auto missedRandom = randomForChanceSequence({ { 75, false }, { 50, false } });
    auto missed = system.collectChanceEffects(
        state,
        Trigger::OnShieldBreak,
        { EffectType::ShieldExplosion, EffectType::AutoUltimate },
        missedRandom);
    CHECK(missed.empty());
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 0);

    auto activatedRandom = randomForChanceSequence({ { 75, true }, { 50, true } });
    auto activated = system.collectChanceEffects(
        state,
        Trigger::OnShieldBreak,
        { EffectType::ShieldExplosion, EffectType::AutoUltimate },
        activatedRandom);
    REQUIRE(activated.size() == 2);
    CHECK(activated[0].effect.type == EffectType::ShieldExplosion);
    CHECK(activated[1].effect.type == EffectType::AutoUltimate);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 1 }) == 0);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 2 }) == 1);

    auto cappedRandom = fixedBattleRandom();
    auto capped = system.collectChanceEffects(
        state,
        Trigger::OnShieldBreak,
        { EffectType::ShieldExplosion },
        cappedRandom);
    CHECK(capped.empty());
}

TEST_CASE("BattleComboTriggerSystem_ChanceEffects_CallerCanRecordAfterSceneActionSucceeds", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::MPRestore, Trigger::OnShieldBreak, 25, 100, 0, 1));

    BattleComboTriggerSystem system;
    auto random = fixedBattleRandom();
    auto candidates = system.collectChanceEffects(
        state,
        Trigger::OnShieldBreak,
        { EffectType::MPRestore },
        random,
        BattleComboActivationRecording::CallerRecords);

    REQUIRE(candidates.size() == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 0);

    system.recordActivation(state, candidates[0].effectId);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
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
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 12, 100));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::MPBlock, Trigger::OnHit, 30, 50));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::DmgReductionPct, Trigger::OnBeingHit, 25, 100));

    auto random = randomForChanceSequence({ { 100, true }, { 50, true } });
    auto events = BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, 7, 9 },
        { EffectType::Stun, EffectType::MPBlock },
        random);

    REQUIRE(events.size() == 2);
    CHECK(events[0].hook == BattleComboTriggerHook::ProjectileHitEnemy);
    CHECK(events[0].sourceUnitId == 7);
    CHECK(events[0].targetUnitId == 9);
    CHECK(events[0].effectId == 0);
    CHECK(events[0].effect.type == EffectType::Stun);
    CHECK(events[1].effectId == 1);
    CHECK(events[1].effect.type == EffectType::MPBlock);
}

TEST_CASE("BattleEffectReader_CombinesComboAndSkillWithoutLosingSourceIdentity", "[battle][combo][magic]")
{
    RoleComboState combo;
    BattleEffectState skill;
    const auto comboId = combo.applyConfiguredEffect({ EffectType::MPOnHit, 5 });
    const auto skillId = skill.applyConfiguredEffect({ EffectType::MPOnHit, 7 });
    skill.applyConfiguredEffect(triggeredEffect(EffectType::Stun, Trigger::OnHit, 9, 100));

    BattleEffectSources sources;
    sources.combo = { { BattleEffectSourceKind::Combo, BattleSkillSlot::None }, &combo };
    sources.skill = { { BattleEffectSourceKind::Skill, BattleSkillSlot::Normal }, &skill };

    BattleEffectReader reader;
    CHECK(reader.sumAlways(sources, EffectType::MPOnHit) == 12);
    CHECK(reader.firstAlwaysRef(sources, EffectType::MPOnHit).store.kind == BattleEffectSourceKind::Combo);
    CHECK(reader.firstAlwaysRef(sources, EffectType::MPOnHit).localId == comboId);

    auto random = fixedBattleRandom();
    auto events = reader.collectTriggerEvents(
        sources,
        { BattleComboTriggerHook::DamageDealt, 1, 2, false, true },
        { EffectType::Stun },
        random,
        BattleComboActivationRecording::CallerRecords);

    REQUIRE(events.size() == 1);
    CHECK(events[0].effectRef.store.kind == BattleEffectSourceKind::Skill);
    CHECK(events[0].effectRef.store.slot == BattleSkillSlot::Normal);
    CHECK(events[0].effectRef.localId == RoleComboEffectId{ 1 });
    CHECK(skill.triggeredEffectActivationCount(RoleComboEffectId{ 1 }) == 0);

    BattleEffectCommands().recordActivation(sources, events[0].effectRef);
    CHECK(skill.triggeredEffectActivationCount(RoleComboEffectId{ 1 }) == 1);
    CHECK(combo.triggeredEffectActivationCount(comboId) == 0);
    CHECK(skill.effect(skillId).value == 7);
}

TEST_CASE("BattleComboTriggerSystem_TriggerInputFiltersUltimateAndSideProjectiles", "[battle][combo][magic]")
{
    RoleComboState state;
    state.applyConfiguredEffect(triggeredEffect(EffectType::UltimateExtraProjectiles, Trigger::OnUltimate, 2, 100));
    state.applyConfiguredEffect(triggeredEffect(EffectType::MPBlock, Trigger::OnHit, 12, 100));

    auto normalRandom = fixedBattleRandom();
    CHECK(BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::AfterSkillCast, 1, -1, false, true },
        { EffectType::UltimateExtraProjectiles },
        normalRandom).empty());

    auto ultimateRandom = fixedBattleRandom();
    auto ultimate = BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::AfterSkillCast, 1, -1, true, true },
        { EffectType::UltimateExtraProjectiles },
        ultimateRandom);
    REQUIRE(ultimate.size() == 1);
    CHECK(ultimate[0].effect.type == EffectType::UltimateExtraProjectiles);

    auto sideRandom = fixedBattleRandom();
    CHECK(BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::DamageDealt, 1, 2, false, false },
        { EffectType::MPBlock },
        sideRandom).empty());
}

TEST_CASE("BattleComboTriggerSystem_TriggerEvents_SameSeedProducesSameEventStream", "[battle][combo][unit]")
{
    auto makeState = []() {
        RoleComboState state;
        state.applyConfiguredEffect(
            triggeredEffect(EffectType::Stun, Trigger::OnHit, 12, 100));
        state.applyConfiguredEffect(
            triggeredEffect(EffectType::MPBlock, Trigger::OnHit, 30, 55));
        state.applyConfiguredEffect(
            triggeredEffect(EffectType::Execute, Trigger::OnHit, 15, 20));
        return state;
    };
    auto collectWithSeed = [&](unsigned int seed, RoleComboState& state) {
        auto random = BattleRuntimeRandom(seed);
        return BattleComboTriggerSystem().collectTriggerEvents(
            state,
            { BattleComboTriggerHook::ProjectileHitEnemy, 4, 8 },
            { EffectType::Stun, EffectType::MPBlock, EffectType::Execute },
            random);
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
        CHECK(first[i].effectId == second[i].effectId);
        CHECK(first[i].effect.type == second[i].effect.type);
        CHECK(first[i].effect.value == second[i].effect.value);
    }
    CHECK(firstState.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == secondState.triggeredEffectActivationCount(RoleComboEffectId{ 0 }));
}

TEST_CASE("BattleComboTriggerSystem_TriggerEvents_MaxCountConsumesOnlyOnActivation", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 12, 50, 0, 1));

    BattleComboTriggerSystem system;
    auto missedRandom = randomForChanceSequence({ { 50, false } });
    auto missed = system.collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, 1, 2 },
        { EffectType::Stun },
        missedRandom);
    CHECK(missed.empty());
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 0);

    auto activatedRandom = randomForChanceSequence({ { 50, true } });
    auto activated = system.collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, 1, 2 },
        { EffectType::Stun },
        activatedRandom);
    REQUIRE(activated.size() == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);

    auto cappedRandom = fixedBattleRandom();
    auto capped = system.collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, 1, 2 },
        { EffectType::Stun },
        cappedRandom);
    CHECK(capped.empty());
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
}

TEST_CASE("BattleComboTriggerSystem_QueryMatchingTriggerEffects_DoesNotConsumeActivationCounts", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::ProjectileBounce, Trigger::OnHit, 2, 40, 0, 1));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Execute, Trigger::OnHit, 15, 25));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnBeingHit, 5, 100));

    auto onHit = BattleComboTriggerSystem().matchingTriggerEffects(
        state,
        { BattleComboTriggerHook::ProjectileHitEnemy, 1, 2 },
        { EffectType::ProjectileBounce, EffectType::Execute });

    REQUIRE(onHit.size() == 2);
    CHECK(onHit[0].effectId == 0);
    CHECK(onHit[0].effect.type == EffectType::ProjectileBounce);
    CHECK(onHit[1].effectId == 1);
    CHECK(onHit[1].effect.type == EffectType::Execute);
    CHECK(!state.hasTriggeredEffectActivations());

    auto beingHit = BattleComboTriggerSystem().matchingTriggerEffects(
        state,
        { BattleComboTriggerHook::DamageTaken, 2, 1 },
        { EffectType::Stun });
    REQUIRE(beingHit.size() == 1);
    CHECK(beingHit[0].effectId == 2);
    CHECK(beingHit[0].effect.type == EffectType::Stun);
    CHECK(!state.hasTriggeredEffectActivations());
}

TEST_CASE("BattleComboTriggerSystem_TriggerEvents_MapsShieldBreakHook", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::ShieldExplosion, Trigger::OnShieldBreak, 30, 100));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 5, 100));

    auto random = fixedBattleRandom();
    auto events = BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ShieldBreak, 2, 2 },
        { EffectType::ShieldExplosion },
        random,
        BattleComboActivationRecording::CallerRecords);

    REQUIRE(events.size() == 1);
    CHECK(events[0].hook == BattleComboTriggerHook::ShieldBreak);
    CHECK(events[0].sourceUnitId == 2);
    CHECK(events[0].targetUnitId == 2);
    CHECK(events[0].effect.type == EffectType::ShieldExplosion);
    CHECK(!state.hasTriggeredEffectActivations());
}

TEST_CASE("BattleComboTriggerSystem_TriggerEvents_MapShieldBreakEffectsWithoutRecording", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::ShieldExplosion, Trigger::OnShieldBreak, 30, 100));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::AutoUltimate, Trigger::OnShieldBreak, 1, 100));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::TempFlatATK, Trigger::OnShieldBreak, 14, 100, 45));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::MPRestore, Trigger::OnShieldBreak, 25, 100));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 5, 100));

    auto random = fixedBattleRandom();
    auto events = BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::ShieldBreak, 2, 2 },
        {
            EffectType::ShieldExplosion,
            EffectType::AutoUltimate,
            EffectType::TempFlatATK,
            EffectType::MPRestore,
        },
        random,
        BattleComboActivationRecording::CallerRecords);

    REQUIRE(events.size() == 4);
    CHECK(events[0].effect.type == EffectType::ShieldExplosion);
    CHECK(events[0].effect.value == 30);
    CHECK(events[0].effectId == 0);
    CHECK(events[1].effect.type == EffectType::AutoUltimate);
    CHECK(events[1].effectId == 1);
    CHECK(events[2].effect.type == EffectType::TempFlatATK);
    CHECK(events[2].effect.value == 14);
    CHECK(events[2].effect.duration == 45);
    CHECK(events[2].effectId == 2);
    CHECK(events[3].effect.type == EffectType::MPRestore);
    CHECK(events[3].effect.value == 25);
    CHECK(events[3].effectId == 3);
    CHECK(!state.hasTriggeredEffectActivations());
}

TEST_CASE("BattleComboTriggerSystem_FrameTriggerEffects_FilterActiveConditionsWithoutConsumingCounts", "[battle][combo][unit]")
{
    RoleComboState state;
    state.setLastAliveForComboRuntime(true);
    state.extendTriggerTimer({ Trigger::AllyLowHPBurst, -1 }, 3);
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::PctATK, Trigger::WhileLowHP, 30, 50));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::DmgReductionPct, Trigger::LastAlive, 20));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::PctATK, Trigger::AllyLowHPBurst, 10));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::PctATK, Trigger::Always, 50));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::DmgReductionPct, Trigger::Always, 30));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 5, 100));

    auto active = BattleComboTriggerSystem().activeFrameTriggerEffects(
        state,
        { 40, 100, state.lastAliveForComboRuntime() },
        { EffectType::PctATK, EffectType::DmgReductionPct });

    REQUIRE(active.size() == 3);
    CHECK(active[0].effectId == 0);
    CHECK(active[0].effect.type == EffectType::PctATK);
    CHECK(active[1].effectId == 1);
    CHECK(active[1].effect.type == EffectType::DmgReductionPct);
    CHECK(active[2].effectId == 2);
    CHECK(active[2].effect.type == EffectType::PctATK);
    CHECK(!state.hasTriggeredEffectActivations());

    auto inactive = BattleComboTriggerSystem().activeFrameTriggerEffects(
        state,
        { 80, 100, false },
        { EffectType::PctATK, EffectType::DmgReductionPct });
    REQUIRE(inactive.size() == 1);
    CHECK(inactive[0].effectId == 2);
}

TEST_CASE("BattleComboTriggerSystem_DodgeResolutionAddsAdaptationStacksAndConsumesExplicitRoll", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect({ EffectType::DodgeChance, 10 });
    const auto first = state.applyConfiguredEffect({ EffectType::DodgeAdaptation, 5, 4 });
    const auto second = state.applyConfiguredEffect({ EffectType::DodgeAdaptation, 20, 3 });
    state.recordEffectStackAgainst(first, 7, 4, 5);
    state.recordEffectStackAgainst(second, 7, 3, 20);
    state.recordEffectStackAgainst(first, 7, 4, 5);
    state.recordEffectStackAgainst(second, 7, 3, 20);

    auto dodged = BattleComboTriggerSystem().resolveDodge(state, 7, 59.9);
    CHECK(dodged.chancePct == 60);
    CHECK(dodged.dodged);

    auto hit = BattleComboTriggerSystem().resolveDodge(state, 7, 60.0);
    CHECK(hit.chancePct == 60);
    CHECK_FALSE(hit.dodged);
}

TEST_CASE("BattleComboTriggerSystem_DodgeResolutionClampsChance", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect({ EffectType::DodgeChance, 95 });
    const auto adaptation = state.applyConfiguredEffect({ EffectType::DodgeAdaptation, 20, 10 });
    state.recordEffectStackAgainst(adaptation, 3, 10, 20);
    state.recordEffectStackAgainst(adaptation, 3, 10, 20);

    auto result = BattleComboTriggerSystem().resolveDodge(state, 3, 99.5);

    CHECK(result.chancePct == 100);
    CHECK(result.dodged);
}

TEST_CASE("BattleComboTriggerSystem_AttackerHitDamage_AppliesCritNthAndRampingState", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect({ EffectType::CritChance, 50 });
    state.applyConfiguredEffect({ EffectType::CritMultiplier, 200 });
    const auto nth = state.applyConfiguredEffect({ EffectType::EveryNthDouble, 2 });
    state.advanceEffectCounter(nth, 2);
    const auto ramping = state.applyConfiguredEffect({ EffectType::RampingDmg, 25, 3 });
    state.recordEffectStack(ramping, 3, 25);
    state.setEffectIdleTimer(ramping, 90);
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::PctATK, Trigger::WhileLowHP, 50, 90));

    auto random = randomForChanceSequence({ { 50, true } });
    auto result = BattleComboTriggerSystem().shapeAttackerHitDamage(
        state,
        { 100.0, 80, 100, false },
        random);

    CHECK(result.damage == Catch::Approx(900.0));
    REQUIRE(result.events.size() == 2);
    CHECK(result.events[0].type == BattleAttackerHitDamageEventType::Crit);
    CHECK(result.events[0].value == 200);
    CHECK(result.events[1].type == BattleAttackerHitDamageEventType::RampingStack);
    CHECK(result.events[1].value == 25);
    CHECK(result.events[1].value2 == 2);
    CHECK(state.effectStacks(ramping) == 2);
    CHECK(state.effectIdleTimer(ramping) == 90);
}

TEST_CASE("BattleComboTriggerSystem_AttackerHitDamage_IgnoresAlwaysPctAtkRuntimeShape", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect({ EffectType::PctATK, 50 });

    auto random = fixedBattleRandom();
    auto result = BattleComboTriggerSystem().shapeAttackerHitDamage(
        state,
        { 100.0, 80, 100, false },
        random);

    CHECK(result.damage == Catch::Approx(100.0));
}

TEST_CASE("BattleComboTriggerSystem_DefenderHitDamage_AppliesReductionAndAdaptationState", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::DmgReductionPct, Trigger::WhileLowHP, 20, 90));
    const auto adaptation = state.applyConfiguredEffect({ EffectType::Adaptation, 10, 3 });
    state.recordEffectStackAgainst(adaptation, 7, 3, 10);
    const auto dodgeAdaptation = state.applyConfiguredEffect({ EffectType::DodgeAdaptation, 5, 4 });

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
    CHECK(state.effectStacksAgainst(adaptation, 7) == 2);
    CHECK(state.effectStacksAgainst(dodgeAdaptation, 7) == 1);
}

TEST_CASE("BattleComboTriggerSystem_DefenderHitDamage_IgnoresAlwaysDmgReductionRuntimeShape", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect({ EffectType::DmgReductionPct, 20 });

    auto result = BattleComboTriggerSystem().shapeDefenderHitDamage(
        state,
        { 100.0, 40, 100, false, 7 });

    CHECK(result.damage == Catch::Approx(100.0));
}

TEST_CASE("BattleComboTriggerSystem_ExecuteCombo_RecordsOnlyTriggeredExecute", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Execute, Trigger::OnHit, 20, 50, 0, 1));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Execute, Trigger::OnHit, 10, 100, 0, 1));

    auto random = randomForChanceSequence({ { 50, true } });
    auto result = BattleComboTriggerSystem().resolveExecuteCombo(
        state,
        { 4, 8, 50, 200, 15.0, true },
        random);

    CHECK(result.executed);
    CHECK(result.thresholdPct == 20);
    CHECK(result.effectId == 0);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 1 }) == 0);
}

TEST_CASE("BattleComboTriggerSystem_DefenderReactions_ResolveReflect", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect({ EffectType::ProjectileReflect, 40 });

    auto reflectHitRandom = randomForChanceSequence({ { 40, true } });
    CHECK(BattleComboTriggerSystem().resolveProjectileReflect(state, true, reflectHitRandom));
    auto reflectMissRandom = randomForChanceSequence({ { 40, false } });
    CHECK_FALSE(BattleComboTriggerSystem().resolveProjectileReflect(state, true, reflectMissRandom));
    auto notRangedRandom = fixedBattleRandom();
    CHECK_FALSE(BattleComboTriggerSystem().resolveProjectileReflect(state, false, notRangedRandom));
}

TEST_CASE("BattleComboTriggerSystem_TriggerEvents_CollectStunWithoutRecordingActivation", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 12, 100, 0, 1));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::MPBlock, Trigger::OnHit, 20, 100));

    auto random = fixedBattleRandom();
    auto events = BattleComboTriggerSystem().collectTriggerEvents(
        state,
        { BattleComboTriggerHook::DamageDealt, 4, 8 },
        { EffectType::Stun },
        random,
        BattleComboActivationRecording::CallerRecords);

    REQUIRE(events.size() == 1);
    CHECK(events[0].effect.value == 12);
    CHECK(events[0].effectId == 0);
    CHECK(!state.hasTriggeredEffectActivations());
}

TEST_CASE("BattleComboTriggerSystem_ProjectileBouncePrime_AggregatesBounceEffects", "[battle][combo][unit]")
{
    RoleComboState state;
    auto firstBounce = triggeredEffect(EffectType::ProjectileBounce, Trigger::OnHit, 2, 40);
    firstBounce.value2 = 90;
    state.applyConfiguredEffect(firstBounce);
    auto secondBounce = triggeredEffect(EffectType::ProjectileBounce, Trigger::OnHit, 1, 70);
    secondBounce.value2 = 120;
    state.applyConfiguredEffect(secondBounce);
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnHit, 5, 100));

    auto prime = BattleComboTriggerSystem().collectProjectileBouncePrime(
        state,
        { 4, 25, 80 });

    CHECK(prime.count == 3);
    CHECK(prime.chancePct == 100);
    CHECK(prime.rollPct == 25);
    CHECK(prime.range == 120);
    CHECK(!state.hasTriggeredEffectActivations());
}

TEST_CASE("BattleComboTriggerSystem_ExtraProjectileCount_AddsFlatAndTriggeredEffects", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::UltimateExtraProjectiles, Trigger::OnUltimate, 2, 100, 0, 1));
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Stun, Trigger::OnUltimate, 5, 100));

    auto random = fixedBattleRandom();
    auto count = BattleComboTriggerSystem().collectExtraProjectileCount(
        state,
        { BattleComboTriggerHook::AfterSkillCast, 4, -1, true, true },
        3,
        random);

    CHECK(count == 5);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 0 }) == 1);
    CHECK(state.triggeredEffectActivationCount(RoleComboEffectId{ 1 }) == 0);
}

TEST_CASE("BattleComboTriggerSystem_ExecuteCapability_DetectsExecuteEffect", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::Execute, Trigger::OnHit, 20, 50));

    CHECK(BattleComboTriggerSystem().hasExecuteCombo(state, 4));
}

TEST_CASE("BattleComboTriggerSystem_ArmorPenetration_AppliesAlwaysAndTriggeredPen", "[battle][combo][unit]")
{
    RoleComboState state;
    state.applyConfiguredEffect({ EffectType::ArmorPenChance, 100 });
    state.applyConfiguredEffect({ EffectType::ArmorPenPct, 25 });
    state.applyConfiguredEffect(
        triggeredEffect(EffectType::ArmorPen, Trigger::OnHit, 40, 100));

    auto random = fixedBattleRandom();
    auto result = BattleComboTriggerSystem().resolveArmorPenetratedDefense(
        state,
        { 4, 8, 100.0 },
        random);

    CHECK(result.defense == Catch::Approx(45.0));
}
