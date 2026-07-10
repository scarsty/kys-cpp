#pragma once

#include "BattleSceneControls.h"

#include <map>
#include <optional>
#include <vector>

enum class PaperCameraTouchActionType { ControlActivated, PanActivated, Pan, Pair };

struct PaperCameraTouchAction
{
    PaperCameraTouchActionType type{};
    BattleControlId control = BattleControlId::None;
    SDL_FPoint delta{};
    float previousSpan{};
    float currentSpan{};
};

class BattleScenePaperCameraTouch
{
public:
    std::vector<PaperCameraTouchAction> process(const TouchSample& sample, const BattleControlLayout& controls);
    void reset();
    std::size_t activeContactCount() const { return contacts_.size(); }

private:
    enum class Ownership { Ignored, Control, Camera };
    struct Contact
    {
        Ownership ownership{};
        SDL_FPoint position{};
        BattleControlId control = BattleControlId::None;
    };
    struct PairGeometry
    {
        SDL_FPoint centroid{};
        float span{};
    };

    std::vector<TouchFingerKey> cameraKeys() const;
    PairGeometry pairGeometry(const std::vector<TouchFingerKey>& keys) const;
    void rebaselineCamera();
    void handleDown(const TouchSample& sample, const BattleControlLayout& controls);
    std::vector<PaperCameraTouchAction> handleMotion(const TouchSample& sample);
    std::vector<PaperCameraTouchAction> handleUp(const TouchSample& sample, const BattleControlLayout& controls);
    void handleContactCanceled(const TouchSample& sample);

private:
    std::map<TouchFingerKey, Contact> contacts_;
    std::optional<TouchFingerKey> armedControlKey_;
    BattleControlId armedControl_ = BattleControlId::None;
    int controlContactCount_{};
    SDL_FPoint singleBaseline_{};
    SDL_FPoint singlePrevious_{};
    bool singlePanActive_{};
    PairGeometry pairPrevious_{};
    bool pairBaselineValid_{};
};
