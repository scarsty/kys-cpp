#include "BattleRuntimeScenarioTestHelpers.h"
#include "BattleLogTestHelpers.h"
#include "HeadlessBattleRunner.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>

using namespace KysChess;
using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;

namespace
{

EffectRule cloneGenerationRule()
{
    EffectRule rule;
    rule.event = EffectEvent::BattleInitialized;
    rule.actions.push_back({ StateMachineAction{ GenerateClonesAction{ 1 } } });
    return rule;
}

BattleRuntimeSessionCreationInput timeoutInput()
{
    BattleRuntimeSessionCreationInput input;
    input.rules = scenarioRules();
    input.rules.maximumFrames = 1;
    input.units.push_back(scenarioSetupUnit(1, 0, 100, {100, 100, 0}));
    input.units.push_back(scenarioSetupUnit(2, 1, 100, {500, 500, 0}));
    return input;
}

}

TEST_CASE("shared runtime emits one exact timeout event", "[battle][headless][determinism]")
{
    auto result = HeadlessBattleRunner::run(timeoutInput());
    const auto repeated = HeadlessBattleRunner::run(timeoutInput());

    CHECK(result.summary.outcome == BattleOutcome::Timeout);
    CHECK(result.summary.endFrame == 1);
    REQUIRE(result.digestEvents.size() == 1);
    CHECK(result.digestEvents.front().type == BattleGameplayEventType::BattleEnded);
    CHECK(result.digestEvents.front().frame == 1);
    CHECK(result.digest == repeated.digest);
    CHECK(result.finalRuntime.attacks.attacks.empty());
    CHECK(result.finalRuntime.nextFrame.queuedAttacks().empty());
    CHECK(result.finalRuntime.nextFrame.queuedDamage().empty());
    CHECK(result.finalRuntime.effectIntegration.casts.empty());
    CHECK(result.finalRuntime.effectIntegration.queuedCommandBatches.empty());
    CHECK(result.finalRuntime.effectIntegration.damageContinuations.empty());
    CHECK(result.finalRuntime.units.pendingCastCount() == 0);
    const auto lifecycle = result.finalRuntime.castLifecycle.snapshot();
    CHECK(lifecycle.terminalState == BattleCastLifecycleTerminalState::BattleEnded);
    CHECK(lifecycle.battleEndedFrame == 1);
    CHECK(lifecycle.activeCasts.empty());
    CHECK(lifecycle.work.empty());
    CHECK(result.finalRuntime.castLifecycle.drainReadyEvents(1).empty());
}

TEST_CASE("battle digest includes the remaining shared strengthening pool", "[battle][headless][determinism][backbone]")
{
    auto result = HeadlessBattleRunner::run(timeoutInput());
    result.finalRuntime.units.require(1).damage.strengthening = {2, 50};
    const auto original = HeadlessBattleRunner::digest(result);

    SECTION("remaining charges affect the digest")
    {
        --result.finalRuntime.units.require(1).damage.strengthening.charges;
    }
    SECTION("strengthening percentage affects the digest")
    {
        result.finalRuntime.units.require(1).damage.strengthening.damagePercent = 37;
    }
    CHECK(HeadlessBattleRunner::digest(result) != original);
}

TEST_CASE("battle summary marks surviving summoned clones separately", "[battle][headless][summary][summon]")
{
    auto input = timeoutInput();
    input.setup.units.push_back({
        .unitId = 2,
        .realRoleId = 2,
        .team = 1,
        .star = 1,
        .baseMaxHp = 100,
        .baseAttack = 30,
        .baseDefence = 5,
    });
    input.setup.neigongDefinitions.push_back({ 9001, { cloneGenerationRule() } });
    input.setup.obtainedNeigongMagicIdsByTeam[1].push_back(9001);
    input.setup.cloneSources.push_back({ 2, 2, 100, 1, -1, 0 });
    input.setup.cloneCells.push_back({ 3, 4, true, false, 1 });

    const auto result = HeadlessBattleRunner::run(input);

    const auto summoned = std::ranges::find(result.summary.survivors, 3, &BattleSurvivorSummary::unitId);
    REQUIRE(summoned != result.summary.survivors.end());
    CHECK(summoned->summoned);
    const auto initialEnemy = std::ranges::find(result.summary.survivors, 2, &BattleSurvivorSummary::unitId);
    REQUIRE(initialEnemy != result.summary.survivors.end());
    CHECK_FALSE(initialEnemy->summoned);
}

