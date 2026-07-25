#include "BattleSetupFactory.h"
#include "ChessBattleMapCatalog.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessPvp.h"
#include "ChessReplayJournal.h"
#include "ChessReplayVerifier.h"
#include "ChessStandaloneBattle.h"

#include <catch2/catch_test_macros.hpp>
#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <set>
#include <string>

using namespace KysChess;

namespace
{

struct CheckpointFixture
{
    std::shared_ptr<const ChessGameContent> content;
    ChessSessionCheckpoint checkpoint;
};

CheckpointFixture checkpointFixture(
    Difficulty difficulty = Difficulty::Hard,
    std::string version = "1.4.0")
{
    auto content = Test::managementContent(100, difficulty, std::move(version));
    ChessGameSession session(content, 0x1234);
    REQUIRE(session.submitAndDrain(Test::buySlot(0)).accepted);
    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {1};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    return {
        std::move(content),
        ChessSessionCheckpoint::capture(session, 7, "PvP 測試"),
    };
}

std::string fullSlotPayload(const ChessSessionCheckpoint& checkpoint)
{
    ChessSaveSlotData slot;
    slot.scene = {53, 21, 54, 1};
    slot.checkpoint = checkpoint.toData();
    return glz::write_json(slot).value();
}

ChessAction deployment(std::initializer_list<int> ids)
{
    ChessAction action;
    action.type = ChessActionType::SetDeployment;
    action.chessInstanceIds.assign(ids);
    return action;
}

ChessAction formation(std::vector<int> slots)
{
    ChessAction action;
    action.type = ChessActionType::SetFormation;
    action.chessInstanceIds = std::move(slots);
    return action;
}

}

TEST_CASE("offline PvP verifier accepts direct checkpoints and full slot envelopes", "[chess][pvp][save]")
{
    const auto fixture = checkpointFixture();

    const auto direct = ChessPvpSaveVerifier::verify(
        fixture.content,
        fixture.checkpoint.serializeJson());
    REQUIRE(direct.valid);
    REQUIRE(direct.composition.pieces.size() == 1);
    CHECK(direct.composition.pieces.front().chessInstanceId == 1);
    CHECK(direct.composition.formationSlots.front() == 1);

    const auto full = ChessPvpSaveVerifier::verify(
        fixture.content,
        fullSlotPayload(fixture.checkpoint));
    REQUIRE(full.valid);
    CHECK(full.composition == direct.composition);
}

TEST_CASE("external save payload parser distinguishes checkpoints from full slots", "[chess][checkpoint][save]")
{
    const auto fixture = checkpointFixture();
    ChessCheckpointError error;

    const auto direct = parseChessSavePayload(
        fixture.checkpoint.serializeJson(),
        error);
    REQUIRE(direct);
    CHECK(error == ChessCheckpointError::None);
    CHECK_FALSE(direct->scene);
    CHECK(direct->checkpoint.state == fixture.checkpoint.state);

    const auto full = parseChessSavePayload(
        fullSlotPayload(fixture.checkpoint),
        error);
    REQUIRE(full);
    CHECK(error == ChessCheckpointError::None);
    REQUIRE(full->scene);
    CHECK(full->scene->inSubMap == 53);
    CHECK(full->scene->subMapX == 21);
    CHECK(full->scene->subMapY == 54);
    CHECK(full->scene->faceTowards == 1);
    CHECK(full->checkpoint.state == fixture.checkpoint.state);
}

