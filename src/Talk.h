#pragma once
#include "RunNode.h"
#include <optional>
#include <string>
#include <vector>

enum class TalkPointerAction { Ignore, Capture, Activate, Cancel };

class TalkPointerState
{
public:
    TalkPointerAction process(const PointerEvent& event)
    {
        const PointerIdentity identity{event.source, event.pointerId, event.button};
        if (event.phase == PointerPhase::ButtonDown && event.button == SDL_BUTTON_LEFT && !pointer_)
        {
            pointer_ = identity;
            return TalkPointerAction::Capture;
        }
        if (event.phase == PointerPhase::ButtonUp && pointer_ && *pointer_ == identity)
        {
            pointer_.reset();
            return TalkPointerAction::Activate;
        }
        if (event.phase == PointerPhase::Cancel && pointer_
            && pointer_->source == identity.source
            && pointer_->pointerId == identity.pointerId)
        {
            pointer_.reset();
            return TalkPointerAction::Cancel;
        }
        return TalkPointerAction::Ignore;
    }

private:
    std::optional<PointerIdentity> pointer_;
};

class Talk : public RunNode
{
public:
    Talk() {}
    Talk(std::string c, int h = -1) : Talk() { setContent(c); setHeadID(h); }
    virtual ~Talk() {}

    virtual void draw() override;
    virtual PointerResult onPointerEvent(const PointerEvent& event) override;
    //virtual void dealEvent(BP_Event& e) override;
    virtual void onPressedOK() override;
private:
    std::string content_;
    int head_id_ = -1;
    int head_style_ = 0;
    int current_line_ = 0;
    int width_ = 40;
    const int height_ = 4;    //每页行数
    std::vector<std::string> content_lines_;
    TalkPointerState pointer_state_;
public:
    void setContent(std::string c) { content_ = c; }
    void setHeadID(int h) { head_id_ = h; }
    void setHeadStyle(int s) { head_style_ = s; }
    virtual void onEntrance() override;

    DEFAULT_CANCEL_EXIT;
};
