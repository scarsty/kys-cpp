#include "BattleRuntimeScenarioTestHelpers.h"
#include "BattleCoreTestHelpers.h"
#include "ChessBattleEffectParser.h"
#include "Find.h"
#include "battle/BattleEffectCommandSystem.h"
#include "battle/BattleHealSystem.h"
#include "battle/BattleStatusSystem.h"
#include "BattleRuntimeRecordTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace KysChess;
using namespace KysChess::Battle;
using namespace KysChess::Battle::Test;

namespace
{

BattleRuntimeSessionCreationInput basicSessionInput()
{
    BattleRuntimeSessionCreationInput input;
    input.rules = scenarioRules();
    input.randomSeed = 1;
    input.battleFrame = 0;
    input.units = {
        scenarioSetupUnit(0, 0, 100, { 100, 100, 0 }),
        scenarioSetupUnit(1, 1, 100, { 500, 100, 0 }),
    };
    return input;
}

BattleActionSkillSeed scenarioRangedSkill()
{
    BattleActionSkillSeed skill;
    skill.id = 501;
    skill.name = "定序彈道";
    skill.soundId = 9;
    skill.attackAreaType = 3;
    skill.magicType = 1;
    skill.visualEffectId = 77;
    skill.selectDistance = 4;
    skill.actProperty = 40;
    skill.magicPower = 80;
    return skill;
}

BattleRuntimeSessionCreationInput actionProjectileSessionInput()
{
    BattleRuntimeSessionCreationInput input;
    input.rules = scenarioRules();
    input.randomSeed = 1;
    input.battleFrame = 0;

    auto attacker = scenarioSetupUnit(0, 0, 100, { 100, 100, 0 });
    attacker.actionPlan = BattleActionPlanSeed{
        .hasEquippedSkill = true,
        .normalSkill = scenarioRangedSkill(),
    };
    input.units.push_back(attacker);

    input.units.push_back(scenarioSetupUnit(1, 1, 100, { 180, 100, 0 }));
    return input;
}

ChessMagicEffectDefinition realUltimateDefinition(int magicId)
{
    std::vector<ChessMagicEffectDefinition> definitions;
    const auto path = std::filesystem::current_path()
        / "config"
        / "chess_magic_effects.yaml";
    if (!loadMagicEffectsFile(path.string(), definitions))
    {
        throw std::runtime_error("無法載入頂層 chess_magic_effects.yaml");
    }
    const auto definition = std::ranges::find(
        definitions,
        magicId,
        &ChessMagicEffectDefinition::magicId);
    if (definition == definitions.end())
    {
        throw std::runtime_error("頂層大招設定缺少測試武功");
    }
    return *definition;
}

BattleActionSkillSeed verticalSliceSkill(int magicId, int magicType = 1)
{
    BattleActionSkillSeed skill;
    skill.id = magicId;
    skill.name = "真實設定大招";
    skill.attackAreaType = 3;
    skill.magicType = magicType;
    skill.visualEffectId = magicId;
    skill.selectDistance = 12;
    skill.actProperty = 40;
    skill.magicPower = 80;
    return skill;
}

void equipVerticalSliceUltimate(
    BattleRuntimeSessionCreationInput& input,
    BattleSetupUnitInput& unit,
    int magicId,
    int magicType = 1)
{
    unit.actionPlan = BattleActionPlanSeed{
        .hasEquippedSkill = true,
        .normalSkill = verticalSliceSkill(501, magicType),
        .ultimateSkill = verticalSliceSkill(magicId, magicType),
    };
}

BattleSetupUnitInput verticalSliceUnit(
    int id,
    int team,
    int hp,
    int maxHp,
    int mp,
    Pointf position,
    int star = 1)
{
    auto unit = scenarioSetupUnit(id, team, hp, position);
    unit.vitals.maxHp = maxHp;
    unit.vitals.mp = mp;
    unit.vitals.maxMp = 100;
    unit.stats = { 100, 30, 10 };
    unit.star = star;
    return unit;
}

BattleRuntimeState initializedVerticalSliceState(BattleRuntimeSessionCreationInput input)
{
    auto creation = BattleRuntimeSession::createInitialized(std::move(input));
    return creation.session.runtime();
}

std::optional<BattlePresentationFrame> runUntil(
    BattleRuntimeState& state,
    int maximumFrames,
    const auto& predicate)
{
    BattleFrameRunner runner;
    for (int frame = 0; frame < maximumFrames; ++frame)
    {
        auto result = runner.runFrame(state);
        if (predicate(state, result))
        {
            return result;
        }
    }
    return std::nullopt;
}

BattleRuntimeSessionCreationInput singleUltimateInput(
    int magicId,
    int casterStar = 1,
    int magicType = 1,
    Pointf targetPosition = { 300, 100, 0 })
{
    BattleRuntimeSessionCreationInput input;
    input.rules = scenarioRules();
    input.randomSeed = 12345;

    auto caster = verticalSliceUnit(
        0,
        0,
        1000,
        1000,
        100,
        { 100, 100, 0 },
        casterStar);
    equipVerticalSliceUltimate(input, caster, magicId, magicType);
    input.units.push_back(caster);

    auto target = verticalSliceUnit(
        1,
        1,
        10000,
        10000,
        0,
        targetPosition);
    target.stats.defence = 100;
    input.units.push_back(target);
    input.setup.magicEffectDefinitions.push_back(
        realUltimateDefinition(magicId));
    return input;
}

BattleRuntimeSessionCreationInput divineFlickInput(
    int mp,
    int normalMagicId = 501)
{
    constexpr int DivineFlickMagicId = 18;

    BattleRuntimeSessionCreationInput input;
    input.rules = scenarioRules();
    input.randomSeed = 12345;

    auto caster = verticalSliceUnit(
        0, 0, 1000, 1000, mp, { 100, 100, 0 });
    caster.actionPlan = BattleActionPlanSeed{
        .hasEquippedSkill = true,
        .normalSkill = verticalSliceSkill(normalMagicId),
        .ultimateSkill = verticalSliceSkill(DivineFlickMagicId),
    };
    caster.actionPlan->normalSkill.attackAreaType = 0;
    caster.actionPlan->normalSkill.selectDistance = 1;
    caster.actionPlan->ultimateSkill.attackAreaType = 0;
    caster.actionPlan->ultimateSkill.selectDistance = 1;
    input.units.push_back(caster);

    auto target = verticalSliceUnit(
        1, 1, 10000, 10000, 0, { 300, 100, 0 });
    target.frozen = 1000;
    target.frozenMax = 1000;
    input.units.push_back(target);
    input.setup.magicEffectDefinitions.push_back(
        realUltimateDefinition(DivineFlickMagicId));
    return input;
}

constexpr int CopyRuntimeMagicId = 9101;
constexpr int CopyRuntimeCandidateMagicId = 9102;
constexpr int CopyRuntimeCandidateUnitId = 1;
constexpr int CopyRuntimeOriginalTargetUnitId = 2;
constexpr int CopyRuntimeFallbackTargetUnitId = 3;

BattleActionSkillSeed copyRuntimeSkill(
    std::string name,
    int visualEffectId,
    int magicPower)
{
    BattleActionSkillSeed skill;
    skill.id = visualEffectId;
    skill.name = std::move(name);
    skill.soundId = visualEffectId;
    skill.hurtType = 1;
    skill.attackAreaType = 1;
    skill.magicType = 1;
    skill.visualEffectId = visualEffectId;
    skill.selectDistance = 12;
    skill.actProperty = 40;
    skill.magicPower = magicPower;
    return skill;
}

void equipCopyRuntimeSkills(
    BattleRuntimeSessionCreationInput& input,
    BattleSetupUnitInput& unit,
    BattleActionSkillSeed ultimateSkill)
{
    unit.actionPlan = BattleActionPlanSeed{
        .hasEquippedSkill = true,
        .normalSkill = verticalSliceSkill(9200),
        .ultimateSkill = std::move(ultimateSkill),
    };
}

ChessMagicEffectDefinition copyRuntimeDefinition()
{
    CopyAttackDefinitionAction copy;
    copy.sourceUnits.kind = EffectSelectorKind::Allies;
    copy.sourceUnits.count = 1;
    copy.sourceUnits.excludeOwner = true;
    copy.filter.conditions = {
        CopiedMagicCondition::HasUltimateAttackDefinition,
        CopiedMagicCondition::ExcludesRecursiveEffects,
    };
    copy.copyCount = 1;
    copy.propagation = CastPropagationPolicy::SuppressUltimateRules;

    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::UltimateCommitted;
    rule.selector.kind = EffectSelectorKind::Self;
    rule.actions.push_back({ StateMachineAction{ std::move(copy) } });

    ChangeResourceAction sourceRuleMarker;
    sourceRuleMarker.resource = BattleResource::Shield;
    sourceRuleMarker.amount.flat = 7;
    sourceRuleMarker.kind = ResourceChangeKind::Grant;
    EffectRule sourceRule;
    sourceRule.id = EffectRuleId{ 2 };
    sourceRule.event = EffectEvent::UltimateCommitted;
    sourceRule.selector.kind = EffectSelectorKind::Self;
    sourceRule.actions.push_back({ std::move(sourceRuleMarker) });
    return {
        .magicId = CopyRuntimeMagicId,
        .name = "測試複製武功",
        .rules = { std::move(rule), std::move(sourceRule) },
    };
}

BattleRuntimeSessionCreationInput copyRuntimeInput(
    bool candidateAlive = true,
    bool candidateHasUltimate = true,
    bool originalTargetAlive = true,
    bool includeFallbackTarget = false,
    bool freezeCopier = false,
    bool includeCandidate = true)
{
    BattleRuntimeSessionCreationInput input;
    input.rules = scenarioRules();
    input.randomSeed = 24680;

    auto copier = verticalSliceUnit(
        0, 0, 1000, 1000, 100, { 100, 100, 0 });
    if (freezeCopier)
    {
        copier.frozen = 1000;
        copier.frozenMax = 1000;
    }
    equipCopyRuntimeSkills(
        input,
        copier,
        copyRuntimeSkill("複製者原招", 9101, 60));
    input.units.push_back(copier);

    if (includeCandidate)
    {
        auto candidate = verticalSliceUnit(
            CopyRuntimeCandidateUnitId,
            0,
            candidateAlive ? 1000 : 0,
            1000,
            0,
            { 120, 100, 0 });
        candidate.alive = candidateAlive;
        candidate.frozen = 1000;
        candidate.frozenMax = 1000;
        if (candidateHasUltimate)
        {
            equipCopyRuntimeSkills(
                input,
                candidate,
                copyRuntimeSkill("候選者絕招", 9102, 140));
        }
        input.units.push_back(candidate);
    }

    auto originalTarget = verticalSliceUnit(
        CopyRuntimeOriginalTargetUnitId,
        1,
        originalTargetAlive ? 10000 : 0,
        10000,
        0,
        { 250, 100, 0 });
    originalTarget.alive = originalTargetAlive;
    originalTarget.frozen = 1000;
    originalTarget.frozenMax = 1000;
    input.units.push_back(originalTarget);

    if (includeFallbackTarget)
    {
        auto fallbackTarget = verticalSliceUnit(
            CopyRuntimeFallbackTargetUnitId,
            1,
            10000,
            10000,
            0,
            { 350, 100, 0 });
        fallbackTarget.frozen = 1000;
        fallbackTarget.frozenMax = 1000;
        input.units.push_back(fallbackTarget);
    }
    input.setup.magicEffectDefinitions.push_back(copyRuntimeDefinition());
    return input;
}

const BattleAttackInstance* copiedRuntimeAttack(const BattleRuntimeState& state)
{
    const auto attack = std::ranges::find_if(
        state.attacks.attacks,
        [](const BattleAttackInstance& candidate)
        {
            return candidate.provenance.valid()
                && candidate.provenance.cast.origin
                    == CastOriginKind::CopiedAttack;
        });
    return attack != state.attacks.attacks.end() ? &*attack : nullptr;
}

}  // namespace

