#include "ChessReplayVerifier.h"

#include "ChessReplayJournal.h"
#include "ChessRuntimeConstants.h"
#include <algorithm>
#include <cassert>
#include <format>
#include <limits>

namespace KysChess
{
namespace
{

ChessReplayVerificationResult mismatch(
    ChessReplayMismatch category,
    std::uint64_t sequence,
    std::string message)
{
    return {false, category, sequence, std::move(message)};
}

std::string integerList(const std::vector<int>& values)
{
    std::string result = "[";
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index > 0)
        {
            result += ",";
        }
        result += std::to_string(values[index]);
    }
    result += "]";
    return result;
}

std::string pieceDescription(const ChessGameSession& session, int instanceId)
{
    const auto piece = session.state().roster.find(instanceId);
    if (piece == session.state().roster.end())
    {
        return std::format("棋子 #{}", instanceId);
    }
    const auto* role = session.content().role(piece->second.roleId);
    return role
        ? std::format("{} #{}", role->Name, instanceId)
        : std::format("棋子 #{}", instanceId);
}

std::string actionDescription(
    const ChessGameSession& session,
    const ChessAction& action)
{
    const auto& state = session.state();
    const auto& content = session.content();
    switch (action.type)
    {
    case ChessActionType::BuyShopSlot:
        if (action.shopSlot >= 0
            && action.shopSlot < static_cast<int>(state.shop.size()))
        {
            const auto* role = content.role(state.shop[action.shopSlot].roleId);
            if (role)
            {
                return std::format(
                    "購買商店第 {} 格「{}」",
                    action.shopSlot + 1,
                    role->Name);
            }
        }
        return std::format("購買商店第 {} 格", action.shopSlot + 1);
    case ChessActionType::RefreshShop: return "刷新商店";
    case ChessActionType::SetShopLocked:
        return action.value ? "鎖定商店" : "解除商店鎖定";
    case ChessActionType::SellChess:
        return std::format("出售{}", pieceDescription(session, action.chessInstanceId));
    case ChessActionType::SetDeployment:
        return std::format("調整出戰陣容 {}", integerList(action.chessInstanceIds));
    case ChessActionType::BuyExp: return "購買經驗";
    case ChessActionType::AddBan:
        if (const auto* role = content.role(action.roleId))
        {
            return std::format("禁用角色「{}」", role->Name);
        }
        return std::format("禁用角色 #{}", action.roleId);
    case ChessActionType::SkipForcedBans: return "略過剩餘禁用選擇";
    case ChessActionType::Equip:
    {
        std::string equipment = std::format("裝備 #{}", action.equipmentInstanceId);
        if (const auto found = state.equipmentInventory.find(action.equipmentInstanceId);
            found != state.equipmentInventory.end())
        {
            if (const auto* item = content.item(found->second.itemId))
            {
                equipment = std::format("「{}」", item->name);
            }
        }
        return std::format(
            "將{}交給{}",
            equipment,
            pieceDescription(session, action.targetChessInstanceId));
    }
    case ChessActionType::BuyLegendaryEquipment:
        if (const auto* item = content.item(action.itemId))
        {
            return std::format("購買神兵「{}」", item->name);
        }
        return std::format("購買神兵 #{}", action.itemId);
    case ChessActionType::SetPositionSwapEnabled:
        return action.value ? "開啟戰前換位" : "關閉戰前換位";
    case ChessActionType::RerollEnemySeed: return "重擲下一戰敵方陣容";
    case ChessActionType::PrepareBattle:
        return std::format("準備第 {} 場戰鬥", state.fight + 1);
    case ChessActionType::ChooseMap:
        if (const auto map = content.battleMaps().find(action.mapId);
            map != content.battleMaps().end())
        {
            return std::format("選擇戰場「{}」", map->second.name);
        }
        return std::format("選擇戰場 #{}", action.mapId);
    case ChessActionType::SwapPositions:
        return std::format(
            "交換戰鬥單位 #{} 與 #{} 的位置",
            action.chessInstanceId,
            action.targetChessInstanceId);
    case ChessActionType::StartBattle:
        if (state.preparedBattle)
        {
            if (const auto map = content.battleMaps().find(
                    state.preparedBattle->chosenMapId);
                map != content.battleMaps().end())
            {
                return std::format(
                    "開始第 {} 場戰鬥「{}」",
                    state.fight + 1,
                    map->second.name);
            }
            return std::format("開始第 {} 場戰鬥", state.fight + 1);
        }
        return "開始戰鬥";
    case ChessActionType::ChooseReward:
        return std::format("選擇獎勵「{}」", action.rewardId);
    case ChessActionType::StartChallenge:
        return std::format("開始遠征「{}」", action.challengeName);
    case ChessActionType::FinishRun: return "結束本局";
    case ChessActionType::SetFormation:
        return std::format("調整十格陣形 {}", integerList(action.chessInstanceIds));
    }
    std::unreachable();
}

}

