#include "ChessBattleResultModel.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>

namespace KysChess
{
std::array<ChessBattleResultTeam, 2> buildChessBattleResultTeams(
    const BattlePostBattleSummary& summary,
    const BattleReport& report,
    const PreparedChessBattle& prepared,
    const ChessGameContent& content,
    const std::array<std::optional<ChessTalentId>, 2>& talents)
{
    std::array<ChessBattleResultTeam, 2> result;
    auto synergies = preparedChessBattleSynergies(prepared, content);
    for (int side = 0; side < 2; ++side)
    {
        auto& team = result[side];
        team.talent = talents[side];
        team.synergies = std::move(synergies[side]);
        for (int id : prepared.obtainedNeigongIdsByTeam[side])
        {
            const auto found = std::ranges::find(content.neigong(), id, &NeigongDef::magicId);
            assert(found != content.neigong().end());
            if (!team.neigongNames.empty()) team.neigongNames += " · ";
            team.neigongNames += found->name;
        }
        for (const auto& unit : side == 0 ? summary.allies : summary.enemies)
        {
            ChessBattleResultUnit entry;
            entry.unit = unit;
            entry.weaponName = chessItemDisplayName(content, unit.weaponId);
            entry.armorName = chessItemDisplayName(content, unit.armorId);
            if (const auto found = report.stats().find(unit.identity.battleId); found != report.stats().end())
            {
                const auto& stats = found->second;
                entry.damageDealt = stats.damageDealt;
                entry.damageTaken = stats.damageTaken;
                entry.kills = stats.kills;
                const int lastActiveFrame = stats.lastActiveFrame > 0 ? stats.lastActiveFrame : report.battleEndFrame();
                if (lastActiveFrame > 0)
                    entry.dps = static_cast<int>(static_cast<std::int64_t>(entry.damageDealt) * 60 / lastActiveFrame);
                for (const auto& [skillId, damage] : stats.damagePerSkillId)
                {
                    const int slot = damage > entry.skillDamage[0] ? 0 : damage > entry.skillDamage[1] ? 1 : -1;
                    if (slot < 0) continue;
                    if (slot == 0)
                    {
                        entry.skillNames[1] = std::move(entry.skillNames[0]);
                        entry.skillDamage[1] = entry.skillDamage[0];
                    }
                    const auto* magic = content.magic(skillId);
                    entry.skillNames[slot] = magic ? magic->Name : std::format("武學{}", skillId);
                    entry.skillDamage[slot] = damage;
                }
            }
            team.damageDealt += entry.damageDealt;
            if (!unit.dead) ++team.survivors;
            team.units.push_back(std::move(entry));
        }
        std::ranges::stable_sort(team.units, [](const auto& a, const auto& b) { return a.damageDealt > b.damageDealt; });
    }
    return result;
}
}
