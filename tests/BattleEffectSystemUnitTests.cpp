#include "battle/BattleEffectSystem.h"
#include "battle/BattleStatusSystem.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>

using namespace KysChess::Battle;

namespace
{

BattleEffectWorld effectWorld()
{
    BattleEffectWorld world;
    world.units = {
        { 1, 0, true, 50, 100, 0, 100, 40, 0, 0 },
        { 2, 0, true, 70, 100, 0, 100, 20, 0, 0 },
        { 3, 1, true, 30, 100, 0, 100, 60, 0, 0 },
        { 4, 1, false, 30, 100, 0, 100, 60, 0, 0 },
    };
    return world;
}

BattleEffectDefinition effect(int id,
                              BattleHook hook,
                              BattleEffectTarget target,
                              std::string executor,
                              int value,
                              int maxCount = 0)
{
    BattleEffectDefinition definition;
    definition.id = id;
    definition.type = executor;
    definition.hook = hook;
    definition.target = target;
    definition.executorKey = std::move(executor);
    definition.value = value;
    definition.maxCount = maxCount;
    return definition;
}

const BattleEffectUnit& unitById(const BattleEffectWorld& world, int id)
{
    for (const auto& unit : world.units)
    {
        if (unit.id == id)
        {
            return unit;
        }
    }
    FAIL("unit not found");
}

}  // namespace

TEST_CASE("BattleEffectDispatcher_TargetSelectors_OperateAtPerUnitLevel", "[battle][effects][unit]")
{
    BattleEffectRegistry registry;

    struct Case
    {
        BattleEffectTarget target;
        std::vector<int> expectedUnitIds;
    };

    const std::vector<Case> cases = {
        { BattleEffectTarget::Source, { 1 } },
        { BattleEffectTarget::Self, { 1 } },
        { BattleEffectTarget::Target, { 3 } },
        { BattleEffectTarget::SourceTeam, { 1, 2 } },
        { BattleEffectTarget::TargetTeam, { 3 } },
    };

    for (size_t i = 0; i < cases.size(); ++i)
    {
        auto world = effectWorld();
        BattleEffectDispatcher dispatcher(registry);
        dispatcher.addEffect(effect(static_cast<int>(i + 1),
            BattleHook::AttackHit,
            cases[i].target,
            "添加护盾",
            25));

        auto commands = dispatcher.dispatch(world, { BattleHook::AttackHit, 1, 3 });
        REQUIRE(commands.size() == cases[i].expectedUnitIds.size());
        for (int id : cases[i].expectedUnitIds)
        {
            CHECK(unitById(world, id).shield == 25);
        }
    }
}

TEST_CASE("BattleEffectDispatcher_SharedExecutors_MutateOnlyResolvedTargets", "[battle][effects][unit]")
{
    BattleEffectRegistry registry;

    SECTION("heal percent caps at max hp")
    {
        auto world = effectWorld();
        BattleEffectDispatcher dispatcher(registry);
        dispatcher.addEffect(effect(10, BattleHook::AfterHeal, BattleEffectTarget::Target, "治疗百分比", 80));

        auto commands = dispatcher.dispatch(world, { BattleHook::AfterHeal, 1, 3 });
        REQUIRE(commands.size() == 1);
        CHECK(commands[0].type == BattleEffectCommandType::Heal);
        CHECK(commands[0].targetUnitId == 3);
        CHECK(commands[0].value == 70);
        CHECK(unitById(world, 3).hp == 100);
        CHECK(unitById(world, 1).hp == 50);
    }

    SECTION("invincibility uses max remaining frames")
    {
        auto world = effectWorld();
        world.units[2].invincible = 12;
        BattleEffectDispatcher dispatcher(registry);
        dispatcher.addEffect(effect(11, BattleHook::SkillFinished, BattleEffectTarget::Source, "添加无敌帧", 30));

        auto commands = dispatcher.dispatch(world, { BattleHook::SkillFinished, 3, -1 });
        REQUIRE(commands.size() == 1);
        CHECK(commands[0].type == BattleEffectCommandType::AddInvincibility);
        CHECK(unitById(world, 3).invincible == 30);
    }

    SECTION("cooldown percent modifier does not create cooldown from zero")
    {
        auto world = effectWorld();
        world.units[0].cooldown = 100;
        world.units[1].cooldown = 0;
        BattleEffectDispatcher dispatcher(registry);
        dispatcher.addEffect(effect(12, BattleHook::AfterHeal, BattleEffectTarget::SourceTeam, "冷却百分比修正", 25));

        auto commands = dispatcher.dispatch(world, { BattleHook::AfterHeal, 1, 3 });
        REQUIRE(commands.size() == 1);
        CHECK(commands[0].targetUnitId == 1);
        CHECK(unitById(world, 1).cooldown == 75);
        CHECK(unitById(world, 2).cooldown == 0);
    }
}

