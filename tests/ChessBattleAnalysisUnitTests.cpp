#include "ChessBattleAnalysis.h"
#include "ChessJsonCodec.h"
#include "ChessGameSessionTestHelpers.h"
#include "BattleLogTestHelpers.h"
#include "battle/BattleLogSegments.h"

#include <catch2/catch_test_macros.hpp>
#include <array>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("battle analysis exports effect lifecycle types and stack attribution", "[chess][battle-analysis][logging]")
{
    using namespace KysChess::Battle;
    const auto content = managementContent();
    PreparedChessBattle prepared;
    HeadlessBattleResult battle;
    auto source = BattleLogTest::reportUnit(1, 10, 0, 0, "施術者");
    auto target = BattleLogTest::reportUnit(2, 10, 1, 0, "目標");
    const std::array cases{
        std::pair{ BattleStatusSemanticId::TrueQi, "true_qi_applied" },
        std::pair{ BattleStatusSemanticId::BattleSpirit, "battle_spirit_applied" },
        std::pair{ BattleStatusSemanticId::AttributeModifier, "attribute_modifier_applied" },
        std::pair{ BattleStatusSemanticId::DamageModifier, "damage_modifier_applied" },
        std::pair{ BattleStatusSemanticId::HealModifier, "heal_modifier_applied" },
        std::pair{ BattleStatusSemanticId::AreaCreated, "area_created" },
        std::pair{ BattleStatusSemanticId::AreaRefreshed, "area_refreshed" },
        std::pair{ BattleStatusSemanticId::AreaRemoved, "area_removed" },
        std::pair{ BattleStatusSemanticId::StatusConsumed, "status_consumed" },
        std::pair{ BattleStatusSemanticId::StatusRemoved, "status_removed" },
        std::pair{ BattleStatusSemanticId::DamageAbsorption, "damage_absorption_applied" },
        std::pair{ BattleStatusSemanticId::DamageAbsorptionEnded, "damage_absorption_ended" },
        std::pair{ BattleStatusSemanticId::AttackCopied, "attack_copied" },
        std::pair{ BattleStatusSemanticId::FreeCast, "free_cast" },
    };
    BattleReportBuilder builder;
    for (const auto& [status, expected] : cases)
    {
        builder.recordStatus(&source, &target, BattleLogCategory::Status,
            BattleLogPerspective::Targeted, battleLogText("效果記錄"), 12, status,
            BattleResourceSemanticId::None, 5, 0, 0, 0, -1, -1, 0, "magic", "測試絕招", {}, -1, 3);
    }
    battle.report = builder.report();
    const auto analysis = analyzeChessBattleResult(*content, prepared, battle);
    REQUIRE(analysis.effectActivations.size() == cases.size());
    for (std::size_t i = 0; i < cases.size(); ++i)
    {
        const auto& activation = analysis.effectActivations[i];
        CHECK(activation.type == cases[i].second);
        CHECK(activation.sourceUnitId == 1);
        CHECK(activation.sourceName == "測試絕招");
        CHECK(activation.sourceKind == "magic");
        CHECK(activation.targetUnitId == 2);
        CHECK(activation.frame == 12);
        CHECK(activation.stackCount == 3);
    }
    const auto dto = ProtocolDetail::battleResultDto(*content, prepared, battle, ProtocolDetail::BattleReportDetail::Full);
    const auto decoded = ProtocolDetail::readJson<ProtocolDetail::BattleResultDto>(ProtocolDetail::writeJson(dto));
    REQUIRE(decoded);
    REQUIRE(decoded->effect_activations);
    REQUIRE(decoded->effect_activations->size() == cases.size());
    CHECK(decoded->effect_activations->front().source_name == "測試絕招");
    CHECK(decoded->effect_activations->front().stack_count == 3);
}

