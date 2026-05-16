#pragma once

#include "BattleLogViewModel.h"

#include <string_view>

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
};

class BattleLogImGuiView
{
public:
    void show(BattleLogWindowState& state, const BattleLogViewModel& model) const
    {
        state.model = model;
        state.model.open = true;
        state.inputGuardFrames = 10;
        state.hoverGuard = true;
        state.dragging = false;
        state.childFlip ^= 1;
        state.filter = {};
    }

    void hide(BattleLogWindowState& state) const
    {
        state.model.open = false;
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
