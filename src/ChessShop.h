#pragma once

#include "ChessPool.h"
#include "ChessRandom.h"
#include "GameDataStore.h"

namespace KysChess
{

class ChessRoleSave;

class ChessShop
{
public:
    ChessShop(ChessRandom& random, ChessRoleSave& roleSave);
    ChessShop(ChessRandom& random, ChessRoleSave& roleSave, const GameDataStore& store);

    void exportTo(GameDataStore& store) const;

    bool isLocked() const { return locked_; }
    void setLocked(bool locked) { locked_ = locked; }

    ChessPool& pool() { return pool_; }
    const ChessPool& pool() const { return pool_; }

private:
    bool locked_ = false;
    ChessPool pool_;
};

}