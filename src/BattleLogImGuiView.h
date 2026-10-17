#pragma once

#include "BattleLogViewModel.h"

#include <string_view>
#include <functional>
#include <utility>

inline std::string_view battleLogCategoryFilterLabel(BattleLogEntryCategory category)
{
    switch (category)
    {
    case BattleLogEntryCategory::Any:
        return "全部類型";
    case BattleLogEntryCategory::Damage:
        return "傷害";
    case BattleLogEntryCategory::Heal:
        return "治療";
    case BattleLogEntryCategory::Status:
        return "狀態";
    case BattleLogEntryCategory::Cast:
        return "施放";
    case BattleLogEntryCategory::ProjectileCancel:
        return "彈道取消";
    case BattleLogEntryCategory::Lifecycle:
        return "擊倒";
    case BattleLogEntryCategory::BattleEnd:
        return "戰鬥結束";
    }
    return "全部類型";
}

struct BattleLogWindowState
{
    BattleLogViewModel model;
    int inputGuardFrames = 0;
    bool hoverGuard = false;
    bool dragging = false;
    int childFlip = 0;
    BattleLogFilter filter;
    bool followLatest = true;
    std::function<void()> togglePause;
};

class BattleLogImGuiView
{
public:
    void show(BattleLogWindowState& state, const BattleLogViewModel& model, std::function<void()> togglePause = {}) const
    {
        state.model = model;
        state.model.open = true;
        state.inputGuardFrames = 10;
        state.hoverGuard = true;
        state.dragging = false;
        state.childFlip ^= 1;
        state.filter = {};
        state.followLatest = true;
        state.togglePause = std::move(togglePause);
    }

    void update(BattleLogWindowState& state, BattleLogViewModel model) const
    {
        model.open = state.model.open;
        state.model = std::move(model);
    }

    void hide(BattleLogWindowState& state) const
    {
        state.model.open = false;
        state.togglePause = {};
        state.inputGuardFrames = 0;
        state.hoverGuard = false;
        state.dragging = false;
    }

    bool isOpen(const BattleLogWindowState& state) const
    {
        return state.model.open;
    }

    void render(BattleLogWindowState& state) const;
};