TEST_CASE("offline PvP verifier rejects incompatible versions and non-hard saves", "[chess][pvp][save]")
{
    const auto current = checkpointFixture();

    const auto oldVersion = checkpointFixture(Difficulty::Hard, "1.3.0");
    const auto versionResult = ChessPvpSaveVerifier::verify(
        current.content,
        oldVersion.checkpoint.serializeJson());
    CHECK_FALSE(versionResult.valid);
    CHECK(versionResult.error == ChessPvpSaveError::VersionMismatch);

    const auto devSave = checkpointFixture(Difficulty::Hard, "dev");
    const auto devSaveResult = ChessPvpSaveVerifier::verify(
        current.content,
        devSave.checkpoint.serializeJson());
    INFO(devSaveResult.message);
    CHECK(devSaveResult.valid);

    const auto devBuild = checkpointFixture(Difficulty::Hard, "dev");
    const auto devBuildResult = ChessPvpSaveVerifier::verify(
        devBuild.content,
        oldVersion.checkpoint.serializeJson());
    INFO(devBuildResult.message);
    CHECK(devBuildResult.valid);

    for (const Difficulty difficulty : {Difficulty::Easy, Difficulty::Normal})
    {
        const auto other = checkpointFixture(difficulty);
        const auto result = ChessPvpSaveVerifier::verify(
            current.content,
            other.checkpoint.serializeJson());
        CHECK_FALSE(result.valid);
        CHECK(result.error == ChessPvpSaveError::HardModeRequired);
    }
}

TEST_CASE("offline PvP verifier audits actions evidence and replay footer", "[chess][pvp][save][replay]")
{
    const auto fixture = checkpointFixture();

    SECTION("modified action")
    {
        auto checkpoint = fixture.checkpoint;
        checkpoint.replay.decisions.front().action.shopSlot = 999;
        const auto result = ChessPvpSaveVerifier::verify(
            fixture.content,
            checkpoint.serializeJson());
        CHECK_FALSE(result.valid);
        CHECK(result.error == ChessPvpSaveError::ReplayVerificationFailed);
        CHECK(result.sequence == 1);
        CHECK(result.message.contains("購買商店第 1000 格"));
        CHECK_FALSE(result.message.contains("重播驗證在第"));
    }
    SECTION("modified evidence")
    {
        auto checkpoint = fixture.checkpoint;
        checkpoint.replay.decisions.front().evidenceHash.front() ^= 1;
        const auto result = ChessPvpSaveVerifier::verify(
            fixture.content,
            checkpoint.serializeJson());
        CHECK_FALSE(result.valid);
        CHECK(result.error == ChessPvpSaveError::ReplayVerificationFailed);
        CHECK(result.message.contains("購買商店第 1 格"));
        CHECK_FALSE(result.message.contains("雜湊"));
    }
    SECTION("modified footer")
    {
        auto checkpoint = fixture.checkpoint;
        checkpoint.replay.footer.terminalEvidenceHash.front() ^= 1;
        const auto result = ChessPvpSaveVerifier::verify(
            fixture.content,
            checkpoint.serializeJson());
        CHECK_FALSE(result.valid);
        CHECK(result.error == ChessPvpSaveError::ReplayVerificationFailed);
    }
}

TEST_CASE("offline PvP verifier compares checkpoint state random state and every final hash", "[chess][pvp][save][snapshot]")
{
    const auto fixture = checkpointFixture();

    SECTION("modified state")
    {
        auto checkpoint = fixture.checkpoint;
        ++checkpoint.state.money;
        const auto result = ChessPvpSaveVerifier::verify(
            fixture.content,
            checkpoint.serializeJson());
        CHECK_FALSE(result.valid);
        CHECK(result.error == ChessPvpSaveError::SnapshotStateMismatch);
    }
    SECTION("modified random state")
    {
        auto checkpoint = fixture.checkpoint;
        checkpoint.random.streams.front().words.front() ^= 1;
        const auto result = ChessPvpSaveVerifier::verify(
            fixture.content,
            checkpoint.serializeJson());
        CHECK_FALSE(result.valid);
        CHECK(result.error == ChessPvpSaveError::SnapshotRandomMismatch);
    }
    SECTION("forged snapshot hash cannot cover a changed snapshot")
    {
        auto checkpoint = fixture.checkpoint;
        ++checkpoint.state.money;
        checkpoint.snapshotHash = chessStateHash(checkpoint.state, checkpoint.random);
        const auto result = ChessPvpSaveVerifier::verify(
            fixture.content,
            checkpoint.serializeJson());
        CHECK_FALSE(result.valid);
        CHECK(result.error == ChessPvpSaveError::SnapshotStateMismatch);
    }
    SECTION("modified stored hash")
    {
        auto checkpoint = fixture.checkpoint;
        checkpoint.snapshotHash.front() ^= 1;
        const auto result = ChessPvpSaveVerifier::verify(
            fixture.content,
            checkpoint.serializeJson());
        CHECK_FALSE(result.valid);
        CHECK(result.error == ChessPvpSaveError::SnapshotHashMismatch);
    }
}

