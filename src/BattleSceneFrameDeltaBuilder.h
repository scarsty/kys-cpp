#pragma once

#include "BattleSceneUnitStore.h"
#include "Point.h"
#include "Random.h"
#include "battle/BattlePresentation.h"

#include <optional>
#include <string>
#include <vector>

struct BattleSceneFrameBloodEffectCommand
{
    int followUnitId = -1;
    std::string path;
};

struct BattleSceneFrameHurtFlashCommand
{
    int unitId = -1;
    int frames = 0;
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
    std::vector<BattleSceneFrameHurtFlashCommand> hurtFlashes;
    std::vector<int> effectSoundIds;
    std::vector<int> attackSoundIds;
    std::vector<KysChess::Battle::BattleFrameRumbleEvent> rumbles;
};

struct BattleSceneFrameDeltaBuildContext
{
    const BattleSceneUnitStore* units = nullptr;
    RandomDouble* random = nullptr;
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
        const KysChess::Battle::BattlePresentationFrame& frame,
        int currentBattleResult,
        BattleSceneFrameDeltaBuildContext context) const;

private:
    void collectDamageSceneEffects(
        const KysChess::Battle::BattlePresentationFrame& frame,
        int currentBattleResult,
        BattleSceneFrameDeltaBuildContext& context,
        BattleSceneFrameDelta& result) const;
};
