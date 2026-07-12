#pragma once

#include "ChessBattleEffects.h"
#include "ChessDiagnostics.h"

#include <set>
#include <string>
#include <vector>

namespace KysChess
{

class ChessGameContent;
struct ResolvedChessCombo;
struct ChessSessionState;

struct ComboThreshold
{
    int count;
    std::string name;
    std::vector<ComboEffect> effects;
};

struct ComboDef
{
    int id;
    std::string name;
    std::vector<int> memberRoleIds;
    std::vector<ComboThreshold> thresholds;
    bool isAntiCombo = false;
    bool starSynergyBonus = false;
};

struct ChessComboProgress
{
    std::set<int> memberRoleIds;
    int physicalCount{};
    int effectiveCount{};
    int activeThresholdIndex = -1;
    int nextThresholdIndex = -1;
    int displayTargetCount{};
    bool active = false;
    bool isAntiCombo = false;
    bool starSynergyBonus = false;
};

std::vector<ComboDef> loadChessCombos(
    const std::string& path,
    const ChessTextConverter& toTraditional,
    const ChessDiagnosticSink& diagnostics);

ChessComboProgress evaluateChessComboProgress(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const ComboDef& combo);

ChessComboProgress chessComboProgress(
    const ComboDef& combo,
    const ResolvedChessCombo& resolved);

std::string formatChessComboProgressCount(const ChessComboProgress& progress);

bool chessRosterHasActiveComboEffect(
    const ChessSessionState& state,
    const ChessGameContent& content,
    EffectType effectType);

struct ChessComboGoldBonus
{
    int amount{};
    int sourceComboId = -1;

    bool operator==(const ChessComboGoldBonus&) const = default;
};

ChessComboGoldBonus resolveChessComboGoldBonus(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const std::set<int>& survivingChessInstanceIds);

int calculateChessComboGoldBonus(
    const ChessSessionState& state,
    const ChessGameContent& content,
    const std::set<int>& survivingChessInstanceIds);

}  // namespace KysChess