TEST_CASE("offline PvP verifier rejects stable hard saves without deployed pieces", "[chess][pvp][save]")
{
    auto content = Test::managementContent(100, Difficulty::Hard, "1.4.0");
    ChessGameSession session(content, 77);
    const auto checkpoint = ChessSessionCheckpoint::capture(session, 1);

    const auto result = ChessPvpSaveVerifier::verify(
        content,
        checkpoint.serializeJson());
    CHECK_FALSE(result.valid);
    CHECK(result.error == ChessPvpSaveError::NoDeployedPieces);
}

TEST_CASE("offline PvP verifier reports invalid persistent formations", "[chess][pvp][save][formation]")
{
    const auto fixture = checkpointFixture();
    auto checkpoint = fixture.checkpoint;
    checkpoint.state.formationSlots.front() = -1;

    const auto result = ChessPvpSaveVerifier::verify(
        fixture.content,
        checkpoint.serializeJson());
    CHECK_FALSE(result.valid);
    CHECK(result.error == ChessPvpSaveError::InvalidFormation);
}

TEST_CASE("PvP composition resolves deployed equipment instances to item IDs", "[chess][pvp][composition]")
{
    auto content = Test::managementContent(100, Difficulty::Hard);
    ChessGameSession session(content, 88);
    auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
    checkpoint.state.roster.emplace(
        7,
        ChessSessionPiece{7, 10, 3, true, 11, 12, 9});
    checkpoint.state.formationSlots[6] = 7;
    checkpoint.state.equipmentInventory.emplace(
        11,
        ChessEquipmentInstance{11, 501, 7});
    checkpoint.state.equipmentInventory.emplace(
        12,
        ChessEquipmentInstance{12, 502, 7});
    REQUIRE(checkpoint.restore(session) == ChessCheckpointError::None);

    const auto composition = extractChessPvpComposition(session);
    REQUIRE(composition.pieces.size() == 1);
    CHECK(composition.formationSlots[6] == 7);
    CHECK(composition.pieces.front() == (ChessPvpPiece{7, 10, 3, 501, 502, 9}));
}

TEST_CASE("cooperative replay audit advances by bounded decisions", "[chess][replay][audit]")
{
    const auto fixture = checkpointFixture();
    ChessReplayAudit audit(fixture.content, fixture.checkpoint.replay);
    REQUIRE_FALSE(audit.finished());

    audit.step(1, 1);
    CHECK(audit.completedDecisionCount() == 1);
    REQUIRE_FALSE(audit.finished());
    audit.step(1, 1);
    CHECK(audit.finished());
    const auto result = audit.takeResult();
    CHECK(result.verification.valid);
    REQUIRE(result.reconstructedSession);
    CHECK(result.reconstructedSession->state() == fixture.checkpoint.state);
}

