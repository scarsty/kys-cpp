#pragma once
#include "RunNode.h"
#include "Save.h"
#include "Font.h"
#include "TextureManager.h"

class ChessUIStatus : public RunNode
{
public:
    ChessUIStatus() = default;
    virtual ~ChessUIStatus() = default;

    void setRole(Role* r, int star = 0) { role_ = r; star_ = star; }
    virtual void draw() override;

private:
    Role* role_ = nullptr;
    int star_ = 0;
};