TEST_CASE("BattleRuntimeScenario_BasicSessionDigestTracksFramesAndRuntime", "[battle][scenario][runtime]")
{
    auto creation = BattleRuntimeSession::createInitialized(basicSessionInput());
    auto session = std::move(creation.session);

    auto digests = runScenarioFrames(session, 2);

    REQUIRE(digests.size() == 2);
    CHECK(digests[0].frame == 1);
    CHECK(digests[1].frame == 2);
    CHECK(digests[1].aliveUnitIds == std::vector<int>{ 0, 1 });
    CHECK(digests[1].hpByUnitId.at(0) == 100);
    CHECK(digests[1].hpByUnitId.at(1) == 100);
    CHECK(digests[1].activeAttackCount == 0);
    CHECK(digests[1].pendingAttackSpawnCount == 0);
    CHECK_FALSE(digests[1].battleEnded);
}

TEST_CASE("BattleRuntimeScenario_ActionProjectileDamageClearsCastState", "[battle][scenario][runtime]")
{
    auto creation = BattleRuntimeSession::createInitialized(actionProjectileSessionInput());
    auto session = std::move(creation.session);

    std::vector<BattleScenarioFrameDigest> digests;
    BattleScenarioFrameDigest damageDigest;
    bool sawDamage = false;
    for (int i = 0; i < 80; ++i)
    {
        auto result = session.runFrame();
        auto digest = digestScenarioFrame(session.runtime(), result);
        digests.push_back(digest);
        if (!sawDamage && !digest.committedHpDamage.empty())
        {
            damageDigest = digest;
            sawDamage = true;
        }
        if (sawDamage
            && digest.pendingCastCount == 0
            && !digest.haveActionByUnitId.at(0)
            && digest.operationTypeByUnitId.at(0) == BattleOperationType::None)
        {
            break;
        }
    }

    REQUIRE(!digests.empty());
    REQUIRE(sawDamage);
    const auto& finalDigest = digests.back();
    CHECK(damageDigest.damageDefenderIds == std::vector<int>{ 1 });
    CHECK(damageDigest.committedHpDamage.front() > 0);
    CHECK(finalDigest.pendingCastCount == 0);
    CHECK_FALSE(finalDigest.haveActionByUnitId.at(0));
    CHECK(finalDigest.operationTypeByUnitId.at(0) == BattleOperationType::None);
    CHECK(finalDigest.pendingAttackSpawnCount == 0);
}

