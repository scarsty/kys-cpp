#include "UIStatusDrawable.h"

namespace KysChess {

UIStatusDrawable::UIStatusDrawable()
    : ChessDrawableOnCall([this](DrawableOnCall*) { if (uiStatus_) uiStatus_->draw(); }),
      uiStatus_(std::make_shared<ChessUIStatus>()) {
}

UIStatusDrawable::UIStatusDrawable(const std::vector<Chess>& previewData)
    : ChessDrawableOnCall([this](DrawableOnCall*) { if (uiStatus_) uiStatus_->draw(); }),
      uiStatus_(std::make_shared<ChessUIStatus>()),
      previewData_(previewData) {
}

void UIStatusDrawable::updateScreenWithContext(const DrawableItemContext& context) {
    ChessDrawableItemContext chessContext{context};
    chessContext.itemIndex = context.itemIndex;

    if (chessContext.itemIndex >= 0 && chessContext.itemIndex < previewData_.size()) {
        chessContext.previewData = previewData_[context.itemIndex];
    }

    ChessDrawableOnCall::updateScreenWithChessContext(chessContext);

    uiStatus_->setChess(chessContext.previewData);
}

}