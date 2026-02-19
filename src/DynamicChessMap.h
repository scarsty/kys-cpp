#pragma once
#include "BattleSceneHades.h"
#include <memory>
#include <vector>

struct DynamicBattleRoles
{
    std::vector<int> teammate_ids;  // Role IDs for teammates (up to 10)
    std::vector<int> teammate_stars; // Star levels per teammate
    std::vector<int> enemy_ids;     // Role IDs for enemies (up to 20)
    std::vector<int> enemy_stars;   // Star levels per enemy
};

class DynamicChessMap
{
public:
    // Creates a BattleSceneHades with randomly selected map and extended battle info
    // teammates: Role IDs for teammates (up to 10)
    // enemies: Role IDs for enemies (up to 20)
    static std::shared_ptr<BattleSceneHades> createBattle(const DynamicBattleRoles& roles);

private:
    struct MapInfo
    {
        int battle_id;
        int battlefield_id;
        int enemy_count;
        std::vector<std::pair<int, int>> existing_positions;  // (x, y) for 6 existing teammates
        std::vector<std::pair<int, int>> new_positions;       // (x, y) for 4 new teammates
        const char* name;
        const char* ascii_map;
        const char* description;
    };

    static const std::vector<MapInfo>& getTopMaps();
};
