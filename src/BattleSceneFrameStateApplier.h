#pragma once

#include "BattleSceneUnitStore.h"
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

struct BattleSceneFrameStateApplyResult
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
};

struct BattleSceneFrameStateApplyContext
{
    BattleSceneUnitStore* units = nullptr;
    std::map<int, KysChess::RoleComboState>* comboStates = nullptr;
    std::unordered_map<int, int>* hurtFlashTimers = nullptr;
    RandomDouble* random = nullptr;
    std::function<Pointf(int, int)> pos45To90;
    std::function<void(int)> transferAntiCombo;
    double gravity = 0.0;
    bool manualCameraEnabled = false;
    int hurtFlashDuration = 0;
    int blinkSoundEffectId = -1;
    int deathZoomFrames = 0;
    int battleEndZoomFrames = 0;
    int deathSlowFrames = 0;
    int battleEndSlowFrames = 0;
};

class BattleSceneFrameStateApplier
{
public:
    BattleSceneFrameStateApplyResult apply(
        const KysChess::Battle::BattleFrameResult& frameResult,
        int currentBattleResult,
        BattleSceneFrameStateApplyContext context) const;

private:
    void applyUnitApplications(
        const std::vector<KysChess::Battle::BattleFrameUnitApplication>& applications,
        BattleSceneFrameStateApplyContext& context) const;
    void applyStatusState(
        const KysChess::Battle::BattleFrameStateApplications& applications,
        BattleSceneFrameStateApplyContext& context) const;
    void applyDamageTransactions(
        const KysChess::Battle::BattleFrameResult& frameResult,
        int currentBattleResult,
        BattleSceneFrameStateApplyContext& context,
        BattleSceneFrameStateApplyResult& result) const;
    void applyTeamEffectState(
        const std::vector<KysChess::Battle::BattleTeamEffectEvent>& events,
        BattleSceneFrameStateApplyContext& context) const;
    void applyFrameApplications(
        const KysChess::Battle::BattleFrameApplications& applications,
        BattleSceneFrameStateApplyContext& context,
        BattleSceneFrameStateApplyResult& result) const;
    void applyMovementPresentation(
        const std::vector<KysChess::Battle::BattleFrameMovementPresentationUnitResult>& movements,
        BattleSceneFrameStateApplyContext& context) const;
    void applyActionResults(
        const std::vector<KysChess::Battle::BattleFrameActionUnitResult>& actions,
        BattleSceneFrameStateApplyContext& context,
        BattleSceneFrameStateApplyResult& result) const;
};
