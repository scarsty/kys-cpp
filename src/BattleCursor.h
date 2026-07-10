#pragma once
#include "RunNode.h"
#include "Head.h"
#include "Types.h"
#include "UIStatus.h"

#include <optional>

class BattleScene;

enum class BattleCursorPointerAction { Ignore, Update, UpdateAndCapture, UpdateAndConfirm, Cancel };

class BattleCursorPointerState
{
public:
    BattleCursorPointerAction process(const PointerEvent& event)
    {
        const PointerIdentity identity{event.source, event.pointerId, event.button};
        if (event.phase == PointerPhase::Motion)
        {
            return BattleCursorPointerAction::Update;
        }
        if (event.phase == PointerPhase::ButtonDown && event.button == SDL_BUTTON_LEFT)
        {
            if (!event.insidePresent || pointer_) return BattleCursorPointerAction::Ignore;
            pointer_ = identity;
            return BattleCursorPointerAction::UpdateAndCapture;
        }
        if (event.phase == PointerPhase::ButtonUp && pointer_ && *pointer_ == identity)
        {
            pointer_.reset();
            return BattleCursorPointerAction::UpdateAndConfirm;
        }
        if (event.phase == PointerPhase::Cancel && pointer_
            && pointer_->source == identity.source
            && pointer_->pointerId == identity.pointerId)
        {
            pointer_.reset();
            return BattleCursorPointerAction::Cancel;
        }
        return BattleCursorPointerAction::Ignore;
    }

private:
    std::optional<PointerIdentity> pointer_;
};

//因为战斗场景的操作分为多种情况，写在原处比较麻烦，故单独列出一类用以操作光标
//注意，AI选择目标的行为也在这里面
class BattleCursor : public RunNode
{
public:
    BattleCursor(BattleScene* b);
    ~BattleCursor();

    int *select_x_ = nullptr, *select_y_ = nullptr;

    Role* role_ = nullptr;
    Magic* magic_ = nullptr;
    int level_index_ = 0;
    void setRoleAndMagic(Role* r, Magic* m = nullptr, int l = 0);

    std::shared_ptr<Head> head_selected_;
    //void setHead(Head* h) { head_selected_ = h; }
    std::shared_ptr<Head> getHead() { return head_selected_; }

    std::shared_ptr<UIStatus> ui_status_;
    std::shared_ptr<UIStatus> getUIStatus() { return ui_status_; }

    int mode_ = Move;
    enum
    {
        Other = -1,
        Move,
        Action,
        Check,
    };
    void setMode(int m) { mode_ = m; }
    int getMode() { return mode_; }

    BattleScene* battle_scene_ = nullptr;

    virtual void dealEvent(EngineEvent& e) override;
    PointerResult onPointerEvent(const PointerEvent& event) override;

    void setCursor(int x, int y);
    void updateCursorFromPointer(SDL_FPoint position);

    virtual void onEntrance() override;

    virtual void onPressedOK() override { exitWithResult(0); }
    virtual void onPressedCancel() override { exitWithResult(-1); }

private:
    BattleCursorPointerState pointer_state_;
};
