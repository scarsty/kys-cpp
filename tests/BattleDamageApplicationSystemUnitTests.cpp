#include "battle/BattleDamageApplicationSystem.h"

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

BattleDamageApplicationInput applicationInput()
{
    BattleDamageApplicationInput input;
    input.frame = 77;
    input.units = {
        { 1, 0, true },
        { 2, 1, true },
    };
    return input;
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
    auto input = applicationInput();
    auto damage = damageInput(1, 2, 20);
    damage.attacker.killHealPct = 25;
    damage.attacker.bloodlustAttackPerKill = 7;
    input.pendingTransactions.push_back(damage);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    CHECK(result.transactions[0].killed);
    REQUIRE(findLifecycleEvent(result, BattleDamageLifecycleEventType::UnitDied));
    CHECK(findLifecycleEvent(result, BattleDamageLifecycleEventType::UnitDied)->targetUnitId == 2);
    REQUIRE(findLifecycleEvent(result, BattleDamageLifecycleEventType::KillRecorded));
    CHECK(findLifecycleEvent(result, BattleDamageLifecycleEventType::KillRecorded)->sourceUnitId == 1);
    CHECK(result.transactions[0].attacker.hp == 65);
    CHECK(result.transactions[0].attacker.attack == 19);
}

TEST_CASE("BattleDamageApplication_DeathPreventionLeavesUnitAliveAndEmitsPresentation", "[battle][damage_application][unit]")
{
    auto input = applicationInput();
    auto damage = damageInput(1, 2, 20);
    damage.defender.deathPrevention = true;
    damage.defender.deathPreventionFrames = 30;
    input.pendingTransactions.push_back(damage);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.transactions.size() == 1);
    CHECK_FALSE(result.transactions[0].killed);
    CHECK(result.transactions[0].defender.alive);
    CHECK(result.transactions[0].defender.hp == 1);
    REQUIRE(result.presentationEvents.size() == 1);
    CHECK(result.presentationEvents[0].type == BattlePresentationEventType::StatusLog);
    CHECK(result.presentationEvents[0].targetUnitId == 2);
    CHECK(result.presentationEvents[0].durationFrames == 30);
}

TEST_CASE("BattleDamageApplication_DeathAoeBecomesProjectileCommand", "[battle][damage_application][unit]")
{
    auto input = applicationInput();
    input.pendingTransactions.push_back(damageInput(1, 2, 20));
    input.unitEffects[2].deathAoePct = 40;
    input.unitEffects[2].deathAoeStunFrames = 5;
    input.unitEffects[2].deathAoeMaxTargets = 3;

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.commands.size() == 1);
    const auto* command = std::get_if<BattleDeathAoeProjectileCommand>(&result.commands[0]);
    REQUIRE(command);
    CHECK(command->sourceUnitId == 2);
    CHECK(command->trackedTargetUnitId == 1);
    CHECK(command->damage == 40);
    CHECK(command->damagePct == 40);
    CHECK(command->stunFrames == 5);
    CHECK(command->maxTargets == 3);
}

TEST_CASE("BattleDamageApplication_AllyDeathEffectsBecomeExplicitCommands", "[battle][damage_application][unit]")
{
    auto input = applicationInput();
    input.units.push_back({ 3, 1, true });
    input.pendingTransactions.push_back(damageInput(1, 2, 20));

    BattleDeathEffectUnit dead;
    dead.id = 2;
    dead.team = 1;
    dead.alive = true;
    dead.hp = 10;
    dead.maxHp = 100;
    dead.comboIds = { 9 };
    dead.appliedEffects.push_back({ EffectType::DeathMedical, 20, 0, "", Trigger::Always, 0, 0, 0, 9 });

    BattleDeathEffectUnit ally;
    ally.id = 3;
    ally.team = 1;
    ally.alive = true;
    ally.hp = 50;
    ally.maxHp = 100;
    ally.attack = 10;
    ally.defence = 8;
    ally.shieldPctMaxHp = 30;
    ally.comboIds = { 9 };
    ally.appliedEffects.push_back({ EffectType::AllyDeathStatBoost, 4, 0, "", Trigger::Always, 0, 0, 0, 9 });
    ally.appliedEffects.push_back({ EffectType::ShieldOnAllyDeath, 1, 0, "", Trigger::Always, 0, 0, 0, 9 });

    input.deathEffects.units = { dead, ally };
    input.deathEffects.regularSynergyComboIds.insert(9);

    auto result = BattleDamageApplicationSystem().apply(input);

    REQUIRE(result.commands.size() == 3);
    CHECK(std::holds_alternative<BattleTempAttackBuffCommand>(result.commands[0]));
    CHECK(std::holds_alternative<BattleUnitHealCommand>(result.commands[1]));
    CHECK(std::holds_alternative<BattleUnitShieldCommand>(result.commands[2]));
}

TEST_CASE("BattleDamageApplication_ReturnsBattleResultWithoutSceneTeamScan", "[battle][damage_application][unit]")
{
    auto input = applicationInput();
    input.pendingTransactions.push_back(damageInput(1, 2, 20));

    auto result = BattleDamageApplicationSystem().apply(input);

    CHECK(result.battleEnded);
    CHECK(result.winningTeam == 0);
    REQUIRE(findLifecycleEvent(result, BattleDamageLifecycleEventType::BattleEnded));
    CHECK(findLifecycleEvent(result, BattleDamageLifecycleEventType::BattleEnded)->value == 0);
}
