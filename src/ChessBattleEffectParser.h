#pragma once

#include "ChessGameplayEffect.h"

namespace KysChess
{

bool parseMagicEffects(const YAML::Node& root, std::vector<ChessMagicEffectDefinition>& out, const std::string& context,
                       const ChessDiagnosticSink& diagnostics = {});
bool loadMagicEffectsFile(const std::string& path, std::vector<ChessMagicEffectDefinition>& out,
                          const ChessDiagnosticSink& diagnostics = {});

}    // namespace KysChess
