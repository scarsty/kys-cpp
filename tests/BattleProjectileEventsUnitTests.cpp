#include "battle/BattleProjectileEvents.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace KysChess::Battle;

namespace
{

BattleAttackInstance combatAttack(int attackId, int sourceUnitId)
{
    BattleAttackInstance attack{ BattleAttackPayload{
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary } };
    attack.id = attackId;
    attack.state.attackSourceUnitId = sourceUnitId;
    attack.state.totalFrame = 30;
    attack.state.visualEffectId = 7;
    return attack;
}

}  // namespace

TEST_CASE("BattleProjectileEvents_ResolvedSentinelFrameBecomesThePresentationFrame", "[battle][projectile][events]")
{
    BattleAttackState world;
    BattleAttackEvent event;
    event.type = BattleAttackEventType::Moved;
    event.attackId = 10;

    std::vector<BattleVisualEvent> output;
    appendVisualEvents(event, world, 42, output);

    REQUIRE(output.size() == 1);
    CHECK(output[0].type == BattleVisualEventType::ProjectileMoved);
    CHECK(output[0].frame == 42);
}

TEST_CASE("BattleProjectileEvents_BounceEmitsBounceThenSpawnedProjectileWithResolvedFrames", "[battle][projectile][events]")
{
    BattleAttackState world;
    world.attacks.push_back(combatAttack(11, 5));

    BattleAttackEvent event;
    event.type = BattleAttackEventType::Bounce;
    event.attackId = 10;
    event.unitId = 3;
    event.otherAttackId = 11;

    std::vector<BattleVisualEvent> output;
    appendVisualEvents(event, world, 7, output);

    REQUIRE(output.size() == 2);
    CHECK(output[0].type == BattleVisualEventType::ProjectileBounced);
    CHECK(output[0].targetUnitId == 3);
    CHECK(output[0].amount == 11);
    CHECK(output[0].frame == 7);
    CHECK(output[1].type == BattleVisualEventType::ProjectileSpawned);
    CHECK(output[1].effectId == 11);
    CHECK(output[1].sourceUnitId == 5);
    CHECK(output[1].visualEffectId == 7);
    CHECK(output[1].frame == 7);
}

TEST_CASE("BattleProjectileEvents_CoalescesStopLogsBySourceAndReason", "[battle][projectile][events]")
{
    BattleAttackState world;
    world.attacks.push_back(combatAttack(10, 9));

    std::vector<BattleAttackEvent> events;
    for (int i = 0; i < 2; ++i)
    {
        BattleAttackEvent event;
        event.type = BattleAttackEventType::TargetLost;
        event.attackId = 10;
        events.push_back(event);
    }

    std::vector<BattleLogEvent> logEvents;
    appendProjectileCancellationLogEvents(world, events, logEvents, false);

    REQUIRE(logEvents.size() == 1);
    CHECK(logEvents[0].sourceUnitId == 9);
    CHECK(logEvents[0].segments.size() > 0);
}
