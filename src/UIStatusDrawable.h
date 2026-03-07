#pragma once
#include "ChessDrawableOnCall.h"
#include "ChessUIStatus.h"
#include <memory>
#include <vector>

namespace KysChess {

class UIStatusDrawable : public ChessDrawableOnCall {
public:
    UIStatusDrawable();
    UIStatusDrawable(const std::vector<Chess>& previewData);
    void updateScreenWithContext(const DrawableItemContext& context) override;

    ChessUIStatus& getUIStatus() { return *uiStatus_; }

private:
    std::shared_ptr<ChessUIStatus> uiStatus_;
    std::vector<Chess> previewData_;
};

}