#include "ChessDrawableOnCall.h"

namespace KysChess {

ChessDrawableOnCall::ChessDrawableOnCall(std::function<void(DrawableOnCall*)> draw)
    : DrawableOnCall(draw)
{
}

void ChessDrawableOnCall::updateScreenWithContext(const DrawableItemContext& context)
{
    DrawableOnCall::updateScreenWithContext(context);
    chessContext_.itemIndex = context.itemIndex;
    chessContext_.previewData = {};
    if (context.itemIndex >= 0 && context.itemIndex < previewData_.size())
    {
        chessContext_.previewData = previewData_[context.itemIndex];
    }
}

void ChessDrawableOnCall::updateScreenWithChessContext(const ChessDrawableItemContext& context)
{
    DrawableOnCall::updateScreenWithContext(context);
    chessContext_ = context;
}

void ChessDrawableOnCall::setPreviewData(std::vector<Chess> previewData)
{
    previewData_ = std::move(previewData);
}

}