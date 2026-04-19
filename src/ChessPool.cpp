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

const std::array<Color, 5> kTierColors = {{
    {175, 238, 238, 255},
    {0, 255, 0, 255},
    {30, 144, 255, 255},
    {186, 96, 255, 255},
    {255, 0, 0, 255},
}};

}    // namespace

ChessPool::ChessPool(ChessRandom& random, ChessRoleSave& roleSave)
    : random_(random)
    , roleSave_(roleSave)
{
}

ChessPool::ChessPool(ChessRandom& random, ChessRoleSave& roleSave, const std::vector<StoredShopEntry>& shop, const std::vector<int>& rejectedRoleIds)
    : ChessPool(random, roleSave)
{
    for (const auto& shopEntry : shop)
    {
        auto* role = roleSave_.getRole(shopEntry.roleId);
        current_.emplace_back(role, role ? role->Cost : shopEntry.tier);
    }
    rejected_.insert(rejectedRoleIds.begin(), rejectedRoleIds.end());
}

void ChessPool::ensurePoolLoaded()
{
    auto difficulty = ChessBalance::getDifficulty();
    if (poolLoaded_ && loadedDifficulty_ == difficulty)
    {
        return;
    }

    if (poolLoaded_ && loadedDifficulty_ != difficulty)
    {
        rejected_.clear();
        current_.clear();
    }

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

Color ChessPool::GetTierColor(int tier)
{
    return kTierColors[std::clamp(tier - 1, 0, static_cast<int>(kTierColors.size()) - 1)];
}

Role* ChessPool::selectFromPool(int tier, const std::unordered_set<int>& alreadySelected)
{
    ensurePoolLoaded();
    const auto& roles = rolesByTier_[tier - 1];

    std::vector<int> candidates;
    std::vector<int> rejectedCandidates;
    for (auto roleId : roles)
    {
        if (banned_.contains(roleId) || alreadySelected.contains(roleId))
        {
            continue;
        }
        if (tier <= 4 && rejected_.contains(roleId))
        {
            rejectedCandidates.push_back(roleId);
            continue;
        }
        candidates.push_back(roleId);
    }

    auto& pool = candidates.empty() ? rejectedCandidates : candidates;
    if (pool.empty())
    {
        return nullptr;
    }
    return roleSave_.getRole(pool[random_.shopRandInt(static_cast<int>(pool.size()))]);
}

bool ChessPool::tierHasCandidate(int tier, const std::unordered_set<int>& alreadySelected) const
{
    for (auto roleId : rolesByTier_[tier - 1])
    {
        if (!banned_.contains(roleId) && !alreadySelected.contains(roleId))
        {
            return true;
        }
    }
    return false;
}

void ChessPool::setBannedRoleIds(const std::set<int>& banned)
{
    banned_ = banned;
}

// Private: generate new shop items for the given level.
void ChessPool::generateShop(int level)
{
    ensurePoolLoaded();

    std::unordered_set<int> selected;
    current_.clear();
    current_.reserve(ChessBalance::config().shopSlotCount);

    for (int i = 0; i < ChessBalance::config().shopSlotCount; ++i)
    {
        // Compute effective weights: only include tiers that still have candidates.
        int totalWeight = 0;
        std::array<int, 5> weights = {};
        for (int t = 0; t < 5; ++t)
        {
            if (tierHasCandidate(t + 1, selected))
            {
                weights[t] = ChessBalance::config().shopWeights[level][t];
                totalWeight += weights[t];
            }
        }
        if (totalWeight == 0)
        {
            break;
        }

        auto val = random_.shopRandInt(totalWeight);
        int cum = 0;
        int selectedTier = 1;
        for (int t = 0; t < 5; ++t)
        {
            cum += weights[t];
            if (val < cum)
            {
                selectedTier = t + 1;
                break;
            }
        }

        auto* role = selectFromPool(selectedTier, selected);
        if (!role)
        {
            break;
        }

        current_.emplace_back(role, selectedTier);
        selected.insert(role->ID);
    }
}

void ChessPool::removeChessAt(int idx) {
    current_.erase(current_.begin() + idx);
}

void ChessPool::refresh(int level) {
    // The roles still in the shop become "rejected" for the next generation,
    // preventing the same unchosen pieces from reappearing immediately.
    rejected_.clear();
    for (const auto& [role, tier] : current_)
    {
        if (role)
        {
            rejected_.insert(role->ID);
        }
    }
    current_.clear();
    generateShop(level);
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
