#include "battle/BattleHitResolver.h"

#include "BattleLogTestHelpers.h"
#include "BattleRuntimeRecordTestHelpers.h"
#include "battle/BattleRuntimeRandom.h"
#include "battle/BattleProjectileReflectionPolicy.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>

using namespace KysChess::Battle;

namespace
{

BattleHitResolutionInput hitInput()
{
    BattleHitResolutionInput input;
    input.attackEvent.type = BattleAttackEventType::Hit;
    input.attackEvent.attackId = 0;
    input.attackEvent.sourceUnitId = 1;
    input.attackEvent.unitId = 2;
    input.attackEvent.frame = 0;
    input.attackEvent.totalFrame = 10;
    input.attackEvent.position = { 1.0f, 0.0f, 0.0f };
    input.attackEvent.operationType = BattleOperationType::Melee;
    input.attackEvent.delivery = BattleAttackDelivery::contact();
    input.attackEvent.payloadClass = BattleProjectilePayloadClass::combat();
    input.attackEvent.reflectionLineage = BattleAttackReflectionLineageKind::Ordinary;
    input.attackEvent.strengthPct = 100;
    input.attackEvent.provenance = {
        .cast = {
            .rootCastId = BattleCastId{ 1 },
            .castId = BattleCastId{ 1 },
            .sourceUnitId = 1,
            .magicId = 101,
        },
        .attackId = BattleAttackId{ 1 },
        .rootAttack = true,
        .mainProjectile = true,
    };
    input.attacker.id = 1;
    input.attacker.team = 0;
    input.attacker.vitals = { 80, 100, 50, 100 };
    input.attacker.motion.position = { -1.0f, 0.0f, 0.0f };
    input.defender.id = 2;
    input.defender.team = 1;
    input.defender.vitals = { 100, 100, 20, 100 };
    input.defender.motion.position = { 0.0f, 0.0f, 0.0f };
    input.defender.motion.facing = { 1.0f, 0.0f, 0.0f };
    input.skill.id = 101;
    input.skill.resolvedBaseDamage = 50;
    input.skill.potency.emplace(100, 50);
    return input;
}

BattleHitResolutionResult resolveHit(const BattleHitResolutionInput& input)
{
    BattleRuntimeRandom random(1u);
    return BattleHitResolver().resolve(input, random);
}

const BattleHpDamageCommand* firstHpDamageCommand(
    const BattleHitResolutionResult& result)
{
    const auto command = std::ranges::find_if(
        result.commands,
        [](const BattleGameplayCommand& value)
        {
            return std::holds_alternative<BattleHpDamageCommand>(value);
        });
    return command == result.commands.end()
        ? nullptr
        : &std::get<BattleHpDamageCommand>(*command);
}

const BattleAcceptedHitSideEffectCommand* firstAcceptedHitCommand(
    const BattleHitResolutionResult& result)
{
    const auto command = std::ranges::find_if(
        result.commands,
        [](const BattleGameplayCommand& value)
        {
            return std::holds_alternative<BattleAcceptedHitSideEffectCommand>(value);
        });
    return command == result.commands.end()
        ? nullptr
        : &std::get<BattleAcceptedHitSideEffectCommand>(*command);
}

BattleRuntimeUnit runtimeUnit(int id, int team, Pointf position)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = true;
    unit.vitals = { 100, 100, 0, 100 };
    unit.motion.position = position;
    return unit;
}

BattleNearbyTrackingProcDescriptor nearbyTrackingProc()
{
    BattleNearbyTrackingProcDescriptor proc;
    proc.rule.binding = {
        .kind = KysChess::EffectSourceKind::Equipment,
        .sourceId = 56,
        .ownerUnitId = 1,
        .sourceTeam = 0,
    };
    proc.rule.ruleId = KysChess::EffectRuleId{ 77 };
    proc.chancePct = 100;
    proc.behavior = KysChess::NearbyTrackingAttackBehavior{ 80, 45 };
    return proc;
}

}  // namespace

TEST_CASE("BattleHitResolver ignores non-hit attack events", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.attackEvent.type = BattleAttackEventType::Moved;

    const auto result = resolveHit(input);

    CHECK(result.commands.empty());
    CHECK(result.logEvents.empty());
}

