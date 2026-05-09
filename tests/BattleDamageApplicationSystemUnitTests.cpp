#include "battle/BattleDamageApplicationSystem.h"
#include "battle/BattleCore.h"

#include <catch2/catch_test_macros.hpp>

#include <variant>

using namespace KysChess::Battle;
using KysChess::EffectType;
using KysChess::Trigger;

namespace
{

BattleDamageTransactionInput damageInput(int attackerUnitId, int defenderUnitId, int damage)
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = attackerUnitId;
    input.request.defenderUnitId = defenderUnitId;
    input.request.baseDamage = damage;
    input.request.preResolvedDamage = true;
    input.attacker.id = attackerUnitId;
    input.attacker.alive = true;
    input.attacker.hp = 40;
    input.attacker.maxHp = 100;
    input.attacker.attack = 12;
    input.defender.id = defenderUnitId;
    input.defender.alive = true;
    input.defender.hp = 10;
    input.defender.maxHp = 100;
    input.defenderStatus.id = defenderUnitId;
    input.defenderStatus.alive = true;
    input.defenderStatus.hp = 10;
    input.defenderStatus.maxHp = 100;
    return input;
}

BattleRuntimeUnit runtimeUnit(int id, int team, int hp, int maxHp, int attack)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = hp > 0;
    unit.hp = hp;
    unit.maxHp = maxHp;
    unit.attack = attack;
    return unit;
}

struct BattleDamageApplicationTestFrame
{
    BattleDamageApplicationInput input;
    BattleUnitStore runtimeUnits;
    BattleDeathEffectStore deathEffects;
    std::vector<BattleDamageTransactionInput> pendingTransactions;
    std::vector<BattleDamagePresentationInput> pendingPresentation;
    std::map<int, BattleDamageApplicationUnitEffects> unitEffects;
    std::map<int, int> pendingAliveByTeam;
    BattleProjectileFollowUpContext projectileFollowUps;

    BattleDamageApplicationTestFrame()
    {
        input.frame = 77;
        input.units = {
            { 1, 0, true },
            { 2, 1, true },
        };
        BattleRuntimeUnit attacker;
        attacker.id = 1;
        attacker.team = 0;
        attacker.alive = true;
        attacker.hp = 40;
        attacker.maxHp = 100;
        attacker.position = { 0.0f, 0.0f, 0.0f };
        attacker.grid = { 0, 0 };
        BattleRuntimeUnit defender;
        defender.id = 2;
        defender.team = 1;
        defender.alive = true;
        defender.hp = 10;
        defender.maxHp = 100;
        defender.position = { 36.0f, 0.0f, 0.0f };
        defender.grid = { 1, 0 };
        runtimeUnits.units = { attacker, defender };
        BattleDeathEffectExtras attackerEffects;
        attackerEffects.id = 1;
        BattleDeathEffectExtras defenderEffects;
        defenderEffects.id = 2;
        deathEffects.units = { attackerEffects, defenderEffects };
        input.deathEffectUnits = &runtimeUnits;
        input.deathEffects = &deathEffects;
        input.pendingTransactions = &pendingTransactions;
        input.pendingPresentation = &pendingPresentation;
        input.unitEffects = &unitEffects;
        input.pendingAliveByTeam = &pendingAliveByTeam;
        input.projectileFollowUps = &projectileFollowUps;
        input.projectileFollowUpUnits = &runtimeUnits;
        projectileFollowUps.projectileSpeed = 12.0;
    }
};

BattleDamageApplicationTestFrame applicationInput()
{
    return BattleDamageApplicationTestFrame{};
}

