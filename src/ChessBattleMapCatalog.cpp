#include "ChessBattleMapCatalog.h"
#include "ChessPvp.h"

#include <algorithm>

namespace KysChess
{

const std::vector<ChessBattleMapCatalogEntry>& ChessBattleMapCatalog::entries()
{
    static const std::vector<ChessBattleMapCatalogEntry> maps{
        {
            6,
            10,
            {{30, 39}, {32, 37}, {32, 34}, {28, 32}, {22, 21}, {22, 18},
             {32, 36}, {32, 35}, {22, 20}, {22, 19}, {32, 38}},
            {{31, 39}, {31, 38}, {31, 37}},
            "迷洞巷戰",
            {{23, 35}, {24, 35}, {24, 37}},
        },
        // 暫停輪替：敵軍從四面包圍我方，開局過於困難。
        // {
        //     13,
        //     20,
        //     {{25, 27}, {24, 27}, {25, 28}, {24, 28}, {25, 25}, {24, 25},
        //      {25, 26}, {24, 26}, {25, 29}, {24, 29}},
        //     {{26, 25}, {26, 26}, {26, 27}},
        //     "十面埋伏",
        // },
        {
            17,
            13,
            {{29, 20}, {31, 23}, {33, 26}, {32, 20}, {34, 23}, {35, 20},
             {34, 20}, {33, 23}, {33, 20}, {32, 23}, {31, 20}},
            {{35, 21}, {34, 21}, {35, 22}},
            "雙線迎擊",
            {{19, 33}, {20, 33}, {20, 35}},
        },
        {
            21,
            10,
            {{33, 27}, {34, 25}, {34, 30}, {35, 27}, {35, 29}, {35, 24},
             {35, 25}, {34, 27}, {35, 30}, {35, 28}, {34, 26}},
            {{36, 26}, {36, 27}, {34, 28}},
            "庭院圍戰",
            {{28, 26}, {28, 28}, {28, 30}},
        },
        {
            24,
            11,
            {{35, 32}, {35, 28}, {35, 26}, {35, 34}, {35, 36}, {35, 24},
             {35, 35}, {35, 33}, {35, 27}, {35, 25}, {35, 30}},
            {{36, 32}, {36, 31}, {36, 28}},
            "長廊固守",
            {{23, 26}, {23, 30}, {23, 34}},
        },
        {
            26,
            11,
            {{30, 29}, {33, 28}, {32, 33}, {34, 31}, {36, 28}, {36, 35},
             {35, 28}, {34, 28}, {37, 35}, {37, 28}, {33, 32}},
            {{35, 29}, {35, 30}, {35, 31}},
            "側殿突圍",
            {{24, 23}, {25, 24}, {26, 23}},
        },
        {
            54,
            15,
            {{24, 27}, {24, 29}, {24, 25}, {24, 31}, {24, 23}, {24, 33},
             {24, 32}, {24, 30}, {24, 28}, {24, 26}, {24, 24}},
            {{25, 27}, {25, 29}, {25, 31}},
            "長廊夾擊",
            {{14, 26}, {14, 27}, {14, 28}},
        },
        {
            56,
            20,
            {{17, 29}, {18, 29}, {19, 29}, {20, 29}, {21, 29}, {22, 29},
             {23, 29}, {16, 29}, {22, 30}, {22, 28}, {23, 30}},
            {{19, 30}, {20, 30}, {21, 30}},
            "山門死戰",
            {{11, 28}, {12, 28}, {13, 28}},
        },
        {
            60,
            10,
            {{32, 20}, {36, 17}, {35, 20}, {36, 24}, {38, 20}, {41, 20},
             {40, 20}, {39, 20}, {37, 20}, {36, 20}, {36, 22}},
            {{33, 20}, {34, 20}, {36, 21}},
            "散陣遭遇",
            {{17, 22}, {17, 23}, {17, 25}},
        },
        {
            80,
            14,
            {{32, 34}, {32, 31}, {32, 28}, {32, 25}, {32, 22}, {32, 19},
             {32, 33}, {32, 32}, {32, 30}, {32, 29}, {32, 27}},
            {{31, 32}, {31, 31}, {31, 30}},
            "狹道拒守",
            {{20, 32}, {21, 35}, {22, 32}},
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
    for (const auto& map : entries())
    {
        const auto found = content.battleMaps().find(map.battleId);
        if (found != content.battleMaps().end()
            && static_cast<int>(map.teammatePositions.size()) >= allyCount
            && map.enemyCapacity >= enemyCount
            && static_cast<int>(std::min(found->second.enemyX.size(), found->second.enemyY.size())) >= enemyCount)
            result.push_back(map.battleId);
    }
    return result;
}

std::string_view ChessBattleMapCatalog::displayName(int battleId)
{
    if (battleId == ChessPvpMapLayout::BattleId)
    {
        return ChessPvpMapLayout::DisplayName;
    }
    const auto* map = find(battleId);
    return map ? map->name : std::string_view{};
}

}  // namespace KysChess