TEST_CASE("BattleHitResolver uses the resolved skill base damage", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.resolvedBaseDamage = 70;

    const auto result = resolveHit(input);

    CHECK(result.shapedHpDamage == Catch::Approx(70.0));
    CHECK(result.finalHpDamage == 70);
}

TEST_CASE("BattleHitResolver applies typed damage modifiers in phase order", "[battle][hit_resolver][damage][unit]")
{
    auto input = hitInput();
    input.skill.resolvedBaseDamage = 100;
    input.damageModifiers.outgoingBeforeCritical.push_back({
        KysChess::DamageModifierOperation::PercentAdd,
        20,
    });
    input.damageModifiers.incomingAfterBase.push_back({
        KysChess::DamageModifierOperation::FlatAdd,
        -10,
    });

    const auto result = resolveHit(input);

    CHECK(result.finalHpDamage == 110);
}

TEST_CASE("BattleHitResolver saturates extreme additive damage modifiers",
          "[battle][hit_resolver][damage][unit][boundary]")
{
    SECTION("positive percentage totals saturate instead of narrowing negative")
    {
        auto input = hitInput();
        input.skill.resolvedBaseDamage = 100;
        input.damageModifiers.outgoingBeforeCritical = {
            {
                KysChess::DamageModifierOperation::PercentAdd,
                std::numeric_limits<int>::max(),
                std::numeric_limits<int>::max(),
            },
            {
                KysChess::DamageModifierOperation::PercentAdd,
                std::numeric_limits<int>::max(),
                std::numeric_limits<int>::max(),
            },
        };

        const auto result = resolveHit(input);

        CHECK(result.finalHpDamage == std::numeric_limits<int>::max());
        CHECK(result.shapedHpDamage
            == Catch::Approx(static_cast<double>(std::numeric_limits<int>::max())));
    }

    SECTION("negative percentage totals retain the global reduction cap")
    {
        auto input = hitInput();
        input.skill.resolvedBaseDamage = 100;
        input.damageModifiers.incomingBase = {
            {
                KysChess::DamageModifierOperation::PercentAdd,
                std::numeric_limits<int>::min(),
            },
            {
                KysChess::DamageModifierOperation::PercentAdd,
                std::numeric_limits<int>::min(),
            },
        };

        const auto result = resolveHit(input);

        CHECK(result.finalHpDamage == 20);
    }

    SECTION("flat additions saturate at the battle integer boundary")
    {
        auto input = hitInput();
        input.skill.resolvedBaseDamage = 100;
        input.damageModifiers.incomingAfterBase = {
            {
                KysChess::DamageModifierOperation::FlatAdd,
                std::numeric_limits<int>::max(),
                std::numeric_limits<int>::max(),
            },
        };

        const auto result = resolveHit(input);

        CHECK(result.finalHpDamage == std::numeric_limits<int>::max());
    }
}

TEST_CASE("BattleHitResolver applies final damage modifiers after random variance", "[battle][hit_resolver][damage][unit]")
{
    auto input = hitInput();
    input.skill.resolvedBaseDamage = 50;
    input.randomDamageVariance = 10;
    input.damageModifiers.incomingFinal.push_back({
        KysChess::DamageModifierOperation::PercentAdd,
        25,
    });

    const auto result = resolveHit(input);

    CHECK(result.shapedHpDamage == Catch::Approx(75.0));
    CHECK(result.finalHpDamage == 75);
}

TEST_CASE("BattleHitResolver applies the typed single-hit cap", "[battle][hit_resolver][damage][unit]")
{
    auto input = hitInput();
    input.skill.resolvedBaseDamage = 100;
    input.defender.vitals.maxHp = 100;
    input.damageModifiers.incomingFinal.push_back({
        KysChess::DamageModifierOperation::CapSingleHitAtMaxHpPercent,
        25,
    });

    const auto result = resolveHit(input);

    CHECK(result.finalHpDamage == 25);
}

