#include "battle/BattleCore.h"
#include "battle/BattleHitResolver.h"

#include <catch2/catch_test_macros.hpp>

#include <variant>

using namespace KysChess::Battle;

namespace
{

constexpr double SceneTileWidth = 36.0;
constexpr double MaxEffectiveBattleReach = 480.0;
constexpr double LegacyMinimumVectorNorm = 0.0001;

BattleMovementConfig runtimeMovementConfig()
{
    BattleMovementGeometry geometry;
    geometry.tileWidth = SceneTileWidth;
    geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    geometry.meleeAttackHitRadius = SceneTileWidth * 2.0;
    geometry.dashFrames = 5;
    geometry.dashCooldownFrames = 18;
    geometry.maxRangedReach = MaxEffectiveBattleReach;
    return BattleGeometry(geometry).movementConfig();
}

BattleUnitState runtimeUnit(int id, int team, Pointf position)
{
    BattleUnitState state;
    state.id = id;
    state.realRoleId = id;
    state.team = team;
    state.alive = true;
    state.position = position;
    state.speed = 5.0;
    state.reach = 137.5;
    return state;
}

BattleFrameState runtimeFrameState()
{
    BattleFrameState state;
    state.world.frame = 6;
    state.world.config = runtimeMovementConfig();
    state.world.canStandAt = [](Pointf) { return true; };
    state.world.units = {
        runtimeUnit(1, 0, { 100, 100, 0 }),
        runtimeUnit(2, 1, { 500, 100, 0 }),
    };
    state.attacks.hitRadius = SceneTileWidth * 2.0;
    state.attacks.minimumVectorNorm = LegacyMinimumVectorNorm;
    state.attacks.bounceSpawnDistance = SceneTileWidth * 1.5;
    state.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;
    return state;
}

BattleFrameUnitRuntimeInput finishingSkillRuntime()
{
    BattleFrameUnitRuntimeInput input;
    input.state.cooldown = 1;
    input.state.actType = 2;
    input.state.operationType = BattleOperationType::RangedProjectile;
    input.state.haveAction = true;
    input.state.physicalPower = 4;
    input.frame = 6;
    input.mpRegenIntervalFrames = 3;
    input.physicalPowerRegenIntervalFrames = 3;
    return input;
}

BattleAttackInstance cancelProjectile(int id, int attackerUnitId)
{
    BattleAttackInstance attack;
    attack.id = id;
    attack.frame = 5;
    attack.state.attackerUnitId = attackerUnitId;
    attack.state.skillId = 100 + id;
    attack.state.totalFrame = 30;
    attack.state.operationType = BattleOperationType::RangedProjectile;
    attack.state.position = { 500, 500, 0 };
    return attack;
}

}  // namespace

TEST_CASE("BattleFrameRunner_AdvanceFrame_EmitsSkillFinishedTeamHealCommand", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();

    KysChess::RoleComboState combo;
    combo.postSkillInvincFrames = 12;
    combo.onSkillTeamHealPending = true;
    combo.onSkillTeamHeal = 7;
    combo.onSkillTeamHealPct = 3;
    state.combo.units.emplace(1, combo);

    BattleFrameRuntimeUnitInput runtime;
    runtime.unitId = 1;
    runtime.input = finishingSkillRuntime();
    runtime.hp = 80;
    runtime.maxHp = 100;
    runtime.alive = true;
    state.runtime.units.push_back(runtime);

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(state.runtime.committedResults.size() == 1);
    CHECK(state.runtime.committedResults[0].unitId == 1);
    CHECK(state.runtime.committedResults[0].result.skillFinished);
    REQUIRE(state.runtime.committedResults[0].comboEvents.size() == 1);
    CHECK(state.runtime.committedResults[0].comboEvents[0].type == BattleComboFrameRuntimeEventType::PostSkillInvincibility);

    REQUIRE(result.commands.size() == 1);
    const auto* command = std::get_if<BattleTeamHealCommand>(&result.commands[0]);
    REQUIRE(command);
    CHECK(command->sourceUnitId == 1);
    CHECK(command->flatHeal == 7);
    CHECK(command->pctHeal == 3);
    CHECK(command->reason == "技能群療");
    CHECK_FALSE(state.combo.units.at(1).onSkillTeamHealPending);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesProjectileCancelDamageCommand", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.attacks.units = {
        { 1, 0, true, false, false, { 100, 100, 0 } },
        { 2, 1, true, false, false, { 900, 900, 0 } },
    };
    state.attacks.attacks.push_back(cancelProjectile(10, 1));
    state.attacks.attacks.push_back(cancelProjectile(20, 2));
    state.projectileCancel.baseDamages.push_back({ 10, -1, 25 });
    state.projectileCancel.baseDamages.push_back({ 20, -1, 12 });

    auto result = BattleFrameRunner().advanceFrame(state);

    REQUIRE(result.attackEvents.size() == 3);
    const auto& cancel = result.attackEvents[2];
    CHECK(cancel.type == BattleAttackEventType::ProjectileCancel);
    CHECK(cancel.projectileCancelDamage == 25);
    CHECK(cancel.otherProjectileCancelDamage == 12);

    REQUIRE(result.commands.size() == 1);
    const auto* command = std::get_if<BattleProjectileCancelDamageCommand>(&result.commands[0]);
    REQUIRE(command);
    CHECK(command->attackId == 10);
    CHECK(command->otherAttackId == 20);
    CHECK(command->sourceUnitId == 1);
    CHECK(command->otherSourceUnitId == 2);
    CHECK(command->damage == 25);
    CHECK(command->otherDamage == 12);

    REQUIRE(state.attacks.attacks.size() == 2);
    CHECK(state.attacks.attacks[0].state.projectileCancelWeaken == 12);
    CHECK(state.attacks.attacks[1].state.projectileCancelWeaken == 25);
}
