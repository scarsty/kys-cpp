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
    CHECK(rows[0].name == "護盾");
    CHECK(rows[0].value == "200");
    CHECK(rows[0].icon == BattleEffectIcon::Shield);
    CHECK_FALSE(rows[0].negative);
    CHECK(rows[1].negative);
    CHECK(rows[1].name == "眩暈");
    CHECK(rows[1].icon == BattleEffectIcon::Stun);
    CHECK(rows[1].duration == "剩 18 幀");
    CHECK(rows[1].source == "同名棋子");
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
