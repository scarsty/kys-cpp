#pragma once

#include "BattleEffectSystem.h"

#include <span>
#include <memory>
#include <memory_resource>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeState;
struct BattleRuntimeUnit;
struct BattleRuntimeUnitRecord;
struct BattleActionPlanSeed;
class BattleComboRuntimeFacts;
struct BattleStatusEffectState;

// Shared runtime attribute queries. effectAdjustedAttribute keeps the optional
// event source so conditional rules evaluate in the caller's event context;
// call sites without one use the -1 default.
int effectAdjustedAttribute(
    const BattleRuntimeState& state,
    int unitId,
    BattleAttribute attribute,
    int baseValue,
    int eventSourceUnitId = -1);
int areaAttributeDelta(const BattleRuntimeState& state, int unitId, BattleAttribute attribute);
int areaAdjustedSpeed(const BattleRuntimeState& state, int unitId, int baseSpeed);
int effectAndAreaAdjustedRateAttribute(
    const BattleRuntimeState& state,
    int unitId,
    BattleAttribute attribute,
    int baseValue);
int effectAndAreaAdjustedSpeed(const BattleRuntimeState& state, int unitId, int baseSpeed);
bool areaDamageChannelMatches(BattleDamageKind kind, DamageChannel channel);
int areaOutgoingDamagePctDelta(
    const BattleRuntimeState& state,
    int sourceUnitId,
    BattleDamageKind damageKind);

// Runtime setup calls this once after every canonical unit (including clones)
// has been appended. Each unit owns one binding for its selected ultimate.
void appendRuntimeMagicEffectRules(
    BattleRuntimeState& runtime,
    std::span<const ChessMagicEffectDefinition> definitions);

EffectUnitSnapshot makeEffectUnitSnapshot(
    const BattleRuntimeState& runtime,
    const BattleRuntimeUnitRecord& record,
    std::pmr::memory_resource* memoryResource = std::pmr::get_default_resource());
EffectUnitSnapshot makeEffectUnitSnapshot(
    const BattleRuntimeUnit& unit,
    const BattleComboRuntimeFacts& comboFacts,
    const BattleStatusEffectState& statusEffects,
    const BattleActionPlanSeed* actionPlan,
    std::pmr::memory_resource* memoryResource = std::pmr::get_default_resource());
std::pmr::vector<EffectUnitSnapshot> makeEffectUnitSnapshots(
    const BattleRuntimeState& runtime,
    std::pmr::memory_resource* memoryResource = std::pmr::get_default_resource());
void refreshEffectStatusSnapshot(
    EffectUnitSnapshot& snapshot,
    const BattleStatusEffectState& effects);

// Owns the copied unit facts used by an effect event. readView() is recreated
// on demand, so moving this snapshot never leaves a cached dangling span.
class BattleEffectRuntimeSnapshot
{
public:
    explicit BattleEffectRuntimeSnapshot(const BattleRuntimeState& runtime);
    ~BattleEffectRuntimeSnapshot();
    BattleEffectRuntimeSnapshot(const BattleEffectRuntimeSnapshot&) = delete;
    BattleEffectRuntimeSnapshot& operator=(const BattleEffectRuntimeSnapshot&) = delete;
    BattleEffectRuntimeSnapshot(BattleEffectRuntimeSnapshot&&) noexcept;
    BattleEffectRuntimeSnapshot& operator=(BattleEffectRuntimeSnapshot&&) noexcept;

    std::span<const EffectUnitSnapshot> units() const;
    BattleEffectReadView readView() const;

private:
    struct Storage;
    // Storage 的位址在移動快照時保持不變，內層容器的 allocator 不會懸空。
    std::unique_ptr<Storage> storage_;
    float tileWidth_{};
};

}  // namespace KysChess::Battle
