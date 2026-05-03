#include "BattleEffectSystem.h"

#include <algorithm>
#include <utility>

namespace KysChess::Battle
{

BattleEffectContext::BattleEffectContext(BattleEffectWorld& world,
                                         const BattleEffectEvent& event,
                                         const BattleEffectDefinition& effect)
    : world_(world),
      event_(event),
      effect_(effect)
{
}

BattleEffectWorld& BattleEffectContext::world()
{
    return world_;
}

const BattleEffectEvent& BattleEffectContext::event() const
{
    return event_;
}

const BattleEffectDefinition& BattleEffectContext::effect() const
{
    return effect_;
}

std::vector<BattleEffectUnit*> BattleEffectContext::targets() const
{
    switch (effect_.target)
    {
    case BattleEffectTarget::Self:
    case BattleEffectTarget::Source:
        if (auto* source = findUnit(event_.sourceUnitId))
        {
            return { source };
        }
        return {};
    case BattleEffectTarget::Target:
        if (auto* target = findUnit(event_.targetUnitId))
        {
            return { target };
        }
        return {};
    case BattleEffectTarget::SourceTeam:
        if (auto* source = findUnit(event_.sourceUnitId))
        {
            return teamUnits(source->team);
        }
        return {};
    case BattleEffectTarget::TargetTeam:
        if (auto* target = findUnit(event_.targetUnitId))
        {
            return teamUnits(target->team);
        }
        return {};
    }
    return {};
}

void BattleEffectContext::emit(BattleEffectCommand command)
{
    command.effectId = effect_.id;
    if (command.sourceUnitId < 0)
    {
        command.sourceUnitId = event_.sourceUnitId;
    }
    commands_.push_back(std::move(command));
}

const std::vector<BattleEffectCommand>& BattleEffectContext::commands() const
{
    return commands_;
}

BattleEffectUnit* BattleEffectContext::findUnit(int id) const
{
    auto it = std::find_if(world_.units.begin(), world_.units.end(), [&](const BattleEffectUnit& unit)
        {
            return unit.id == id && unit.alive;
        });
    return it == world_.units.end() ? nullptr : &*it;
}

std::vector<BattleEffectUnit*> BattleEffectContext::teamUnits(int team) const
{
    std::vector<BattleEffectUnit*> units;
    for (auto& unit : world_.units)
    {
        if (unit.alive && unit.team == team)
        {
            units.push_back(&unit);
        }
    }
    return units;
}

void HealPercentExecutor::execute(BattleEffectContext& context) const
{
    for (auto* target : context.targets())
    {
        int before = target->hp;
        int amount = std::max(1, target->maxHp * context.effect().value / 100);
        target->hp = std::min(target->maxHp, target->hp + amount);
        int healed = target->hp - before;
        if (healed > 0)
        {
            context.emit({
                BattleEffectCommandType::Heal,
                context.effect().id,
                context.event().sourceUnitId,
                target->id,
                healed,
                context.effect().type,
            });
        }
    }
}

void AddShieldExecutor::execute(BattleEffectContext& context) const
{
    for (auto* target : context.targets())
    {
        target->shield += std::max(0, context.effect().value);
        context.emit({
            BattleEffectCommandType::AddShield,
            context.effect().id,
            context.event().sourceUnitId,
            target->id,
            context.effect().value,
            context.effect().type,
        });
    }
}

void AddInvincibilityExecutor::execute(BattleEffectContext& context) const
{
    for (auto* target : context.targets())
    {
        int before = target->invincible;
        target->invincible = std::max(target->invincible, context.effect().value);
        if (target->invincible > before)
        {
            context.emit({
                BattleEffectCommandType::AddInvincibility,
                context.effect().id,
                context.event().sourceUnitId,
                target->id,
                target->invincible - before,
                context.effect().type,
            });
        }
    }
}

void ModifyCooldownPercentExecutor::execute(BattleEffectContext& context) const
{
    for (auto* target : context.targets())
    {
        if (target->cooldown <= 0)
        {
            continue;
        }

        int before = target->cooldown;
        target->cooldown = static_cast<int>(target->cooldown * (1.0 - context.effect().value / 100.0));
        target->cooldown = std::max(0, target->cooldown);
        if (target->cooldown != before)
        {
            context.emit({
                BattleEffectCommandType::ModifyCooldown,
                context.effect().id,
                context.event().sourceUnitId,
                target->id,
                target->cooldown - before,
                context.effect().type,
            });
        }
    }
}

void DedicatedEffectExecutor::execute(BattleEffectContext& context) const
{
    for (auto* target : context.targets())
    {
        context.emit({
            BattleEffectCommandType::DedicatedEffect,
            context.effect().id,
            context.event().sourceUnitId,
            target->id,
            context.effect().value,
            context.effect().type,
        });
    }
}

BattleEffectRegistry::BattleEffectRegistry()
{
    registerExecutor("治疗百分比", std::make_unique<HealPercentExecutor>());
    registerExecutor("添加护盾", std::make_unique<AddShieldExecutor>());
    registerExecutor("添加无敌帧", std::make_unique<AddInvincibilityExecutor>());
    registerExecutor("冷却百分比修正", std::make_unique<ModifyCooldownPercentExecutor>());
    registerExecutor("专用执行器", std::make_unique<DedicatedEffectExecutor>());
}

void BattleEffectRegistry::registerExecutor(std::string key, std::unique_ptr<IBattleEffectExecutor> executor)
{
    executors_[std::move(key)] = std::move(executor);
}

const IBattleEffectExecutor* BattleEffectRegistry::executorFor(const std::string& key) const
{
    auto it = executors_.find(key);
    return it == executors_.end() ? nullptr : it->second.get();
}

BattleEffectDispatcher::BattleEffectDispatcher(const BattleEffectRegistry& registry)
    : registry_(registry)
{
}

void BattleEffectDispatcher::addEffect(BattleEffectDefinition effect)
{
    effects_.push_back(std::move(effect));
}

std::vector<BattleEffectCommand> BattleEffectDispatcher::dispatch(BattleEffectWorld& world,
                                                                  const BattleEffectEvent& event) const
{
    std::vector<BattleEffectCommand> commands;
    for (const auto& effect : effects_)
    {
        if (effect.hook != event.hook)
        {
            continue;
        }
        if (effect.maxCount > 0 && world.activationCounts[effect.id] >= effect.maxCount)
        {
            continue;
        }

        auto* executor = registry_.executorFor(effect.executorKey);
        if (!executor)
        {
            continue;
        }

        BattleEffectContext context(world, event, effect);
        executor->execute(context);
        if (!context.commands().empty())
        {
            world.activationCounts[effect.id]++;
            commands.insert(commands.end(), context.commands().begin(), context.commands().end());
        }
    }
    return commands;
}

}  // namespace KysChess::Battle