TEST_CASE("BattleHitResolver widens typed single-hit cap arithmetic",
          "[battle][hit_resolver][damage][unit][boundary]")
{
    auto input = hitInput();
    input.skill.resolvedBaseDamage = 100;
    input.defender.vitals.maxHp = std::numeric_limits<int>::max();
    input.damageModifiers.incomingFinal.push_back({
        KysChess::DamageModifierOperation::CapSingleHitAtMaxHpPercent,
        100,
    });

    const auto result = resolveHit(input);

    CHECK(result.finalHpDamage == 100);
}

TEST_CASE("BattleHitResolver returns dash hitstun as an accepted-hit command", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.attackEvent.operationType = BattleOperationType::Dash;
    input.attackEvent.totalFrame = 20;
    input.skill.resolvedBaseDamage = 90;

    const auto result = resolveHit(input);

    const auto* command = firstAcceptedHitCommand(result);
    REQUIRE(command);
    CHECK(command->sourceUnitId == 1);
    CHECK(command->targetUnitId == 2);
    CHECK(command->damage.acceptedHit);
    CHECK(command->damage.hitstunFrames == 5);
}

TEST_CASE("BattleHitResolver emits baseline and configured knockback commands", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.attacker.motion.position = input.defender.motion.position;
    input.defender.motion.facing = { -1.0f, 0.0f, 0.0f };
    BattleKnockbackProcDescriptor proc;
    proc.chancePct = 100;
    proc.action.direction = KysChess::ForceMoveDirection::AwayFromSource;
    proc.action.distancePixels = 7;
    proc.action.lockFrames = 4;
    proc.action.collision = KysChess::ForceMoveCollision::StopBeforeBlocked;
    proc.action.blocked = KysChess::ForceMoveBlockedResult::Stop;
    input.knockbackProcs.push_back(proc);

    const auto result = resolveHit(input);

    std::vector<BattleKnockbackCommand> knockbacks;
    for (const auto& command : result.commands)
    {
        if (const auto* knockback = std::get_if<BattleKnockbackCommand>(&command))
        {
            knockbacks.push_back(*knockback);
        }
    }
    REQUIRE(knockbacks.size() == 2);
    CHECK(knockbacks[0].direction.x == Catch::Approx(-1.0f));
    CHECK(knockbacks[0].distance == Catch::Approx(1.0));
    CHECK(knockbacks[1].distance == Catch::Approx(7.0));
    CHECK(knockbacks[1].lockFrames == 4);
    CHECK(knockbacks[1].collision == KysChess::ForceMoveCollision::StopBeforeBlocked);
    CHECK(knockbacks[1].blocked == KysChess::ForceMoveBlockedResult::Stop);
}

TEST_CASE("BattleHitResolver consumes typed critical attributes", "[battle][hit_resolver][typed-attribute]")
{
    auto input = hitInput();
    input.attackerCriticalChancePct = 100;
    input.attackerCriticalMultiplierPct = 185;

    const auto result = resolveHit(input);

    CHECK(result.critical);
    CHECK(result.criticalMultiplier == 185);
    CHECK(result.shapedHpDamage == Catch::Approx(92.5));
    const auto* damage = firstHpDamageCommand(result);
    REQUIRE(damage);
    CHECK(damage->criticalMultiplier == 185);
    CHECK(BattleLogTest::joinSegments(damage->segments) == "暴擊 x1.85");
}

TEST_CASE("BattleHitResolver reports the full reflected-return lineage",
          "[battle][hit_resolver][projectile_reflection][report]")
{
    auto input = hitInput();
    input.attackEvent.reflectionLineage =
        BattleAttackReflectionLineageKind::ReflectedReturn;
    input.attackEvent.provenance.origin = BattleAttackOriginKind::Reflection;
    input.attackEvent.provenance.rootAttack = false;
    input.attackEvent.provenance.mainProjectile = false;

    SECTION("初始回程")
    {
        const auto result = resolveHit(input);
        const auto* damage = firstHpDamageCommand(result);
        REQUIRE(damage);
        CHECK(BattleLogTest::joinSegments(damage->segments) == "彈道反射");
    }

    SECTION("回程彈射後代保留兩種來源資訊")
    {
        input.attackEvent.provenance.origin = BattleAttackOriginKind::Bounce;
        input.attackEvent.provenance.sharedHitGroupId = 9;

        const auto result = resolveHit(input);
        const auto* damage = firstHpDamageCommand(result);
        REQUIRE(damage);
        CHECK(BattleLogTest::joinSegments(damage->segments)
            == "彈道反射、連鎖彈");
    }
}

