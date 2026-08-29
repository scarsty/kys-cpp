#include "Find.h"
#include "battle/BattleRuntimeUnits.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace KysChess;
using namespace KysChess::Battle;

TEST_CASE("BattleFind_RequireDenseByIdIndexesVectorByUnitId", "[battle][find][unit]")
{
    std::vector<BattleRuntimeUnit> units;
    BattleRuntimeUnit first;
    first.id = 0;
    first.vitals.hp = 10;
    units.push_back(first);
    BattleRuntimeUnit second;
    second.id = 1;
    second.vitals.hp = 20;
    units.push_back(second);

    CHECK(requireDenseById(units, 0).vitals.hp == 10);
    CHECK(requireDenseById(units, 1).vitals.hp == 20);

    requireDenseById(units, 1).vitals.hp = 25;
    CHECK(units[1].vitals.hp == 25);
}

TEST_CASE("BattleFind_TryDenseByIdReturnsNullForMissingDenseIndex", "[battle][find][unit]")
{
    std::vector<BattleRuntimeUnit> units;
    BattleRuntimeUnit only;
    only.id = 0;
    units.push_back(only);

    CHECK(tryDenseById(units, -1) == nullptr);
    CHECK(tryDenseById(units, 1) == nullptr);
}
