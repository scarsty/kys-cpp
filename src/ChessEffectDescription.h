#pragma once

#include "ChessGameplayEffect.h"
#include "ChessNonBattleRules.h"
#include "DisplayText.h"

namespace KysChess
{

enum class EffectDescriptionRowKind
{
    Field,
    ListItem,
    Heading,
    Prose,
    Summary,
};

enum class EffectDescriptionSemanticBreak
{
    None,
    Block,
    Branch,
    Sequence,
    ActionGroup,
    Qualifier,
};

struct RenderedEffectDescriptionRow
{
    EffectDescriptionRowKind kind{};
    std::string text;
    int indent{};
    EffectDescriptionSemanticBreak breakBefore{};
    DisplayTextWrapping wrapping{};
};

struct RenderedEffectDescriptionBlock
{
    std::vector<RenderedEffectDescriptionRow> rows;
};

struct RenderedEffectDescriptionSection
{
    std::optional<std::string> heading;
    std::vector<RenderedEffectDescriptionBlock> blocks;
};

struct RenderedEffectDescription
{
    std::vector<RenderedEffectDescriptionSection> sections;
};

RenderedEffectDescription describeGameplayEffects(std::span<const GameplayEffect> effects,
                                                  EffectDescriptionStyle style);

RenderedEffectDescription describeGameplayEffectsAndManagementRules(
    std::span<const GameplayEffect> effects,
    std::span<const ChessNonBattleRule> managementRules,
    EffectDescriptionStyle style);

void appendStandaloneDescriptionRow(
    RenderedEffectDescription& description,
    std::string text);

std::vector<std::string> effectDescriptionTextRows(const RenderedEffectDescription& rendered);
std::string joinEffectDescriptionRows(const RenderedEffectDescription& rendered, std::string_view separator = "\n");

}    // namespace KysChess