TEST_CASE("BattleHitResolver preserves ordinary projectile variant reporting",
          "[battle][hit_resolver][projectile_reflection][report]")
{
    auto input = hitInput();
    input.attackEvent.provenance.rootAttack = false;
    input.attackEvent.provenance.mainProjectile = false;
    input.attackEvent.provenance.origin = BattleAttackOriginKind::Bounce;

    SECTION("普通彈射後代沿用既有追蹤彈標籤")
    {
        input.attackEvent.track = true;

        const auto result = resolveHit(input);
        const auto* damage = firstHpDamageCommand(result);
        REQUIRE(damage);
        CHECK(BattleLogTest::joinSegments(damage->segments) == "追蹤彈");
    }

    SECTION("普通共享命中群組沿用既有連鎖彈標籤")
    {
        input.attackEvent.provenance.sharedHitGroupId = 9;

        const auto result = resolveHit(input);
        const auto* damage = firstHpDamageCommand(result);
        REQUIRE(damage);
        CHECK(BattleLogTest::joinSegments(damage->segments) == "連鎖彈");
    }
}

TEST_CASE("BattleHitResolver builds payload-dependent projectile reflection requests",
          "[battle][hit_resolver][projectile_reflection][policy]")
{
    auto input = hitInput();

    SECTION("戰鬥彈道攜帶命中時凍結的威力")
    {
        const auto request = makeBattleProjectileReflectionRequest(input);
        REQUIRE(request.potency);
        CHECK(*request.potency == BattleAttackPotencySnapshot{ 100, 50 });
    }

    SECTION("腳本彈道不要求戰鬥威力")
    {
        input.attackEvent.payloadClass =
            BattleProjectilePayloadClass::scriptedControl();
        input.attackEvent.scriptedStunFrames = 7;
        input.skill.potency.reset();

        const auto request = makeBattleProjectileReflectionRequest(input);
        CHECK_FALSE(request.potency);
    }
}

TEST_CASE("BattleHitResolver reflects ranged projectiles through typed attributes", "[battle][hit_resolver][typed-attribute]")
{
    auto input = hitInput();
    input.attackEvent.operationType = BattleOperationType::RangedProjectile;
    input.attackEvent.delivery = BattleAttackDelivery::projectile();
    input.defenderProjectileReflectChancePct = 100;

    const auto result = resolveHit(input);

    REQUIRE(result.reflection);
    CHECK(result.reflection->incomingAttackId == 0);
    CHECK(result.reflection->reflectorUnitId == 2);
    CHECK(result.reflection->originalAttackerUnitId == 1);
    REQUIRE(result.reflection->potency);
    CHECK(*result.reflection->potency == BattleAttackPotencySnapshot{ 100, 50 });
    const auto* damage = firstHpDamageCommand(result);
    REQUIRE(damage);
    CHECK(damage->sourceUnitId == 1);
    CHECK(damage->targetUnitId == 2);
    CHECK(damage->damageKind == KysChess::BattleDamageKind::Physical);
    CHECK(damage->provenance.attackId == input.attackEvent.provenance.attackId);
    CHECK(damage->provenance.cast.castId == input.attackEvent.provenance.cast.castId);
}

TEST_CASE("BattleHitResolver emits typed skill-reflect damage without defence side effects", "[battle][hit_resolver][typed-attribute]")
{
    auto input = hitInput();
    input.defenderSkillReflectPercent = 20;

    const auto result = resolveHit(input);

    const auto reflected = std::ranges::find_if(
        result.commands,
        [](const BattleGameplayCommand& command)
        {
            const auto* damage = std::get_if<BattleHpDamageCommand>(&command);
            return damage && damage->sourceUnitId == 2 && damage->targetUnitId == 1;
        });
    REQUIRE(reflected != result.commands.end());
    const auto& damage = std::get<BattleHpDamageCommand>(*reflected);
    CHECK(damage.damage == 10);
    CHECK_FALSE(damage.triggersDefenseEffects);
}

