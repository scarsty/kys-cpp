#pragma once
#include "DrawableOnCall.h"
#include "Chess.h"

struct Role;


namespace KysChess {

struct ChessDrawableItemContext : public DrawableItemContext {
    Chess previewData;
};

class ChessDrawableOnCall : public DrawableOnCall {
public:
    ChessDrawableOnCall(std::function<void(DrawableOnCall*)> draw);
    void updateScreenWithContext(const DrawableItemContext& context) override;
    void updateScreenWithChessContext(const ChessDrawableItemContext& context);
    Chess getPreviewData() const { return chessContext_.previewData; }

protected:
    ChessDrawableItemContext chessContext_;
};

}