TEST_CASE("simultaneous wipe is player victory", "[battle][headless][determinism]")
{
    BattleRuntimeState runtime;
    seedScenarioRuntimeUnits(runtime, {
        scenarioRuntimeUnit(1, 0, 1, {100, 100, 0}),
        scenarioRuntimeUnit(2, 1, 1, {120, 100, 0}),
    });
    runtime.nextFrame.queueDamage(scenarioPreResolvedDamage(2, 1, 1));
    runtime.nextFrame.queueDamage(scenarioPreResolvedDamage(1, 2, 1));
    BattleRuntimeSession session(std::move(runtime));

    const auto frame = session.runFrame();

    CHECK(session.runtime().result.ended);
    CHECK(session.runtime().result.outcome == BattleOutcome::PlayerVictory);
    CHECK(session.runtime().result.winningTeam == 0);
    CHECK(std::ranges::count_if(frame.gameplayEvents, [](const auto& event) {
        return event.type == BattleGameplayEventType::BattleEnded;
    }) == 1);
}

TEST_CASE("battle digest excludes localized report names", "[battle][headless][determinism]")
{
    auto firstAttacker = BattleLogTest::reportUnit(1, 10, 0, 1, "角色甲");
    auto secondAttacker = BattleLogTest::reportUnit(1, 10, 0, 1, "角色乙");
    auto defender = BattleLogTest::reportUnit(2, 20, 1, 2, "敵人");
    BattleReportBuilder firstReport;
    BattleReportBuilder secondReport;
    firstReport.recordDamage(&firstAttacker, &defender, 50, "武學甲", 10, {}, 77);
    secondReport.recordDamage(&secondAttacker, &defender, 50, "武學乙", 10, {}, 77);

    HeadlessBattleResult first;
    first.summary.outcome = BattleOutcome::PlayerVictory;
    first.summary.endFrame = 10;
    first.report = firstReport.report();
    HeadlessBattleResult second = first;
    second.report = secondReport.report();

    CHECK(HeadlessBattleRunner::digest(first) == HeadlessBattleRunner::digest(second));
}

TEST_CASE("battle digest includes persistent area runtime state", "[battle][headless][determinism][area]")
{
    HeadlessBattleResult withoutArea;
    withoutArea.summary.outcome = BattleOutcome::Timeout;
    withoutArea.summary.endFrame = 20;
    HeadlessBattleResult withArea = withoutArea;

    BattleAreaEffect area;
    area.id = BattleAreaId{ 3 };
    area.source = {
        .kind = EffectSourceKind::Magic,
        .sourceId = 86,
        .ownerUnitId = 7,
        .sourceTeam = 0,
    };
    area.sourceTeam = 0;
    area.geometry = { .shape = AreaShape::Circle, .radiusTiles = 5 };
    area.anchor = {
        .kind = BattleAreaAnchorKind::FixedWorldPosition,
        .fixedPosition = { 100, 200, 0 },
    };
    area.createdFrame = 10;
    area.expiresFrameExclusive = 100;
    area.sourceDeath = AreaSourceDeathPolicy::PersistUntilExpiry;
    area.mergeKey = { .source = area.source, .ruleId = EffectRuleId{ 9 } };
    area.merge = AreaMergePolicy::RefreshSameSource;
    withArea.finalRuntime.areas.nextAreaId = 4;
    withArea.finalRuntime.areas.areas.push_back(area);

    CHECK(HeadlessBattleRunner::digest(withArea)
        != HeadlessBattleRunner::digest(withoutArea));

    auto movedArea = withArea;
    movedArea.finalRuntime.areas.areas[0].anchor.fixedPosition.x += 1;
    CHECK(HeadlessBattleRunner::digest(movedArea)
        != HeadlessBattleRunner::digest(withArea));

    auto removedArea = withoutArea;
    removedArea.finalRuntime.areas.nextAreaId = 1;
    CHECK(HeadlessBattleRunner::digest(removedArea)
        != HeadlessBattleRunner::digest(withoutArea));
}

