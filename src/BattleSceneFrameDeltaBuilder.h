#pragma once

#include "BattleSceneUnitStore.h"
#include "ChessCombo.h"
#include "Point.h"
#include "Random.h"
#include "battle/BattleCore.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace KysChess
{
struct RoleComboState;
}

struct BattleSceneFrameBloodEffectCommand
{
    int followUnitId = -1;
    std::string path;
};

struct BattleSceneFrameDelta
{
    bool battleEnded = false;
    int battleResult = -1;
    bool unitDied = false;
    std::vector<int> diedUnitIds;
    int sceneShake = 0;
    int frozenFrames = 0;
    int slowFrames = 0;
    int closeUpFrames = 0;
    std::optional<Pointf> cameraFocus;
    int jitterX = 0;
    int jitterY = 0;
    std::vector<BattleSceneFrameBloodEffectCommand> bloodEffects;
    std::vector<int> effectSoundIds;
    std::vector<int> attackSoundIds;
    std::vector<KysChess::Battle::BattleFrameRumbleEvent> rumbles;
    std::vector<KysChess::Battle::BattleLogEvent> logEvents;
};

struct BattleSceneFrameDeltaBuildContext
{
    const BattleSceneUnitStore* units = nullptr;
    std::map<int, KysChess::RoleComboState>* comboStates = nullptr;
    std::unordered_map<int, int>* hurtFlashTimers = nullptr;
    RandomDouble* random = nullptr;
    std::function<std::vector<KysChess::AntiComboTransferEvent>(int)> transferAntiCombo;
    bool manualCameraEnabled = false;
    int hurtFlashDuration = 0;
    int blinkSoundEffectId = -1;
    int deathZoomFrames = 0;
    int battleEndZoomFrames = 0;
    int deathSlowFrames = 0;
    int battleEndSlowFrames = 0;
};

class BattleSceneFrameDeltaBuilder
{
public:
    BattleSceneFrameDelta build(
        const KysChess::Battle::BattleFrameResult& frameResult,
        int currentBattleResult,
        BattleSceneFrameDeltaBuildContext context) const;

private:
    void collectDamageSceneEffects(
        const KysChess::Battle::BattleFrameResult& frameResult,
        int currentBattleResult,
        BattleSceneFrameDeltaBuildContext& context,
        BattleSceneFrameDelta& result) const;
    void collectFrameApplicationSceneEffects(
        const KysChess::Battle::BattleFrameApplications& applications,
        BattleSceneFrameDelta& result) const;
    void collectActionSceneEffects(
        const std::vector<KysChess::Battle::BattleFrameActionUnitResult>& actions,
        const BattleSceneFrameDeltaBuildContext& context,
        BattleSceneFrameDelta& result) const;
};