TEST_CASE("BattleRuntimeScenario_ProjectileCancellationDigest", "[battle][scenario][runtime]")
{
    BattleRuntimeState state;
    seedScenarioRuntimeUnits(state, {
        scenarioRuntimeUnit(0, 0, 100, { 100, 100, 0 }),
        scenarioRuntimeUnit(1, 1, 100, { 900, 900, 0 }),
    });
    state.units.requireCore(0).style = CombatStyle::Ranged;
    state.units.requireCore(1).style = CombatStyle::Ranged;
    appendScenarioTrackedRootAttack(
        state,
        scenarioCancelProjectile(10, 0, 25));
    appendScenarioTrackedRootAttack(
        state,
        scenarioCancelProjectile(20, 1, 12));

    BattleRuntimeSession session(std::move(state));

    auto result = session.runFrame();
    auto digest = digestScenarioFrame(session.runtime(), result);

    CHECK(digest.attackTypes == std::vector<BattleGameplayEventType>{
        BattleGameplayEventType::ProjectileMoved,
        BattleGameplayEventType::ProjectileMoved,
        BattleGameplayEventType::ProjectileCancelled,
    });
    CHECK(std::ranges::find(
        digest.gameplayTypes,
        BattleGameplayEventType::ProjectileCancelled) != digest.gameplayTypes.end());
    CHECK(digestHasLogText(digest, "抵消彈道 #10 vs #20（25 - 12 = 13）"));
    CHECK(digest.activeAttackCount == 2);
    CHECK(digest.activeAttackIds == std::vector<int>{ 10, 20 });
    CHECK(digest.projectileCancelWeakenByAttackId.at(10) == 12);
    CHECK(digest.projectileCancelWeakenByAttackId.at(20) == 25);
}

TEST_CASE("BattleRuntimeScenario_RealQingnangHealsLowestHpAlly", "[battle][scenario][runtime][ultimate-effect][vertical]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = scenarioRules();
    input.randomSeed = 12345;

    auto caster = verticalSliceUnit(
        0, 0, 1000, 1000, 100, { 100, 100, 0 });
    equipVerticalSliceUltimate(input, caster, 127);
    input.units.push_back(caster);
    input.units.push_back(verticalSliceUnit(
        1, 0, 400, 2000, 0, { 120, 120, 0 }));
    input.units.push_back(verticalSliceUnit(
        2, 0, 300, 1000, 0, { 140, 120, 0 }));
    input.units.push_back(verticalSliceUnit(
        3, 1, 10000, 10000, 0, { 300, 100, 0 }));
    input.setup.magicEffectDefinitions.push_back(
        realUltimateDefinition(127));

    auto state = initializedVerticalSliceState(std::move(input));
    const auto committed = runUntil(state, 180, [](const auto& runtime, const auto&)
    {
        return std::ranges::any_of(runtime.heals.events, [](const auto& event)
        {
            return event.type == BattleHealEventType::Applied;
        });
    });

    REQUIRE(committed);
    const auto applied = std::ranges::find_if(state.heals.events, [](const auto& event)
    {
        return event.type == BattleHealEventType::Applied;
    });
    REQUIRE(applied != state.heals.events.end());
    CHECK(applied->request.sourceUnitId == 0);
    CHECK(applied->request.targetUnitId == 1);
    CHECK(applied->calculatedAmount == 140);
    CHECK(applied->appliedAmount == 140);
    CHECK(state.units.requireCore(1).vitals.hp == 540);
    CHECK(state.units.requireCore(2).vitals.hp == 300);
}

TEST_CASE("BattleRuntimeScenario_RealDivineFlickPlansBaseMeleeAsRangedOnlyForItsUltimate",
          "[battle][scenario][runtime][ultimate-effect][vertical][exact-runtime]")
{
    constexpr int DivineFlickMagicId = 18;
    constexpr int OtherMagicId = 19;

    SECTION("matching ultimate acquires and casts on a ranged target")
    {
        auto state = initializedVerticalSliceState(divineFlickInput(100));
        const auto& initialCaster = state.units.requireCore(0);
        CHECK(initialCaster.style == CombatStyle::Melee);
        CHECK(state.units.requireCore(1).motion.position.x
              - initialCaster.motion.position.x
              > state.movement.config.meleeAttackReach);

        BattleFrameRunner runner;
        const auto started = runner.runFrame(state);

        const auto& caster = state.units.requireCore(0);
        CHECK(caster.style == CombatStyle::Ranged);
        CHECK(caster.reach
              > state.units.requireCore(1).motion.position.x
                  - caster.motion.position.x);
        CHECK(caster.motion.position.x == 100.0f);
        const auto* pending = state.units.require(0).pendingCast();
        REQUIRE(pending != nullptr);
        CHECK(pending->targetUnitId == 1);
        CHECK(pending->effectCast.provenance.ultimate);
        CHECK(pending->operationType == BattleOperationType::RangedProjectile);
        CHECK(pending->effectCast.provenance.magicId == DivineFlickMagicId);
        CHECK(pending->skillPlan.forceRanged);
        CHECK(std::ranges::any_of(started.gameplayEvents, [](const auto& event)
        {
            return event.type == BattleGameplayEventType::CastStarted
                && event.sourceUnitId == 0
                && event.targetUnitId == 1;
        }));

        const auto released = runUntil(state, 60, [](const auto& runtime, const auto&)
        {
            return std::ranges::any_of(runtime.attacks.attacks, [](const auto& attack)
            {
                return attack.provenance.valid()
                    && attack.provenance.cast.magicId == DivineFlickMagicId;
            });
        });

        REQUIRE(released);
        const auto attack = std::ranges::find_if(
            state.attacks.attacks,
            [](const auto& candidate)
            {
                return candidate.provenance.valid()
                    && candidate.provenance.cast.magicId
                        == DivineFlickMagicId;
        });
        REQUIRE(attack != state.attacks.attacks.end());
        CHECK(attack->state.operationType
              == BattleOperationType::RangedProjectile);
        CHECK(attack->provenance.cast.ultimate);
        const auto castContext = state.effectIntegration.casts.find(
            attack->provenance.cast.castId);
        REQUIRE(castContext != state.effectIntegration.casts.end());
        CHECK(castContext->second.originalTargetUnitId == 1);
    }

    SECTION("a different ultimate does not inherit the bound magic rule")
    {
        auto state = initializedVerticalSliceState(divineFlickInput(100));
        auto plan = *state.units.require(0).actionPlan();
        plan.ultimateSkill.id = OtherMagicId;
        plan.ultimateSkill.visualEffectId = OtherMagicId;
        state.units.require(0).setActionPlan(std::move(plan));

        BattleFrameRunner runner;
        runner.runFrame(state);

        const auto& caster = state.units.requireCore(0);
        CHECK(caster.vitals.mp == caster.vitals.maxMp);
        CHECK(caster.style == CombatStyle::Melee);
        CHECK(caster.reach == state.movement.config.meleeAttackReach);
        CHECK(state.units.require(0).pendingCast() == nullptr);
    }

    SECTION("a normal cast of the same magic does not inherit its ultimate rule")
    {
        auto state = initializedVerticalSliceState(divineFlickInput(
            0,
            DivineFlickMagicId));

        BattleFrameRunner runner;
        runner.runFrame(state);

        const auto& caster = state.units.requireCore(0);
        CHECK(caster.vitals.mp < caster.vitals.maxMp);
        CHECK(caster.style == CombatStyle::Melee);
        CHECK(caster.reach == state.movement.config.meleeAttackReach);
        CHECK(state.units.require(0).pendingCast() == nullptr);
    }
}

