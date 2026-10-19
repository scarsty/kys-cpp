#pragma once

#include "PointerInput.h"

#include <map>
#include <optional>
#include <span>
#include <vector>

enum class BattleControlId { None, Speed, PaperView, CameraMode, Log, Pause, Surrender,
    InspectorPrevious, InspectorNext, InspectorBody, InspectorEffects };

class BattleControlLayout
{
public:
    void set(BattleControlId id, SDL_Rect rect) { rects_[id] = rect; }
    std::optional<SDL_Rect> rect(BattleControlId id) const;
    BattleControlId hitTest(SDL_FPoint point) const;

private:
    std::map<BattleControlId, SDL_Rect> rects_;
};

BattleControlLayout makeBattleControlLayout(int uiWidth);

struct BattleInspectorPage
{
    int firstRow{};
    int rowCount{};
    int height{};
};

// 固定邏輯畫布；左欄獨佔垂直空間，底欄不隨效果數量移動。
struct BattleInspectorLayout
{
    static constexpr SDL_Rect summary{304, 560, 960, 144};
    // 左下保留瀏覽器浮動視窗控制的位置。
    static constexpr SDL_Rect previous{104, 656, 40, 36};
    static constexpr SDL_Rect next{240, 656, 40, 36};
    static constexpr int effectGap = 4;
    static constexpr int effectHeader = 36;
    static constexpr int effectFooter = 52;
    static constexpr int effectCapacity = 688 - effectHeader - effectFooter;
    SDL_Rect effects{};
    std::vector<BattleInspectorPage> pages;
    int page{};
};

BattleInspectorLayout makeBattleInspectorLayout(std::span<const int> cardHeights, int page);
void addBattleInspectorControls(BattleControlLayout& controls, const BattleInspectorLayout& inspector);

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
