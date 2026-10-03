#pragma once

#include "BattleEffectSystem.h"

#include <array>
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

// 事件生命週期內的快照與內層容器共用堆疊 arena；禁止移動以固定 allocator 位址。
class BattleEffectRuntimeSnapshot
{
public:
    explicit BattleEffectRuntimeSnapshot(const BattleRuntimeState& runtime);
    BattleEffectRuntimeSnapshot(const BattleEffectRuntimeSnapshot&) = delete;
    BattleEffectRuntimeSnapshot& operator=(const BattleEffectRuntimeSnapshot&) = delete;

    std::span<const EffectUnitSnapshot> units() const;
    BattleEffectReadView readView() const;

private:
    std::array<std::byte, 16 * 1024> buffer_;
    std::pmr::monotonic_buffer_resource memory_{ buffer_.data(), buffer_.size() };
    std::pmr::vector<EffectUnitSnapshot> units_;
    float tileWidth_{};
};

}  // namespace KysChess::Battle