TEST_CASE("BattleRuntimeScenario_RealSanqingHealsAlliesToppedUpByItsRecovery", "[battle][scenario][runtime][ultimate-effect][vertical]")
{
    auto input = singleUltimateInput(133, 3, 1, { 220, 100, 0 });
    input.units.push_back(verticalSliceUnit(2, 0, 400, 1000, 80, { 100, 180, 0 }));
    input.units.push_back(verticalSliceUnit(3, 0, 400, 1000, 20, { 100, 260, 0 }));
    auto state = initializedVerticalSliceState(std::move(input));
    REQUIRE(runUntil(state, 180, [](const auto& runtime, const auto&)
    {
        return runtime.units.requireCore(2).vitals.mp == 100;
    }));
    CHECK(state.units.requireCore(2).vitals.hp == 499);
    CHECK(state.units.requireCore(3).vitals.mp >= 40);
    CHECK(state.units.requireCore(3).vitals.mp < 100);
    CHECK(state.units.requireCore(3).vitals.hp == 400);
    CHECK(state.units.requireCore(2).shield == 0);
    CHECK(state.units.requireCore(3).shield == 0);
}

TEST_CASE("BattleRuntimeScenario_RealShenzhaoSpends75MpAndGrantsStarShield", "[battle][scenario][runtime][ultimate-effect][vertical]")
{
    auto state = initializedVerticalSliceState(singleUltimateInput(
        94,
        3,
        1,
        { 220, 100, 0 }));

    const auto committed = runUntil(state, 180, [](const auto& runtime, const auto&)
    {
        return runtime.units.requireCore(0).shield > 0;
    });

    CAPTURE(
        state.movement.frame,
        state.units.requireCore(0).vitals.hp,
        state.units.requireCore(0).vitals.mp,
        state.units.requireCore(0).shield,
        state.units.requireCore(0).haveAction,
        state.units.pendingCastCount(),
        state.result.ended,
        state.result.winningTeam);
    REQUIRE(committed);
    CHECK(state.units.requireCore(0).vitals.mp == 25);
    CHECK(state.units.requireCore(0).shield == 300);
}

TEST_CASE("BattleRuntimeScenario_RealWitheredBoneModifiesHitAndHealTransactions", "[battle][scenario][runtime][ultimate-effect][vertical]")
{
    const auto dealtDamage = [](const auto& frame)
    {
        return std::ranges::any_of(frame.logEvents, [](const auto& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.sourceUnitId == 0
                && event.targetUnitId == 1;
        });
    };

    BattleHealRequest heal;
    heal.sourceUnitId = 0;
    heal.targetUnitId = 1;
    heal.kind = BattleHealKind::Direct;
    heal.amount = fixedHealAmount(1000);
    auto input = singleUltimateInput(
        11,
        1,
        1,
        { 220, 100, 0 });
    input.units[1].vitals.hp = 5000;
    auto state = initializedVerticalSliceState(std::move(input));
    const auto hit = runUntil(state, 100, [&](const auto&, const auto& frame)
    {
        return dealtDamage(frame);
    });
    REQUIRE(hit);
    const auto damage = std::ranges::find_if(
        hit->logEvents,
        [](const auto& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.sourceUnitId == 0
                && event.targetUnitId == 1;
        });
    REQUIRE(damage != hit->logEvents.end());
    CHECK(damage->amount == 82);

    const auto status = BattleStatusSystem({}).snapshot(
        state.units.require(1).statusDamageState());
    CHECK(status.has(BattleStatusKind::WitheredBone));
    CHECK(status.damageTakenPct == 25);
    REQUIRE(status.healTransactionModifiers.size() == 1);
    CHECK(status.healTransactionModifiers.front().percent == 25);

    const auto healed = BattleHealSystem().commit(state, heal);
    CHECK(healed.calculatedAmount == 1000);
    CHECK(healed.modifiedAmount == 250);
    CHECK(healed.appliedAmount == 250);
}