ChessReplayAudit::ChessReplayAudit(
    std::shared_ptr<const ChessGameContent> content,
    ChessReplay replay)
    : replay_(std::move(replay))
{
    const bool versionCompatible = replay_.header.gameVersion == content->gameVersion()
        || replay_.header.gameVersion == "dev"
        || content->gameVersion() == "dev";
    if (!versionCompatible
        || replay_.header.options.battleFrameLimit != kChessBattleFrameLimit)
    {
        fail(ChessReplayMismatch::Header, 0, "重播版本不相容");
        return;
    }
    if (replay_.header.contentFingerprint != content->contentFingerprint())
    {
        fail(ChessReplayMismatch::Header, 0, "重播規則內容與目前載入內容不相符");
        return;
    }
    const auto expectedDifficulty = content->difficulty() == Difficulty::Easy
        ? "easy"
        : content->difficulty() == Difficulty::Normal ? "normal" : "hard";
    if (replay_.header.difficulty != expectedDifficulty)
    {
        fail(ChessReplayMismatch::Header, 0, "重播難度與規則內容不相符");
        return;
    }

    if (content->gameVersion() != replay_.header.gameVersion)
    {
        content = content->withGameVersion(replay_.header.gameVersion);
    }

    session_ = std::make_unique<ChessGameSession>(
        std::move(content),
        replay_.header.rootSeed,
        replay_.header.options,
        ChessSessionExecutionMode::ReplayVerification);
}

void ChessReplayAudit::fail(
    ChessReplayMismatch category,
    std::uint64_t sequence,
    std::string message)
{
    verification_ = mismatch(category, sequence, std::move(message));
    finished_ = true;
}

bool ChessReplayAudit::completeDecision(const ChessActionResult& actual)
{
    const auto& expected = replay_.decisions[nextDecision_];
    const std::uint64_t sequence = nextDecision_ + 1;
    if (actual.evidenceHash != expected.evidenceHash)
    {
        fail(
            ChessReplayMismatch::Evidence,
            sequence,
            std::format(
                "第 {} 個操作（{}）的執行結果與存檔記錄不一致",
                sequence,
                currentActionDescription_));
        return false;
    }
    ++nextDecision_;
    return true;
}

void ChessReplayAudit::verifyFooter()
{
    const auto actualReplay = session_->exportReplay();
    if (!actualReplay
        || actualReplay->footer.terminalEvidenceHash != replay_.footer.terminalEvidenceHash
        || actualReplay->footer.finalStateHash != replay_.footer.finalStateHash
        || actualReplay->footer.complete != replay_.footer.complete
        || actualReplay->footer.fightReached != replay_.footer.fightReached)
    {
        fail(
            ChessReplayMismatch::Footer,
            replay_.decisions.size(),
            "重播結束後的局面與存檔記錄不一致");
        return;
    }
    verification_ = {true, ChessReplayMismatch::None, replay_.decisions.size(), {}};
    finished_ = true;
}

