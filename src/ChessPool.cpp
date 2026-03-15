#include "ChessPool.h"

#include <algorithm>
#include <array>
#include <print>

#include "ChessBalance.h"
#include "ChessRandom.h"
#include "ChessRoleSave.h"
#include "GameUtil.h"
#include "yaml-cpp/yaml.h"

namespace KysChess
{

namespace
{

std::string poolPathForDifficulty(Difficulty difficulty)
{
    return GameUtil::PATH() + ((difficulty == Difficulty::Easy) ? "config/chess_pool_easy.yaml" : "config/chess_pool.yaml");
}

}    // namespace

ChessPool::ChessPool(ChessRandom& random, ChessRoleSave& roleSave)
    : random_(random)
    , roleSave_(roleSave)
{
}

ChessPool::ChessPool(ChessRandom& random, ChessRoleSave& roleSave, const std::vector<StoredShopEntry>& shop)
    : ChessPool(random, roleSave)
{
    for (const auto& shopEntry : shop)
    {
        auto* role = roleSave_.getRole(shopEntry.roleId);
        current_.emplace_back(role, role ? role->Cost : shopEntry.tier);
    }
    getNewChess_ = current_.empty();
}

void ChessPool::ensurePoolLoaded()
{
    auto difficulty = ChessBalance::getDifficulty();
    if (poolLoaded_ && loadedDifficulty_ == difficulty)
    {
        return;
    }

    current_.clear();
    rejected_.clear();
    getNewChess_ = true;

    reloadPool();
    loadedDifficulty_ = difficulty;
    poolLoaded_ = true;
}

void ChessPool::reloadPool()
{
    for (auto& roles : rolesByTier_)
    {
        roles.clear();
    }
    roleIdsInPool_.clear();

    const auto poolPath = poolPathForDifficulty(ChessBalance::getDifficulty());
    try
    {
        loadPoolNode(YAML::LoadFile(poolPath));
    }
    catch (const YAML::Exception& e)
    {
        std::print("【棋池配置】无法读取文件 {}: {}\n", poolPath, e.what());
    }
}

void ChessPool::loadPoolNode(const YAML::Node& root)
{
    auto appendRole = [&](int roleId) {
        auto tier = getRoleTier(roleId);
        if (tier < 1 || tier > 5)
        {
            std::print("【棋池配置】角色 {} 的费用无效，已跳过\n", roleId);
            return;
        }
        rolesByTier_[tier - 1].push_back(roleId);
        roleIdsInPool_.insert(roleId);
    };

    if (root.IsMap() && root["角色"] && root["角色"].IsSequence())
    {
        for (const auto& roleNode : root["角色"])
        {
            appendRole(roleNode.as<int>());
        }
        return;
    }

    if (!root.IsSequence())
    {
        std::print("【棋池配置】棋池格式无效，缺少「角色」列表\n");
        return;
    }

    for (const auto& entry : root)
    {
        auto roles = entry["角色"];
        if (!roles || !roles.IsSequence())
        {
            continue;
        }
        for (const auto& roleNode : roles)
        {
            appendRole(roleNode.as<int>());
        }
    }
}

int ChessPool::getRoleTier(int roleId) const
{
    auto* role = roleSave_.getRole(roleId);
    if (role && role->Cost > 0)
    {
        return role->Cost;
    }
    return -1;
}

Color ChessPool::GetTierColor(int tier) {
    static Color colors[] = {{175,238,238},{0,255,0},{30,144,255},{75,0,130},{255,0,0}};
    return colors[std::clamp(tier - 1, 0, 4)];
}

Role* ChessPool::selectFromPool(int tier)
{
    ensurePoolLoaded();
    const auto& roles = rolesByTier_[tier - 1];
    if (roles.empty())
    {
        return nullptr;
    }

    for (int attempts = 0; attempts < 100; ++attempts)
    {
        auto idx = roles[random_.shopRandInt(static_cast<int>(roles.size()))];
        if (banned_.contains(idx))
        {
            continue;
        }
        auto role = roleSave_.getRole(idx);
        if (tier <= 4 && rejected_.contains(role))
        {
            continue;
        }
        return role;
    }

    Role* fallback = nullptr;
    for (auto idx : roles)
    {
        if (banned_.contains(idx))
        {
            continue;
        }

        auto role = roleSave_.getRole(idx);
        if (tier <= 4 && rejected_.contains(role))
        {
            if (!fallback)
            {
                fallback = role;
            }
            continue;
        }
        return role;
    }

    return fallback;
}

void ChessPool::setBannedRoleIds(const std::set<int>& banned)
{
    banned_ = banned;
}

std::vector<std::pair<Role*, int>> ChessPool::getChessFromPool(int level)
{
    ensurePoolLoaded();
    if (!getNewChess_) {
        return current_;
    }

    getNewChess_ = false;

    std::vector<std::pair<Role*, int>> roles;
    roles.reserve(ChessBalance::config().shopSlotCount);
    rejected_.clear();

    for (int i = 0; i < ChessBalance::config().shopSlotCount; ++i)
    {
        Role* selectedRole = nullptr;
        int selectedTier = -1;

        for (int attempt = 0; attempt < 100 && !selectedRole; ++attempt)
        {
            auto val = random_.shopRandInt(100);
            auto cur = 0;
            for (int tier = 1; tier <= 5; ++tier)
            {
                auto w = ChessBalance::config().shopWeights[level][tier - 1];
                cur += w;
                if (val < cur)
                {
                    selectedRole = selectFromPool(tier);
                    if (selectedRole)
                    {
                        selectedTier = tier;
                    }
                    break;
                }
            }
        }

        if (!selectedRole)
        {
            for (int tier = 1; tier <= 5; ++tier)
            {
                selectedRole = selectFromPool(tier);
                if (selectedRole)
                {
                    selectedTier = tier;
                    break;
                }
            }
        }

        if (!selectedRole)
        {
            break;
        }

        roles.emplace_back(selectedRole, selectedTier);
        rejected_.insert(selectedRole);
    }

    current_ = roles;

    return roles;
}

void ChessPool::removeChessAt(int idx) {
    current_.erase(current_.begin() + idx);
}

void ChessPool::refresh() {
    getNewChess_ = true;
}

Role* ChessPool::selectEnemyFromPool(int tier)
{
    ensurePoolLoaded();
    auto idx = rolesByTier_[tier - 1][random_.enemyRandInt(static_cast<int>(rolesByTier_[tier - 1].size()))];
    return roleSave_.getRole(idx);
}

bool ChessPool::isRoleInPool(int roleId)
{
    ensurePoolLoaded();
    return roleIdsInPool_.contains(roleId);
}

const std::vector<int>& ChessPool::getRolesOfTier(int tier)
{
    static const std::vector<int> empty;
    ensurePoolLoaded();
    if (tier < 1 || tier > 5)
    {
        return empty;
    }
    return rolesByTier_[tier - 1];
}

}    // namespace KysChess