TEST_CASE("BattleHitResolver commits projectile and skill reflection independently",
          "[battle][hit_resolver][typed-attribute][projectile_reflection]")
{
    auto input = hitInput();
    input.attackEvent.operationType = BattleOperationType::RangedProjectile;
    input.attackEvent.delivery = BattleAttackDelivery::projectile();
    input.defenderProjectileReflectChancePct = 100;
    input.defenderSkillReflectPercent = 20;

    const auto result = resolveHit(input);

    REQUIRE(result.reflection);
    const auto normalHit = std::ranges::find_if(
        result.commands,
        [](const BattleGameplayCommand& command)
        {
            const auto* damage = std::get_if<BattleHpDamageCommand>(&command);
            return damage && damage->sourceUnitId == 1 && damage->targetUnitId == 2;
        });
    REQUIRE(normalHit != result.commands.end());
    const auto skillReflection = std::ranges::find_if(
        result.commands,
        [](const BattleGameplayCommand& command)
        {
            const auto* damage = std::get_if<BattleHpDamageCommand>(&command);
            return damage && damage->sourceUnitId == 2 && damage->targetUnitId == 1;
        });
    REQUIRE(skillReflection != result.commands.end());
    CHECK(std::get<BattleHpDamageCommand>(*skillReflection).damage == 10);
}

TEST_CASE("BattleHitResolver emits typed nearby tracking follow-up and activation", "[battle][hit_resolver][typed-effect]")
{
    auto input = hitInput();
    input.attackEvent.velocity = { 4.0f, 0.0f, 0.0f };
    input.nearbyTrackingProcs.push_back(nearbyTrackingProc());

    const auto result = resolveHit(input);

    const auto nearby = std::ranges::find_if(
        result.commands,
        [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleNearbyTrackingProjectilesCommand>(command);
        });
    REQUIRE(nearby != result.commands.end());
    const auto& command = std::get<BattleNearbyTrackingProjectilesCommand>(*nearby);
    CHECK(command.centerTargetUnitId == 2);
    CHECK(command.rangePixels == 80);
    CHECK(command.damagePct == 45);
    REQUIRE(result.activatedRuntimeRules.size() == 1);
    CHECK(result.activatedRuntimeRules.front().ruleId == KysChess::EffectRuleId{ 77 });
}

TEST_CASE("BattleHitResolver suppresses typed nearby tracking follow-up when requested", "[battle][hit_resolver][typed-effect]")
{
    auto input = hitInput();
    input.attackEvent.suppressNearbyTrackingProjectileProc = true;
    input.nearbyTrackingProcs.push_back(nearbyTrackingProc());

    const auto result = resolveHit(input);

    CHECK(std::ranges::none_of(
        result.commands,
        [](const BattleGameplayCommand& command)
        {
            return std::holds_alternative<BattleNearbyTrackingProjectilesCommand>(command);
        }));
    CHECK(result.activatedRuntimeRules.empty());
}

TEST_CASE("BattleProjectileFollowUpResolver expands nearby tracking targets", "[battle][hit_resolver][projectile]")
{
    auto units = KysChess::Battle::Test::runtimeRecords({
        runtimeUnit(0, 0, { 0.0f, 0.0f, 0.0f }),
        runtimeUnit(1, 1, { 40.0f, 0.0f, 0.0f }),
        runtimeUnit(2, 1, { 80.0f, 0.0f, 0.0f }),
    });
    units.requireCore(1).grid = { 1, 0 };
    units.requireCore(2).grid = { 2, 0 };

    BattleAttackEvent prototype;
    prototype.sourceUnitId = 0;
    prototype.unitId = 1;
    prototype.skillId = 101;
    prototype.visualEffectId = 44;
    prototype.position = { 0, 0, 0 };
    prototype.velocity = { 5, 0, 0 };
    prototype.totalFrame = 30;
    prototype.operationType = BattleOperationType::RangedProjectile;
    prototype.provenance = {
        .cast = {
            .rootCastId = BattleCastId{ 1 },
            .castId = BattleCastId{ 1 },
            .sourceUnitId = 0,
            .magicId = 101,
        },
        .attackId = BattleAttackId{ 1 },
        .rootAttack = true,
        .mainProjectile = true,
    };
    std::vector<BattleGameplayCommand> commands{
        BattleNearbyTrackingProjectilesCommand{
            prototype,
            2,
            100,
            40,
        },
    };
    BattleProjectileFollowUpContext context;
    context.projectileSpeed = 10.0;

    const auto expanded = expandBattleProjectileFollowUpCommands(
        commands,
        context,
        units);

    REQUIRE(expanded.commands.size() == 2);
    const auto* first = std::get_if<BattleProjectileSpawnCommand>(&expanded.commands[0]);
    const auto* second = std::get_if<BattleProjectileSpawnCommand>(&expanded.commands[1]);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first->request.initial.preferredTargetUnitId == 2);
    CHECK(second->request.initial.preferredTargetUnitId == 1);
    CHECK(first->request.initial.suppressNearbyTrackingProjectileProc);
    CHECK_FALSE(first->request.provenance.mainProjectile);
    CHECK(first->request.initial.strengthPct == 40);
}

