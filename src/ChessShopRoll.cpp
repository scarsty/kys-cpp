#include "ChessShopRoll.h"

namespace KysChess
{

bool ShopCandidateBuckets::empty() const
{
    return preferred.empty();
}

const std::vector<int>& ShopCandidateBuckets::selectable() const
{
    return preferred;
}

ShopCandidateBuckets buildShopCandidateBuckets(
    std::span<const int> tierRoleIds,
    int tier,
    const std::set<int>& bannedRoleIds,
    const std::unordered_set<int>& rejectedRoleIds)
{
    ShopCandidateBuckets buckets;
    for (auto roleId : tierRoleIds)
    {
        if (bannedRoleIds.contains(roleId))
        {
            continue;
        }
        if (tier <= 4 && rejectedRoleIds.contains(roleId))
        {
            buckets.previousRollBlocked.push_back(roleId);
            continue;
        }
        buckets.preferred.push_back(roleId);
    }
    return buckets;
}

bool tierHasShopCandidate(
    std::span<const int> tierRoleIds,
    int tier,
    const std::set<int>& bannedRoleIds,
    const std::unordered_set<int>& rejectedRoleIds)
{
    return !buildShopCandidateBuckets(tierRoleIds, tier, bannedRoleIds, rejectedRoleIds).empty();
}

}    // namespace KysChess