TEST_CASE("BattleHealSystem_DispatchesStatusOnlyHealEventsAndHonorsContributionLiveness",
          "[battle][heal][status][liveness]")
{
    const auto stateWithBehavior = [](std::shared_ptr<const StatusBehaviorDefinition> behavior)
    {
        auto state = initializedVerticalSliceState(basicSessionInput());
        state.effectRules = {};
        auto& source = state.units.requireCore(0);
        auto& target = state.units.requireCore(1);
        source.vitals.hp = source.vitals.maxHp;
        target.vitals.hp = 50;
        target.vitals.maxHp = 100;
        target.vitals.mp = 0;
        target.vitals.maxMp = 100;

        const EffectSourceBinding binding{
            .kind = EffectSourceKind::Magic,
            .sourceId = 9300,
            .ownerUnitId = 1,
            .sourceTeam = target.team,
        };
        const EffectRuleId producerRuleId{ 9300 };
        auto& effects = state.units.require(1).status.effects;
        effects.statuses.push_back({
            .kind = BattleStatusKind::TrueQi,
            .producer = StatusProducerKey{ binding, producerRuleId, 0 },
            .producerFamily = StatusProducerFamilyKey{
                binding.kind,
                binding.sourceId,
                binding.ownerUnitId,
                producerRuleId,
                0,
            },
            .behavior = behavior,
            .behaviorRuntime = initialStatusBehaviorRuntime(behavior),
            .sourceUnitId = binding.ownerUnitId,
            .remainingFrames = 120,
            .maximumFrames = 120,
            .stacks = 1,
            .origin = BattleStatusEffectOrigin{ binding, producerRuleId, 0 },
            .appliedSequence = effects.nextStatusSequence++,
        });
        REQUIRE(state.effectRules.rules().empty());
        return state;
    };
    const auto directHeal = []
    {
        BattleHealRequest request;
        request.sourceUnitId = 0;
        request.targetUnitId = 1;
        request.kind = BattleHealKind::Direct;
        request.amount = fixedHealAmount(10);
        return request;
    };

    SECTION("heal-attempt modifier remains active without configured rules")
    {
        ModifyHealTransactionAction halve;
        halve.operation = HealModifierOperation::MultiplyReceived;
        halve.percent = 50;
        halve.kinds = { "直接" };
        EffectRule rule;
        rule.id = EffectRuleId{ 1 };
        rule.event = EffectEvent::HealAttempted;
        rule.observation = EffectObservationScope::StatusHolderEventTarget;
        rule.selector.kind = EffectSelectorKind::StatusHolder;
        rule.actions = { EffectAction{ halve } };
        auto behavior = std::make_shared<StatusBehaviorDefinition>();
        behavior->rules = { std::move(rule) };
        auto state = stateWithBehavior(std::move(behavior));

        const auto result = BattleHealSystem().commit(state, directHeal());

        CHECK(result.calculatedAmount == 10);
        CHECK(result.modifiedAmount == 5);
        CHECK(result.appliedAmount == 5);
    }

    SECTION("canonical all-kind status modifier matches every runtime heal kind")
    {
        ModifyHealTransactionAction block;
        block.operation = HealModifierOperation::Block;
        for (const auto& entry : effectHealKindCatalog)
            block.kinds.emplace_back(entry.authorLabel);
        EffectRule rule;
        rule.id = EffectRuleId{ 1 };
        rule.event = EffectEvent::HealAttempted;
        rule.observation = EffectObservationScope::StatusHolderEventTarget;
        rule.selector.kind = EffectSelectorKind::StatusHolder;
        rule.actions = { EffectAction{ block } };
        auto behavior = std::make_shared<StatusBehaviorDefinition>();
        behavior->rules = { std::move(rule) };

        for (std::size_t index = 0;
             index < static_cast<std::size_t>(BattleHealKind::Count);
             ++index)
        {
            const auto kind = static_cast<BattleHealKind>(index);
            CAPTURE(kind);
            auto state = stateWithBehavior(behavior);
            auto request = directHeal();
            request.kind = kind;

            const auto result = BattleHealSystem().commit(state, request);

            CHECK(result.modifiedAmount == 0);
            CHECK(result.appliedAmount == 0);
        }
    }

    SECTION("an earlier lifecycle action invalidates a later snapshotted modifier")
    {
        EffectRule consume;
        consume.id = EffectRuleId{ 1 };
        consume.event = EffectEvent::HealAttempted;
        consume.observation = EffectObservationScope::StatusHolderEventTarget;
        consume.selector.kind = EffectSelectorKind::StatusHolder;
        consume.actions = { EffectAction{ ConsumeThisStatusAction{} } };

        ModifyHealTransactionAction block;
        block.operation = HealModifierOperation::Block;
        block.kinds = { "直接" };
        EffectRule blockRule;
        blockRule.id = EffectRuleId{ 2 };
        blockRule.event = EffectEvent::HealAttempted;
        blockRule.observation = EffectObservationScope::StatusHolderEventTarget;
        blockRule.selector.kind = EffectSelectorKind::StatusHolder;
        blockRule.actions = { EffectAction{ block } };
        auto behavior = std::make_shared<StatusBehaviorDefinition>();
        behavior->rules = { std::move(consume), std::move(blockRule) };
        auto state = stateWithBehavior(std::move(behavior));

        const auto result = BattleHealSystem().commit(state, directHeal());

        CHECK(result.modifiedAmount == 10);
        CHECK(result.appliedAmount == 10);
        CHECK_FALSE(state.units.require(1).status.effects.has(BattleStatusKind::TrueQi));
    }

    SECTION("heal-applied actions are queued without configured rules")
    {
        ChangeResourceAction restoreMp;
        restoreMp.resource = BattleResource::Mp;
        restoreMp.kind = ResourceChangeKind::Restore;
        restoreMp.amount.flat = 7;
        EffectRule rule;
        rule.id = EffectRuleId{ 1 };
        rule.event = EffectEvent::HealApplied;
        rule.observation = EffectObservationScope::StatusHolderEventTarget;
        rule.selector.kind = EffectSelectorKind::StatusHolder;
        rule.actions = { EffectAction{ restoreMp } };
        auto behavior = std::make_shared<StatusBehaviorDefinition>();
        behavior->rules = { std::move(rule) };
        auto state = stateWithBehavior(std::move(behavior));

        const auto result = BattleHealSystem().commit(state, directHeal());
        REQUIRE(result.appliedAmount == 10);
        REQUIRE(state.effectIntegration.queuedCommandBatches.size() == 1);
        REQUIRE(state.effectIntegration.queuedCommandBatches.front().commands.size() == 1);
        CHECK(std::holds_alternative<ChangeResourceEffectCommand>(
            state.effectIntegration.queuedCommandBatches.front().commands.front().value));

        BattleFrameRunner().runFrame(state);
        // 此幀先執行正常的 1 點回魔，再歸約 HealApplied 的 7 點回復。
        CHECK(state.units.requireCore(1).vitals.mp == 8);
    }
}