TEST_CASE("battle report keeps poison payloads applications and MP drains independent",
          "[chess][battle-analysis][poison]")
{
    using namespace KysChess::Battle;
    const auto content = managementContent();
    PreparedChessBattle prepared;
    HeadlessBattleResult battle;
    battle.summary.outcome = BattleOutcome::PlayerVictory;
    for (int id : {1, 2, 3})
    {
        prepared.units.push_back({.unitId = id, .roleId = 10, .team = id == 3 ? 1 : 0});
        battle.initialization.roleDeltas.push_back({
            .unitId = id, .star = 1, .vitals = {100, 100, 50, 100},
            .stats = {40, 30, 20},
        });
    }
    auto poisonOnly = BattleLogTest::reportUnit(1, 10, 0, 0, "施毒來源");
    auto poisonAndDrain = BattleLogTest::reportUnit(2, 10, 0, 0, "施毒奪內來源");
    auto target = BattleLogTest::reportUnit(3, 10, 1, 0, "目標");
    BattleReportBuilder builder;
    for (const auto* source : {&poisonOnly, &poisonAndDrain})
    {
        builder.recordStatus(source, &target, BattleLogCategory::Status,
            BattleLogPerspective::Targeted, battleLogText("施毒"), 1, BattleStatusSemanticId::PoisonPayload,
            BattleResourceSemanticId::None, 7, 3);
    }
    builder.recordStatus(&poisonOnly, &target, BattleLogCategory::Status,
        BattleLogPerspective::Targeted, battleLogText("中毒"), 1, BattleStatusSemanticId::Poison);
    builder.recordStatus(&poisonAndDrain, &target, BattleLogCategory::Status,
        BattleLogPerspective::Targeted, battleLogText("奪內"), 2, BattleStatusSemanticId::MagicPointsDrained,
        BattleResourceSemanticId::MagicPoints, 12);
    battle.report = builder.report();

    const auto analysis = analyzeChessBattleResult(*content, prepared, battle);
    REQUIRE(analysis.unitStats.size() == 3);
    const auto& first = analysis.unitStats[0];
    CHECK(first.poisonPayloadEvents == 1);
    CHECK(first.poisonApplicationEvents == 1);
    CHECK(first.magicPointsDrained == 0);
    CHECK(first.magicPointsDrainEvents == 0);
    const auto& second = analysis.unitStats[1];
    CHECK(second.poisonPayloadEvents == 1);
    CHECK(second.poisonApplicationEvents == 0);
    CHECK(second.magicPointsDrained == 12);
    CHECK(second.magicPointsDrainEvents == 1);
    REQUIRE(analysis.effectActivations.size() == 4);
    CHECK(analysis.effectActivations[0].poisonPercent == 7);
    CHECK(analysis.effectActivations[0].scheduledTicks == 3);
}

TEST_CASE("battle analysis reconciles the authoritative report and summary",
          "[chess][battle-analysis]")
{
    ChessGameSession session(configuredMapChoiceContent(), 61);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(2)).accepted);
    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    REQUIRE(session.submitAndDrain(prepare).accepted);
    ChessAction map;
    map.type = ChessActionType::ChooseMap;
    map.mapId = session.state().preparedBattle->mapCandidates.front();
    REQUIRE(session.submitAndDrain(map).accepted);
    ChessAction start;
    start.type = ChessActionType::StartBattle;
    REQUIRE(session.submitAndDrain(start).accepted);
    REQUIRE(session.lastBattlePrepared());
    REQUIRE(session.lastBattleResult());

    const auto analysis = analyzeChessBattleResult(
        session.content(),
        *session.lastBattlePrepared(),
        *session.lastBattleResult());
    CHECK(analysis.outcome == session.lastBattleResult()->summary.outcome);
    CHECK(analysis.endFrame == session.lastBattleResult()->summary.endFrame);
    CHECK(analysis.digest == session.lastBattleResult()->digest);
    CHECK(analysis.unitStats.size() == session.lastBattlePrepared()->units.size());
    int analyzedDamage{};
    int reportedDamage{};
    for (const auto& unit : analysis.unitStats)
    {
        analyzedDamage += unit.damageDealt;
        if (const auto found = session.lastBattleResult()->report.stats().find(unit.unitId);
            found != session.lastBattleResult()->report.stats().end())
        {
            reportedDamage += found->second.damageDealt;
        }
    }
    CHECK(analyzedDamage == reportedDamage);
    CHECK(analysis.summary.contains(analysis.outcomeDescription));
}