const BattleDamageLifecycleEvent* findLifecycleEvent(
    const BattleDamageApplicationResult& result,
    BattleDamageLifecycleEventType type)
{
    for (const auto& event : result.lifecycleEvents)
    {
        if (event.type == type)
        {
            return &event;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("BattleDamageApplication_FatalDamageEmitsDeathAndKillRewardEvents", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    auto damage = damageInput(1, 2, 20);
    damage.attacker.killHealPct = 25;
    damage.attacker.bloodlustAttackPerKill = 7;
    frame.pendingTransactions.push_back(damage);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    CHECK(result.transactions[0].killed);
    REQUIRE(findLifecycleEvent(result, BattleDamageLifecycleEventType::UnitDied));
    CHECK(findLifecycleEvent(result, BattleDamageLifecycleEventType::UnitDied)->targetUnitId == 2);
    REQUIRE(findLifecycleEvent(result, BattleDamageLifecycleEventType::KillRecorded));
    CHECK(findLifecycleEvent(result, BattleDamageLifecycleEventType::KillRecorded)->sourceUnitId == 1);
    CHECK(result.transactions[0].attacker.hp == 65);
    CHECK(result.transactions[0].attacker.attack == 19);
    REQUIRE(result.gameplayEvents.size() >= 1);
    CHECK(result.gameplayEvents[0].type == BattleGameplayEventType::UnitDied);
    CHECK(result.gameplayEvents[0].frame == 77);
    CHECK(result.gameplayEvents[0].sourceUnitId == 1);
    CHECK(result.gameplayEvents[0].targetUnitId == 2);
}

TEST_CASE("BattleDamageApplication_AggregatesPendingDamageByDefenderWhenRequested", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.aggregatePendingTransactionsByDefender = true;
    input.units.push_back({ 3, 0, true });
    input.deathEffectUnits->units.push_back(runtimeUnit(3, 0, 80, 100, 0));

    auto first = damageInput(1, 2, 3);
    auto second = damageInput(3, 2, 4);
    second.attacker.id = 3;
    second.attacker.hp = 80;
    second.attacker.maxHp = 100;
    frame.pendingTransactions.push_back(first);
    frame.pendingTransactions.push_back(second);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    CHECK(result.transactions[0].attacker.id == 3);
    CHECK(result.transactions[0].defender.hp == 3);
}

TEST_CASE("BattleDamageApplication_AggregatedPendingDamageUsesLastPresentationMetadata", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.aggregatePendingTransactionsByDefender = true;
    input.units.push_back({ 3, 0, true });
    input.deathEffectUnits->units.push_back(runtimeUnit(3, 0, 80, 100, 0));

    auto first = damageInput(1, 2, 3);
    auto second = damageInput(3, 2, 4);
    second.attacker.id = 3;
    frame.pendingTransactions.push_back(first);
    frame.pendingTransactions.push_back(second);

    BattleDamagePresentationInput firstPresentation;
    firstPresentation.enabled = true;
    firstPresentation.skillName = "先手";
    firstPresentation.detailText = "第一段";
    firstPresentation.normalDamageColor = { 10, 20, 30, 255 };
    firstPresentation.normalDamageTextSize = 22;

    BattleDamagePresentationInput secondPresentation;
    secondPresentation.enabled = true;
    secondPresentation.skillName = "終段";
    secondPresentation.detailText = "第二段";
    secondPresentation.critical = true;
    secondPresentation.emphasizedDamageColor = { 40, 50, 60, 255 };
    secondPresentation.emphasizedDamageTextSize = 33;
    frame.pendingPresentation.push_back(firstPresentation);
    frame.pendingPresentation.push_back(secondPresentation);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    REQUIRE(result.visualEvents.size() == 1);
    CHECK(result.visualEvents[0].type == BattleVisualEventType::DamageNumber);
    CHECK(result.visualEvents[0].targetUnitId == 2);
    CHECK(result.visualEvents[0].amount == 7);
    CHECK(result.visualEvents[0].textSize == 33);
    CHECK(result.visualEvents[0].color.r == 40);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Damage);
    CHECK(result.logEvents[0].sourceUnitId == 3);
    CHECK(result.logEvents[0].targetUnitId == 2);
    CHECK(result.logEvents[0].amount == 7);
    CHECK(result.logEvents[0].skillName == "終段");
    CHECK(result.logEvents[0].detailText == "第二段");
}

TEST_CASE("BattleDamageApplication_DeathPreventionLeavesUnitAliveAndEmitsLog", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    auto damage = damageInput(1, 2, 20);
    damage.defender.deathPrevention = true;
    damage.defender.deathPreventionFrames = 30;
    frame.pendingTransactions.push_back(damage);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    CHECK_FALSE(result.transactions[0].killed);
    CHECK(result.transactions[0].defender.alive);
    CHECK(result.transactions[0].defender.hp == 1);
    REQUIRE(result.logEvents.size() == 1);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].targetUnitId == 2);
    CHECK(result.logEvents[0].amount == 30);
}