TEST_CASE("BattleProjectileFollowUpResolver applies the configured minimum to area projectiles",
          "[battle][hit_resolver][projectile]")
{
    auto units = KysChess::Battle::Test::runtimeRecords({
        runtimeUnit(0, 0, { 0.0f, 0.0f, 0.0f }),
        runtimeUnit(1, 1, { 40.0f, 0.0f, 0.0f }),
    });
    units.requireCore(0).grid = { 0, 0 };
    units.requireCore(1).grid = { 1, 0 };

    BattleAreaProjectileFollowUp followUp;
    followUp.cast = {
        .rootCastId = BattleCastId{ 1 },
        .castId = BattleCastId{ 1 },
        .sourceUnitId = 0,
        .magicId = 101,
    };
    followUp.expansionWork = {
        .id = BattleCastWorkId{ 1 },
        .castId = BattleCastId{ 1 },
    };
    followUp.sourceUnitId = 0;
    followUp.areaSize = 1;
    followUp.trackedTargetUnitId = 1;
    followUp.maxTargets = 1;
    followUp.damage = 20;

    BattleProjectileFollowUpContext context;
    context.projectileSpeed = 10.0;
    context.minimumProjectileFrames = 37;
    context.areaProjectileFramePadding = 0;
    context.areaSpawnDistance = 0.0;

    const auto expanded = expandBattleAreaProjectileFollowUp(
        followUp,
        context,
        units);

    REQUIRE(expanded.commands.size() == 1);
    const auto* projectile = std::get_if<BattleProjectileSpawnCommand>(
        &expanded.commands.front());
    REQUIRE(projectile);
    CHECK(projectile->request.initial.totalFrame == 37);
}

TEST_CASE("BattleHitResolver emits MP damage for non-HP skills", "[battle][hit_resolver][unit]")
{
    auto input = hitInput();
    input.skill.hurtType = 1;

    const auto result = resolveHit(input);

    const auto command = std::ranges::find_if(
        result.commands,
        [](const BattleGameplayCommand& value)
        {
            return std::holds_alternative<BattleMpDamageCommand>(value);
        });
    REQUIRE(command != result.commands.end());
    CHECK(result.finalHpDamage == 0);
    CHECK(result.finalMpDamage == 50);
    CHECK(std::get<BattleMpDamageCommand>(*command).damage.mpOnHit == 40);
}

TEST_CASE("BattleHitResolver suppresses MP recovery for afterimage attacks",
          "[battle][hit_resolver][unit][ultimate-effect]")
{
    auto input = hitInput();
    input.skill.hurtType = 1;
    input.attackEvent.provenance.origin = BattleAttackOriginKind::Echo;
    input.attackEvent.provenance.propagation = CastPropagationPolicy::NoEffectRules;
    input.attackEvent.provenance.parentAttackId = BattleAttackId{ 2 };
    input.attackEvent.provenance.rootAttack = false;
    input.attackEvent.provenance.mainProjectile = false;

    const auto result = resolveHit(input);

    const auto command = std::ranges::find_if(
        result.commands,
        [](const BattleGameplayCommand& value)
        {
            return std::holds_alternative<BattleMpDamageCommand>(value);
        });
    REQUIRE(command != result.commands.end());
    CHECK(result.finalMpDamage == 50);
    CHECK(std::get<BattleMpDamageCommand>(*command).damage.mpOnHit == 0);
}

