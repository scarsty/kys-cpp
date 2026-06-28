#pragma once

#include <set>
#include <span>
#include <unordered_set>
#include <vector>

namespace KysChess
{

struct ShopCandidateBuckets
{
    std::vector<int> preferred;
    std::vector<int> previousRollBlocked;

    bool empty() const;
    const std::vector<int>& selectable() const;
};

ShopCandidateBuckets buildShopCandidateBuckets(
    std::span<const int> tierRoleIds,
    int tier,
    const std::set<int>& bannedRoleIds,
    const std::unordered_set<int>& rejectedRoleIds);

bool tierHasShopCandidate(
    std::span<const int> tierRoleIds,
    int tier,
    const std::set<int>& bannedRoleIds,
    const std::unordered_set<int>& rejectedRoleIds);

}    // namespace KysChess
