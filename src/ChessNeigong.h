#pragma once

#include "ChessBattleEffectTypes.h"
#include "ChessDiagnostics.h"
#include "Types.h"

#include <array>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <span>
namespace KysChess
{

struct NeigongDef
{
    int magicId;
    int itemId;
    int tier;
    std::string name;
    std::vector<EffectRule> rules;
    std::vector<GameplayEffect> effects;
};

struct NeigongConfig
{
    int additionalOptionCost = 4;
    int choiceCount = 3;
    std::map<int, std::vector<int>> tiersByBoss;
};

bool loadChessNeigong(
    const std::string& path,
    std::span<Item* const> items,
    const std::function<const Magic*(int)>& findMagic,
    const ChessDiagnosticSink& diagnostics,
    NeigongConfig& config,
    std::vector<NeigongDef>& pool);

}    // namespace KysChess
