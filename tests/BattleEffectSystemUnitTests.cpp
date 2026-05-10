#include "battle/BattleCore.h"
#include "battle/BattleStatusSystem.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <tuple>
#include <vector>

using namespace KysChess::Battle;

namespace
{

BattleUnitStore effectUnits()
{
    BattleUnitStore units;
    for (auto [id, team, alive, hp, mp, cooldown] : {
        std::tuple{ 0, 0, false, 0, 0, 0 },
        std::tuple{ 1, 0, true, 50, 0, 40 },
        std::tuple{ 2, 0, true, 70, 0, 20 },
        std::tuple{ 3, 1, true, 30, 0, 60 },
        std::tuple{ 4, 1, false, 30, 0, 60 },
    })
    {
        BattleRuntimeUnit unit;
        unit.id = id;
        unit.team = team;
        unit.alive = alive;
        unit.vitals.hp = hp;
        unit.vitals.maxHp = 100;
        unit.vitals.mp = mp;
        unit.vitals.maxMp = 100;
        unit.animation.cooldown = cooldown;
        units.units.push_back(unit);
    }
    return units;
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

const BattleRuntimeUnit& unitById(const BattleUnitStore& units, int id)
{
    return units.requireUnit(id);
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
        auto units = effectUnits();
        std::map<int, int> activationCounts;
        BattleEffectDispatcher dispatcher(registry);
        dispatcher.addEffect(effect(static_cast<int>(i + 1),
            BattleHook::AttackHit,
            cases[i].target,
            "添加護盾",
            25));

        auto commands = dispatcher.dispatch(units, activationCounts, { BattleHook::AttackHit, 1, 3 });
        REQUIRE(commands.size() == cases[i].expectedUnitIds.size());
        for (int id : cases[i].expectedUnitIds)
        {
            CHECK(unitById(units, id).shield == 25);
        }
    }
}

TEST_CASE("BattleEffectDispatcher_SharedExecutors_MutateOnlyResolvedTargets", "[battle][effects][unit]")
{
    BattleEffectRegistry registry;

    SECTION("heal percent caps at max hp")
    {
        auto units = effectUnits();
        std::map<int, int> activationCounts;
        BattleEffectDispatcher dispatcher(registry);
        dispatcher.addEffect(effect(10, BattleHook::AfterHeal, BattleEffectTarget::Target, "治療百分比", 80));

        auto commands = dispatcher.dispatch(units, activationCounts, { BattleHook::AfterHeal, 1, 3 });
        REQUIRE(commands.size() == 1);
        CHECK(commands[0].type == BattleEffectCommandType::Heal);
        CHECK(commands[0].targetUnitId == 3);
        CHECK(commands[0].value == 70);
        CHECK(unitById(units, 3).vitals.hp == 100);
        CHECK(unitById(units, 1).vitals.hp == 50);
    }

    SECTION("invincibility uses max remaining frames")
    {
        auto units = effectUnits();
        std::map<int, int> activationCounts;
        units.units[3].invincible = 12;
        BattleEffectDispatcher dispatcher(registry);
        dispatcher.addEffect(effect(11, BattleHook::SkillFinished, BattleEffectTarget::Source, "添加無敵幀", 30));

        auto commands = dispatcher.dispatch(units, activationCounts, { BattleHook::SkillFinished, 3, -1 });
        REQUIRE(commands.size() == 1);
        CHECK(commands[0].type == BattleEffectCommandType::AddInvincibility);
        CHECK(unitById(units, 3).invincible == 30);
    }

    SECTION("cooldown percent modifier does not create cooldown from zero")
    {
        auto units = effectUnits();
        std::map<int, int> activationCounts;
        units.units[1].animation.cooldown = 100;
        units.units[2].animation.cooldown = 0;
        BattleEffectDispatcher dispatcher(registry);
        dispatcher.addEffect(effect(12, BattleHook::AfterHeal, BattleEffectTarget::SourceTeam, "冷卻百分比修正", 25));

        auto commands = dispatcher.dispatch(units, activationCounts, { BattleHook::AfterHeal, 1, 3 });
        REQUIRE(commands.size() == 1);
        CHECK(commands[0].targetUnitId == 1);
        CHECK(unitById(units, 1).animation.cooldown == 75);
        CHECK(unitById(units, 2).animation.cooldown == 0);
    }
}

TEST_CASE("BattleEffectDispatcher_SharedExecutors_CoverNamedBattleInvincibilityEffects", "[battle][effects][unit]")
{
    const std::vector<std::string> invincibilityExecutors = {
        "受傷無敵",
        "死亡無敵",
        "技能後無敵",
    };

    for (size_t i = 0; i < invincibilityExecutors.size(); ++i)
    {
        auto units = effectUnits();
        std::map<int, int> activationCounts;
        BattleEffectRegistry registry;
        BattleEffectDispatcher dispatcher(registry);
        dispatcher.addEffect(effect(static_cast<int>(30 + i),
            BattleHook::SkillFinished,
            BattleEffectTarget::Source,
            invincibilityExecutors[i],
            20));

        auto commands = dispatcher.dispatch(units, activationCounts, { BattleHook::SkillFinished, 1, -1 });
        REQUIRE(commands.size() == 1);
        CHECK(commands[0].type == BattleEffectCommandType::AddInvincibility);
        CHECK(commands[0].label == invincibilityExecutors[i]);
        CHECK(unitById(units, 1).invincible == 20);
    }
}

TEST_CASE("BattleEffectDispatcher_SharedExecutors_RestoreResourceAndEmitDelta", "[battle][effects][unit]")
{
    auto units = effectUnits();
    std::map<int, int> activationCounts;
    units.units[1].vitals.mp = 85;
    BattleEffectRegistry registry;
    BattleEffectDispatcher dispatcher(registry);
    dispatcher.addEffect(effect(40, BattleHook::AfterHeal, BattleEffectTarget::Source, "回內力", 30));

    auto commands = dispatcher.dispatch(units, activationCounts, { BattleHook::AfterHeal, 1, 3 });
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == BattleEffectCommandType::ModifyResource);
    CHECK(commands[0].sourceUnitId == 1);
    CHECK(commands[0].targetUnitId == 1);
    CHECK(commands[0].value == 15);
    CHECK(unitById(units, 1).vitals.mp == 100);
}

