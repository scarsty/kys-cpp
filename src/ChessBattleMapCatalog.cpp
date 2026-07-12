#include "ChessBattleMapCatalog.h"

#include <algorithm>
#include <tuple>

namespace KysChess
{

const std::vector<ChessBattleMapCatalogEntry>& ChessBattleMapCatalog::entries()
{
    static const std::vector<ChessBattleMapCatalogEntry> maps{
        {
            6,
            10,
            {{30, 39}, {32, 37}, {32, 34}, {28, 32}, {22, 21}, {22, 18},
             {32, 36}, {32, 35}, {22, 20}, {22, 19}},
            {{31, 39}, {31, 38}, {31, 37}},
            "迷洞巷戰",
        },
        {
            13,
            20,
            {{25, 27}, {24, 27}, {25, 28}, {24, 28}, {25, 25}, {24, 25},
             {25, 26}, {24, 26}, {25, 29}, {24, 29}},
            {{26, 25}, {26, 26}, {26, 27}},
            "十面埋伏",
        },
        {
            17,
            13,
            {{29, 20}, {31, 23}, {33, 26}, {32, 20}, {34, 23}, {35, 20},
             {34, 20}, {33, 23}, {33, 20}, {32, 23}},
            {{35, 21}, {34, 21}, {35, 22}},
            "雙線迎擊",
        },
        {
            21,
            10,
            {{33, 27}, {34, 25}, {34, 30}, {35, 27}, {35, 29}, {35, 24},
             {35, 25}, {34, 27}, {35, 30}, {35, 28}},
            {{36, 26}, {36, 27}, {34, 28}},
            "庭院圍戰",
        },
        {
            24,
            11,
            {{35, 32}, {35, 28}, {35, 26}, {35, 34}, {35, 36}, {35, 24},
             {35, 35}, {35, 33}, {35, 27}, {35, 25}},
            {{36, 32}, {36, 31}, {36, 28}},
            "長廊固守",
        },
        {
            26,
            11,
            {{30, 29}, {33, 28}, {32, 33}, {34, 31}, {36, 28}, {36, 35},
             {35, 28}, {34, 28}, {37, 35}, {37, 28}},
            {{35, 29}, {35, 30}, {35, 31}},
            "側殿突圍",
        },
        {
            54,
            15,
            {{24, 27}, {24, 29}, {24, 25}, {24, 31}, {24, 23}, {24, 33},
             {24, 32}, {24, 30}, {24, 28}, {24, 26}},
            {{25, 27}, {25, 29}, {25, 31}},
            "長廊夾擊",
        },
        {
            56,
            20,
            {{17, 29}, {18, 29}, {19, 29}, {20, 29}, {21, 29}, {22, 29},
             {23, 29}, {16, 29}, {22, 30}, {22, 28}},
            {{19, 30}, {20, 30}, {21, 30}},
            "山門死戰",
        },
        {
            60,
            10,
            {{32, 20}, {36, 17}, {35, 20}, {36, 24}, {38, 20}, {41, 20},
             {40, 20}, {39, 20}, {37, 20}, {36, 20}},
            {{33, 20}, {34, 20}, {36, 21}},
            "散陣遭遇",
        },
        {
            80,
            14,
            {{32, 34}, {32, 31}, {32, 28}, {32, 25}, {32, 22}, {32, 19},
             {32, 33}, {32, 32}, {32, 30}, {32, 29}},
            {{31, 32}, {31, 31}, {31, 30}},
            "狹道拒守",
        },
    };
    return maps;
}

const ChessBattleMapCatalogEntry* ChessBattleMapCatalog::find(int battleId)
{
    const auto& maps = entries();
    const auto found = std::ranges::find(maps, battleId, &ChessBattleMapCatalogEntry::battleId);
    return found == maps.end() ? nullptr : &*found;
}

std::vector<int> ChessBattleMapCatalog::fittingMapIds(
    const ChessGameContent& content,
    int allyCount,
    int enemyCount)
{
    std::vector<int> result;
    std::vector<const ChessBattleMapCatalogEntry*> availableCuratedMaps;
    for (const auto& map : entries())
    {
        if (!content.battleMaps().contains(map.battleId))
        {
            continue;
        }
        availableCuratedMaps.push_back(&map);
        if (static_cast<int>(map.teammatePositions.size()) >= allyCount
            && map.enemyCapacity >= enemyCount)
        {
            result.push_back(map.battleId);
        }
    }
    if (!result.empty())
    {
        return result;
    }

    if (!availableCuratedMaps.empty())
    {
        const auto best = std::ranges::max_element(availableCuratedMaps, {}, [](const auto* map) {
            return std::tuple{
                std::min(static_cast<int>(map->teammatePositions.size()), 10),
                map->enemyCapacity,
            };
        });
        result.push_back((*best)->battleId);
        return result;
    }

    for (const auto& [id, map] : content.battleMaps())
    {
        if (static_cast<int>(map.teammateX.size()) >= allyCount
            && static_cast<int>(map.enemyX.size()) >= enemyCount)
        {
            result.push_back(id);
        }
    }
    if (result.empty() && !content.battleMaps().empty())
    {
        const auto best = std::ranges::max_element(
            content.battleMaps(),
            {},
            [](const auto& entry) {
                return static_cast<int>(entry.second.teammateX.size())
                    + static_cast<int>(entry.second.enemyX.size());
            });
        result.push_back(best->first);
    }
    return result;
}

std::string_view ChessBattleMapCatalog::displayName(int battleId)
{
    const auto* map = find(battleId);
    return map ? map->name : std::string_view{};
}

}  // namespace KysChess
