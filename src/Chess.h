#pragma once

#include "Save.h"

namespace KysChess
{

struct RoleAndStar
{
    Role* role = nullptr;
    int star = 1;

    auto operator<=>(const RoleAndStar&) const = default;
    bool operator==(const RoleAndStar&) const = default;
};

struct ChessInstanceID {
    int value = -1;
    auto operator<=>(const ChessInstanceID&) const = default;
};

struct ItemInstanceID {
    int value = -1;
    auto operator<=>(const ItemInstanceID&) const = default;
};


struct InstancedItem
{
    ItemInstanceID id{};
    int itemId = -1;
};

struct Chess
{
    Role* role = nullptr;
    int star = 1;
    ChessInstanceID id{};
    bool selectedForBattle = false;
    InstancedItem weaponInstance{};
    InstancedItem armorInstance{};
    int fightsWon = 0;
};

constexpr auto k_nonExistentItem = ItemInstanceID{-1};
constexpr auto k_nonExistentChess = ChessInstanceID{-1};

}    //namespace KysChess