TEST_CASE("persistent formation validates exact placement and preserves empty slots", "[chess][formation][replay]")
{
    auto content = Test::managementContent();
    ChessGameSession session(content, 9);
    REQUIRE(session.submitAndDrain(Test::buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(Test::buySlot(1)).accepted);
    REQUIRE(session.submitAndDrain(deployment({1, 2})).accepted);

    auto slots = std::vector<int>(kChessFormationSlotCount, -1);
    slots[4] = 2;
    slots[9] = 1;
    const auto randomBeforeFormationEdit = session.random().state();
    const auto setResult = session.submitAndDrain(formation(slots));
    REQUIRE(setResult.accepted);
    CHECK(session.state().formationSlots == slots);
    CHECK(session.random().state() == randomBeforeFormationEdit);

    auto duplicate = slots;
    duplicate[4] = 1;
    CHECK(session.submitAndDrain(formation(duplicate)).error == ChessRuleErrorCode::InvalidFormation);
    auto missing = slots;
    missing[4] = -1;
    CHECK(session.submitAndDrain(formation(missing)).error == ChessRuleErrorCode::InvalidFormation);

    const auto replay = session.exportReplay();
    REQUIRE(replay);
    CHECK(ChessReplayVerifier::verify(content, *replay).valid);
    ChessReplayJsonError replayError;
    const auto parsed = parseChessReplayJsonl(
        serializeChessReplayJsonl(*replay),
        replayError);
    REQUIRE(parsed);
    CHECK(ChessReplayVerifier::verify(content, *parsed).valid);
}

TEST_CASE("deployment selling and merging maintain persistent formation", "[chess][formation]")
{
    auto content = Test::managementContent();

    SECTION("newly deployed pieces use the first empty slot and survivors retain theirs")
    {
        ChessGameSession session(content, 10);
        REQUIRE(session.submitAndDrain(Test::buySlot(0)).accepted);
        REQUIRE(session.submitAndDrain(Test::buySlot(1)).accepted);
        REQUIRE(session.submitAndDrain(deployment({1})).accepted);
        auto slots = std::vector<int>(kChessFormationSlotCount, -1);
        slots[9] = 1;
        REQUIRE(session.submitAndDrain(formation(slots)).accepted);

        REQUIRE(session.submitAndDrain(deployment({1, 2})).accepted);
        CHECK(session.state().formationSlots[9] == 1);
        CHECK(session.state().formationSlots[0] == 2);
        REQUIRE(session.submitAndDrain(deployment({2})).accepted);
        CHECK(session.state().formationSlots[9] == -1);
        CHECK(session.state().formationSlots[0] == 2);

        ChessAction sell;
        sell.type = ChessActionType::SellChess;
        sell.chessInstanceId = 2;
        REQUIRE(session.submitAndDrain(sell).accepted);
        CHECK(std::ranges::all_of(session.state().formationSlots, [](int id) { return id == -1; }));
    }

    SECTION("merge clears consumed IDs and places the upgraded deployed piece")
    {
        ChessGameSession session(content, 11);
        REQUIRE(session.submitAndDrain(Test::buySlot(0)).accepted);
        REQUIRE(session.submitAndDrain(Test::buySlot(1)).accepted);
        REQUIRE(session.submitAndDrain(deployment({1})).accepted);
        auto slots = std::vector<int>(kChessFormationSlotCount, -1);
        slots[7] = 1;
        REQUIRE(session.submitAndDrain(formation(slots)).accepted);
        REQUIRE(session.submitAndDrain(Test::buySlot(2)).accepted);

        REQUIRE(session.state().roster.size() == 1);
        const int upgradedId = session.state().roster.begin()->first;
        CHECK(upgradedId == 4);
        CHECK(session.state().roster.begin()->second.deployed);
        CHECK(session.state().formationSlots[0] == upgradedId);
        CHECK(std::ranges::count(session.state().formationSlots, upgradedId) == 1);
    }
}

TEST_CASE("PvP arena layout is mirrored unique walkable and outside the random map pool", "[chess][pvp][map]")
{
    const auto content = Test::actualContent(Difficulty::Hard);
    REQUIRE(content);
    REQUIRE(content->battleMaps().contains(ChessPvpMapLayout::BattleId));
    std::string error;
    REQUIRE(ChessPvpMapLayout::validate(*content, error));
    CHECK(error.empty());
    CHECK_FALSE(std::ranges::contains(
        ChessBattleMapCatalog::fittingMapIds(*content, 1, 1),
        ChessPvpMapLayout::BattleId));

    std::set<std::pair<int, int>> positions;
    const auto collect = [&](const auto& values) {
        for (const auto& point : values)
        {
            positions.emplace(point.x, point.y);
        }
    };
    collect(ChessPvpMapLayout::localFormation());
    collect(ChessPvpMapLayout::opponentFormation());
    collect(ChessPvpMapLayout::localAdditionalSpawns());
    collect(ChessPvpMapLayout::opponentAdditionalSpawns());
    CHECK(positions.size() == 26);
}

TEST_CASE("PvP battle 133 stays excluded when random map selection uses raw content fallback", "[chess][pvp][map]")
{
    ChessGameContentData data;
    ChessBattleMapDefinition pvp;
    pvp.id = ChessPvpMapLayout::BattleId;
    pvp.teammateX = {1};
    pvp.teammateY = {1};
    pvp.enemyX = {2};
    pvp.enemyY = {2};
    data.battleMaps.emplace(pvp.id, std::move(pvp));
    const ChessGameContent content(std::move(data));

    CHECK(ChessBattleMapCatalog::fittingMapIds(content, 1, 1).empty());
}

TEST_CASE("PvP standalone build uses explicit formation and isolated campaign state", "[chess][pvp][battle]")
{
    const auto content = Test::actualContent(Difficulty::Hard);
    REQUIRE(content);
    ChessStandaloneBattleRequest request;
    request.stableBattleId = "offline_pvp";
    request.rootSeed = 456;
    request.mapId = ChessPvpMapLayout::BattleId;
    request.battleSeed = 456;
    request.layout = PreparedChessBattleLayout::PvpArena;
    request.teams[0].pieces.push_back({4, 2, -1, -1, 1001, 7});
    request.teams[1].pieces.push_back({160, 3, -1, -1, 2001, 9});
    request.teams[0].formationSlots.assign(kChessFormationSlotCount, -1);
    request.teams[1].formationSlots.assign(kChessFormationSlotCount, -1);
    request.teams[0].formationSlots[9] = 1001;
    request.teams[1].formationSlots[0] = 2001;

    std::string error;
    auto build = ChessStandaloneBattle::prepare(content, request, error);
    REQUIRE(build);
    CHECK(build->rootSeed == 456);
    CHECK(build->preparedBattle.battleSeed == 456);
    auto input = BattleSetupFactory::build(
        build->preparedBattle,
        *build->content,
        kChessBattleFrameLimit);
    REQUIRE(input.units.size() == 2);
    CHECK(input.units[0].gridX == ChessPvpMapLayout::localFormation()[9].x);
    CHECK(input.units[0].gridY == ChessPvpMapLayout::localFormation()[9].y);
    CHECK(input.units[1].gridX == ChessPvpMapLayout::opponentFormation()[0].x);
    CHECK(input.units[1].gridY == ChessPvpMapLayout::opponentFormation()[0].y);
    CHECK(std::ranges::count_if(input.setup.cloneCells, [](const auto& cell) {
        return cell.team == 0;
    }) == 3);
    CHECK(std::ranges::count_if(input.setup.cloneCells, [](const auto& cell) {
        return cell.team == 1;
    }) == 3);

    const auto headless = HeadlessBattleRunner::run(input);
    const auto repeated = HeadlessBattleRunner::run(input);
    CHECK(repeated.digest == headless.digest);
    ChessGameSession campaign(content, 999);
    REQUIRE(campaign.submitAndDrain(Test::buySlot(0)).accepted);
    auto campaignCheckpoint = ChessSessionCheckpoint::capture(campaign, 1);
    REQUIRE_FALSE(campaignCheckpoint.state.roster.empty());
    campaignCheckpoint.state.roster.begin()->second.fightsWon = 12;
    campaignCheckpoint.snapshotHash = chessStateHash(
        campaignCheckpoint.state,
        campaignCheckpoint.random);
    REQUIRE(campaignCheckpoint.restore(campaign) == ChessCheckpointError::None);
    const auto campaignState = campaign.state();
    const auto campaignRandom = campaign.random().state();
    const auto campaignReplaySize = campaign.journal().decisions().size();
    auto standalone = std::move(*build).createSession();
    ChessAction start;
    start.type = ChessActionType::StartBattle;
    REQUIRE(standalone->submitAndDrain(start).accepted);
    REQUIRE(standalone->lastBattleResult());
    CHECK(standalone->lastBattleResult()->digest == headless.digest);
    CHECK(campaign.state() == campaignState);
    CHECK(campaign.random().state() == campaignRandom);
    CHECK(campaign.journal().decisions().size() == campaignReplaySize);
    CHECK(campaign.state().roster.begin()->second.fightsWon == 12);
}
