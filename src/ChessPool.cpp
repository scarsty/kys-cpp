#include "ChessPool.h"

#include <algorithm>
#include <array>
#include "ChessBalance.h"
#include "Save.h"
#include "GameState.h"

namespace KysChess
{

namespace
{

const std::array<std::vector<int>, 5>& chessPool()
{
    return ChessBalance::config().chessPool;
}

}    // namespace


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
        auto idx = chessPool()[tier - 1][GameState::get().shopRandInt(chessPool()[tier - 1].size())];
        auto role = Save::getInstance()->getRole(idx);
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
        auto val = GameState::get().shopRandInt(100);
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
    for (auto [r, price] : roles)
    {
        rejected_.insert(r);
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

void ChessPool::restoreShop(std::vector<std::pair<Role*, int>> shop) {
    current_ = std::move(shop);
    getNewChess_ = current_.empty();
    // rejected_ is only used during getChessFromPool generation,
    // which clears and rebuilds it each time, so no need to restore it.
    rejected_.clear();
}

Role* ChessPool::selectEnemyFromPool(int tier)
{
    auto idx = chessPool()[tier - 1][GameState::get().enemyRandInt(chessPool()[tier - 1].size())];
    return Save::getInstance()->getRole(idx);
}

const std::vector<int>& ChessPool::getRolesOfTier(int tier)
{
    return chessPool()[tier - 1];
}

}    // namespace KysChess