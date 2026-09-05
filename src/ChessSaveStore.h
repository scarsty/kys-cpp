#pragma once

#include "ChessSessionCheckpoint.h"

#include <map>

namespace KysChess
{

struct ChessSaveSlotSummary
{
    std::string slotId;
    std::uint64_t revision{};
    std::string label;
    int fight{};
    int level{};
    int money{};
    int rosterCount{};
    std::uint64_t replaySequence{};
    ChessSha256 stateHash{};
    ChessTalentId talent = ChessTalentId::DivineArms;
};

struct ChessTimelineReplacement
{
    std::string loadedSlot;
    std::uint64_t previousSequence{};
    std::uint64_t restoredSequence{};
    std::uint64_t discardedActiveActions{};
    ChessSha256 currentStateHash{};
};

class ChessSaveStore
{
public:
    ChessCheckpointError save(
        std::string slotId,
        const ChessGameSession& session,
        std::string label = {});
    ChessCheckpointError load(
        const std::string& slotId,
        ChessGameSession& session,
        ChessTimelineReplacement& replacement) const;
    const ChessSessionCheckpoint* inspect(const std::string& slotId) const;
    std::vector<ChessSaveSlotSummary> list() const;
    std::optional<std::string> exportSave(const std::string& slotId) const;
    ChessCheckpointError importSave(
        std::string slotId,
        std::string_view payload,
        std::string_view gameVersion);
    ChessCheckpointError importSave(
        std::string slotId,
        const ChessSessionCheckpointData& data,
        std::string_view gameVersion);
    void restoreSave(
        std::string slotId,
        ChessSessionCheckpoint checkpoint);

private:
    void importCheckpoint(
        std::string slotId,
        ChessSessionCheckpoint checkpoint);
    std::map<std::string, ChessSessionCheckpoint> slots_;
    std::uint64_t nextRevision_ = 1;
};

}
