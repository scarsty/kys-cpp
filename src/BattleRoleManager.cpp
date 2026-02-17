#include "BattleRoleManager.h"
#include "Save.h"

namespace KysChess
{

std::map<int, BattleRoleManager::RoleBackup> BattleRoleManager::backups_;

std::vector<int> BattleRoleManager::applyEnhancements(const std::vector<Chess>& selectedChess)
{
    restoreRoles();  // Clean up any previous enhancements

    std::vector<int> roleIds;
    auto save = Save::getInstance();

    for (const auto& chess : selectedChess)
    {
        Role* role = chess.role;
        roleIds.push_back(role->ID);

        if (chess.star > 0)
        {
            // Backup original stats
            RoleBackup backup;
            backup.MaxHP = role->MaxHP;
            backup.HP = role->HP;
            backup.Attack = role->Attack;
            backup.Defence = role->Defence;
            backups_[role->ID] = backup;

            // Apply star enhancements
            int starBonus = chess.star;
            role->MaxHP += HP_PER_STAR * starBonus;
            role->HP = role->MaxHP;  // Start at full HP
            role->Attack += ATK_PER_STAR * starBonus;
            role->Defence += DEF_PER_STAR * starBonus;
        }
    }

    return roleIds;
}

void BattleRoleManager::restoreRoles()
{
    auto save = Save::getInstance();

    for (const auto& [roleId, backup] : backups_)
    {
        Role* role = save->getRole(roleId);
        if (role)
        {
            role->MaxHP = backup.MaxHP;
            role->HP = backup.HP;
            role->Attack = backup.Attack;
            role->Defence = backup.Defence;
        }
    }

    backups_.clear();
}

}  // namespace KysChess
