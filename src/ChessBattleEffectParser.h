#pragma once

#include "ChessBattleEffectTypes.h"
#include "ChessDiagnostics.h"

namespace YAML { class Node; }

namespace KysChess
{

bool parseEffectRule(
    const YAML::Node& node,
    EffectRule& out,
    EffectRuleId id,
    const std::string& context,
    const ChessDiagnosticSink& diagnostics = {});
bool validateEffectAuthoringDescriptorProbes(std::string& error);
bool parseMagicEffects(
    const YAML::Node& root,
    std::vector<ChessMagicEffectDefinition>& out,
    const std::string& context,
    const ChessDiagnosticSink& diagnostics = {});
bool loadMagicEffectsFile(
    const std::string& path,
    std::vector<ChessMagicEffectDefinition>& out,
    const ChessDiagnosticSink& diagnostics = {});

}  // namespace KysChess
