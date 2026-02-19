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
    // Star scaling multipliers — loaded from chess_balance.yaml

    // Apply star bonus to a single role (used by both player and enemy sides)
    static void applyStarBonus(Role* role, int stars);

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
        int Fist, Sword, Knife, Unusual, HiddenWeapon;
        int Speed;
    };

    static std::map<int, RoleBackup> backups_;
};

}  // namespace KysChess
