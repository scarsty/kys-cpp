#include "ChessReplayJournal.h"

#include "ChessCanonicalEncoding.h"
#include "ChessRuntimeConstants.h"

#include <algorithm>

namespace KysChess
{
namespace
{

std::vector<std::uint8_t> canonicalChessRngProjection(const ChessRunRandom& random)
{
    ChessCanonicalWriter writer("RNG1");
    writer.writeU64(random.enemyPlanKey());
    for (std::uint64_t tag = 1; tag <= 7; ++tag)
    {
        writer.writeU64(tag);
        writer.writeU64(random.streamState(static_cast<ChessRngStream>(tag)).rawDrawCount);
    }
    return std::move(writer).takeBytes();
}

}

std::vector<std::uint8_t> canonicalChessAction(const ChessAction& action)
{
    ChessCanonicalWriter writer("ACT1");
    writer.writeU16(static_cast<std::uint16_t>(action.type));
    switch (action.type)
    {
    case ChessActionType::BuyShopSlot:
        writer.writeI32(action.shopSlot);
        break;
    case ChessActionType::SetShopLocked:
    case ChessActionType::SetPositionSwapEnabled:
        writer.writeBool(action.value);
        break;
    case ChessActionType::SellChess:
        writer.writeI32(action.chessInstanceId);
        break;
    case ChessActionType::SetDeployment:
    {
        auto ids = action.chessInstanceIds;
        std::ranges::sort(ids);
        writer.writeCollectionSize(ids.size());
        for (const int id : ids)
        {
            writer.writeI32(id);
        }
        break;
    }
    case ChessActionType::AddBan:
        writer.writeI32(action.roleId);
        break;
    case ChessActionType::Equip:
        writer.writeI32(action.equipmentInstanceId);
        writer.writeI32(action.targetChessInstanceId);
        break;
    case ChessActionType::BuyLegendaryEquipment:
        writer.writeI32(action.itemId);
        break;
    case ChessActionType::ChooseMap:
        writer.writeI32(action.mapId);
        break;
    case ChessActionType::SwapPositions:
        writer.writeI32(action.chessInstanceId);
        writer.writeI32(action.targetChessInstanceId);
        break;
    case ChessActionType::ChooseReward:
        writer.writeString(action.rewardId);
        break;
    case ChessActionType::StartChallenge:
        writer.writeString(action.challengeName);
        break;
    case ChessActionType::RefreshShop:
    case ChessActionType::BuyExp:
    case ChessActionType::SkipForcedBans:
    case ChessActionType::RerollEnemySeed:
    case ChessActionType::PrepareBattle:
    case ChessActionType::StartBattle:
    case ChessActionType::RerollReward:
    case ChessActionType::FinishRun:
        break;
    }
    return std::move(writer).takeBytes();
}

ChessSha256 canonicalChessRngDigest(const ChessRunRandom& random)
{
    return chessSha256(canonicalChessRngProjection(random));
}

std::vector<std::uint8_t> canonicalChessState(
    const ChessSessionState& state,
    const ChessRunRandom& random)
{
    ChessCanonicalWriter writer("STA1");
    writer.writeU16(static_cast<std::uint16_t>(state.difficulty));
    writer.writeI32(state.money);
    writer.writeI32(state.experience);
    writer.writeI32(state.level);
    writer.writeI32(state.fight);
    writer.writeBool(state.campaignComplete);
    writer.writeBool(state.shopLocked);
    writer.writeBool(state.freeShopRefreshAvailable);
    writer.writeI32(state.freeShopRefreshGrantedFight);
    writer.writeI32(state.nextChessInstanceId);
    writer.writeI32(state.nextEquipmentInstanceId);
    writer.writeU16(static_cast<std::uint16_t>(state.phase));
    writer.writeBool(state.options.positionSwapEnabled);
    writer.writeI32(kChessBattleFrameLimit);
    writer.writeCollectionSize(state.roster.size());
    for (const auto& [id, piece] : state.roster)
    {
        writer.writeI32(id);
        writer.writeI32(piece.roleId);
        writer.writeI32(piece.star);
        writer.writeI32(piece.weaponInstanceId);
        writer.writeI32(piece.armorInstanceId);
        writer.writeI32(piece.fightsWon);
    }
    std::vector<int> deployment;
    for (const auto& [id, piece] : state.roster)
    {
        if (piece.deployed)
        {
            deployment.push_back(id);
        }
    }
    writer.writeCollectionSize(deployment.size());
    for (const int id : deployment)
    {
        writer.writeI32(id);
    }
    writer.writeCollectionSize(state.equipmentInventory.size());
    for (const auto& [id, equipment] : state.equipmentInventory)
    {
        writer.writeI32(id);
        writer.writeI32(equipment.itemId);
        writer.writeI32(equipment.assignedChessInstanceId);
    }
    writer.writeCollectionSize(state.shop.size());
    for (const auto& slot : state.shop)
    {
        writer.writeI32(slot.roleId);
        writer.writeI32(slot.tier);
    }
    auto writeIntSet = [&](const std::set<int>& values) {
        writer.writeCollectionSize(values.size());
        for (const int value : values)
        {
            writer.writeI32(value);
        }
    };
    writeIntSet(state.rejectedRoleIds);
    writeIntSet(state.seenRoleIds);
    writeIntSet(state.bannedRoleIds);
    writeIntSet(state.obtainedNeigongIds);
    writer.writeCollectionSize(state.completedChallengeNames.size());
    for (const auto& name : state.completedChallengeNames)
    {
        writer.writeString(name);
    }
    writer.writeOptionalPresence(state.preparedBattle.has_value());
    if (state.preparedBattle)
    {
        const auto& battle = *state.preparedBattle;
        writer.writeU16(static_cast<std::uint16_t>(battle.kind));
        writer.writeString(battle.stableBattleId);
        writer.writeCollectionSize(battle.units.size());
        for (const auto& unit : battle.units)
        {
            writer.writeI32(unit.unitId);
            writer.writeI32(unit.chessInstanceId);
            writer.writeI32(unit.roleId);
            writer.writeI32(unit.team);
            writer.writeI32(unit.star);
            writer.writeI32(unit.weaponItemId);
            writer.writeI32(unit.armorItemId);
            writer.writeI32(unit.fightsWon);
            writer.writeI32(unit.x);
            writer.writeI32(unit.y);
        }
        auto mapCandidates = battle.mapCandidates;
        std::ranges::sort(mapCandidates);
        writer.writeCollectionSize(mapCandidates.size());
        for (const int mapId : mapCandidates)
        {
            writer.writeI32(mapId);
        }
        writer.writeI32(battle.chosenMapId);
        writer.writeCollectionSize(battle.formationSwaps.size());
        for (const auto& [first, second] : battle.formationSwaps)
        {
            writer.writeI32(first);
            writer.writeI32(second);
        }
        writer.writeU32(battle.battleSeed);
        writer.writeU64(battle.preparationCheckpoint.enemyPlanKey);
        for (std::size_t index = 0;
             index < battle.preparationCheckpoint.preparationStreams.size();
             ++index)
        {
            const auto& stream = battle.preparationCheckpoint.preparationStreams[index];
            writer.writeU64(static_cast<std::uint64_t>(index) + 3);
            writer.writeU64(stream.rawDrawCount);
        }
    }
    writer.writeCollectionSize(state.pendingRewards.size());
    for (const auto& pending : state.pendingRewards)
    {
        writer.writeString(pending.id);
        writer.writeU16(static_cast<std::uint16_t>(pending.kind));
        writer.writeI32(pending.rerollCost);
        writer.writeI32(pending.parameter);
        writer.writeI32(pending.choiceCount);
        auto eligibleTiers = pending.eligibleTiers;
        std::ranges::sort(eligibleTiers);
        writer.writeCollectionSize(eligibleTiers.size());
        for (const int tier : eligibleTiers)
        {
            writer.writeI32(tier);
        }
        writer.writeBool(pending.rerolled);
        writer.writeString(pending.challengeName);
        writer.writeCollectionSize(pending.options.size());
        for (const auto& option : pending.options)
        {
            writer.writeString(option.id);
            writer.writeU16(static_cast<std::uint16_t>(option.kind));
            writer.writeI32(option.value);
            writer.writeI32(option.value2);
        }
    }
    writer.writeU16(static_cast<std::uint16_t>(state.lastBattleOutcome));
    writer.writeI32(state.lastBattleEndFrame);
    writer.writeHash(state.lastBattleDigest);
    writer.writeBytes(canonicalChessRngProjection(random));
    return std::move(writer).takeBytes();
}

ChessSha256 canonicalChessStateHash(const ChessSessionState& state, const ChessRunRandom& random)
{
    return chessSha256(canonicalChessState(state, random));
}

ChessSha256 canonicalChessEventHash(const std::vector<ChessSemanticEvent>& events)
{
    ChessCanonicalWriter writer("EVT1");
    writer.writeCollectionSize(events.size());
    for (const auto& event : events)
    {
        const auto fields = chessSemanticEventStableFields(event);
        writer.writeU16(static_cast<std::uint16_t>(event.type));
        writer.writeI32(fields.primaryId);
        writer.writeI32(fields.secondaryId);
        writer.writeI32(fields.value);
        writer.writeString(fields.stableId);
    }
    return chessSha256(writer.bytes());
}

std::vector<std::uint8_t> canonicalChessReplayHeader(const ChessReplayHeader& header)
{
    ChessCanonicalWriter writer("HDR1");
    writer.writeString("KYS_CHESS_REPLAY");
    writer.writeString(header.gameVersion);
    writer.writeString(header.difficulty);
    writer.writeU64(header.rootSeed);
    writer.writeBool(header.options.positionSwapEnabled);
    writer.writeI32(header.options.battleFrameLimit);
    return std::move(writer).takeBytes();
}

ChessReplayJournal::ChessReplayJournal(ChessReplayHeader header)
    : header_(std::move(header)),
      chainHash_(initialReplayChain(canonicalChessReplayHeader(header_)))
{
}

ChessReplayJournal::ChessReplayJournal(const ChessReplay& replay)
    : header_(replay.header),
      decisions_(replay.decisions),
      chainHash_(replay.footer.terminalChainHash)
{
}

const ChessReplayDecisionRecord& ChessReplayJournal::append(
    ChessSessionPhase phase,
    const ChessAction& action,
    const ChessSha256& preStateHash,
    const ChessSha256& postStateHash,
    const ChessSha256& eventHash,
    const ChessSha256& rngDigest)
{
    ChessReplayDecisionRecord record;
    record.sequence = decisions_.size() + 1;
    record.phase = phase;
    record.action = action;
    record.preStateHash = preStateHash;
    record.postStateHash = postStateHash;
    record.eventHash = eventHash;
    record.rngDigest = rngDigest;
    record.previousChainHash = chainHash_;
    record.chainHash = extendReplayChain(
        chainHash_,
        canonicalChessAction(action),
        postStateHash,
        eventHash,
        rngDigest);
    chainHash_ = record.chainHash;
    decisions_.push_back(std::move(record));
    return decisions_.back();
}

ChessReplay ChessReplayJournal::exportReplay(
    const ChessSessionState& state,
    const ChessSha256& finalStateHash) const
{
    ChessReplay replay;
    replay.header = header_;
    replay.decisions = decisions_;
    replay.footer.complete = state.phase == ChessSessionPhase::Complete;
    replay.footer.terminalChainHash = chainHash_;
    replay.footer.finalStateHash = finalStateHash;
    replay.footer.fightReached = state.fight;
    return replay;
}

}
