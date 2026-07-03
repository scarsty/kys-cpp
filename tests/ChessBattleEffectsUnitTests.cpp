#include "ChessBattleEffects.h"

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

#include <vector>

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

TEST_CASE("ChessBattleEffects_StoresConfiguredEffectsWithStableIds", "[battle][effects]")
{
    RoleComboState state;

    const auto first = state.applyConfiguredEffect(effect(EffectType::MPOnHit, 5));
    const auto second = state.applyConfiguredEffect(effect(EffectType::MPOnHit, 7));
    const auto stun = state.applyConfiguredEffect(effect(EffectType::Stun, 12, 0, Trigger::OnHit));

    REQUIRE(state.effectIdsInAppendOrder().size() == 3);
    CHECK(first == RoleComboEffectId{ 0 });
    CHECK(second == RoleComboEffectId{ 1 });
    CHECK(stun == RoleComboEffectId{ 2 });
    CHECK(state.effect(first).type == EffectType::MPOnHit);
    CHECK(state.effect(second).type == EffectType::MPOnHit);
    CHECK(state.effect(stun).type == EffectType::Stun);
    CHECK(state.effect(stun).origin == RoleComboEffectOrigin::Configured);

    REQUIRE(state.effectIds(Trigger::OnHit, EffectType::Stun).size() == 1);
    CHECK(state.effectIds(Trigger::OnHit, EffectType::Stun)[0] == stun);
    CHECK(state.effectIdsInAppendOrder().size() == 3);
}

TEST_CASE("ChessBattleEffects_ComboEffectDescSeparatesChanceTriggerFromDescription", "[chess][effects]")
{
    ComboEffect eff = effect(EffectType::NearbyTrackingProjectiles, 220, 40, Trigger::OnHit);
    eff.triggerValue = 25;

    CHECK(comboEffectDesc(eff) == "25%擊中觸發: 命中時向220範圍內敵人各發一枚40%追蹤彈");
}

TEST_CASE("ChessBattleEffects_ShieldBreakCompactDescOmitsDefaultChance", "[chess][effects]")
{
    ComboEffect alwaysShieldBreak = effect(EffectType::AutoUltimate, 1, 0, Trigger::OnShieldBreak);
    CHECK(comboEffectCompactDesc(alwaysShieldBreak) == "盾爆·自動絕招");

    ComboEffect chanceShieldBreak = alwaysShieldBreak;
    chanceShieldBreak.triggerValue = 50;
    CHECK(comboEffectCompactDesc(chanceShieldBreak) == "盾爆50%·自動絕招");
}

TEST_CASE("ChessBattleEffects_ParseAndDescribeEnemyMpDamageAll", "[chess][effects]")
{
    auto node = YAML::Load(R"(
类型: 全场杀内
数值: 10
触发: 攻擊出手時概率
触发参数: 100
)");

    ComboEffect eff;
    REQUIRE(ChessBattleEffects::parseEffect(node, eff, "測試全場殺內"));

    CHECK(eff.type == EffectType::EnemyMpDamageAll);
    CHECK(eff.value == 10);
    CHECK(eff.trigger == Trigger::OnCast);
    CHECK(eff.triggerValue == 100);
    CHECK(comboEffectDesc(eff) == "全場敵人內力-10");
    CHECK(comboEffectCompactDesc(eff) == "全敵內-10");
}

