#include "ChessBattleEffects.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

namespace
{

ComboEffect effect(EffectType type, int value, int value2 = 0, Trigger trigger = Trigger::Always)
{
    ComboEffect result;
    result.type = type;
    result.value = value;
    result.value2 = value2;
    result.trigger = trigger;
    return result;
}

}  // namespace

TEST_CASE("ChessBattleEffects_StoresRuntimeEffectsAsConfiguredInstances", "[battle][effects]")
{
    RoleComboState state;

    ChessBattleEffects::applyEffect(state, effect(EffectType::MPOnHit, 5));
    ChessBattleEffects::applyEffect(state, effect(EffectType::MPOnHit, 7));
    ChessBattleEffects::applyEffect(state, effect(EffectType::Stun, 12, 0, Trigger::OnHit));

    REQUIRE(state.appliedEffects.size() == 3);
    CHECK(state.appliedEffects[0].type == EffectType::MPOnHit);
    CHECK(state.appliedEffects[1].type == EffectType::MPOnHit);
    CHECK(state.appliedEffects[2].type == EffectType::Stun);
    REQUIRE(state.triggeredEffects.size() == 1);
    CHECK(state.triggeredEffects[0].type == EffectType::Stun);
}

TEST_CASE("ChessBattleEffects_QueryHelpersReadConfiguredEffects", "[battle][effects]")
{
    RoleComboState state;

    ChessBattleEffects::applyEffect(state, effect(EffectType::MPOnHit, 5));
    ChessBattleEffects::applyEffect(state, effect(EffectType::MPOnHit, 7));
    ChessBattleEffects::applyEffect(state, effect(EffectType::FlatShield, 13));
    ChessBattleEffects::applyEffect(state, effect(EffectType::ShieldPctMaxHP, 25));
    ChessBattleEffects::applyEffect(state, effect(EffectType::ForceRangedAttack, 150, 4));

    CHECK(sumAlwaysEffectValue(state, EffectType::MPOnHit) == 12);
    CHECK(sumAlwaysEffectValue(state, EffectType::FlatShield) == 13);
    CHECK(maxAlwaysEffectValue(state, EffectType::ShieldPctMaxHP) == 25);
    CHECK(maxAlwaysEffectValue2(state, EffectType::ForceRangedAttack) == 4);
    REQUIRE(firstAlwaysEffect(state, EffectType::ForceRangedAttack) != nullptr);
    CHECK(firstAlwaysEffect(state, EffectType::ForceRangedAttack)->value == 150);
    CHECK(firstAlwaysEffect(state, EffectType::ForceRangedAttack)->value2 == 4);
    REQUIRE(maxAlwaysEffectByValue(state, EffectType::ForceRangedAttack) != nullptr);
    CHECK(maxAlwaysEffectByValue(state, EffectType::ForceRangedAttack)->value == 150);
    CHECK(maxAlwaysEffectByValue(state, EffectType::ForceRangedAttack)->value2 == 4);
}

TEST_CASE("ChessBattleEffects_QueryHelpersKeepPairedEffectValues", "[battle][effects]")
{
    RoleComboState state;

    ChessBattleEffects::applyEffect(state, effect(EffectType::DamageImmunityAfterFrames, 5, 2));
    ChessBattleEffects::applyEffect(state, effect(EffectType::DamageImmunityAfterFrames, 12, 5));
    ChessBattleEffects::applyEffect(state, effect(EffectType::DamageImmunityAfterFrames, 3, 9));

    CHECK(maxAlwaysEffectValue(state, EffectType::DamageImmunityAfterFrames) == 12);
    CHECK(maxAlwaysEffectValue2(state, EffectType::DamageImmunityAfterFrames) == 9);
    REQUIRE(maxAlwaysEffectByValue(state, EffectType::DamageImmunityAfterFrames) != nullptr);
    CHECK(maxAlwaysEffectByValue(state, EffectType::DamageImmunityAfterFrames)->value == 12);
    CHECK(maxAlwaysEffectByValue(state, EffectType::DamageImmunityAfterFrames)->value2 == 5);
}
