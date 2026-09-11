#pragma once

#include "ChessBattleEffectTypes.h"
#include "ChessDiagnostics.h"

namespace YAML { class Node; }

namespace KysChess
{

// 卡片摘要只供玩家速讀；數值引用在載入時由同一份效果配置解析。
bool parseEffectCardSummary(
    const YAML::Node& container,
    std::vector<std::string>& out,
    const std::string& context,
    const ChessDiagnosticSink& diagnostics = {});

bool parseEffectRule(
    const YAML::Node& node,
    EffectRule& out,
    EffectRuleId id,
    const std::string& context,
    const ChessDiagnosticSink& diagnostics = {},
    bool statusBehavior = false);
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