TEST_CASE("ChessBattleEffects_RuntimeGrantsShareStoreButKeepOrigin", "[battle][effects]")
{
    RoleComboState state;

    const auto configuredMp = state.applyConfiguredEffect(effect(EffectType::MPOnHit, 5), 10);
    const auto grantedMp = state.grantRuntimeEffect(effect(EffectType::MPOnHit, 7), 20);
    const auto configuredStun = state.applyConfiguredEffect(effect(EffectType::Stun, 12, 0, Trigger::OnHit), 10);
    const auto grantedStun = state.grantRuntimeEffect(effect(EffectType::Stun, 14, 0, Trigger::OnHit), 20);

    CHECK(state.sumAlways(EffectType::MPOnHit) == 12);
    CHECK(state.hasComboApplied(10));
    CHECK(state.hasComboApplied(20));
    CHECK(state.effect(grantedMp).origin == RoleComboEffectOrigin::RuntimeGrant);
    CHECK(state.effect(grantedStun).origin == RoleComboEffectOrigin::RuntimeGrant);

    REQUIRE(state.effectIdsInAppendOrder().size() == 4);
    CHECK(state.effectIdsInAppendOrder()[0] == configuredMp);
    CHECK(state.effectIdsInAppendOrder()[1] == grantedMp);
    CHECK(state.effectIdsInAppendOrder()[2] == configuredStun);
    CHECK(state.effectIdsInAppendOrder()[3] == grantedStun);

    std::vector<RoleComboEffectId> configuredIds;
    for (RoleComboEffectId id : state.effectIdsInAppendOrder())
    {
        if (state.effect(id).origin == RoleComboEffectOrigin::Configured)
        {
            configuredIds.push_back(id);
        }
    }
    REQUIRE(configuredIds.size() == 2);
    CHECK(configuredIds[0] == configuredMp);
    CHECK(configuredIds[1] == configuredStun);

    REQUIRE(state.effectIds(Trigger::OnHit, EffectType::Stun).size() == 2);
    CHECK(state.effectIds(Trigger::OnHit, EffectType::Stun)[0] == configuredStun);
    CHECK(state.effectIds(Trigger::OnHit, EffectType::Stun)[1] == grantedStun);
}

TEST_CASE("ChessBattleEffects_TypedLookupReturnsConfiguredAndRuntimeEffects", "[battle][effects]")
{
    RoleComboState state;

    const auto configured = state.applyConfiguredEffect(effect(EffectType::Stun, 12, 0, Trigger::OnHit));
    const auto runtime = state.grantRuntimeEffect(effect(EffectType::Stun, 14, 0, Trigger::OnHit));

    REQUIRE(state.effectIds(Trigger::OnHit, EffectType::Stun).size() == 2);
    CHECK(state.effectIds(Trigger::OnHit, EffectType::Stun)[0] == configured);
    CHECK(state.effectIds(Trigger::OnHit, EffectType::Stun)[1] == runtime);
    CHECK(state.effect(configured).origin == RoleComboEffectOrigin::Configured);
    CHECK(state.effect(runtime).origin == RoleComboEffectOrigin::RuntimeGrant);
}

TEST_CASE("ChessBattleEffects_ConfiguredEffectsAreSelectedByOrigin", "[battle][effects]")
{
    RoleComboState state;

    const auto configured = state.applyConfiguredEffect(effect(EffectType::FlatShield, 5));
    const auto runtime = state.grantRuntimeEffect(effect(EffectType::FlatShield, 9));

    std::vector<RoleComboEffectId> configuredIds;
    for (RoleComboEffectId id : state.effectIds(Trigger::Always, EffectType::FlatShield))
    {
        if (state.effect(id).origin == RoleComboEffectOrigin::Configured)
        {
            configuredIds.push_back(id);
        }
    }

    REQUIRE(configuredIds.size() == 1);
    CHECK(configuredIds[0] == configured);
    CHECK(configuredIds[0] != runtime);
}

TEST_CASE("ChessBattleEffects_QueryHelpersReadConfiguredAndGrantedEffects", "[battle][effects]")
{
    RoleComboState state;

    state.applyConfiguredEffect(effect(EffectType::MPOnHit, 5));
    state.applyConfiguredEffect(effect(EffectType::MPOnHit, 7));
    state.applyConfiguredEffect(effect(EffectType::FlatShield, 13));
    state.applyConfiguredEffect(effect(EffectType::ShieldPctMaxHP, 25));
    state.applyConfiguredEffect(effect(EffectType::ForceRangedAttack, 150, 4));
    state.grantRuntimeEffect(effect(EffectType::MPOnHit, 3));

    CHECK(state.sumAlways(EffectType::MPOnHit) == 15);
    CHECK(state.sumAlways(EffectType::FlatShield) == 13);
    CHECK(state.maxAlways(EffectType::ShieldPctMaxHP) == 25);
    CHECK(state.maxAlwaysValue2(EffectType::ForceRangedAttack) == 4);

    const auto* firstForceRanged = state.firstAlways(EffectType::ForceRangedAttack);
    REQUIRE(firstForceRanged != nullptr);
    CHECK(firstForceRanged->value == 150);
    CHECK(firstForceRanged->value2 == 4);

    const auto* maxForceRanged = state.maxAlwaysByValue(EffectType::ForceRangedAttack);
    REQUIRE(maxForceRanged != nullptr);
    CHECK(maxForceRanged->value == 150);
    CHECK(maxForceRanged->value2 == 4);
}

