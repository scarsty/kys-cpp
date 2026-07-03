#include "Chess.h"
#include "ChessCombo.h"

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <string>
#include <utility>
#include <vector>

using namespace KysChess;

namespace
{

Role makeRole(int id, int cost = 1)
{
    Role role;
    role.ID = id;
    role.Cost = cost;
    return role;
}

Chess makeChess(Role& role, int star = 1, std::vector<std::string> actAsComboNames = {})
{
    Chess chess;
    chess.role = &role;
    chess.star = star;
    chess.actAsComboNames = std::move(actAsComboNames);
    return chess;
}

ComboDef makeCombo(std::vector<int> members, std::vector<int> thresholds)
{
    ComboDef combo;
    combo.id = 0;
    combo.name = "拳師";
    combo.memberRoleIds = std::move(members);
    for (int count : thresholds)
    {
        combo.thresholds.push_back({count, std::format("{}人", count), {}});
    }
    return combo;
}

}    // namespace

TEST_CASE("combo progress formats active combos against the next tier", "[chess][combo]")
{
    auto role1 = makeRole(1);
    auto role2 = makeRole(2);
    auto role3 = makeRole(3);
    auto combo = makeCombo({1, 2, 3, 4, 5}, {3, 5});

    const auto progress = evaluateComboProgress(combo, {
                                                           makeChess(role1),
                                                           makeChess(role2),
                                                           makeChess(role3),
                                                       });

    CHECK(progress.active);
    CHECK(progress.activeThresholdIdx == 0);
    CHECK(progress.displayTargetCount == 5);
    CHECK(formatComboProgressCount(progress) == "3/5 ✓");
}

TEST_CASE("combo progress formats capped combos against the max tier", "[chess][combo]")
{
    auto role1 = makeRole(1);
    auto role2 = makeRole(2);
    auto role3 = makeRole(3);
    auto role4 = makeRole(4);
    auto role5 = makeRole(5);
    auto combo = makeCombo({1, 2, 3, 4, 5}, {3, 5});

    const auto progress = evaluateComboProgress(combo, {
                                                           makeChess(role1),
                                                           makeChess(role2),
                                                           makeChess(role3),
                                                           makeChess(role4),
                                                           makeChess(role5),
                                                       });

    CHECK(progress.active);
    CHECK(progress.activeThresholdIdx == 1);
    CHECK(progress.displayTargetCount == 5);
    CHECK(formatComboProgressCount(progress) == "5/5 ✓");
}

TEST_CASE("combo progress includes equipment act-as synergy members", "[chess][combo]")
{
    auto role1 = makeRole(1);
    auto role2 = makeRole(99);
    auto combo = makeCombo({1, 2, 3}, {2, 3});

    const auto progress = evaluateComboProgress(combo, {
                                                           makeChess(role1),
                                                           makeChess(role2, 1, {"拳師"}),
                                                       });

    CHECK(progress.physicalCount == 2);
    CHECK(progress.effectiveCount == 2);
    CHECK(progress.active);
    CHECK(progress.displayTargetCount == 3);
    CHECK(formatComboProgressCount(progress) == "2/3 ✓");
}

TEST_CASE("combo progress formats star synergy physical plus extra count", "[chess][combo]")
{
    auto role1 = makeRole(1);
    auto role2 = makeRole(2);
    auto combo = makeCombo({1, 2, 3}, {3, 5});
    combo.starSynergyBonus = true;

    const auto progress = evaluateComboProgress(combo, {
                                                           makeChess(role1),
                                                           makeChess(role2, 2),
                                                       });

    CHECK(progress.physicalCount == 2);
    CHECK(progress.effectiveCount == 3);
    CHECK(progress.active);
    CHECK(progress.displayTargetCount == 5);
    CHECK(formatComboProgressCount(progress) == "2+1/5 ✓");
}