TEST_CASE("BattleRuntimeScenario_RealFiveTigerSpawnsFanAndDebuffsDefence", "[battle][scenario][runtime][ultimate-effect][vertical]")
{
    auto state = initializedVerticalSliceState(singleUltimateInput(
        59,
        1,
        3,
        { 220, 100, 0 }));

    std::size_t maximumFiveTigerAttacks{};
    const auto spawned = runUntil(state, 180, [&](const auto& runtime, const auto&)
    {
        const auto count = static_cast<std::size_t>(std::ranges::count_if(
            runtime.attacks.attacks,
            [](const auto& attack)
        {
            return attack.state.skillId == 59;
        }));
        maximumFiveTigerAttacks = std::max(maximumFiveTigerAttacks, count);
        return count >= 5;
    });
    CAPTURE(
        state.movement.frame,
        state.units.requireCore(0).vitals.hp,
        state.units.requireCore(0).vitals.mp,
        state.units.requireCore(0).haveAction,
        state.units.pendingCastCount(),
        state.attacks.attacks.size(),
        state.attacks.nextAttackId,
        state.effectIntegration.nextSharedHitGroupId,
        maximumFiveTigerAttacks,
        state.result.ended,
        state.result.winningTeam);
    REQUIRE(spawned);

    std::vector<const BattleAttackInstance*> fan;
    for (const auto& attack : state.attacks.attacks)
    {
        if (attack.state.skillId == 59)
        {
            fan.push_back(&attack);
        }
    }
    REQUIRE(fan.size() == 5);
    CHECK(std::ranges::all_of(fan, [](const auto* attack)
    {
        return attack->state.through
            && attack->provenance.mainProjectile
            && attack->state.strengthPct == 60;
    }));
    REQUIRE(fan.front()->provenance.sharedHitGroupId > 0);
    CHECK(std::ranges::all_of(fan, [&](const auto* attack)
    {
        return attack->provenance.sharedHitGroupId
            == fan.front()->provenance.sharedHitGroupId;
    }));
    std::vector<float> verticalVelocities;
    for (const auto* attack : fan)
    {
        verticalVelocities.push_back(attack->state.velocity.y);
    }
    std::ranges::sort(verticalVelocities);
    CHECK(std::ranges::adjacent_find(verticalVelocities)
          == verticalVelocities.end());

    const auto debuffed = runUntil(state, 100, [](const auto& runtime, const auto&)
    {
        return std::ranges::any_of(
            runtime.effectCommands.attributeModifiers,
            [](const auto& modifier)
            {
                return modifier.targetUnitId == 1
                    && modifier.attribute == BattleAttribute::Defence;
            });
    });
    REQUIRE(debuffed);
    const auto& modifier = *std::ranges::find_if(
        state.effectCommands.attributeModifiers,
        [](const auto& candidate)
        {
            return candidate.targetUnitId == 1
                && candidate.attribute == BattleAttribute::Defence;
        });
    CHECK(modifier.operation == AttributeOperation::PercentAdd);
    CHECK(modifier.amount == -30);
    CHECK(modifier.expiresFrameExclusive
          == modifier.appliedFrame + 90);
    CHECK(BattleEffectCommandSystem::queryAttribute(state, {
        .unitId = 1,
        .attribute = BattleAttribute::Defence,
        .baseValue = 100,
        .frame = state.movement.frame,
    }) == 70);
}

TEST_CASE("BattleRuntimeScenario_RealSunflowerSpawnsAfterimagesWithTheRootAttack", "[battle][scenario][runtime][ultimate-effect][vertical]")
{
    auto input = singleUltimateInput(
        105,
        1,
        1,
        { 300, 100, 0 });
    input.units.push_back(verticalSliceUnit(
        2, 1, 10000, 10000, 0, { 320, 40, 0 }));
    input.units.push_back(verticalSliceUnit(
        3, 1, 10000, 10000, 0, { 320, 160, 0 }));
    auto state = initializedVerticalSliceState(std::move(input));

    const auto spawned = runUntil(state, 180, [](const auto&, const auto& frame)
    {
        return std::ranges::any_of(frame.gameplayEvents, [](const auto& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.sourceUnitId == 0;
        });
    });

    REQUIRE(spawned);
    CHECK(state.nextFrame.queuedAttacks().empty());

    std::vector<const BattleAttackInstance*> attacks;
    for (const auto& attack : state.attacks.attacks)
    {
        if (attack.state.attackSourceUnitId == 0
            && attack.state.skillId == 105)
        {
            attacks.push_back(&attack);
        }
    }
    const auto root = std::ranges::find_if(attacks, [](const auto* attack)
    {
        return attack->provenance.rootAttack;
    });
    REQUIRE(root != attacks.end());
    CHECK((*root)->provenance.mainProjectile);
    CHECK((*root)->provenance.propagation == CastPropagationPolicy::SourceRules);
    CHECK((*root)->state.strengthPct == 100);
    std::vector<const BattleAttackInstance*> echoes;
    std::ranges::copy_if(attacks, std::back_inserter(echoes), [](const auto* attack)
    {
        return attack->provenance.origin == BattleAttackOriginKind::Echo
            && !attack->provenance.mainProjectile
            && attack->state.strengthPct == 50;
    });
    REQUIRE(echoes.size() == 2);
    std::vector<int> echoTargets;
    for (const auto* echo : echoes)
    {
        echoTargets.push_back(echo->state.preferredTargetUnitId);
    }
    std::ranges::sort(echoTargets);
    CHECK(echoTargets == std::vector<int>{ 2, 3 });
    CHECK(std::ranges::all_of(echoes, [&](const auto* echo)
    {
        return echo->provenance.parentAttackId
                == (*root)->provenance.attackId
            && echo->provenance.propagation
                == CastPropagationPolicy::NoEffectRules;
    }));
    CHECK(std::ranges::any_of(spawned->gameplayEvents, [&](const auto& event)
    {
        return event.type == BattleGameplayEventType::AttackSpawned
            && event.effectId == (*root)->id;
    }));
    CHECK(std::ranges::all_of(echoes, [&](const auto* echo)
    {
        return std::ranges::any_of(spawned->gameplayEvents, [&](const auto& event)
        {
            return event.type == BattleGameplayEventType::AttackSpawned
                && event.effectId == echo->id;
        });
    }));
}

