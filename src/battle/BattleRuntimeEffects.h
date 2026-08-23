#pragma once

#include "BattleEffectSystem.h"

#include <span>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeState;
struct BattleRuntimeUnitRecord;

// Runtime setup calls this once after every canonical unit (including clones)
// has been appended. Each unit owns one binding for its selected ultimate.
void appendRuntimeMagicEffectRules(
    BattleRuntimeState& runtime,
    std::span<const ChessMagicEffectDefinition> definitions);

EffectUnitSnapshot makeEffectUnitSnapshot(
    const BattleRuntimeState& runtime,
    const BattleRuntimeUnitRecord& record);
std::vector<EffectUnitSnapshot> makeEffectUnitSnapshots(const BattleRuntimeState& runtime);

// Owns the copied unit facts used by an effect event. readView() is recreated
// on demand, so moving this snapshot never leaves a cached dangling span.
class BattleEffectRuntimeSnapshot
{
public:
    explicit BattleEffectRuntimeSnapshot(const BattleRuntimeState& runtime);

    std::span<const EffectUnitSnapshot> units() const;
    BattleEffectReadView readView() const;

private:
    std::vector<EffectUnitSnapshot> units_;
    float tileWidth_ = 1.0f;
};

}  // namespace KysChess::Battle
