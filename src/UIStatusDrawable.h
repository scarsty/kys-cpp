#pragma once
#include "DrawableOnCall.h"
#include "ChessUIStatus.h"
#include <memory>

class UIStatusDrawable : public DrawableOnCall {
public:
    UIStatusDrawable();
    void updateScreenWithID(int id) override;

    ChessUIStatus& getUIStatus() { return *uiStatus_; }

private:
    std::shared_ptr<ChessUIStatus> uiStatus_;
};