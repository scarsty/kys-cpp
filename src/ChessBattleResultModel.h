#pragma once

#include "BattlePostBattleSummary.h"
#include "BattleReport.h"
#include "ChessPreparedBattleAnalysis.h"
#include "ChessTalent.h"

namespace KysChess
{
struct ChessBattleResultUnit
{
    BattlePostBattleUnitSummary unit;
    int damageDealt{};
    int damageTaken{};
    int kills{};
    int dps{};
    std::string weaponName;
    std::string armorName;
    std::array<std::string, 2> skillNames;
    std::array<int, 2> skillDamage{};
};

struct ChessBattleResultTeam
{
    std::vector<ChessBattleResultUnit> units;
    std::vector<ChessComboMetadata> synergies;
    std::optional<ChessTalentId> talent;
    std::string neigongNames;
    int damageDealt{};
    int survivors{};
};

// 保留開戰陣容的羈絆與棋手身分；戰後存活數不影響羈絆摘要。
std::array<ChessBattleResultTeam, 2> buildChessBattleResultTeams(
    const BattlePostBattleSummary& summary,
    const BattleReport& report,
    const PreparedChessBattle& prepared,
    const ChessGameContent& content,
    const std::array<std::optional<ChessTalentId>, 2>& talents);
}
