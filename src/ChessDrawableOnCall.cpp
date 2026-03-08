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
}

void ChessDrawableOnCall::updateScreenWithChessContext(const ChessDrawableItemContext& context)
{
    DrawableOnCall::updateScreenWithContext(context);
    chessContext_ = context;
}

}