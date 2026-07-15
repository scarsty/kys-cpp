#pragma once

#include "ChessCatalogQueries.h"
#include "HeadlessBattleRunner.h"
#include "PreparedChessBattle.h"

#include <optional>
#include <string>
#include <vector>

namespace KysChess
{

struct ChessBattleSkillDamage
{
    int skillId = -1;
    std::string name;
    int damage{};
};

struct ChessBattleDamageBreakdown
{
    int skill{};
    int basicAttack{};
    int status{};
    int combo{};
    int equipment{};
    int other{};
};

struct ChessBattleSurvivorAnalysis
{
    int unitId = -1;
    int chessInstanceId = -1;
    int roleId = -1;
    std::string name;
    int team = -1;
    int hp{};
    int mp{};
    bool summoned{};
};

struct ChessBattleUnitAnalysis
{
    int unitId = -1;
    int roleId = -1;
    std::string name;
    int team = -1;
    int star = 1;
    int damageDealt{};
    int damageTaken{};
    int kills{};
    int healingDone{};
    int magicPointsRestored{};
    int magicPointsDrained{};
    int magicPointsDrainEvents{};
    int projectilePotentialDamageCancelled{};
    int projectileCancellations{};
    int blocks{};
    int dodges{};
    int poisonPayloadEvents{};
    int poisonApplicationEvents{};
    int poisonTicks{};
    int poisonDamage{};
    int bleedApplications{};
    int hitstunApplications{};
    int hitstunDurationFrames{};
    int stunApplications{};
    int stunDurationFrames{};
    int knockbackApplications{};
    int magicBlockApplications{};
    int magicBlockDurationFrames{};
    int cooldownManipulations{};
    int cooldownManipulationFrames{};
    int invulnerabilityTriggers{};
    int deathPreventionTriggers{};
    ChessCalculatedStats initialCombatStats;
    ChessCalculatedStats initialStatDeltaFromSpecialEffects;
    int enemyAttackDebuff{};
    int enemyDefenceDebuff{};
    ChessBattleDamageBreakdown damageBreakdown;
    std::vector<ChessBattleSkillDamage> skillDamage;
    std::vector<ChessBattleSkillDamage> nonSkillDamageSources;
};

struct ChessBattleEffectActivation
{
    std::string type;
    std::string description;
    int frame{};
    std::optional<int> sourceUnitId;
    std::optional<std::string> sourceName;
    std::optional<int> sourceTeam;
    std::optional<std::string> sourceKind;
    std::optional<int> targetUnitId;
    std::optional<std::string> targetName;
    std::optional<int> targetTeam;
    std::optional<int> value;
    std::optional<int> durationFrames;
    std::optional<int> previousValue;
    std::optional<int> newValue;
    std::optional<int> delta;
    std::optional<int> abilityId;
    std::optional<std::string> abilityName;
    std::optional<int> poisonPercent;
    std::optional<int> scheduledTicks;
    std::optional<int> sourceProjectileId;
    std::optional<int> opposingProjectileId;
    std::optional<int> sourceValueBefore;
    std::optional<int> opposingValueBefore;
    std::optional<int> cancelledPotentialDamage;
    std::optional<int> sourceValueAfter;
    std::optional<int> opposingValueAfter;
};

struct ChessBattleImportantEffect
{
    std::string type;
    std::optional<int> sourceUnitId;
    std::optional<std::string> sourceName;
    std::optional<int> sourceTeam;
    std::optional<std::string> sourceKind;
    std::optional<int> targetUnitId;
    std::optional<std::string> targetName;
    std::optional<int> targetTeam;
    std::optional<int> count;
    std::optional<int> value;
    std::optional<int> attackDelta;
    std::optional<int> defenceDelta;
    std::string description;
};

struct ChessBattleKeyEvent
{
    int frame{};
    std::string description;
};

struct ChessBattleResultAnalysis
{
    Battle::BattleOutcome outcome = Battle::BattleOutcome::InProgress;
    std::string outcomeDescription;
    int endFrame{};
    std::vector<ChessBattleSurvivorAnalysis> survivors;
    std::vector<ChessBattleUnitAnalysis> unitStats;
    std::vector<ChessBattleImportantEffect> importantEffects;
    std::vector<ChessBattleEffectActivation> effectActivations;
    std::vector<ChessBattleKeyEvent> keyEvents;
    std::string summary;
    ChessSha256 digest{};
};

std::string chessBattleTeamName(int team);
std::string chessBattleOutcomeDescription(Battle::BattleOutcome outcome);
ChessBattleResultAnalysis analyzeChessBattleResult(
    const ChessGameContent& content,
    const PreparedChessBattle& prepared,
    const HeadlessBattleResult& battle);

}  // namespace KysChess