TEST_CASE("battle digest includes cast dispatch and terminal frames", "[battle][headless][determinism][lifecycle]")
{
    HeadlessBattleResult first;
    first.summary.outcome = BattleOutcome::Timeout;
    first.summary.endFrame = 40;
    const auto firstCast = first.finalRuntime.castLifecycle.beginRootCast({
        1,
        100,
        true,
    });
    first.finalRuntime.castLifecycle.completeWork(firstCast.commitBarrier);
    REQUIRE(first.finalRuntime.castLifecycle.drainReadyEvents(10).size() == 1);
    REQUIRE(first.finalRuntime.castLifecycle.drainReadyEvents(11).size() == 1);

    HeadlessBattleResult second;
    second.summary = first.summary;
    const auto secondCast = second.finalRuntime.castLifecycle.beginRootCast({
        1,
        100,
        true,
    });
    second.finalRuntime.castLifecycle.completeWork(secondCast.commitBarrier);
    REQUIRE(second.finalRuntime.castLifecycle.drainReadyEvents(20).size() == 1);
    REQUIRE(second.finalRuntime.castLifecycle.drainReadyEvents(21).size() == 1);

    CHECK(HeadlessBattleRunner::digest(first)
        != HeadlessBattleRunner::digest(second));

    auto battleEnded = first;
    battleEnded.finalRuntime.castLifecycle.cancelOutstandingForBattleEnd(40);
    CHECK(HeadlessBattleRunner::digest(battleEnded)
        != HeadlessBattleRunner::digest(first));
}

TEST_CASE("battle digest includes random seed and draw count", "[battle][headless][determinism][random]")
{
    HeadlessBattleResult baseline;
    baseline.summary.outcome = BattleOutcome::Timeout;
    baseline.summary.endFrame = 20;

    auto differentSeed = baseline;
    differentSeed.finalRuntime.random = BattleRuntimeRandom{ 9 };
    CHECK(HeadlessBattleRunner::digest(differentSeed)
        != HeadlessBattleRunner::digest(baseline));

    auto consumedRandom = baseline;
    (void)consumedRandom.finalRuntime.random.nextInt(100);
    CHECK(HeadlessBattleRunner::digest(consumedRandom)
        != HeadlessBattleRunner::digest(baseline));
}

TEST_CASE("battle digest includes canonical queued attack lineage", "[battle][headless][determinism][lifecycle]")
{
    HeadlessBattleResult sourceRules;
    sourceRules.summary.outcome = BattleOutcome::Timeout;
    sourceRules.summary.endFrame = 20;
    const auto sourceCast = sourceRules.finalRuntime.castLifecycle.beginRootCast({
        1,
        100,
        true,
        CastOriginKind::Ultimate,
        CastPropagationPolicy::SourceRules,
    });
    const auto sourceAttack = sourceRules.finalRuntime.castLifecycle.reserveAttack(
        sourceCast.provenance.castId,
        {
            .rootAttack = true,
            .propagation = CastPropagationPolicy::SourceHitRulesOnly,
        });
    BattleAttackSpawnRequest sourceRequest{ BattleAttackPayload(
        BattleAttackDelivery::contact(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    sourceRequest.provenance = sourceAttack.provenance;
    sourceRequest.castWork = sourceAttack.work;
    sourceRules.finalRuntime.nextFrame.queueAttack(std::move(sourceRequest));

    auto suppressed = sourceRules;
    suppressed.finalRuntime.nextFrame.mutableAttacksForReducer()[0]
        .provenance.propagation = CastPropagationPolicy::SuppressUltimateRules;

    CHECK(HeadlessBattleRunner::digest(sourceRules)
        != HeadlessBattleRunner::digest(suppressed));

    auto projectileDelivery = sourceRules;
    projectileDelivery.finalRuntime.nextFrame.mutableAttacksForReducer()[0]
        .initial.delivery = BattleAttackDelivery::projectile();
    CHECK(HeadlessBattleRunner::digest(sourceRules)
        != HeadlessBattleRunner::digest(projectileDelivery));

    auto scriptedPayload = sourceRules;
    scriptedPayload.finalRuntime.nextFrame.mutableAttacksForReducer()[0]
        .initial.payloadClass = BattleProjectilePayloadClass::scriptedDamage();
    CHECK(HeadlessBattleRunner::digest(sourceRules)
        != HeadlessBattleRunner::digest(scriptedPayload));

    auto reflectedLineage = sourceRules;
    reflectedLineage.finalRuntime.nextFrame.mutableAttacksForReducer()[0]
        .initial.reflectionLineage = BattleAttackReflectionLineageKind::ReflectedReturn;
    CHECK(HeadlessBattleRunner::digest(sourceRules)
        != HeadlessBattleRunner::digest(reflectedLineage));

    auto potencySnapshot = sourceRules;
    potencySnapshot.finalRuntime.nextFrame.mutableAttacksForReducer()[0]
        .initial.potencySnapshot = BattleAttackPotencySnapshot{ 50, 600 };
    CHECK(HeadlessBattleRunner::digest(sourceRules)
        != HeadlessBattleRunner::digest(potencySnapshot));
}