TEST_CASE("BattleEffectDispatcher_HookAndActivationLimit_AreDeterministic", "[battle][effects][unit]")
{
    auto units = effectUnits();
    std::map<int, int> activationCounts;
    BattleEffectRegistry registry;
    BattleEffectDispatcher dispatcher(registry);
    dispatcher.addEffect(effect(20, BattleHook::AttackHit, BattleEffectTarget::Target, "專用執行器", 7, 1));
    dispatcher.addEffect(effect(21, BattleHook::Frame, BattleEffectTarget::Target, "專用執行器", 99));

    auto first = dispatcher.dispatch(units, activationCounts, { BattleHook::AttackHit, 1, 3 });
    auto second = dispatcher.dispatch(units, activationCounts, { BattleHook::AttackHit, 1, 3 });
    auto wrongHook = dispatcher.dispatch(units, activationCounts, { BattleHook::SkillFinished, 1, 3 });

    REQUIRE(first.size() == 1);
    CHECK(first[0].type == BattleEffectCommandType::DedicatedEffect);
    CHECK(first[0].effectId == 20);
    CHECK(first[0].sourceUnitId == 1);
    CHECK(first[0].targetUnitId == 3);
    CHECK(first[0].value == 7);
    CHECK(second.empty());
    CHECK(wrongHook.empty());
    CHECK(activationCounts[20] == 1);
    CHECK(activationCounts[21] == 0);
}

TEST_CASE("BattleStatusSystem_PoisonBleedAndTimers_TickPerUnit", "[battle][status][unit]")
{
    BattleStatusUnitState unit;
    unit.id = 7;
    unit.alive = true;
    unit.hp = 80;
    unit.maxHp = 200;
    unit.attack = 120;
    unit.effects.poisonTimer = 2;
    unit.effects.poisonTickPct = 10;
    unit.effects.poisonSourceId = 1;
    unit.effects.bleedStacks = 3;
    unit.effects.bleedTimer = 1;
    unit.effects.bleedSourceId = 2;
    unit.effects.mpBlockTimer = 3;
    unit.effects.damageImmunityAfterFrames = 4;
    unit.effects.damageImmunityDuration = 11;
    unit.effects.damageImmunityTimer = 1;
    unit.effects.tempAttackBuffs.push_back({ 15, 1 });
    unit.effects.damageReduceDebuffs.push_back({ 1, 30 });

    auto result = BattleStatusSystem({ 30 }).tick(unit);

    CHECK(unit.attack == 105);
    CHECK(unit.effects.poisonTimer == 1);
    CHECK(unit.effects.bleedTimer == 10);
    CHECK(unit.effects.bleedStacks == 3);
    CHECK(unit.effects.mpBlockTimer == 2);
    CHECK(unit.effects.damageReduceDebuffs.empty());
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
    unit.effects.bleedStacks = 5;
    unit.effects.bleedTimer = 0;
    unit.effects.tempAttackBuffs.push_back({ 10, 1 });

    auto result = BattleStatusSystem({ 30 }).tick(unit);
    CHECK(result.events.empty());
    CHECK(unit.attack == 100);
    CHECK(unit.effects.bleedStacks == 5);
}
