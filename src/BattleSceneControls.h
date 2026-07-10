#pragma once

#include "PointerInput.h"

#include <map>
#include <optional>

enum class BattleControlId { None, Speed, PaperView, CameraMode, Log, Pause };

class BattleControlLayout
{
public:
    void set(BattleControlId id, SDL_Rect rect) { rects_[id] = rect; }
    std::optional<SDL_Rect> rect(BattleControlId id) const;
    BattleControlId hitTest(SDL_FPoint point) const;

private:
    std::map<BattleControlId, SDL_Rect> rects_;
};

BattleControlLayout makeBattleControlLayout(int uiWidth, bool logVisible, bool cameraModeVisible);

class BattleControlCapture
{
public:
    BattleControlId onDown(PointerIdentity pointer, SDL_FPoint point, const BattleControlLayout& layout);
    BattleControlId onUp(PointerIdentity pointer, SDL_FPoint point, const BattleControlLayout& layout);
    void cancel(PointerIdentity pointer);
    void reset();
    bool owns(PointerIdentity pointer) const { return pointer_ && *pointer_ == pointer; }

private:
    std::optional<PointerIdentity> pointer_;
    BattleControlId armed_ = BattleControlId::None;
};
