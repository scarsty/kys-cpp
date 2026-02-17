#pragma once

#include "Chess.h"
#include "Save.h"
#include <vector>
#include <map>

namespace KysChess
{

// Manages dynamic role creation for battles
// Handles star enhancements by temporarily modifying roles
class BattleRoleManager
{
public:
    // Star enhancement constants
    static constexpr int HP_PER_STAR = 100;
    static constexpr int ATK_PER_STAR = 60;
    static constexpr int DEF_PER_STAR = 10;

    // Applies star enhancements to selected chess roles
    // Returns role IDs that can be used in battle
    static std::vector<int> applyEnhancements(const std::vector<Chess>& selectedChess);

    // Restores roles to their original state after battle
    static void restoreRoles();

private:
    struct RoleBackup
    {
        int MaxHP;
        int HP;
        int Attack;
        int Defence;
    };

    static std::map<int, RoleBackup> backups_;
};

}  // namespace KysChess
