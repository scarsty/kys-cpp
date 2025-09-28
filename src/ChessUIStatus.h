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

    void setRole(Role* r) { role_ = r; }
    virtual void draw() override;

private:
    Role* role_ = nullptr;
};