TEST_CASE("BattleRuntimeScenario_CopyAttackCastsSelectedUltimateOnceWithoutMpOrRuleRecursion", "[battle][scenario][runtime][ultimate-effect][copy]")
{
    auto state = initializedVerticalSliceState(copyRuntimeInput());
    auto baselineInput = copyRuntimeInput();
    baselineInput.setup.magicEffectDefinitions.front().rules.erase(
        baselineInput.setup.magicEffectDefinitions.front().rules.begin());
    auto baseline = initializedVerticalSliceState(std::move(baselineInput));
    const auto spawned = runUntil(state, 180, [](const auto& runtime, const auto&)
    {
        return copiedRuntimeAttack(runtime) != nullptr;
    });

    CAPTURE(
        state.movement.frame,
        state.units.requireCore(0).vitals.mp,
        state.units.pendingCastCount(),
        state.attacks.attacks.size(),
        state.attacks.nextAttackId,
        state.result.ended,
        state.result.winningTeam);
    REQUIRE(spawned);

    const auto copiedCount = std::ranges::count_if(
        state.attacks.attacks,
        [](const BattleAttackInstance& attack)
        {
            return attack.provenance.valid()
                && attack.provenance.cast.origin
                    == CastOriginKind::CopiedAttack;
        });
    REQUIRE(copiedCount > 0);
    const auto& copied = *copiedRuntimeAttack(state);
    CHECK(std::ranges::all_of(
        state.attacks.attacks,
        [&](const BattleAttackInstance& attack)
        {
            return attack.provenance.cast.origin
                    != CastOriginKind::CopiedAttack
                || attack.provenance.cast.castId
                    == copied.provenance.cast.castId;
        }));
    CHECK(std::ranges::count_if(
        state.attacks.attacks,
        [](const BattleAttackInstance& attack)
        {
            return attack.provenance.cast.origin
                    == CastOriginKind::CopiedAttack
                && attack.provenance.rootAttack;
        }) == 1);
    CHECK(copied.state.attackSourceUnitId == 0);
    CHECK(copied.state.skillId == CopyRuntimeCandidateMagicId);
    CHECK(copied.state.skillName == "候選者絕招");
    CHECK(copied.state.visualEffectId == 9102);
    CHECK(copied.state.skillMagicPower == 140);
    CHECK(copied.provenance.cast.ultimate);
    CHECK(state.units.requireCore(CopyRuntimeCandidateUnitId).alive);
    CHECK(state.units.requireCore(0).shield == 7);

    BattleFrameRunner baselineRunner;
    while (baseline.movement.frame < state.movement.frame)
    {
        baselineRunner.runFrame(baseline);
    }
    CHECK(state.units.requireCore(0).vitals.mp
          == baseline.units.requireCore(0).vitals.mp);
    CHECK(state.units.requireCore(0).vitals.mp < 100);

    const auto& child = copied.provenance;
    CHECK(child.cast.sourceUnitId == 0);
    CHECK(child.cast.magicId == CopyRuntimeCandidateMagicId);
    CHECK(child.cast.ultimate);
    CHECK(child.cast.origin == CastOriginKind::CopiedAttack);
    CHECK(child.cast.propagation
          == CastPropagationPolicy::SuppressUltimateRules);
    CHECK(child.propagation
          == CastPropagationPolicy::SuppressUltimateRules);
    CHECK(child.rootAttack);
    REQUIRE(child.cast.parentCastId);
    CHECK(child.cast.rootCastId == *child.cast.parentCastId);
    CHECK(child.cast.castId != child.cast.rootCastId);

    const auto& parent = state.castLifecycle.runtime(
        *child.cast.parentCastId).provenance;
    CHECK(parent.castId == child.cast.rootCastId);
    CHECK(parent.rootCastId == parent.castId);
    CHECK_FALSE(parent.parentCastId);
    CHECK(parent.sourceUnitId == 0);
    CHECK(parent.magicId == CopyRuntimeMagicId);
    CHECK(parent.origin == CastOriginKind::Ultimate);
    CHECK(parent.propagation == CastPropagationPolicy::SourceRules);

    const auto childCastId = child.cast.castId;
    const auto parentCastId = parent.castId;
    const auto childContext = state.effectIntegration.casts.find(childCastId);
    REQUIRE(childContext != state.effectIntegration.casts.end());
    CHECK(childContext->second.originalTargetUnitId
          == CopyRuntimeOriginalTargetUnitId);
    CHECK(state.castLifecycle.containsCast(childCastId));
    CHECK(state.castLifecycle.containsCast(parentCastId));
    CHECK(state.castLifecycle.outstandingWork(childCastId) > 0);
    CHECK(state.castLifecycle.outstandingWork(parentCastId) > 0);

    auto& copierStatus = state.units.require(0).status.effects;
    copierStatus.setFrames(BattleStatusKind::Stun, 1000, 1000);
    const auto settled = runUntil(state, 180, [&](const auto& runtime, const auto&)
    {
        return !runtime.castLifecycle.containsCast(childCastId)
            && !runtime.castLifecycle.containsCast(parentCastId);
    });
    REQUIRE(settled);
    CHECK_FALSE(state.effectIntegration.casts.contains(childCastId));
    CHECK_FALSE(state.effectIntegration.casts.contains(parentCastId));
    CHECK(state.units.requireCore(0).shield == 7);

    // Parent and copied child are the only casts allocated by this scenario.
    // A leaked or recursively allocated child would advance the next ID.
    const auto nextRoot = state.castLifecycle.beginRootCast({
        .sourceUnitId = 0,
        .magicId = 9999,
    });
    CHECK(nextRoot.provenance.castId == BattleCastId{ 3 });
}

TEST_CASE("BattleRuntimeScenario_CopyAttackUsesTheParentRetargetAfterWindupTargetDeath", "[battle][scenario][runtime][ultimate-effect][copy]")
{
    auto input = copyRuntimeInput(true, true, true, true);
    const auto original = std::ranges::find(
        input.units,
        CopyRuntimeOriginalTargetUnitId,
        &BattleSetupUnitInput::unitId);
    REQUIRE(original != input.units.end());
    original->vitals.hp = 10;
    auto state = initializedVerticalSliceState(std::move(input));
    state.nextFrame.queueDamage(scenarioPreResolvedDamage(
        CopyRuntimeCandidateUnitId,
        CopyRuntimeOriginalTargetUnitId,
        10));

    BattleFrameRunner firstFrameRunner;
    firstFrameRunner.runFrame(state);
    REQUIRE_FALSE(state.units.requireCore(
        CopyRuntimeOriginalTargetUnitId).alive);
    const auto* pending = state.units.require(0).pendingCast();
    REQUIRE(pending != nullptr);
    CHECK(pending->targetUnitId == CopyRuntimeOriginalTargetUnitId);

    const auto spawned = runUntil(state, 180, [](const auto& runtime, const auto&)
    {
        return copiedRuntimeAttack(runtime) != nullptr;
    });
    REQUIRE(spawned);
    const auto& copied = *copiedRuntimeAttack(state);
    CHECK(state.units.requireCore(CopyRuntimeFallbackTargetUnitId).alive);
    CHECK(state.units.requireCore(CopyRuntimeFallbackTargetUnitId).team
          != state.units.requireCore(0).team);
    REQUIRE(copied.provenance.cast.parentCastId);
    const auto childContext = state.effectIntegration.casts.find(
        copied.provenance.cast.castId);
    REQUIRE(childContext != state.effectIntegration.casts.end());
    CHECK(childContext->second.originalTargetUnitId
          == CopyRuntimeFallbackTargetUnitId);
    const auto parentContext = state.effectIntegration.casts.find(
        *copied.provenance.cast.parentCastId);
    REQUIRE(parentContext != state.effectIntegration.casts.end());
    CHECK(parentContext->second.originalTargetUnitId
          == CopyRuntimeFallbackTargetUnitId);
}

TEST_CASE("BattleRuntimeScenario_CopyAttackDoesNotLeakLifecycleWhenWindupLosesEveryTarget", "[battle][scenario][runtime][ultimate-effect][copy]")
{
    auto input = copyRuntimeInput();
    const auto original = std::ranges::find(
        input.units,
        CopyRuntimeOriginalTargetUnitId,
        &BattleSetupUnitInput::unitId);
    REQUIRE(original != input.units.end());
    original->vitals.hp = 10;
    auto state = initializedVerticalSliceState(std::move(input));
    state.nextFrame.queueDamage(scenarioPreResolvedDamage(
        CopyRuntimeCandidateUnitId,
        CopyRuntimeOriginalTargetUnitId,
        10));

    BattleFrameRunner runner;
    runner.runFrame(state);
    REQUIRE_FALSE(state.units.requireCore(
        CopyRuntimeOriginalTargetUnitId).alive);
    CHECK(state.result.ended);
    CHECK(state.result.winningTeam == 0);
    CHECK(state.units.require(0).pendingCast() == nullptr);

    const BattleCastId cancelledCastId{ 1 };
    CHECK(copiedRuntimeAttack(state) == nullptr);
    CHECK(state.nextFrame.queuedAttacks().empty());
    CHECK(state.effectIntegration.casts.empty());
    CHECK(state.castLifecycle.activeCastCount() == 0);
    CHECK(state.castLifecycle.trackedWorkCount() == 0);
    CHECK(state.effectRules.castScopedRuleCount(cancelledCastId) == 0);
    const auto snapshot = state.castLifecycle.snapshot();
    CHECK(snapshot.terminalState == BattleCastLifecycleTerminalState::BattleEnded);
    CHECK(snapshot.nextCastId == 2);
    REQUIRE(snapshot.retiredCasts.size() == 1);
    CHECK(snapshot.retiredCasts.front().provenance.castId == cancelledCastId);
    CHECK(snapshot.retiredCasts.front().terminalReason
          == BattleCastTerminalReason::BattleEnded);
}

