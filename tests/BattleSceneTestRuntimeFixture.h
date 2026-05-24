#pragma once

#include "BattleSceneUnitStore.h"

#include <format>
#include <utility>
#include <vector>

namespace BattleSceneTest
{
inline constexpr int TestBattleCoordCount = 64;

inline KysChess::Battle::BattleSetupUnitInput makeSetupUnit(
    int id,
    int team,
    int x,
    int y,
    Pointf position,
    int hp = 100)
{
    KysChess::Battle::BattleSetupUnitInput unit;
    unit.unitId = id;
    unit.realRoleId = 1000 + id;
    unit.team = team;
    unit.headId = 10 + id;
    unit.name = std::format("unit{}", id);
    unit.gridX = x;
    unit.gridY = y;
    unit.faceTowards = Towards_RightDown;
    unit.alive = hp > 0;
    unit.vitals = { hp, 100, 0, 100 };
    unit.motion = { position, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
    unit.animation = { 0, 0, 0, -1 };
    unit.cost = 2;
    unit.star = 1;
    return unit;
}

inline KysChess::Battle::BattleRuntimeSessionCreationInput makeSessionInput(
    std::vector<KysChess::Battle::BattleSetupUnitInput> units)
{
    KysChess::Battle::BattleRuntimeSessionCreationInput input;
    input.units = std::move(units);
    input.rules.gridTransform = { 100.0, TestBattleCoordCount };
    input.rules.movementCollisionWorld.tileWidth = 100.0;
    input.rules.movementCollisionWorld.coordCount = TestBattleCoordCount;
    input.setup.units.reserve(input.units.size());
    for (const auto& unit : input.units)
    {
        KysChess::Battle::BattleInitializationUnitSeed seed;
        seed.unitId = unit.unitId;
        seed.realRoleId = unit.realRoleId;
        seed.team = unit.team;
        seed.star = unit.star;
        seed.cost = unit.cost;
        seed.baseMaxHp = unit.vitals.maxHp;
        seed.baseAttack = unit.stats.attack;
        seed.baseDefence = unit.stats.defence;
        seed.baseSpeed = unit.stats.speed;
        seed.baseFist = unit.fist;
        seed.baseSword = unit.sword;
        seed.baseKnife = unit.knife;
        seed.baseUnusual = unit.unusual;
        seed.baseHiddenWeapon = unit.hiddenWeapon;
        input.setup.units.push_back(std::move(seed));
    }
    return input;
}

struct StoreFixture
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> sourceUnits;
    KysChess::Battle::BattleRuntimeSessionCreationInput creationInput;
    KysChess::Battle::BattleRuntimeSessionCreationResult creation;
    KysChess::Battle::BattleRuntimeSession session;
    BattleSceneUnitStore store;

    explicit StoreFixture(std::vector<KysChess::Battle::BattleSetupUnitInput> units)
        : sourceUnits(std::move(units)),
          creationInput(makeSessionInput(sourceUnits)),
          creation(KysChess::Battle::BattleRuntimeSession::createInitialized(creationInput)),
          session(std::move(creation.session))
    {
        store.initialize(session);
    }
};

}  // namespace BattleSceneTest