void ChessReplayAudit::step(std::size_t decisionBudget, int battleFrameBudget)
{
    assert(decisionBudget > 0);
    assert(battleFrameBudget > 0);
    if (finished_)
    {
        return;
    }

    std::size_t decisionsCompleted{};
    int framesRemaining = battleFrameBudget;
    while (!finished_)
    {
        if (session_->transitionPending())
        {
            if (framesRemaining == 0)
            {
                return;
            }
            auto advance = session_->advanceAutomatic(framesRemaining);
            framesRemaining -= advance.framesAdvanced;
            if (!advance.completedAction)
            {
                return;
            }
            if (!completeDecision(*advance.completedAction))
            {
                return;
            }
            ++decisionsCompleted;
            continue;
        }

        if (nextDecision_ == replay_.decisions.size())
        {
            verifyFooter();
            return;
        }
        if (decisionsCompleted == decisionBudget)
        {
            return;
        }

        const auto& expected = replay_.decisions[nextDecision_];
        const std::uint64_t sequence = nextDecision_ + 1;
        currentActionDescription_ = actionDescription(*session_, expected.action);
        const auto legal = session_->legalActions();
        if (std::ranges::none_of(legal, [&](const ChessLegalActionDescriptor& descriptor) {
                return descriptor.type == expected.action.type;
            }))
        {
            fail(
                ChessReplayMismatch::IllegalAction,
                sequence,
                std::format(
                    "第 {} 個操作（{}）在當時不可執行",
                    sequence,
                    currentActionDescription_));
            return;
        }
        const auto actual = session_->beginAction(expected.action);
        if (!actual.accepted)
        {
            fail(
                ChessReplayMismatch::IllegalAction,
                sequence,
                std::format(
                    "第 {} 個操作（{}）無法執行：{}",
                    sequence,
                    currentActionDescription_,
                    actual.description));
            return;
        }
        if (!actual.transitionPending)
        {
            if (!completeDecision(actual))
            {
                return;
            }
            ++decisionsCompleted;
        }
    }
}

ChessReplayAuditResult ChessReplayAudit::takeResult()
{
    assert(finished_);
    ChessReplayAuditResult result;
    result.verification = std::move(verification_);
    if (result.verification.valid)
    {
        result.reconstructedSession = std::move(session_);
    }
    return result;
}

ChessReplayAuditResult ChessReplayAudit::takePrefixResult()
{
    assert(!finished_);
    assert(session_);
    assert(session_->isStableDecisionBoundary());
    ChessReplayAuditResult result;
    result.verification = {
        true,
        ChessReplayMismatch::None,
        nextDecision_,
        {},
    };
    result.reconstructedSession = std::move(session_);
    finished_ = true;
    return result;
}

ChessReplayAuditResult ChessReplayVerifier::audit(
    std::shared_ptr<const ChessGameContent> content,
    const ChessReplay& replay)
{
    ChessReplayAudit audit(std::move(content), replay);
    while (!audit.finished())
    {
        audit.step(std::numeric_limits<std::size_t>::max(), std::numeric_limits<int>::max());
    }
    return audit.takeResult();
}

ChessReplayAuditResult ChessReplayVerifier::reconstructPrefix(
    std::shared_ptr<const ChessGameContent> content,
    const ChessReplay& replay,
    std::size_t decisionCount)
{
    if (decisionCount > replay.decisions.size())
    {
        ChessReplayAuditResult result;
        result.verification = mismatch(
            ChessReplayMismatch::IllegalAction,
            decisionCount,
            std::format(
                "要求的重播前綴序號 {} 超過重播總操作數 {}",
                decisionCount,
                replay.decisions.size()));
        return result;
    }

    ChessReplayAudit audit(std::move(content), replay);
    while (!audit.finished()
        && audit.completedDecisionCount() < decisionCount)
    {
        audit.step(
            decisionCount - audit.completedDecisionCount(),
            std::numeric_limits<int>::max());
    }
    return audit.finished()
        ? audit.takeResult()
        : audit.takePrefixResult();
}

ChessReplayVerificationResult ChessReplayVerifier::verify(
    std::shared_ptr<const ChessGameContent> content,
    const ChessReplay& replay)
{
    return audit(std::move(content), replay).verification;
}

}
