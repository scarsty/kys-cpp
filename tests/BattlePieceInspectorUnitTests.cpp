#include "BattlePieceInspector.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Battle;

TEST_CASE("Battle piece inspector reads live effects and keeps unit identity distinct", "[battle_inspector]")
{
    BattleRuntimeState runtime;
    BattleRuntimeUnitRecord first;
    first.core.id = 3;
    first.core.name = "同名棋子";
    first.core.shield = 200;
    first.status.effects.setFrames(BattleStatusKind::Stun, 18, 30, 3);
    runtime.units.append(std::move(first));
    BattleRuntimeUnitRecord clone;
    clone.core.id = 9;
    clone.core.cloneSourceUnitId = 3;
    clone.core.name = "同名棋子";
    runtime.units.append(std::move(clone));

    auto rows = battlePieceEffectRows(runtime, 3);
    REQUIRE(rows.size() == 2);
    CHECK(rows[1].name == "護盾");
    CHECK(rows[1].value == "200");
    CHECK(rows[1].icon == BattleEffectIcon::Shield);
    CHECK_FALSE(rows[1].negative);
    CHECK(rows[0].negative);
    CHECK(rows[0].lifetime == BattleEffectLifetime::Timed);
    CHECK(rows[1].lifetime == BattleEffectLifetime::Consumable);
    CHECK(rows[0].name == "眩暈");
    CHECK(rows[0].icon == BattleEffectIcon::Stun);
    CHECK(rows[0].duration == "剩 18 幀");
    CHECK(rows[0].source == "同名棋子");
    CHECK(battlePieceEffectRows(runtime, 9).empty());

    runtime.units.require(3).status.effects.clear(BattleStatusKind::Stun);
    runtime.units.requireCore(3).shield = 0;
    runtime.units.requireCore(3).alive = false;
    CHECK(battlePieceEffectRows(runtime, 3).empty());
}

TEST_CASE("Battle piece inspector excludes expired modifiers and describes operations faithfully", "[battle_inspector]")
{
    BattleRuntimeState runtime;
    BattleRuntimeUnitRecord record;
    record.core.id = 0;
    record.core.name = "測試棋子";
    runtime.units.append(std::move(record));
    runtime.movement.frame = 100;
    BattleAttributeModifierInstance modifier;
    modifier.targetUnitId = 0;
    modifier.attribute = BattleAttribute::Attack;
    modifier.operation = AttributeOperation::PercentAdd;
    modifier.amount = -20;
    modifier.negative = true;
    modifier.expiresFrameExclusive = 112;
    modifier.stackCount = 2;
    runtime.effectCommands.attributeModifiers.push_back(modifier);
    modifier.expiresFrameExclusive = 100;
    runtime.effectCommands.attributeModifiers.push_back(modifier);
    const auto rows = battlePieceEffectRows(runtime, 0);
    REQUIRE(rows.size() == 1);
    CHECK(rows.front().negative);
    CHECK(rows.front().name == "攻擊");
    CHECK(rows.front().value == "-20%");
    CHECK(rows.front().quantity == "2層");
    CHECK(rows.front().duration == "剩 12 幀");
    CHECK(rows.front().icon == BattleEffectIcon::Attack);
    CHECK(battleInspectorAttributeAmount(BattleAttribute::Attack, AttributeOperation::Multiply, 80) == "×80%");
    CHECK(battleInspectorAttributeAmount(BattleAttribute::Attack, AttributeOperation::Override, 20) == "設為 20");
    CHECK(battleInspectorAttributeAmount(BattleAttribute::CriticalChance, AttributeOperation::FlatAdd, 35) == "+35 個百分點");
    CHECK(battleInspectorDuration(std::nullopt) == "持續生效");
}

TEST_CASE("Battle inspector prioritizes temporary effects over ongoing bonuses and consumable protection", "[battle_inspector]")
{
    BattleRuntimeState runtime;
    BattleRuntimeUnitRecord record;
    record.core.id = 0;
    record.core.invincible = 24;
    record.core.shield = 80;
    record.status.effects.setFrames(BattleStatusKind::Stun, 18, 30);
    runtime.units.append(std::move(record));
    runtime.movement.frame = 100;

    BattleAttributeModifierInstance modifier;
    modifier.targetUnitId = 0;
    modifier.attribute = BattleAttribute::Attack;
    modifier.operation = AttributeOperation::FlatAdd;
    modifier.amount = 10;
    modifier.stackCount = 1;
    runtime.effectCommands.attributeModifiers.push_back(modifier);
    modifier.attribute = BattleAttribute::Defence;
    modifier.amount = 20;
    modifier.expiresFrameExclusive = 112;
    runtime.effectCommands.attributeModifiers.push_back(modifier);

    const auto rows = battlePieceEffectRows(runtime, 0);
    REQUIRE(rows.size() == 5);
    // 同類效果維持穩定順序；限時減益也必須排在常駐增益之前。
    CHECK(rows[0].name == "無敵");
    CHECK(rows[1].name == "防禦");
    CHECK(rows[2].name == "眩暈");
    for (int i = 0; i < 3; ++i) CHECK(rows[i].lifetime == BattleEffectLifetime::Timed);
    CHECK(rows[1].duration == "剩 12 幀");
    CHECK(rows[2].negative);
    CHECK(rows[3].name == "護盾");
    CHECK(rows[3].lifetime == BattleEffectLifetime::Consumable);
    CHECK(rows[4].name == "攻擊");
    CHECK(rows[4].lifetime == BattleEffectLifetime::Ongoing);

    runtime.movement.frame = 112;
    runtime.units.requireCore(0).invincible = 0;
    runtime.units.require(0).status.effects.clear(BattleStatusKind::Stun);
    const auto expired = battlePieceEffectRows(runtime, 0);
    REQUIRE(expired.size() == 2);
    CHECK(expired[0].lifetime == BattleEffectLifetime::Consumable);
    CHECK(expired[1].lifetime == BattleEffectLifetime::Ongoing);
}

TEST_CASE("Battle inspector distinguishes timed charges from durationless charges and ongoing layers", "[battle_inspector]")
{
    BattleRuntimeState runtime;
    BattleRuntimeUnitRecord record;
    record.core.id = 0;
    record.status.effects.statuses.push_back({.kind = BattleStatusKind::BattleSpirit, .stacks = 3});
    record.status.effects.statuses.push_back({.kind = BattleStatusKind::Blinded, .stacks = 2});
    record.status.effects.statuses.push_back({.kind = BattleStatusKind::NextAttackMiss,
        .remainingFrames = 30, .maximumFrames = 30, .stacks = 1});
    runtime.units.append(std::move(record));
    const auto rows = battlePieceEffectRows(runtime, 0);
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].lifetime == BattleEffectLifetime::Timed);
    CHECK(rows[0].quantity == "1次");
    CHECK(rows[1].lifetime == BattleEffectLifetime::Consumable);
    CHECK(rows[1].negative);
    CHECK(rows[1].quantity == "2次");
    CHECK(rows[2].lifetime == BattleEffectLifetime::Ongoing);
    CHECK(rows[2].quantity == "3層");
}
