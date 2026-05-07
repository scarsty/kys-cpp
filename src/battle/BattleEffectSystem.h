#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace KysChess::Battle
{

struct BattleRuntimeUnit;
struct BattleUnitStore;

enum class BattleHook
{
    BattleStart,
    Frame,
    SkillFinished,
    UltimateCast,
    AttackHit,
    BeingHit,
    BeforeAttackDamage,
    BeforeDefenseDamage,
    AfterHeal,
    ShieldBreak,
    WouldDie,
    Kill,
    SelfDeath,
    SynergyMemberDeath,
};

enum class BattleEffectTarget
{
    Self,
    Source,
    Target,
    SourceTeam,
    TargetTeam,
};

enum class BattleEffectCommandType
{
    Heal,
    AddShield,
    AddInvincibility,
    ModifyResource,
    ModifyCooldown,
    DedicatedEffect,
};

struct BattleEffectDefinition
{
    int id = -1;
    std::string type;
    BattleHook hook = BattleHook::Frame;
    BattleEffectTarget target = BattleEffectTarget::Self;
    std::string executorKey;
    int value = 0;
    int value2 = 0;
    int durationFrames = 0;
    int maxCount = 0;
};

struct BattleEffectEvent
{
    BattleHook hook = BattleHook::Frame;
    int sourceUnitId = -1;
    int targetUnitId = -1;
};

struct BattleEffectCommand
{
    BattleEffectCommandType type = BattleEffectCommandType::DedicatedEffect;
    int effectId = -1;
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int value = 0;
    std::string label;
};

class BattleEffectContext
{
public:
    BattleEffectContext(BattleUnitStore& units,
                        const BattleEffectEvent& event,
                        const BattleEffectDefinition& effect);

    const BattleEffectEvent& event() const;
    const BattleEffectDefinition& effect() const;

    std::vector<BattleRuntimeUnit*> targets() const;
    void emit(BattleEffectCommand command);
    const std::vector<BattleEffectCommand>& commands() const;

private:
    BattleRuntimeUnit* findUnit(int id) const;
    std::vector<BattleRuntimeUnit*> teamUnits(int team) const;

    BattleUnitStore& units_;
    const BattleEffectEvent& event_;
    const BattleEffectDefinition& effect_;
    std::vector<BattleEffectCommand> commands_;
};

class IBattleEffectExecutor
{
public:
    virtual ~IBattleEffectExecutor() = default;
    virtual void execute(BattleEffectContext& context) const = 0;
};

class HealPercentExecutor final : public IBattleEffectExecutor
{
public:
    void execute(BattleEffectContext& context) const override;
};

class AddShieldExecutor final : public IBattleEffectExecutor
{
public:
    void execute(BattleEffectContext& context) const override;
};

class AddInvincibilityExecutor final : public IBattleEffectExecutor
{
public:
    void execute(BattleEffectContext& context) const override;
};

class RestoreMpExecutor final : public IBattleEffectExecutor
{
public:
    void execute(BattleEffectContext& context) const override;
};

class ModifyCooldownPercentExecutor final : public IBattleEffectExecutor
{
public:
    void execute(BattleEffectContext& context) const override;
};

class DedicatedEffectExecutor final : public IBattleEffectExecutor
{
public:
    void execute(BattleEffectContext& context) const override;
};

class BattleEffectRegistry
{
public:
    BattleEffectRegistry();

    void registerExecutor(std::string key, std::unique_ptr<IBattleEffectExecutor> executor);
    const IBattleEffectExecutor* executorFor(const std::string& key) const;

private:
    std::map<std::string, std::unique_ptr<IBattleEffectExecutor>> executors_;
};

class BattleEffectDispatcher
{
public:
    explicit BattleEffectDispatcher(const BattleEffectRegistry& registry);

    void addEffect(BattleEffectDefinition effect);
    std::vector<BattleEffectCommand> dispatch(BattleUnitStore& units,
                                              std::map<int, int>& activationCounts,
                                              const BattleEffectEvent& event) const;

private:
    const BattleEffectRegistry& registry_;
    std::vector<BattleEffectDefinition> effects_;
};

}  // namespace KysChess::Battle
