#include "ChessBattleEffects.h"
#include "ChessMagicEffectDisplay.h"
#include "Types.h"
#include "battle/BattleEffectRuntimeRegistry.h"

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

TEST_CASE("ChessBattleEffects_RuntimeStateClearsEveryEffectTypePending", "[battle][effects]")
{
    RoleComboState state;

    state.setTypePending(EffectType::OnSkillTeamHeal, true);
    state.setTypePending(EffectType::DodgeThenCrit, true);
    state.consumeTypeToggle(EffectType::BlinkAttack);

    state.clearTypePending();

    CHECK_FALSE(state.typePending(EffectType::OnSkillTeamHeal));
    CHECK_FALSE(state.typePending(EffectType::DodgeThenCrit));
    CHECK(state.typeToggle(EffectType::BlinkAttack));
}

TEST_CASE("ChessBattleEffects_MagicYamlLoadsDefinitionsAndNormalizesOffensiveCharmPair", "[battle][effects][magic]")
{
    auto root = YAML::Load(R"(
武功效果:
  - 武功: 5
    名称: 寒冰綿掌
    用途: 普通
    效果:
      - 类型: 攻击冷却延长
        数值: 30
        附加参数: 35
        触发: 攻击命中时概率
        触发参数: 100
)");

    std::vector<ChessMagicEffectDefinition> definitions;
    REQUIRE(ChessBattleEffects::parseMagicEffects(root, definitions, "測試武功效果"));

    REQUIRE(definitions.size() == 1);
    CHECK(definitions[0].magicId == 5);
    REQUIRE(definitions[0].effects.size() == 2);
    CHECK(definitions[0].effects[0].type == EffectType::OffensiveCharm);
    CHECK(definitions[0].effects[0].value == 30);
    CHECK(definitions[0].effects[0].value2 == 35);
    CHECK(definitions[0].effects[0].trigger == Trigger::OnHit);
    CHECK(definitions[0].effects[1].type == EffectType::CharmCDRDebuff);
    CHECK(definitions[0].effects[1].value == 30);
    CHECK(definitions[0].effects[1].value2 == 35);
    CHECK(definitions[0].effects[1].trigger == Trigger::OnHit);
}

TEST_CASE("BattleEffectRuntimeRegistry_DefinesSelectedSkillMagicPolicy", "[battle][effects][magic]")
{
    using KysChess::Battle::BattleEffectRuntimePhase;

    CHECK(KysChess::Battle::isSelectedSkillMagicAllowed(EffectType::PoisonDOT, Trigger::OnHit));
    CHECK(KysChess::Battle::runtimePhaseFor(EffectType::PoisonDOT, Trigger::OnHit)
        == BattleEffectRuntimePhase::HitStatusPayload);
    CHECK(KysChess::Battle::isUltimateOnlyMagicEffect(EffectType::PoisonDOT, Trigger::OnHit));

    CHECK(KysChess::Battle::runtimePhaseFor(EffectType::OnSkillTeamHeal, Trigger::Always)
        == BattleEffectRuntimePhase::CastFinish);
    CHECK_FALSE(KysChess::Battle::isSelectedSkillMagicAllowed(EffectType::OnSkillTeamHeal, Trigger::Always));

    CHECK(KysChess::Battle::isSelectedSkillMagicAllowed(EffectType::OnSkillTeamHeal, Trigger::OnUltimate));
    CHECK(KysChess::Battle::runtimePhaseFor(EffectType::OnSkillTeamHeal, Trigger::OnUltimate)
        == BattleEffectRuntimePhase::CastFinish);
    CHECK(KysChess::Battle::isUltimateOnlyMagicEffect(EffectType::OnSkillTeamHeal, Trigger::OnUltimate));
}

TEST_CASE("ChessBattleEffects_MagicYamlUsesRuntimeRegistryForCastFinishScope", "[battle][effects][magic]")
{
    auto ultimateFinishHeal = YAML::Load(R"(
武功效果:
  - 武功: 26
    名稱: 降龍十八掌
    用途: 普通
    效果:
      - 类型: 群体施治
        数值: 12
        触发: 绝招时
        触发参数: 100
)");
    auto alwaysFinishHeal = YAML::Load(R"(
武功效果:
  - 武功: 26
    名稱: 降龍十八掌
    用途: 絕招
    效果:
      - 类型: 群体施治
        数值: 12
)");

    std::vector<ChessMagicEffectDefinition> definitions;
    REQUIRE(ChessBattleEffects::parseMagicEffects(ultimateFinishHeal, definitions, "絕招完成群療"));
    REQUIRE(definitions.size() == 1);
    REQUIRE(definitions[0].effects.size() == 1);
    CHECK(definitions[0].effects[0].type == EffectType::OnSkillTeamHeal);
    CHECK(definitions[0].effects[0].trigger == Trigger::OnUltimate);

    CHECK_FALSE(ChessBattleEffects::parseMagicEffects(alwaysFinishHeal, definitions, "Always 群療"));
}

