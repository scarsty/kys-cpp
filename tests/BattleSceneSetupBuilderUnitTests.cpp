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

Magic* noMagic(int)
{
    return nullptr;
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
    request.positionForCell = [](int x, int y)
    {
        return Pointf{ static_cast<float>(x * 10), static_cast<float>(y * 10), 0 };
    };
    request.fightFramesForHeadId = testFightFrames;
    request.magicById = noMagic;

    auto unit = KysChess::BattleSceneSetupBuilder::makeSetupUnit(
        request,
        std::span<const KysChess::BattleSceneSetupBuilder::BattleSceneSetupOpponentCell>{});

    CHECK(unit.unitId == 0);
    CHECK(unit.realRoleId == 1001);
    CHECK(unit.team == 0);
    CHECK(unit.gridX == 2);
    CHECK(unit.gridY == 3);
    CHECK(unit.star == 4);
    CHECK(unit.hp == source.MaxHP);
    CHECK(unit.mp == 0);
    CHECK(unit.physicalPower >= 90);
    CHECK(unit.acceleration.z == -3.0f);
    CHECK(source.Team == originalTeam);
    CHECK(source.Star == originalStar);
    CHECK(source.X() == originalX);
    CHECK(source.Y() == originalY);
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
        request.positionForCell = [](int x, int y)
        {
            return Pointf{ static_cast<float>(x), static_cast<float>(y), 0 };
        };
        request.fightFramesForHeadId = testFightFrames;
        request.magicById = noMagic;
    }

    auto result = KysChess::BattleSceneSetupBuilder::buildSetupUnits(requests);

    REQUIRE(result.units.size() == 2);
    CHECK(result.units[0].unitId == 0);
    CHECK(result.units[1].unitId == 1);
}