TEST_CASE("BattleDamageApplication_DeathAoeBecomesProjectileCommand", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    frame.pendingTransactions.push_back(damageInput(1, 2, 20));
    frame.unitEffects[2].deathAoePct = 40;
    frame.unitEffects[2].deathAoeStunFrames = 5;
    frame.unitEffects[2].deathAoeMaxTargets = 3;

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.commands.size() == 1);
    const auto* command = std::get_if<BattleProjectileSpawnCommand>(&result.commands[0]);
    REQUIRE(command);
    CHECK(command->request.initial.attackerUnitId == 2);
    CHECK(command->request.initial.preferredTargetUnitId == 1);
    CHECK(command->request.initial.scriptedDamage == 40);
    CHECK(command->request.initial.scriptedStunFrames == 5);
    CHECK(command->request.initial.track);
}

TEST_CASE("BattleDamageApplication_AllyDeathEffectsBecomeExplicitCommands", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    input.units.push_back({ 3, 1, true });
    frame.pendingTransactions.push_back(damageInput(1, 2, 20));

    BattleRuntimeUnit allyUnit;
    allyUnit.id = 3;
    allyUnit.team = 1;
    allyUnit.alive = true;
    allyUnit.hp = 50;
    allyUnit.maxHp = 100;
    allyUnit.attack = 10;
    allyUnit.defence = 8;
    input.deathEffectUnits->units.push_back(allyUnit);

    BattleDeathEffectExtras dead;
    dead.id = 2;
    dead.comboIds = { 9 };
    dead.appliedEffects.push_back({ EffectType::DeathMedical, 20, 0, "", Trigger::Always, 0, 0, 0, 9 });

    BattleDeathEffectExtras ally;
    ally.id = 3;
    ally.shieldPctMaxHp = 30;
    ally.comboIds = { 9 };
    ally.appliedEffects.push_back({ EffectType::AllyDeathStatBoost, 4, 0, "", Trigger::Always, 0, 0, 0, 9 });
    ally.appliedEffects.push_back({ EffectType::ShieldOnAllyDeath, 1, 0, "", Trigger::Always, 0, 0, 0, 9 });

    input.deathEffects->units = { dead, ally };
    input.deathEffects->regularSynergyComboIds.insert(9);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.commands.size() == 3);
    CHECK(std::holds_alternative<BattleTempAttackBuffCommand>(result.commands[0]));
    CHECK(std::holds_alternative<BattleUnitHealCommand>(result.commands[1]));
    CHECK(std::holds_alternative<BattleUnitShieldCommand>(result.commands[2]));
}

TEST_CASE("BattleDamageApplication_ReturnsBattleResultWithoutSceneTeamScan", "[battle][damage_application][unit]")
{
    auto frame = applicationInput();
    auto& input = frame.input;
    frame.pendingTransactions.push_back(damageInput(1, 2, 20));

    auto result = BattleDamageApplicationSystem().apply(input);

    CHECK(result.battleEnded);
    CHECK(result.winningTeam == 0);
    REQUIRE(findLifecycleEvent(result, BattleDamageLifecycleEventType::BattleEnded));
    CHECK(findLifecycleEvent(result, BattleDamageLifecycleEventType::BattleEnded)->value == 0);
    REQUIRE(result.gameplayEvents.size() == 2);
    CHECK(result.gameplayEvents[1].type == BattleGameplayEventType::BattleEnded);
    CHECK(result.gameplayEvents[1].frame == 77);
    CHECK(result.gameplayEvents[1].amount == 0);
}
