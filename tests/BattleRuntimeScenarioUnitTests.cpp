#include "BattleRuntimeScenarioTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <utility>
#include <vector>

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
    input.comboStates.emplace(0, KysChess::RoleComboState{});
    input.comboStates.emplace(1, KysChess::RoleComboState{});
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
    attacker.hasEquippedSkill = true;
    attacker.normalSkill = scenarioRangedSkill();
    attacker.ultimateSkill.id = -1;
    input.units.push_back(attacker);
    input.actionPlanSeeds.push_back({
        attacker.unitId,
        true,
        attacker.normalSkill,
        attacker.ultimateSkill,
    });

    input.units.push_back(scenarioSetupUnit(1, 1, 100, { 180, 100, 0 }));
    input.comboStates.emplace(0, KysChess::RoleComboState{});
    input.comboStates.emplace(1, KysChess::RoleComboState{});
    return input;
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

TEST_CASE("BattleRuntimeScenario_DamageDeathEffectsAndFollowUpDigest", "[battle][scenario][runtime]")
{
    BattleRuntimeState state;
    seedScenarioRuntimeStores(state, {
        scenarioRuntimeUnit(0, 0, 100, { 100, 100, 0 }),
        scenarioRuntimeUnit(1, 1, 10, { 120, 100, 0 }),
        scenarioRuntimeUnit(2, 1, 50, { 140, 100, 0 }),
    });
    state.unitStore.requireUnit(2).stats.attack = 10;
    state.damage.pendingDamage.push_back(scenarioPreResolvedDamage(0, 1, 10));
    state.damage.unitEffects[1] = { 50, 6, 1 };
    state.projectileFollowUps.projectileSpeed = ScenarioTileWidth / 3.0;
    state.projectileFollowUps.minimumProjectileFrames = 20;
    state.projectileFollowUps.areaProjectileFramePadding = 15;
    state.projectileFollowUps.areaSpawnDistance = ScenarioTileWidth;

    BattleDeathEffectExtras dead;
    dead.id = 1;
    dead.comboIds = { 9 };
    dead.appliedEffects.push_back(
        { KysChess::EffectType::DeathMedical, 20, 0, "", KysChess::Trigger::Always, 0, 0, 0, 9 });

    BattleDeathEffectExtras ally;
    ally.id = 2;
    ally.shieldPctMaxHp = 30;
    ally.comboIds = { 9 };
    ally.appliedEffects.push_back(
        { KysChess::EffectType::AllyDeathStatBoost, 4, 0, "", KysChess::Trigger::Always, 0, 0, 0, 9 });
    ally.appliedEffects.push_back(
        { KysChess::EffectType::ShieldOnAllyDeath, 1, 0, "", KysChess::Trigger::Always, 0, 0, 0, 9 });

    state.deathEffects.store.units = { { 0 }, dead, ally };
    state.deathEffects.store.regularSynergyComboIds.insert(9);

    BattleRuntimeSession session(std::move(state));

    auto first = session.runFrame();
    auto firstDigest = digestScenarioFrame(session.runtime(), first);

    CHECK(firstDigest.aliveUnitIds == std::vector<int>{ 0, 2 });
    CHECK(firstDigest.hpByUnitId.at(1) == 0);
    CHECK(firstDigest.hpByUnitId.at(2) == 70);
    CHECK(firstDigest.shieldByUnitId.at(2) == 30);
    CHECK(firstDigest.pendingAttackSpawnCount == 1);
    CHECK(firstDigest.damageDefenderIds == std::vector<int>{ 1 });
    CHECK(firstDigest.committedHpDamage == std::vector<int>{ 10 });
    CHECK(digestHasLogText(firstDigest, "同袍之死（攻防+4）"));
    CHECK(digestHasLogText(firstDigest, "死亡醫療"));
    CHECK(digestHasLogText(firstDigest, "護盾重獲（30護盾）"));

    auto second = session.runFrame();
    auto secondDigest = digestScenarioFrame(session.runtime(), second);

    CHECK(secondDigest.hpByUnitId.at(0) == 50);
    CHECK(secondDigest.damageDefenderIds == std::vector<int>{ 0 });
    CHECK(secondDigest.committedHpDamage == std::vector<int>{ 50 });
    CHECK(secondDigest.pendingAttackSpawnCount == 0);
    CHECK_FALSE(secondDigest.battleEnded);
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
    seedScenarioRuntimeStores(state, {
        scenarioRuntimeUnit(0, 0, 100, { 100, 100, 0 }),
        scenarioRuntimeUnit(1, 1, 100, { 900, 900, 0 }),
    });
    state.unitStore.requireUnit(0).style = CombatStyle::Ranged;
    state.unitStore.requireUnit(1).style = CombatStyle::Ranged;
    state.attacks.attacks.push_back(scenarioCancelProjectile(10, 0, 25));
    state.attacks.attacks.push_back(scenarioCancelProjectile(20, 1, 12));

    BattleRuntimeSession session(std::move(state));

    auto result = session.runFrame();
    auto digest = digestScenarioFrame(session.runtime(), result);

    CHECK(digest.attackTypes == std::vector<BattleAttackEventType>{
        BattleAttackEventType::Moved,
        BattleAttackEventType::Moved,
        BattleAttackEventType::ProjectileCancel,
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

TEST_CASE("BattleRuntimeScenario_DeathRescueDigest", "[battle][scenario][runtime]")
{
    BattleRuntimeState state;
    seedScenarioRuntimeStores(state, {
        scenarioRuntimeUnit(0, 0, 100, { 100, 100, 0 }),
        scenarioRuntimeUnit(1, 1, 50, { 180, 180, 0 }),
        scenarioRuntimeUnit(2, 1, 100, { 72, 72, 0 }),
    });
    state.damage.pendingDamage.push_back(scenarioPreResolvedDamage(0, 1, 30));
    state.unitStore.requireUnit(0).grid = { 10, 10 };
    state.unitStore.requireUnit(1).grid = { 5, 5 };
    state.unitStore.requireUnit(2).grid = { 3, 2 };
    state.combo.units[2].forcePullProtect = true;
    state.combo.units[2].forcePullProtectRemaining = 1;
    state.rescue.cells = {
        scenarioRescueCell(2, 3),
        scenarioRescueCell(3, 2),
        scenarioRescueCell(5, 5),
    };

    BattleRuntimeSession session(std::move(state));

    auto result = session.runFrame();
    auto digest = digestScenarioFrame(session.runtime(), result);

    CHECK(digest.damageDefenderIds == std::vector<int>{ 1 });
    CHECK(digest.committedHpDamage == std::vector<int>{ 30 });
    CHECK(digest.rescueResultCount == 1);
    CHECK(digest.rescuePulledUnitIds == std::vector<int>{ 1 });
    CHECK(digest.rescuePullerUnitIds == std::vector<int>{ 2 });
    REQUIRE(digest.rescueDestinationCells.size() == 1);
    CHECK(digest.rescueDestinationCells.front().x == 2);
    CHECK(digest.rescueDestinationCells.front().y == 3);
    CHECK(digest.hpByUnitId.at(1) == 30);
    CHECK(session.runtime().unitStore.requireUnit(1).invincible == 10);
    CHECK(session.runtime().combo.units.at(2).forcePullProtectRemaining == 0);
}
