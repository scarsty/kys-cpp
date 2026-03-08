#include "ChessPool.h"

#include <algorithm>
#include <array>
#include "ChessBalance.h"
#include "ChessRandom.h"
#include "ChessRoleSave.h"

namespace KysChess
{

namespace
{

const std::array<std::vector<int>, 5>& chessPool()
{
    return ChessBalance::config().chessPool;
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
        current_.emplace_back(roleSave_.getRole(shopEntry.roleId), shopEntry.tier);
    }
    getNewChess_ = current_.empty();
}


int ChessPool::GetChessTier(int roleId) {
    for (int i = 0; i < chessPool().size(); ++i) {
        if (std::find(chessPool()[i].begin(), chessPool()[i].end(), roleId) != chessPool()[i].end()) {
            return i + 1;
        }
    }
    return -1;
}

Color ChessPool::GetTierColor(int tier) {
    static Color colors[] = {{175,238,238},{0,255,0},{30,144,255},{75,0,130},{255,0,0}};
    return colors[std::clamp(tier - 1, 0, 4)];
}

Role* ChessPool::selectFromPool(int tier)
{
    for (;;)
    {
        auto idx = chessPool()[tier - 1][random_.shopRandInt(static_cast<int>(chessPool()[tier - 1].size()))];
        auto role = roleSave_.getRole(idx);
        if (tier <= 4 && rejected_.contains(role))
        {
            continue;
        }
        return role;
    }
}

std::vector<std::pair<Role*, int>> ChessPool::getChessFromPool(int level)
{
    if (!getNewChess_) {
        return current_;
    }

    getNewChess_ = false;

    std::vector<std::pair<Role*, int>> roles;

    for (int i = 0; i < 5; ++i)
    {
        // 应该是 0~99
        auto val = random_.shopRandInt(100);
        auto cur = 0;
        for (int tier = 1; tier <= 5; ++tier)
        {
            auto w = ChessBalance::config().shopWeights[level][tier - 1];
            cur += w;
            if (val < cur)
            {
                roles.emplace_back(selectFromPool(tier), tier);
                break;
            }
        }
    }

    rejected_.clear();
    for (const auto& entry : roles)
    {
        rejected_.insert(entry.first);
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
    auto idx = chessPool()[tier - 1][random_.enemyRandInt(static_cast<int>(chessPool()[tier - 1].size()))];
    return roleSave_.getRole(idx);
}

const std::vector<int>& ChessPool::getRolesOfTier(int tier)
{
    return chessPool()[tier - 1];
}

}    // namespace KysChess