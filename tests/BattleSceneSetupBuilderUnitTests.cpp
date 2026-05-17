#include "BattleSceneSetupBuilder.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
Role makeSavedRole(int id)
{
    Role role;
    role.ID = id;
    role.Name = "測試角色";
    role.HeadID = 23;
    role.MaxHP = 100;
    role.MaxMP = 30;
    role.Attack = 20;
    role.Defence = 10;
    role.Speed = 40;
    role.Fist = 5;
    role.Sword = 6;
    role.Knife = 7;
    role.Unusual = 8;
    role.HiddenWeapon = 9;
    role.Cost = 3;
    role.Team = 99;
    role.Star = 1;
    role.FaceTowards = Towards_LeftUp;
    role.setPositionOnly(9, 9);
    return role;
}

std::array<int, 5> testFightFrames(int)
{
    return { 1, 2, 3, 4, 5 };
}

void populateSetupFacts(KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest& request)
{
    request.position = {
        static_cast<float>(request.gridX * 10),
        static_cast<float>(request.gridY * 10),
        0,
    };
    request.fightFrames = testFightFrames(request.source->HeadID);
}
}  // namespace

TEST_CASE("BattleSceneSetupBuilder_BuildsSetupUnitWithoutMutatingSavedRole", "[battle][scene_setup]")
{
    Role source = makeSavedRole(1001);
    const int originalTeam = source.Team;
    const int originalStar = source.Star;
    const int originalX = source.X();
    const int originalY = source.Y();

    KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest request;
    request.unitId = 0;
    request.source = &source;
    request.team = 0;
    request.gridX = 2;
    request.gridY = 3;
    request.star = 4;
    request.faceTowardsFallback = Towards_RightDown;
    request.sourceOrder = 0;
    request.gravity = -3.0f;
    populateSetupFacts(request);

    auto unit = KysChess::BattleSceneSetupBuilder::makeSetupUnit(
        request,
        std::span<const KysChess::BattleSceneSetupBuilder::BattleSceneSetupOpponentCell>{});

    CHECK(unit.unitId == 0);
    CHECK(unit.realRoleId == 1001);
    CHECK(unit.team == 0);
    CHECK(unit.gridX == 2);
    CHECK(unit.gridY == 3);
    CHECK(unit.star == 4);
    CHECK(unit.vitals.hp == source.MaxHP);
    CHECK(unit.vitals.mp == 0);
    CHECK(unit.physicalPower >= 90);
    CHECK(unit.motion.acceleration.z == -3.0f);
    CHECK(source.Team == originalTeam);
    CHECK(source.Star == originalStar);
    CHECK(source.X() == originalX);
    CHECK(source.Y() == originalY);
}

TEST_CASE("BattleSceneSetupBuilder_BuildsGroupedUnitValues", "[battle][scene_setup]")
{
    Role source = makeSavedRole(1001);
    source.MaxHP = 120;
    source.MaxMP = 45;
    source.Attack = 35;
    source.Defence = 20;
    source.Speed = 8;

    KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest request;
    request.unitId = 0;
    request.source = &source;
    request.team = 0;
    request.gridX = 2;
    request.gridY = 3;
    request.star = 1;
    request.faceTowardsFallback = Towards_RightDown;
    request.sourceOrder = 0;
    request.position = {
        static_cast<float>(request.gridX),
        static_cast<float>(request.gridY),
        0,
    };
    request.fightFrames = testFightFrames(source.HeadID);

    const std::array requests{ request };
    const auto result = KysChess::BattleSceneSetupBuilder::buildSetupUnits(requests);

    REQUIRE(result.sessionInput.units.size() == 1);
    const auto& unit = result.sessionInput.units[0];
    CHECK(unit.vitals.maxHp == 120);
    CHECK(unit.vitals.hp == 120);
    CHECK(unit.stats.attack == 35);
    CHECK(unit.stats.defence == 20);
    CHECK(unit.stats.speed == 8);
    CHECK(unit.motion.position.x == 2.0f);
    CHECK(unit.motion.position.y == 3.0f);
    CHECK(unit.animation.actType == -1);
}

TEST_CASE("BattleSceneSetupBuilder_EmitsRuntimeSeedRowsWithSetupUnits", "[battle][scene_setup]")
{
    Role source = makeSavedRole(1001);
    source.MaxHP = 130;
    source.Attack = 31;
    source.Defence = 22;
    source.Speed = 13;

    KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest request;
    request.unitId = 0;
    request.source = &source;
    request.team = 0;
    request.gridX = 3;
    request.gridY = 4;
    request.star = 2;
    request.faceTowardsFallback = Towards_RightDown;
    request.sourceOrder = 7;
    populateSetupFacts(request);

    const std::array requests{ request };
    const auto result = KysChess::BattleSceneSetupBuilder::buildSetupUnits(requests);

    REQUIRE(result.sessionInput.units.size() == 1);
    REQUIRE(result.sessionInput.setup.units.size() == 1);
    CHECK(result.sessionInput.setup.units[0].unitId == result.sessionInput.units[0].unitId);
    CHECK(result.sessionInput.setup.units[0].realRoleId == result.sessionInput.units[0].realRoleId);
    CHECK(result.sessionInput.setup.units[0].baseMaxHp == result.sessionInput.units[0].vitals.maxHp);
    CHECK(result.sessionInput.setup.units[0].baseAttack == result.sessionInput.units[0].stats.attack);
    REQUIRE(result.sessionInput.setup.allyRoster.size() == 1);
    CHECK(result.sessionInput.setup.allyRoster[0].unitId == 0);
    CHECK(result.sessionInput.setup.allyRoster[0].sourceOrder == 7);
    REQUIRE(result.sessionInput.setup.enemyRoster.empty());
    REQUIRE(result.sessionInput.setup.cloneSources.size() == 1);
    CHECK(result.sessionInput.setup.cloneSources[0].sourceUnitId == 0);
    CHECK(result.sessionInput.setup.cloneSources[0].power == source.MaxHP + source.Attack + source.Defence);
    REQUIRE(result.sessionInput.actionPlanSeeds.size() == 1);
    CHECK(result.sessionInput.actionPlanSeeds[0].unitId == 0);
    CHECK(result.sessionInput.actionPlanSeeds[0].hasEquippedSkill == result.sessionInput.units[0].hasEquippedSkill);
}

TEST_CASE("BattleSceneSetupBuilder_AssignsDenseUnitsInProvidedOrder", "[battle][scene_setup]")
{
    Role first = makeSavedRole(1001);
    Role second = makeSavedRole(1002);

    std::vector<KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest> requests;
    requests.push_back({ .unitId = 0, .source = &first, .team = 1, .gridX = 1, .gridY = 2, .star = 2, .faceTowardsFallback = Towards_RightDown, .sourceOrder = 0 });
    requests.push_back({ .unitId = 1, .source = &second, .team = 0, .gridX = 5, .gridY = 6, .star = 3, .faceTowardsFallback = Towards_LeftUp, .sourceOrder = 0 });
    for (auto& request : requests)
    {
        request.position = {
            static_cast<float>(request.gridX),
            static_cast<float>(request.gridY),
            0,
        };
        request.fightFrames = testFightFrames(request.source->HeadID);
    }

    auto result = KysChess::BattleSceneSetupBuilder::buildSetupUnits(requests);

    REQUIRE(result.sessionInput.units.size() == 2);
    CHECK(result.sessionInput.units[0].unitId == 0);
    CHECK(result.sessionInput.units[1].unitId == 1);
}
