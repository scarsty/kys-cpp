#include "ChessShop.h"

#include "ChessRoleSave.h"

namespace KysChess
{

ChessShop::ChessShop(ChessRandom& random, ChessRoleSave& roleSave)
    : pool_(random, roleSave)
{
}

ChessShop::ChessShop(ChessRandom& random, ChessRoleSave& roleSave, const GameDataStore& store)
    : pool_(random, roleSave, store.currentShop)
{
    locked_ = store.shopLocked;
    pool_.setBannedRoleIds(store.bannedRoleIds);
}

void ChessShop::exportTo(GameDataStore& store) const
{
    store.shopLocked = locked_;
    store.currentShop.clear();
    for (auto [role, tier] : pool_.getCurrentShop())
    {
        if (!role)
        {
            continue;
        }
        store.currentShop.push_back({role->ID, tier});
    }
}

}