TEST_CASE("BattleHitResolver keeps scripted status and damage payloads", "[battle][hit_resolver][scripted]")
{
    auto input = hitInput();
    input.attackEvent.scriptedDamage = 24;
    input.attackEvent.scriptedDamageAppliesModifiers = true;
    input.attackEvent.scriptedDamageTriggersDefenseEffects = true;
    input.attackEvent.scriptedStunFrames = 7;
    input.attackEvent.scriptedBleedStacks = 2;
    input.attackEvent.scriptedBleedProducer = makeBattleStatusProducerProvenance(
        {
            .kind = KysChess::EffectSourceKind::Combo,
            .sourceId = 31,
            .ownerUnitId = input.attacker.id,
        },
        KysChess::EffectRuleId{ 31 },
        2);
    input.attackEvent.operationType = BattleOperationType::RangedProjectile;
    input.attackEvent.delivery = BattleAttackDelivery::projectile();
    input.attackEvent.payloadClass = BattleProjectilePayloadClass::scriptedControl();
    input.defenderProjectileReflectChancePct = 100;
    input.sharedBleedMaxStacks = 5;

    const auto result = resolveHit(input);

    const auto* accepted = firstAcceptedHitCommand(result);
    REQUIRE(accepted);
    CHECK(accepted->damage.stunFrames == 7);
    CHECK(accepted->damage.bleedStacks == 2);
    CHECK(accepted->damage.bleedMaxStacks == 5);
    const auto* damage = firstHpDamageCommand(result);
    REQUIRE(damage);
    CHECK(damage->damage == 24);
    CHECK_FALSE(damage->preResolvedDamage);
    CHECK(damage->triggersDefenseEffects);
    CHECK(result.finalHpDamage == 24);
    CHECK_FALSE(result.reflection);
}