TEST_CASE("BattleEffectDispatcher_HookAndActivationLimit_AreDeterministic", "[battle][effects][unit]")
{
    auto world = effectWorld();
    BattleEffectRegistry registry;
    BattleEffectDispatcher dispatcher(registry);
    dispatcher.addEffect(effect(20, BattleHook::AttackHit, BattleEffectTarget::Target, "专用执行器", 7, 1));
    dispatcher.addEffect(effect(21, BattleHook::Frame, BattleEffectTarget::Target, "专用执行器", 99));

    auto first = dispatcher.dispatch(world, { BattleHook::AttackHit, 1, 3 });
    auto second = dispatcher.dispatch(world, { BattleHook::AttackHit, 1, 3 });
    auto wrongHook = dispatcher.dispatch(world, { BattleHook::SkillFinished, 1, 3 });

    REQUIRE(first.size() == 1);
    CHECK(first[0].type == BattleEffectCommandType::DedicatedEffect);
    CHECK(first[0].value == 7);
    CHECK(second.empty());
    CHECK(wrongHook.empty());
    CHECK(world.activationCounts[20] == 1);
    CHECK(world.activationCounts[21] == 0);
}

TEST_CASE("BattleStatusSystem_PoisonBleedAndTimers_TickPerUnit", "[battle][status][unit]")
{
    BattleStatusUnitState unit;
    unit.id = 7;
    unit.alive = true;
    unit.hp = 80;
    unit.maxHp = 200;
    unit.attack = 120;
    unit.poisonTimer = 2;
    unit.poisonTickPct = 10;
    unit.poisonSourceId = 1;
    unit.bleedStacks = 3;
    unit.bleedTimer = 1;
    unit.bleedSourceId = 2;
    unit.mpBlockTimer = 3;
    unit.damageImmunityAfterFrames = 4;
    unit.damageImmunityDuration = 11;
    unit.damageImmunityTimer = 1;
    unit.tempAttackBuffs.push_back({ 15, 1 });
    unit.damageReduceDebuffs.push_back({ 1, 30 });

    auto result = BattleStatusSystem({ 30 }).tick(unit);

    CHECK(unit.attack == 105);
    CHECK(unit.poisonTimer == 1);
    CHECK(unit.bleedTimer == 10);
    CHECK(unit.bleedStacks == 3);
    CHECK(unit.mpBlockTimer == 2);
    CHECK(unit.damageReduceDebuffs.empty());
    CHECK(unit.invincible == 11);

    std::map<BattleStatusEventType, int> values;
    for (const auto& event : result.events)
    {
        values[event.type] += event.value;
    }
    CHECK(values[BattleStatusEventType::PoisonDamage] == 8);
    CHECK(values[BattleStatusEventType::BleedDamage] == 6);
    CHECK(values[BattleStatusEventType::TempAttackExpired] == 15);
    CHECK(values[BattleStatusEventType::InvincibilityGranted] == 11);
}

TEST_CASE("BattleStatusSystem_DeadUnitsDoNotTickOrEmit", "[battle][status][unit]")
{
    BattleStatusUnitState unit;
    unit.id = 8;
    unit.alive = false;
    unit.attack = 100;
    unit.bleedStacks = 5;
    unit.bleedTimer = 0;
    unit.tempAttackBuffs.push_back({ 10, 1 });

    auto result = BattleStatusSystem({ 30 }).tick(unit);
    CHECK(result.events.empty());
    CHECK(unit.attack == 100);
    CHECK(unit.bleedStacks == 5);
}

