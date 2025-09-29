#pragma once

#include "Save.h"

#include <unordered_set>

namespace KysChess
{

// Returns a list of pairs of Role* and its star (0-4)
std::vector<std::pair<Role*, int>> getChessFromPool(int level, int pieces);

}    // namespace KysChess