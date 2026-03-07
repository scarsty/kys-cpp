#pragma once
#include "RunNode.h"
#include "Save.h"
#include "Font.h"
#include "TextureManager.h"
#include "Chess.h"

namespace KysChess {

class ChessUIStatus : public RunNode
{
public:
    ChessUIStatus() = default;
    virtual ~ChessUIStatus() = default;

    void setChess(Chess chess) { chess_ = chess; }
    virtual void draw() override;

private:
    Chess chess_;
};

}