TEST_CASE("BattleProjectileReflectionPolicy preserves P01 through P15 eligibility", "[battle][projectile_reflection][policy]")
{
    struct PolicyCase
    {
        const char* id;
        BattleAttackDelivery delivery;
        BattleProjectilePayloadClass payloadClass;
        BattleOperationType operationType;
        BattleAttackCastSubrequestKind subrequestKind;
        BattleAttackOriginKind origin;
        BattleAttackReflectionLineageKind lineage;
        bool allowed;
    };

    const std::array cases{
        PolicyCase{ "P01", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::RangedProjectile, BattleAttackCastSubrequestKind::SkillHit, BattleAttackOriginKind::Initial, BattleAttackReflectionLineageKind::Ordinary, true },
        PolicyCase{ "P02", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::TrackingProjectile, BattleAttackCastSubrequestKind::SkillHit, BattleAttackOriginKind::Initial, BattleAttackReflectionLineageKind::Ordinary, true },
        PolicyCase{ "P03", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::RangedProjectile, BattleAttackCastSubrequestKind::SkillHit, BattleAttackOriginKind::CastDerived, BattleAttackReflectionLineageKind::Ordinary, true },
        PolicyCase{ "P04", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::RangedProjectile, BattleAttackCastSubrequestKind::ExtraProjectile, BattleAttackOriginKind::CastDerived, BattleAttackReflectionLineageKind::Ordinary, true },
        PolicyCase{ "P05", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::TrackingProjectile, BattleAttackCastSubrequestKind::ExtraProjectile, BattleAttackOriginKind::CastDerived, BattleAttackReflectionLineageKind::Ordinary, true },
        PolicyCase{ "P06", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::RangedProjectile, BattleAttackCastSubrequestKind::ExtraProjectile, BattleAttackOriginKind::FollowUp, BattleAttackReflectionLineageKind::Ordinary, true },
        PolicyCase{ "P07", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::RangedProjectile, BattleAttackCastSubrequestKind::SkillHit, BattleAttackOriginKind::Bounce, BattleAttackReflectionLineageKind::Ordinary, true },
        PolicyCase{ "P08", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::RangedProjectile, BattleAttackCastSubrequestKind::ExtraProjectile, BattleAttackOriginKind::FollowUp, BattleAttackReflectionLineageKind::Ordinary, true },
        PolicyCase{ "P09", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::TrackingProjectile, BattleAttackCastSubrequestKind::ExtraProjectile, BattleAttackOriginKind::Echo, BattleAttackReflectionLineageKind::Ordinary, true },
        PolicyCase{ "P10", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::Melee, BattleAttackCastSubrequestKind::MeleeSplash, BattleAttackOriginKind::CastDerived, BattleAttackReflectionLineageKind::Ordinary, false },
        PolicyCase{ "P11", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::combat(), BattleOperationType::Melee, BattleAttackCastSubrequestKind::ExtraProjectile, BattleAttackOriginKind::CastDerived, BattleAttackReflectionLineageKind::Ordinary, false },
        PolicyCase{ "P12", BattleAttackDelivery::contact(), BattleProjectilePayloadClass::combat(), BattleOperationType::Melee, BattleAttackCastSubrequestKind::SkillHit, BattleAttackOriginKind::Initial, BattleAttackReflectionLineageKind::Ordinary, false },
        PolicyCase{ "P13", BattleAttackDelivery::contact(), BattleProjectilePayloadClass::combat(), BattleOperationType::Dash, BattleAttackCastSubrequestKind::DashHit, BattleAttackOriginKind::Initial, BattleAttackReflectionLineageKind::Ordinary, false },
        PolicyCase{ "P14", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::scriptedDamage(), BattleOperationType::RangedProjectile, BattleAttackCastSubrequestKind::ExtraProjectile, BattleAttackOriginKind::Scripted, BattleAttackReflectionLineageKind::Ordinary, false },
        PolicyCase{ "P15", BattleAttackDelivery::projectile(), BattleProjectilePayloadClass::scriptedControl(), BattleOperationType::RangedProjectile, BattleAttackCastSubrequestKind::ExtraProjectile, BattleAttackOriginKind::Scripted, BattleAttackReflectionLineageKind::Ordinary, false },
    };

    const BattleProjectileReflectionPolicy policy;
    for (const auto& test : cases)
    {
        CAPTURE(test.id);
        CHECK(policy.allows({
            test.delivery,
            test.payloadClass,
            test.operationType,
            test.subrequestKind,
            test.origin,
            test.lineage,
        }) == test.allowed);
    }
}

TEST_CASE("BattleProjectileReflectionPolicy rejects the full reflected-return lineage", "[battle][projectile_reflection][policy]")
{
    const BattleProjectileReflectionPolicy policy;
    CHECK_FALSE(policy.allows({
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleOperationType::RangedProjectile,
        BattleAttackCastSubrequestKind::SkillHit,
        BattleAttackOriginKind::Bounce,
        BattleAttackReflectionLineageKind::ReflectedReturn,
    }));
}

static_assert(!std::is_default_constructible_v<BattleAttackDelivery>);
static_assert(!std::is_default_constructible_v<BattleProjectilePayloadClass>);
static_assert(!std::is_default_constructible_v<BattleProjectileReflectionDescriptor>);
static_assert(!std::is_default_constructible_v<BattleAttackPayload>);
static_assert(!std::is_default_constructible_v<BattleAttackSpawnRequest>);

TEST_CASE("BattleHitResolver_UsesGroupedUnitSnapshotFields", "[battle][hit]")
{
    BattleHitUnitSnapshot attacker;
    attacker.id = 1;
    attacker.team = 0;
    attacker.alive = true;
    attacker.vitals = { 80, 100, 10, 20 };
    attacker.stats = { 30, 12, 9 };
    attacker.motion.position = { 1, 2, 0 };
    attacker.motion.facing = { 1, 0, 0 };
    attacker.animation = { 0, 5, 2, 1 };

    CHECK(attacker.vitals.hp == 80);
    CHECK(attacker.stats.attack == 30);
    CHECK(attacker.motion.position.x == 1);
    CHECK(attacker.animation.actType == 1);
}