TEST_CASE("ChessMagicEffectDisplay_InsertsCompactEffectRowsAfterUltimateSkill", "[chess][effects][magic]")
{
    Role role;
    role.Star = 1;
    role.MagicID[0] = 5;
    role.MagicPower[0] = 40;
    role.MagicID[1] = 26;
    role.MagicPower[1] = 80;

    Magic normal;
    normal.ID = 5;
    normal.Name = "寒冰綿掌";
    Magic ultimate;
    ultimate.ID = 26;
    ultimate.Name = "降龍十八掌";

    ComboEffect stun = effect(EffectType::Stun, 14, 0, Trigger::OnHit);
    stun.triggerValue = 100;
    ComboEffect charm = effect(EffectType::OffensiveCharm, 30, 35, Trigger::OnHit);
    charm.triggerValue = 100;
    ComboEffect charmPayload = charm;
    charmPayload.type = EffectType::CharmCDRDebuff;

    std::vector<Magic*> magics{ &normal, &ultimate };
    std::vector<ChessMagicEffectDefinition> definitions{
        { 26, "降龍十八掌", { stun, charm, charmPayload }, "普通+絕招" },
    };

    auto rows = buildChessMagicEffectDisplayRows(role, 1, magics, definitions);

    REQUIRE(rows.size() == 4);
    CHECK(rows[0].kind == ChessMagicEffectDisplayLineKind::Skill);
    CHECK(rows[0].text == "寒冰綿掌");
    CHECK_FALSE(rows[0].ultimate);
    CHECK(rows[1].kind == ChessMagicEffectDisplayLineKind::Skill);
    CHECK(rows[1].text == "降龍十八掌");
    CHECK(rows[1].ultimate);
    CHECK(rows[2].kind == ChessMagicEffectDisplayLineKind::Effect);
    CHECK(rows[2].text == "眩暈(14幀)");
    CHECK(rows[3].kind == ChessMagicEffectDisplayLineKind::Effect);
    CHECK(rows[3].text == "30%增敵CD35%");
}

TEST_CASE("ChessBattleEffects_DisabledMagicEffectsSkipRuntimeDefinitionsAndPanelRows", "[battle][effects][magic]")
{
    auto root = YAML::Load(R"(
啟用: false
武功效果:
  - 武功: 26
    名稱: 降龍十八掌
    用途: 絕招
    效果:
      - 类型: 眩晕
        数值: 14
        触发: 攻击命中时概率
        触发参数: 100
)");

    std::vector<ChessMagicEffectDefinition> definitions;
    REQUIRE(ChessBattleEffects::parseMagicEffects(root, definitions, "停用武功效果"));
    CHECK(definitions.empty());

    Role role;
    role.Star = 1;
    role.MagicID[0] = 5;
    role.MagicPower[0] = 40;
    role.MagicID[1] = 26;
    role.MagicPower[1] = 80;

    Magic normal;
    normal.ID = 5;
    normal.Name = "寒冰綿掌";
    Magic ultimate;
    ultimate.ID = 26;
    ultimate.Name = "降龍十八掌";

    std::vector<Magic*> magics{ &normal, &ultimate };
    auto rows = buildChessMagicEffectDisplayRows(role, 1, magics, definitions);

    REQUIRE(rows.size() == 2);
    CHECK(rows[0].kind == ChessMagicEffectDisplayLineKind::Skill);
    CHECK(rows[0].text == "寒冰綿掌");
    CHECK_FALSE(rows[0].ultimate);
    CHECK(rows[1].kind == ChessMagicEffectDisplayLineKind::Skill);
    CHECK(rows[1].text == "降龍十八掌");
    CHECK(rows[1].ultimate);
}

TEST_CASE("ChessBattleEffects_MagicYamlRejectsDuplicateIdsAndInvalidPassiveScope", "[battle][effects][magic]")
{
    auto duplicate = YAML::Load(R"(
武功效果:
  - 武功: 1
    效果:
      - 类型: 眩晕
        数值: 8
        触发: 攻击命中时概率
        触发参数: 100
  - 武功: 1
    效果:
      - 类型: 眩晕
        数值: 8
        触发: 攻击命中时概率
        触发参数: 100
)");
    auto passive = YAML::Load(R"(
武功效果:
  - 武功: 94
    效果:
      - 类型: 死亡庇护
        数值: 60
)");

    std::vector<ChessMagicEffectDefinition> definitions;
    CHECK_FALSE(ChessBattleEffects::parseMagicEffects(duplicate, definitions, "重複武功"));
    CHECK_FALSE(ChessBattleEffects::parseMagicEffects(passive, definitions, "被動武功"));
}
