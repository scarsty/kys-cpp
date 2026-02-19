#include "BattleRoleManager.h"
#include "ChessBalance.h"
#include "ChessCombo.h"
#include "Save.h"

namespace KysChess
{

std::map<int, BattleRoleManager::RoleBackup> BattleRoleManager::backups_;

void BattleRoleManager::applyStarBonus(Role* role, int stars)
{
    if (stars <= 0) return;
    auto& cfg = ChessBalance::config();
    double hpAtkMult = 1.0 + cfg.starHPAtkMult * stars;
    double defMult = 1.0 + cfg.starDefMult * stars;
    double spdMult = 1.0 + cfg.starSpdMult * stars;
    role->MaxHP = static_cast<int>(role->MaxHP * hpAtkMult);
    role->HP = role->MaxHP;
    role->Attack = static_cast<int>(role->Attack * hpAtkMult);
    role->Defence = static_cast<int>(role->Defence * defMult);
    role->Speed = static_cast<int>(role->Speed * spdMult);
    role->Fist = static_cast<int>(role->Fist * defMult);
    role->Sword = static_cast<int>(role->Sword * defMult);
    role->Knife = static_cast<int>(role->Knife * defMult);
    role->Unusual = static_cast<int>(role->Unusual * defMult);
    role->HiddenWeapon = static_cast<int>(role->HiddenWeapon * defMult);
}

std::vector<int> BattleRoleManager::applyEnhancements(const std::vector<Chess>& selectedChess)
{
    restoreRoles();

    std::vector<int> roleIds;

    for (const auto& chess : selectedChess)
    {
        Role* role = chess.role;
        roleIds.push_back(role->ID);

        // Full heal before battle
        role->HP = role->MaxHP;

        // Always backup (combo buffs modify all roles, not just starred)
        RoleBackup backup;
        backup.MaxHP = role->MaxHP;
        backup.HP = role->HP;
        backup.Attack = role->Attack;
        backup.Defence = role->Defence;
        backup.Fist = role->Fist;
        backup.Sword = role->Sword;
        backup.Knife = role->Knife;
        backup.Unusual = role->Unusual;
        backup.HiddenWeapon = role->HiddenWeapon;
        backup.Speed = role->Speed;
        backups_[role->ID] = backup;

        if (chess.star > 0)
            applyStarBonus(role, chess.star);
    }

    // Detect and apply combo buffs after star bonuses
    auto activeCombos = ChessCombo::detectCombos(selectedChess);
    auto comboStates = ChessCombo::buildComboStates(activeCombos);
    ChessCombo::applyStatBuffs(comboStates);

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
            role->Fist = backup.Fist;
            role->Sword = backup.Sword;
            role->Knife = backup.Knife;
            role->Unusual = backup.Unusual;
            role->HiddenWeapon = backup.HiddenWeapon;
            role->Speed = backup.Speed;
        }
    }

    backups_.clear();
    ChessCombo::clearActiveStates();
}

}  // namespace KysChess