TEST_CASE("BattleRuntimeScenario_CopyAttackSkipsEmptyAndNoUltimateSources", "[battle][scenario][runtime][ultimate-effect][copy]")
{
    const auto checkNoChild = [](BattleRuntimeSessionCreationInput input)
    {
        auto state = initializedVerticalSliceState(std::move(input));
        const auto rootSpawned = runUntil(state, 180, [](const auto& runtime, const auto&)
        {
            return std::ranges::any_of(
                runtime.attacks.attacks,
                [](const BattleAttackInstance& attack)
                {
                    return attack.provenance.valid()
                        && attack.provenance.cast.magicId
                            == CopyRuntimeMagicId
                        && attack.provenance.cast.origin
                            == CastOriginKind::Ultimate;
                });
        });
        REQUIRE(rootSpawned);
        CHECK(copiedRuntimeAttack(state) == nullptr);
        const auto root = std::ranges::find_if(
            state.attacks.attacks,
            [](const BattleAttackInstance& attack)
            {
                return attack.provenance.valid()
                    && attack.provenance.cast.magicId
                        == CopyRuntimeMagicId
                    && attack.provenance.cast.origin
                        == CastOriginKind::Ultimate;
            });
        REQUIRE(root != state.attacks.attacks.end());
        const auto rootCastId = root->provenance.cast.castId;

        auto& status = state.units.require(0).status.effects;
        status.setFrames(BattleStatusKind::Stun, 1000, 1000);
        const auto settled = runUntil(state, 180, [&](const auto& runtime, const auto&)
        {
            return !runtime.castLifecycle.containsCast(rootCastId);
        });
        REQUIRE(settled);
        CHECK_FALSE(state.effectIntegration.casts.contains(rootCastId));
        CHECK(state.units.requireCore(0).shield == 7);

        const auto nextRoot = state.castLifecycle.beginRootCast({
            .sourceUnitId = 0,
            .magicId = 9999,
        });
        CHECK(nextRoot.provenance.castId == BattleCastId{ 2 });
    };

    SECTION("selector has no other living ally")
    {
        checkNoChild(copyRuntimeInput(
            true,
            true,
            true,
            false,
            false,
            false));
    }

    SECTION("selected living ally has no ultimate action definition")
    {
        checkNoChild(copyRuntimeInput(true, false));
    }
}

TEST_CASE("BattleRuntimeScenario_AutoUltimateQueuesOneFreeCopiedChild", "[battle][scenario][runtime][ultimate-effect][copy][auto-ultimate]")
{
    const auto configureAutoUltimate = [](BattleRuntimeState& runtime)
    {
        runtime.units.requireCore(0).vitals.mp = 37;
        ModifyCastAction autoUltimate;
        autoUltimate.autoUltimate = AutoUltimateCastRequest{
            .consumeMp = false,
            .announce = true,
        };
        EffectRule rule;
        rule.id = EffectRuleId{ 1 };
        rule.event = EffectEvent::FrameAdvanced;
        rule.selector.kind = EffectSelectorKind::Self;
        rule.maxActivations = 1;
        rule.intervalFrames = 1;
        rule.actions = { { EffectActionValue{ autoUltimate } } };
        runtime.effectRules.append({
            .kind = EffectSourceKind::Combo,
            .sourceId = 9001,
            .ownerUnitId = 0,
            .sourceTeam = runtime.units.requireCore(0).team,
        }, rule);
    };

    auto state = initializedVerticalSliceState(copyRuntimeInput(
        true,
        true,
        true,
        false,
        true));
    configureAutoUltimate(state);
    BattleFrameRunner autoRunner;
    const auto committed = autoRunner.runFrame(state);
    const bool announced = std::ranges::any_of(
        committed.logEvents,
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Status
                && BattleLogTest::textOf(event)
                    == "自動絕招·複製者原招";
        });
    state.units.require(0).comboFacts = {};
    const auto spawned = runUntil(state, 20, [](const auto& runtime, const auto&)
    {
        return copiedRuntimeAttack(runtime) != nullptr;
    });
    REQUIRE(spawned);
    CHECK(announced);

    auto controlInput = copyRuntimeInput(
        true,
        true,
        true,
        false,
        true);
    controlInput.setup.magicEffectDefinitions.front().rules.erase(
        controlInput.setup.magicEffectDefinitions.front().rules.begin());
    auto control = initializedVerticalSliceState(std::move(controlInput));
    configureAutoUltimate(control);
    BattleFrameRunner controlRunner;
    controlRunner.runFrame(control);
    control.units.require(0).comboFacts = {};
    while (control.movement.frame < state.movement.frame)
    {
        controlRunner.runFrame(control);
    }

    CHECK(state.units.requireCore(0).vitals.mp
          == control.units.requireCore(0).vitals.mp);
    CHECK(state.units.requireCore(0).shield == 7);
    const auto copiedCount = std::ranges::count_if(
        state.attacks.attacks,
        [](const BattleAttackInstance& attack)
        {
            return attack.provenance.valid()
                && attack.provenance.cast.origin
                    == CastOriginKind::CopiedAttack;
        });
    REQUIRE(copiedCount > 0);
    const auto& provenance = copiedRuntimeAttack(state)->provenance.cast;
    CHECK(std::ranges::all_of(
        state.attacks.attacks,
        [&](const BattleAttackInstance& attack)
        {
            return attack.provenance.cast.origin
                    != CastOriginKind::CopiedAttack
                || attack.provenance.cast.castId
                    == provenance.castId;
        }));
    CHECK(std::ranges::count_if(
        state.attacks.attacks,
        [](const BattleAttackInstance& attack)
        {
            return attack.provenance.cast.origin
                    == CastOriginKind::CopiedAttack
                && attack.provenance.rootAttack;
        }) == 1);
    CHECK(provenance.origin == CastOriginKind::CopiedAttack);
    CHECK(provenance.propagation
          == CastPropagationPolicy::SuppressUltimateRules);
    REQUIRE(provenance.parentCastId);
    CHECK(provenance.rootCastId == *provenance.parentCastId);

    const auto nextRoot = state.castLifecycle.beginRootCast({
        .sourceUnitId = 0,
        .magicId = 9999,
    });
    CHECK(nextRoot.provenance.castId == BattleCastId{ 3 });
}