TEST_CASE("ChessBattleEffects_HasAlwaysUsesUnifiedAlwaysIndex", "[battle][effects]")
{
    RoleComboState state;

    CHECK_FALSE(state.hasAlways(EffectType::MPOnHit));
    state.applyConfiguredEffect(effect(EffectType::MPOnHit, 5));
    state.grantRuntimeEffect(effect(EffectType::FlatShield, 13));
    state.applyConfiguredEffect(effect(EffectType::Stun, 12, 0, Trigger::OnHit));

    CHECK(state.hasAlways(EffectType::MPOnHit));
    CHECK(state.hasAlways(EffectType::FlatShield));
    CHECK_FALSE(state.hasAlways(EffectType::Stun));
}

TEST_CASE("ChessBattleEffects_QueryHelpersKeepPairedEffectValues", "[battle][effects]")
{
    RoleComboState state;

    state.applyConfiguredEffect(effect(EffectType::DamageImmunityAfterFrames, 5, 2));
    state.applyConfiguredEffect(effect(EffectType::DamageImmunityAfterFrames, 12, 5));
    state.applyConfiguredEffect(effect(EffectType::DamageImmunityAfterFrames, 3, 9));

    CHECK(state.maxAlways(EffectType::DamageImmunityAfterFrames) == 12);
    CHECK(state.maxAlwaysValue2(EffectType::DamageImmunityAfterFrames) == 9);

    const auto* immunity = state.maxAlwaysByValue(EffectType::DamageImmunityAfterFrames);
    REQUIRE(immunity != nullptr);
    CHECK(immunity->value == 12);
    CHECK(immunity->value2 == 5);
}

TEST_CASE("ChessBattleEffects_RuntimeStateTracksPerEffectCountersAndTimers", "[battle][effects]")
{
    RoleComboState state;
    const auto first = state.applyConfiguredEffect(effect(EffectType::EveryNthDouble, 2));
    const auto second = state.applyConfiguredEffect(effect(EffectType::EveryNthDouble, 2));

    CHECK_FALSE(state.advanceEffectCounter(first, 2));
    CHECK(state.advanceEffectCounter(first, 2));
    CHECK_FALSE(state.advanceEffectCounter(second, 2));

    state.setEffectFrameTimer(first, 3);
    CHECK(state.effectFrameTimerFrames(first) == 3);
    CHECK_FALSE(state.advanceEffectFrameTimer(first, 3));
    CHECK_FALSE(state.advanceEffectFrameTimer(first, 3));
    CHECK(state.advanceEffectFrameTimer(first, 3));
}

TEST_CASE("ChessBattleEffects_RuntimeStateTracksEffectTypePendingAndToggles", "[battle][effects]")
{
    RoleComboState state;

    CHECK_FALSE(state.consumeTypePending(EffectType::OnSkillTeamHeal));
    state.setTypePending(EffectType::OnSkillTeamHeal, true);
    CHECK(state.typePending(EffectType::OnSkillTeamHeal));
    CHECK(state.consumeTypePending(EffectType::OnSkillTeamHeal));
    CHECK_FALSE(state.consumeTypePending(EffectType::OnSkillTeamHeal));

    CHECK_FALSE(state.typeToggle(EffectType::BlinkAttack));
    CHECK_FALSE(state.consumeTypeToggle(EffectType::BlinkAttack));
    CHECK(state.typeToggle(EffectType::BlinkAttack));
    CHECK(state.consumeTypeToggle(EffectType::BlinkAttack));
    CHECK_FALSE(state.typeToggle(EffectType::BlinkAttack));
}
