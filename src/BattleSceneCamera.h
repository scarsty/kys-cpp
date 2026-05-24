#pragma once

#include "BattleSceneUnitStore.h"
#include "Event.h"
#include "Point.h"

#include <optional>

struct BattleSceneCameraBounds
{
    float sceneWidth = 0.0f;
    float sceneHeight = 0.0f;
    float renderCenterX = 0.0f;
    float renderCenterY = 0.0f;
};

class BattleSceneCamera
{
public:
    std::optional<Pointf> handleManualInput(
        const EngineEvent& event,
        const Pointf& center,
        BattleSceneCameraBounds bounds);
    Pointf updateAuto(
        const Pointf& center,
        const BattleSceneUnitStore& units,
        BattleSceneCameraBounds bounds);
    void focusOn(const Pointf& focusPoint, int zoomFrames);
    void reset(Pointf center);
    void decreaseCloseUp();
    static Pointf clampCenter(Pointf center, BattleSceneCameraBounds bounds);

    bool closeUpActive() const;
    int closeUpFrames() const;
    int closeUpTotalFrames() const;

private:
    bool manualDragging_ = false;
    Pointf target_;
    int closeUp_ = 0;
    int closeUpTotal_ = 0;
};
