#pragma once

#include "ChessCombo.h"
#include "ChessGameSession.h"
#include "ChessPvp.h"
#include "ChessReplayHash.h"

#include <glaze/glaze.hpp>
#include <algorithm>
#include <ostream>

namespace KysChess
{

struct ChessTraceRole
{
    int id{};
    std::string name;
    int cost{};
    bool in_pool{};
};

struct ChessTraceCombo
{
    int id{};
    std::string name;
    std::vector<int> members;
    std::vector<int> thresholds;
    bool star_bonus{};
    bool anti{};
    bool npc_only{};
};

struct ChessTraceCatalog
{
    std::string_view format = "kys_chess_usage_trace";
    int format_version = 1;
    std::string engine_version;
    std::string content_fingerprint;
    std::vector<ChessTraceRole> roles;
    std::vector<ChessTraceCombo> combos;
};

struct ChessTraceSnapshot
{
    std::size_t sequence{};
    int fight{};
    bool campaign_complete{};
    int level{};
    std::map<int, ChessSessionPiece> roster;
    std::vector<ChessSessionShopSlot> shop;
    std::map<int, ChessEquipmentInstance> equipment;
    std::vector<ChessComboProgress> combos;
    std::set<int> bans;
    std::set<int> seen;
    int last_outcome{};
};

struct ChessTraceValidation
{
    bool valid{};
    std::uint64_t sequence{};
    std::string message;
};

struct ChessTraceTerminal
{
    ChessTraceValidation validation;
};

class ChessReplayTraceWriter
{
public:
    ChessReplayTraceWriter(std::ostream& output, const ChessGameContent& content)
        : output_(output)
    {
        ChessTraceCatalog catalog;
        catalog.engine_version = content.gameVersion();
        catalog.content_fingerprint = chessSha256Hex(content.contentFingerprint());
        for (const auto& [id, role] : content.roles())
            catalog.roles.push_back({id, role.Name, role.Cost, std::ranges::contains(content.poolRoleIds(), id)});
        for (const auto& combo : content.combos())
        {
            ChessTraceCombo entry{combo.id, combo.name, combo.memberRoleIds, {},
                combo.starSynergyBonus, combo.isAntiCombo, combo.isNpcOnly};
            for (const auto& threshold : combo.thresholds) entry.thresholds.push_back(threshold.count);
            catalog.combos.push_back(std::move(entry));
        }
        write(catalog);
    }

    void observe(std::size_t sequence, const ChessGameSession& session)
    {
        const auto& state = session.state();
        write(ChessTraceSnapshot{sequence, state.fight, state.campaignComplete, state.level,
            state.roster, state.shop, state.equipmentInventory,
            evaluateChessComboProgresses(state, session.content()), state.bannedRoleIds,
            state.seenRoleIds, static_cast<int>(state.lastBattleOutcome)});
    }

    void finish(const ChessPvpSaveVerificationResult& result)
    {
        write(ChessTraceTerminal{{result.valid, result.sequence, result.message}});
    }

private:
    template<typename Value>
    void write(const Value& value)
    {
        output_ << glz::write_json(value).value() << '\n';
    }

    std::ostream& output_;
};

}
